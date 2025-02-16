// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/multiplexer.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/pipe_socket.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/action.hpp"
#include "caf/actor_system.hpp"
#include "caf/async/execution_context.hpp"
#include "caf/config.hpp"
#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/detail/critical.hpp"
#include "caf/detail/latch.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/log/net.hpp"
#include "caf/log/system.hpp"
#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/unordered_flat_map.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#ifndef CAF_WINDOWS
#  include <poll.h>
#  include <signal.h>
#else
#  include "caf/internal/socket_sys_includes.hpp"
#endif // CAF_WINDOWS

namespace caf::net {

namespace {

class default_multiplexer;

#ifndef POLLRDHUP
#  define POLLRDHUP POLLHUP
#endif

#ifndef POLLPRI
#  define POLLPRI POLLIN
#endif

#ifdef CAF_WINDOWS
// From the MSDN: If the POLLPRI flag is set on a socket for the Microsoft
//                Winsock provider, the WSAPoll function will fail.
const short input_mask = POLLIN;
#else
const short input_mask = POLLIN | POLLPRI;
#endif

const short error_mask = POLLRDHUP | POLLERR | POLLHUP | POLLNVAL;

const short output_mask = POLLOUT;

class pollset_updater : public socket_event_layer {
public:
  // -- member types -----------------------------------------------------------

  using super = socket_manager;

  using msg_buf = std::array<std::byte, sizeof(intptr_t) + 1>;

  enum class code : uint8_t {
    start_manager,
    shutdown_reading,
    shutdown_writing,
    run_action,
    delay_action,
    shutdown,
  };

  // -- constructors, destructors, and assignment operators --------------------

  explicit pollset_updater(pipe_socket fd) : fd_(fd) {
    // nop
  }

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<pollset_updater> make(pipe_socket fd) {
    return std::make_unique<pollset_updater>(fd);
  }

  // -- implementation of socket_event_layer -----------------------------------

  error start(socket_manager* owner) override;

  socket handle() const override {
    return fd_;
  }

  void handle_read_event() override;

  void handle_write_event() override {
    owner_->deregister_writing();
  }

  void abort(const error&) override {
    // nop
  }

private:
  pipe_socket fd_;
  socket_manager* owner_ = nullptr;
  default_multiplexer* mpx_ = nullptr;
  msg_buf buf_;
  size_t buf_size_ = 0;
};

/// Multiplexes any number of ::socket_manager objects with a ::socket.
class default_multiplexer : public detail::atomic_ref_counted,
                            public multiplexer {
public:
  // -- member types -----------------------------------------------------------

  struct poll_update {
    short events = 0;
    socket_manager_ptr mgr;
  };

  using poll_update_map = unordered_flat_map<socket, poll_update>;

  using pollfd_list = std::vector<pollfd>;

  using manager_list = std::vector<socket_manager_ptr>;

  // -- friends ----------------------------------------------------------------

  friend class pollset_updater; // Needs access to the `do_*` functions.

  // -- constructors, destructors, and assignment operators --------------------

  explicit default_multiplexer(middleman* parent) : owner_(parent) {
    // nop
  }

  // -- implementation of caf::net::multiplexer --------------------------------

  error init() override {
    auto pipe_handles = make_pipe();
    if (!pipe_handles)
      return std::move(pipe_handles.error());
    auto updater = pollset_updater::make(pipe_handles->first);
    auto mgr = socket_manager::make(this, std::move(updater));
    if (auto err = mgr->start()) {
      close(pipe_handles->second);
      return err;
    }
    write_handle_ = pipe_handles->second;
    pollset_.emplace_back(pollfd{pipe_handles->first.id, input_mask, 0});
    managers_.emplace_back(mgr);
    return none;
  }

  size_t num_socket_managers() const noexcept override {
    return managers_.size();
  }

  middleman& owner() override {
    CAF_ASSERT(owner_ != nullptr);
    return *owner_;
  }
  actor_system& system() override {
    return owner().system();
  }

  // -- implementation of execution_context ------------------------------------

  void ref_execution_context() const noexcept override {
    ref();
  }

  void deref_execution_context() const noexcept override {
    deref();
  }

  void schedule(action what) override {
    auto lg = log::net::trace("");
    if (std::this_thread::get_id() == tid_) {
      pending_actions.push_back(what);
    } else {
      auto ptr = std::move(what).as_intrusive_ptr().release();
      write_to_pipe(pollset_updater::code::run_action, ptr);
    }
  }

  void schedule(steady_time_point when, action what) override {
    auto lg = log::net::trace("");
    if (std::this_thread::get_id() == tid_) {
      scheduled_actions.emplace(when, std::move(what));
    } else {
      auto ptr = new scheduled_actions_map::value_type{when, std::move(what)};
      write_to_pipe(pollset_updater::code::delay_action, ptr);
    }
  }

  void watch(disposable what) override {
    watched_.emplace_back(what);
  }

  // -- thread-safe signaling --------------------------------------------------

  bool start(socket_manager_ptr mgr) override {
    auto lg = log::net::trace("socket = {}", mgr->handle().id);
    if (std::this_thread::get_id() == tid_) {
      return do_start(mgr);
    } else {
      return write_to_pipe(pollset_updater::code::start_manager, mgr.release());
    }
  }

  void shutdown() override {
    auto lg = log::net::trace("");
    // Note: there is no 'shortcut' when calling the function in the
    // default_multiplexer's thread, because do_shutdown calls apply_updates.
    // This must only be called from the pollset_updater.
    log::net::debug("push shutdown event to pipe");
    write_to_pipe(pollset_updater::code::shutdown,
                  static_cast<socket_manager*>(nullptr));
  }

  // -- callbacks for socket managers ------------------------------------------

  void register_reading(socket_manager* mgr) override {
    auto lg = log::net::trace("socket = {}", mgr->handle().id);
    update_for(mgr).events |= input_mask;
  }

  void register_writing(socket_manager* mgr) override {
    auto lg = log::net::trace("socket = {}", mgr->handle().id);
    update_for(mgr).events |= output_mask;
  }

  void deregister_reading(socket_manager* mgr) override {
    auto lg = log::net::trace("socket = {}", mgr->handle().id);
    update_for(mgr).events &= ~input_mask;
  }

  void deregister_writing(socket_manager* mgr) override {
    auto lg = log::net::trace("socket = {}", mgr->handle().id);
    update_for(mgr).events &= ~output_mask;
  }

  void deregister(socket_manager* mgr) override {
    auto lg = log::net::trace("socket = {}", mgr->handle().id);
    update_for(mgr).events = 0;
  }

  bool is_reading(const socket_manager* mgr) const noexcept override {
    return (active_mask_of(mgr) & input_mask) != 0;
  }

  bool is_writing(const socket_manager* mgr) const noexcept override {
    return (active_mask_of(mgr) & output_mask) != 0;
  }

  // -- control flow -----------------------------------------------------------

  bool poll_once(bool blocking) override {
    auto lg = log::net::trace("blocking = {}", blocking);
    if (pollset_.empty())
      return false;
    // We'll call poll() until poll() succeeds or fails.
    for (;;) {
      int timeout = 0;
      if (!blocking) {
        // nop
      } else if (scheduled_actions.empty()) {
        timeout = -1;
      } else {
        auto now = std::chrono::steady_clock::now();
        auto tout = scheduled_actions.begin()->first;
        if (tout > now) {
          namespace sc = std::chrono;
          auto ms = sc::duration_cast<sc::milliseconds>(tout - now).count();
          timeout = std::max(1, static_cast<int>(ms));
        }
      }
      int presult =
#ifdef CAF_WINDOWS
        ::WSAPoll(pollset_.data(), static_cast<ULONG>(pollset_.size()),
                  timeout);
#else
        ::poll(pollset_.data(), static_cast<nfds_t>(pollset_.size()), timeout);
#endif
      if (presult > 0) {
        log::net::debug("poll() on {} sockets reported event(s) {}",
                        pollset_.size(), presult);
        // Scan pollset for events.
        if (auto revents = pollset_[0].revents; revents != 0) {
          // Index 0 is always the pollset updater. This is the only handler
          // that is allowed to modify pollset_ and managers_. Since this may
          // very well mess with the for loop below, we process this handler
          // first.
          auto mgr = managers_[0];
          handle(mgr, pollset_[0].events, revents);
          --presult;
        }
        apply_updates();
        for (size_t i = 1; i < pollset_.size() && presult > 0; ++i) {
          if (auto revents = pollset_[i].revents; revents != 0) {
            handle(managers_[i], pollset_[i].events, revents);
            --presult;
          }
        }
        run_timeouts();
        return true;
      }
      if (presult == 0) {
        // No activity.
        run_timeouts();
        return false;
      }
      auto code = last_socket_error();
      switch (code) {
        case std::errc::interrupted: {
          // A signal was caught. Simply try again.
          log::net::debug("received errc::interrupted, try again");
          break;
        }
        case std::errc::not_enough_memory: {
          log::system::error("poll() failed due to insufficient memory");
          // There's not much we can do other than try again in hope someone
          // else releases memory.
          break;
        }
        default: {
          // Must not happen.
          auto int_code = static_cast<int>(code);
          auto msg = std::generic_category().message(int_code);
          std::string_view prefix = "poll() failed: ";
          msg.insert(msg.begin(), prefix.begin(), prefix.end());
          CAF_CRITICAL(msg.c_str());
        }
      }
    }
  }

  void run_timeouts() {
    // Run all timeouts.
    auto now = std::chrono::steady_clock::now();
    while (!scheduled_actions.empty()
           && scheduled_actions.begin()->first <= now) {
      auto i = scheduled_actions.begin();
      auto next = std::move(i->second);
      scheduled_actions.erase(i);
      next.run();
    }
    apply_updates();
  }

  void apply_updates() override {
    log::net::debug("apply {} updates", updates_.size());
    for (;;) {
      if (!updates_.empty()) {
        for (auto& [fd, update] : updates_) {
          if (auto index = index_of(fd); index == -1) {
            if (update.events != 0) {
              pollfd new_entry{socket_cast<socket_id>(fd), update.events, 0};
              pollset_.emplace_back(new_entry);
              managers_.emplace_back(std::move(update.mgr));
            }
          } else if (update.events != 0) {
            pollset_[index].events = update.events;
            managers_[index].swap(update.mgr);
          } else {
            pollset_.erase(pollset_.begin() + index);
            managers_.erase(managers_.begin() + index);
          }
        }
        updates_.clear();
      }
      while (!pending_actions.empty()) {
        auto next = std::move(pending_actions.front());
        pending_actions.pop_front();
        next.run();
      }
      if (updates_.empty())
        return;
    }
  }

  void set_thread_id() override {
    auto lg = log::net::trace("");
    tid_ = std::this_thread::get_id();
  }

  void run() override {
    auto lg = log::net::trace("");
    log::net::debug("run default_multiplexer input_mask = {}, error_mask = {}, "
                    "output_mask = {}",
                    input_mask, error_mask, output_mask);
    // On systems like Linux, we cannot disable sigpipe on the socket alone. We
    // need to block the signal at thread level since some APIs (such as
    // OpenSSL) are unsafe to call otherwise.
    block_sigpipe();
    while (!shutting_down_ || pollset_.size() > 1 || !watched_.empty()) {
      poll_once(true);
      disposable::erase_disposed(watched_);
    }
    // Close the pipe to block any future event.
    std::lock_guard<std::mutex> guard{write_lock_};
    if (write_handle_ != invalid_socket) {
      close(write_handle_);
      write_handle_ = pipe_socket{};
    }
  }

  // -- internal callbacks the pollset updater ---------------------------------

  void do_shutdown() {
    // Note: calling apply_updates here is only safe because we know that the
    // pollset updater runs outside of the for-loop in run_once.
    log::net::debug("initiate shutdown");
    shutting_down_ = true;
    apply_updates();
    // Skip the first manager (the pollset updater).
    for (size_t i = 1; i < managers_.size(); ++i)
      managers_[i]->dispose();
    apply_updates();
  }

  bool do_start(const socket_manager_ptr& mgr) {
    auto lg = log::net::trace("socket = {}", mgr->handle().id);
    if (!shutting_down_) {
      error err;
      err = mgr->start();
      if (err) {
        log::net::debug("mgr->init failed: {}", err);
        // The socket manager should not register itself for any events if
        // initialization fails. Purge any state just in case.
        update_for(mgr.get()).events = 0;
        return false;
      }
      return true;
    }
    return false;
  }

  // -- utility functions ------------------------------------------------------

  /// Returns the index of `mgr` in the pollset or `-1`.
  ptrdiff_t index_of(const socket_manager_ptr& mgr) const noexcept {
    auto first = managers_.begin();
    auto last = managers_.end();
    auto i = std::find(first, last, mgr);
    return i == last ? -1 : std::distance(first, i);
  }

  /// Returns the index of `fd` in the pollset or `-1`.
  ptrdiff_t index_of(socket fd) const noexcept {
    auto first = pollset_.begin();
    auto last = pollset_.end();
    auto i = std::find_if(first, last,
                          [fd](const pollfd& x) { return x.fd == fd.id; });
    return i == last ? -1 : std::distance(first, i);
  }

  /// Handles an I/O event on given manager.
  void handle(const socket_manager_ptr& mgr, [[maybe_unused]] short events,
              short revents) {
    auto lg = log::net::trace("socket = {}, events = {}, revents = {}",
                              mgr->handle().id, events, revents);
    CAF_ASSERT(mgr != nullptr);
    bool checkerror = true;
    log::net::debug("handle event on socket {}, events = {}, revents = {}",
                    mgr->handle().id, events, revents);
    // Note: we double-check whether the manager is actually reading because a
    // previous action from the pipe may have disabled reading.
    if ((revents & input_mask) != 0 && is_reading(mgr.get())) {
      checkerror = false;
      mgr->handle_read_event();
    }
    // Similar reasoning than before: double-check whether this event should
    // still get dispatched.
    if ((revents & output_mask) != 0 && is_writing(mgr.get())) {
      checkerror = false;
      mgr->handle_write_event();
    }
    if (checkerror && ((revents & error_mask) != 0)) {
      if (revents & POLLNVAL)
        mgr->handle_error(sec::socket_invalid);
      else if (revents & POLLHUP)
        mgr->handle_error(sec::socket_disconnected);
      else
        mgr->handle_error(sec::socket_operation_failed);
      update_for(mgr.get()).events = 0;
    }
  }

  /// Returns a change entry for the socket at given index. Lazily creates a new
  /// entry before returning if necessary.
  poll_update& update_for(ptrdiff_t index) {
    auto fd = socket{pollset_[index].fd};
    if (auto i = updates_.find(fd); i != updates_.end()) {
      return i->second;
    } else {
      updates_.container().emplace_back(fd, poll_update{pollset_[index].events,
                                                        managers_[index]});
      return updates_.container().back().second;
    }
  }

  /// Returns a change entry for the socket of the manager.
  poll_update& update_for(socket_manager* mgr) {
    auto fd = mgr->handle();
    if (auto i = updates_.find(fd); i != updates_.end()) {
      return i->second;
    } else if (auto index = index_of(fd); index != -1) {
      updates_.container().emplace_back(
        fd, poll_update{pollset_[index].events, socket_manager_ptr{mgr}});
      return updates_.container().back().second;
    } else {
      updates_.container().emplace_back(fd, poll_update{0, mgr});
      return updates_.container().back().second;
    }
  }

  /// Writes `opcode` and pointer to `mgr` the the pipe for handling an event
  /// later via the pollset updater.
  /// @warning assumes ownership of @p ptr.
  template <class T>
  bool write_to_pipe(uint8_t opcode, T* ptr) {
    pollset_updater::msg_buf buf;
    // Note: no intrusive_ptr_add_ref(ptr) since we take ownership of `ptr`.
    buf[0] = static_cast<std::byte>(opcode);
    auto value = reinterpret_cast<intptr_t>(ptr);
    memcpy(buf.data() + 1, &value, sizeof(intptr_t));
    ptrdiff_t res = -1;
    { // Lifetime scope of guard.
      std::lock_guard<std::mutex> guard{write_lock_};
      if (write_handle_ != invalid_socket)
        res = write(write_handle_, buf);
    }
    if constexpr (std::is_base_of_v<detail::atomic_ref_counted, T>) {
      if (res <= 0 && ptr)
        intrusive_ptr_release(ptr);
    } else {
      if (res <= 0 && ptr)
        delete ptr;
    }
    return res > 0;
  }

  /// @copydoc write_to_pipe
  template <class Enum, class T>
  std::enable_if_t<std::is_enum_v<Enum>, bool>
  write_to_pipe(Enum opcode, T* ptr) {
    return write_to_pipe(static_cast<uint8_t>(opcode), ptr);
  }

  /// Queries the currently active event bitmask for `mgr`.
  short active_mask_of(const socket_manager* mgr) const noexcept {
    auto fd = mgr->handle();
    if (auto i = updates_.find(fd); i != updates_.end()) {
      return i->second.events;
    } else if (auto index = index_of(fd); index != -1) {
      return pollset_[index].events;
    } else {
      return 0;
    }
  }

  /// Pending actions to run immediately.
  std::deque<action> pending_actions;

  /// The container type for delayed actions.
  using scheduled_actions_map = std::multimap<steady_time_point, action>;

  /// Scheduled actions.
  scheduled_actions_map scheduled_actions;

private:
  // -- member variables -------------------------------------------------------

  /// Bookkeeping data for managed sockets.
  pollfd_list pollset_;

  /// Maps sockets to their owning managers by storing the managers in the same
  /// order as their sockets appear in `pollset_`.
  manager_list managers_;

  /// Caches changes to the events mask of managed sockets until they can safely
  /// take place.
  poll_update_map updates_;

  /// Stores the ID of the thread this multiplexer is running in. Set when
  /// calling `init()`.
  std::thread::id tid_;

  /// Guards `write_handle_`.
  std::mutex write_lock_;

  /// Used for pushing updates to the multiplexer's thread.
  pipe_socket write_handle_;

  /// Points to the owning middleman.
  middleman* owner_;

  /// Signals whether shutdown has been requested.
  bool shutting_down_ = false;

  /// Keeps track of watched disposables.
  std::vector<disposable> watched_;
};

error pollset_updater::start(socket_manager* owner) {
  auto lg = log::net::trace("");
  owner_ = owner;
  mpx_ = static_cast<default_multiplexer*>(owner->mpx_ptr());
  return nonblocking(fd_, true);
}

void pollset_updater::handle_read_event() {
  auto lg = log::net::trace("");
  auto as_mgr = [](intptr_t ptr) {
    return intrusive_ptr{reinterpret_cast<socket_manager*>(ptr), false};
  };
  auto add_action = [this](intptr_t ptr) {
    auto f = action{intrusive_ptr{reinterpret_cast<action::impl*>(ptr), false}};
    mpx_->pending_actions.push_back(std::move(f));
  };
  auto delay_action = [this](intptr_t ptr) {
    using value_type = default_multiplexer::scheduled_actions_map::value_type;
    auto val = std::unique_ptr<value_type>{reinterpret_cast<value_type*>(ptr)};
    mpx_->scheduled_actions.emplace(val->first, std::move(val->second));
  };
  for (;;) {
    CAF_ASSERT((buf_.size() - buf_size_) > 0);
    auto num_bytes
      = read(fd_, make_span(buf_.data() + buf_size_, buf_.size() - buf_size_));
    if (num_bytes > 0) {
      buf_size_ += static_cast<size_t>(num_bytes);
      if (buf_.size() == buf_size_) {
        buf_size_ = 0;
        auto opcode = static_cast<uint8_t>(buf_[0]);
        intptr_t ptr;
        memcpy(&ptr, buf_.data() + 1, sizeof(intptr_t));
        switch (static_cast<code>(opcode)) {
          case code::start_manager:
            mpx_->do_start(as_mgr(ptr));
            break;
          case code::run_action:
            add_action(ptr);
            break;
          case code::delay_action:
            delay_action(ptr);
            break;
          case code::shutdown:
            CAF_ASSERT(ptr == 0);
            mpx_->do_shutdown();
            break;
          default:
            log::system::error("invalid opcode in pollset updater: {}", opcode);
            break;
        }
      }
    } else if (num_bytes == 0) {
      log::net::debug("pipe closed, assume shutdown");
      owner_->deregister();
      return;
    } else if (last_socket_error_is_temporary()) {
      return;
    } else {
      log::system::error("pollset updater failed to read from its pipe");
      owner_->deregister();
      return;
    }
  }
}

} // namespace

// -- multiplexer implementation -----------------------------------------------

// -- static utility functions -------------------------------------------------

void multiplexer::block_sigpipe() {
#ifdef CAF_LINUX
  sigset_t sigpipe_mask;
  sigemptyset(&sigpipe_mask);
  sigaddset(&sigpipe_mask, SIGPIPE);
  sigset_t saved_mask;
  if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, &saved_mask) != 0) {
    perror("pthread_sigmask");
    exit(1);
  }
#endif
}

multiplexer_ptr multiplexer::make(middleman* parent) {
  return make_counted<default_multiplexer>(parent);
}

multiplexer* multiplexer::from(actor_system& sys) {
  return sys.network_manager().mpx_ptr();
}

// -- constructors, destructors, and assignment operators ----------------------

multiplexer::~multiplexer() {
  // nop
}

// -- initialization ---------------------------------------------------------

std::thread multiplexer::launch() {
  auto l = std::make_shared<detail::latch>(2);
  auto fn = [mpx = multiplexer_ptr{this}, l]() mutable {
    mpx->set_thread_id();
    l->count_down();
    l = nullptr;
    mpx->run();
  };
  auto result = std::thread{fn};
  l->count_down_and_wait();
  return result;
}

} // namespace caf::net
