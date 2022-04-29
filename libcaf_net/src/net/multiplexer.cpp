// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/multiplexer.hpp"

#include "caf/action.hpp"
#include "caf/byte.hpp"
#include "caf/config.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/operation.hpp"
#include "caf/net/pollset_updater.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"

#include <algorithm>
#include <optional>

#ifndef CAF_WINDOWS
#  include <poll.h>
#  include <signal.h>
#else
#  include "caf/detail/socket_sys_includes.hpp"
#endif // CAF_WINDOWS

namespace caf::net {

#ifndef POLLRDHUP
#  define POLLRDHUP POLLHUP
#endif

#ifndef POLLPRI
#  define POLLPRI POLLIN
#endif

namespace {

#ifdef CAF_WINDOWS
// From the MSDN: If the POLLPRI flag is set on a socket for the Microsoft
//                Winsock provider, the WSAPoll function will fail.
const short input_mask = POLLIN;
#else
const short input_mask = POLLIN | POLLPRI;
#endif

const short error_mask = POLLRDHUP | POLLERR | POLLHUP | POLLNVAL;

const short output_mask = POLLOUT;

// short to_bitmask(operation mask) {
//   return static_cast<short>((is_reading(mask) ? input_mask : 0)
//                             | (is_writing(mask) ? output_mask : 0));
// }

operation to_operation(const socket_manager_ptr& mgr,
                       std::optional<short> mask) {
  operation res = operation::none;
  if (mgr->read_closed())
    res = block_reads(res);
  if (mgr->write_closed())
    res = block_writes(res);
  if (mask) {
    if ((*mask & input_mask) != 0)
      res = add_read_flag(res);
    if ((*mask & output_mask) != 0)
      res = add_write_flag(res);
  }
  return res;
}

} // namespace

// -- static utility functions -------------------------------------------------

#ifdef CAF_LINUX

void multiplexer::block_sigpipe() {
  sigset_t sigpipe_mask;
  sigemptyset(&sigpipe_mask);
  sigaddset(&sigpipe_mask, SIGPIPE);
  sigset_t saved_mask;
  if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, &saved_mask) == -1) {
    perror("pthread_sigmask");
    exit(1);
  }
}

#else

void multiplexer::block_sigpipe() {
  // nop
}

#endif

// -- constructors, destructors, and assignment operators ----------------------

multiplexer::multiplexer(middleman* owner) : owner_(owner) {
  // nop
}

multiplexer::~multiplexer() {
  // nop
}

// -- initialization -----------------------------------------------------------

error multiplexer::init() {
  auto pipe_handles = make_pipe();
  if (!pipe_handles)
    return std::move(pipe_handles.error());
  auto updater = pollset_updater::make(pipe_handles->first);
  auto mgr = socket_manager::make(this, pipe_handles->first,
                                  std::move(updater));
  if (auto err = mgr->init(settings{}))
    return err;
  write_handle_ = pipe_handles->second;
  pollset_.emplace_back(pollfd{pipe_handles->first.id, input_mask, 0});
  managers_.emplace_back(mgr);
  return none;
}

// -- properties ---------------------------------------------------------------

size_t multiplexer::num_socket_managers() const noexcept {
  return managers_.size();
}

ptrdiff_t multiplexer::index_of(const socket_manager_ptr& mgr) const noexcept {
  auto first = managers_.begin();
  auto last = managers_.end();
  auto i = std::find(first, last, mgr);
  return i == last ? -1 : std::distance(first, i);
}

ptrdiff_t multiplexer::index_of(socket fd) const noexcept {
  auto first = pollset_.begin();
  auto last = pollset_.end();
  auto i = std::find_if(first, last,
                        [fd](const pollfd& x) { return x.fd == fd.id; });
  return i == last ? -1 : std::distance(first, i);
}

middleman& multiplexer::owner() {
  CAF_ASSERT(owner_ != nullptr);
  return *owner_;
}

actor_system& multiplexer::system() {
  return owner().system();
}

operation multiplexer::mask_of(const socket_manager_ptr& mgr) {
  auto fd = mgr->handle();
  if (auto i = updates_.find(fd); i != updates_.end())
    return to_operation(mgr, i->second.events);
  else if (auto index = index_of(mgr); index != -1)
    return to_operation(mgr, pollset_[index].events);
  else
    return to_operation(mgr, std::nullopt);
}

// -- thread-safe signaling ----------------------------------------------------

void multiplexer::discard(const socket_manager_ptr& mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  if (std::this_thread::get_id() == tid_) {
    do_discard(mgr);
  } else {
    write_to_pipe(pollset_updater::code::discard_manager, mgr.get());
  }
}

void multiplexer::shutdown_reading(const socket_manager_ptr& mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  if (std::this_thread::get_id() == tid_) {
    do_shutdown_reading(mgr);
  } else {
    write_to_pipe(pollset_updater::code::shutdown_reading, mgr.get());
  }
}

void multiplexer::shutdown_writing(const socket_manager_ptr& mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  if (std::this_thread::get_id() == tid_) {
    do_shutdown_writing(mgr);
  } else {
    write_to_pipe(pollset_updater::code::shutdown_writing, mgr.get());
  }
}

void multiplexer::schedule(const action& what) {
  CAF_LOG_TRACE("");
  write_to_pipe(pollset_updater::code::run_action, what.ptr());
}

void multiplexer::init(const socket_manager_ptr& mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  if (std::this_thread::get_id() == tid_) {
    do_init(mgr);
  } else {
    write_to_pipe(pollset_updater::code::init_manager, mgr.get());
  }
}

void multiplexer::shutdown() {
  CAF_LOG_TRACE("");
  // Note: there is no 'shortcut' when calling the function in the multiplexer's
  // thread, because do_shutdown calls apply_updates. This must only be called
  // from the pollset_updater.
  CAF_LOG_DEBUG("push shutdown event to pipe");
  write_to_pipe(pollset_updater::code::shutdown,
                static_cast<socket_manager*>(nullptr));
}

// -- callbacks for socket managers --------------------------------------------

void multiplexer::register_reading(socket_manager* mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  update_for(mgr).events |= input_mask;
}

void multiplexer::register_writing(socket_manager* mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  update_for(mgr).events |= output_mask;
}

void multiplexer::deregister_reading(socket_manager* mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  update_for(mgr).events &= ~input_mask;
}

void multiplexer::deregister_writing(socket_manager* mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  update_for(mgr).events &= ~output_mask;
}

void multiplexer::deregister(socket_manager* mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  update_for(mgr).events = 0;
}

bool multiplexer::is_reading(const socket_manager* mgr) const noexcept {
  return (active_mask_of(mgr) & input_mask) != 0;
}

bool multiplexer::is_writing(const socket_manager* mgr) const noexcept {
  return (active_mask_of(mgr) & output_mask) != 0;
}

// -- control flow -------------------------------------------------------------

bool multiplexer::poll_once(bool blocking) {
  CAF_LOG_TRACE(CAF_ARG(blocking));
  if (pollset_.empty())
    return false;
  // We'll call poll() until poll() succeeds or fails.
  for (;;) {
    int presult =
#ifdef CAF_WINDOWS
      ::WSAPoll(pollset_.data(), static_cast<ULONG>(pollset_.size()),
                blocking ? -1 : 0);
#else
      ::poll(pollset_.data(), static_cast<nfds_t>(pollset_.size()),
             blocking ? -1 : 0);
#endif
    if (presult > 0) {
      CAF_LOG_DEBUG("poll() on" << pollset_.size() << "sockets reported"
                                << presult << "event(s)");
      // Scan pollset for events.
      CAF_LOG_DEBUG("scan pollset for socket events");
      if (auto revents = pollset_[0].revents; revents != 0) {
        // Index 0 is always the pollset updater. This is the only handler that
        // is allowed to modify pollset_ and managers_. Since this may very well
        // mess with the for loop below, we process this handler first.
        auto mgr = managers_[0];
        handle(mgr, pollset_[0].events, revents);
        --presult;
      }
      for (size_t i = 1; i < pollset_.size() && presult > 0; ++i) {
        if (auto revents = pollset_[i].revents; revents != 0) {
          handle(managers_[i], pollset_[i].events, revents);
          --presult;
        }
      }
      apply_updates();
      return true;
    } else if (presult == 0) {
      // No activity.
      return false;
    } else {
      auto code = last_socket_error();
      switch (code) {
        case std::errc::interrupted: {
          // A signal was caught. Simply try again.
          CAF_LOG_DEBUG("received errc::interrupted, try again");
          break;
        }
        case std::errc::not_enough_memory: {
          CAF_LOG_ERROR("poll() failed due to insufficient memory");
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
}

void multiplexer::poll() {
  while (poll_once(false))
    ; // repeat
}

void multiplexer::apply_updates() {
  CAF_LOG_DEBUG("apply" << updates_.size() << "updates");
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
}

void multiplexer::set_thread_id() {
  CAF_LOG_TRACE("");
  tid_ = std::this_thread::get_id();
}

void multiplexer::run() {
  CAF_LOG_TRACE("");
  // On systems like Linux, we cannot disable sigpipe on the socket alone. We
  // need to block the signal at thread level since some APIs (such as OpenSSL)
  // are unsafe to call otherwise.
  block_sigpipe();
  while (!shutting_down_ || pollset_.size() > 1)
    poll_once(true);
  // Close the pipe to block any future event.
  std::lock_guard<std::mutex> guard{write_lock_};
  if (write_handle_ != invalid_socket) {
    close(write_handle_);
    write_handle_ = pipe_socket{};
  }
}

// -- utility functions --------------------------------------------------------

void multiplexer::handle(const socket_manager_ptr& mgr,
                         [[maybe_unused]] short events, short revents) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id)
                << CAF_ARG(events) << CAF_ARG(revents));
  CAF_ASSERT(mgr != nullptr);
  bool checkerror = true;
  // Note: we double-check whether the manager is actually reading because a
  // previous action from the pipe may have disabled reading.
  if ((revents & input_mask) != 0 && is_reading(mgr.get())) {
    checkerror = false;
    mgr->handle_read_event();
  }
  // Similar reasoning than before: double-check whether this event should still
  // get dispatched.
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

multiplexer::poll_update& multiplexer::update_for(ptrdiff_t index) {
  auto fd = socket{pollset_[index].fd};
  if (auto i = updates_.find(fd); i != updates_.end()) {
    return i->second;
  } else {
    updates_.container().emplace_back(fd, poll_update{pollset_[index].events,
                                                      managers_[index]});
    return updates_.container().back().second;
  }
}

multiplexer::poll_update& multiplexer::update_for(socket_manager* mgr) {
  auto fd = mgr->handle();
  if (auto i = updates_.find(fd); i != updates_.end()) {
    return i->second;
  } else if (auto index = index_of(fd); index != -1) {
    updates_.container().emplace_back(fd, poll_update{pollset_[index].events,
                                                      socket_manager_ptr{mgr}});
    return updates_.container().back().second;
  } else {
    updates_.container().emplace_back(fd, poll_update{0, mgr});
    return updates_.container().back().second;
  }
}

template <class T>
void multiplexer::write_to_pipe(uint8_t opcode, T* ptr) {
  pollset_updater::msg_buf buf;
  if (ptr) {
    intrusive_ptr_add_ref(ptr);
  }
  buf[0] = static_cast<std::byte>(opcode);
  auto value = reinterpret_cast<intptr_t>(ptr);
  memcpy(buf.data() + 1, &value, sizeof(intptr_t));
  ptrdiff_t res = -1;
  { // Lifetime scope of guard.
    std::lock_guard<std::mutex> guard{write_lock_};
    if (write_handle_ != invalid_socket)
      res = write(write_handle_, buf);
  }
  if (res <= 0 && ptr)
    intrusive_ptr_release(ptr);
}

short multiplexer::active_mask_of(const socket_manager* mgr) const noexcept {
  auto fd = mgr->handle();
  if (auto i = updates_.find(fd); i != updates_.end()) {
    return i->second.events;
  } else if (auto index = index_of(fd); index != -1) {
    return pollset_[index].events;
  } else {
    return 0;
  }
}

// -- internal callbacks the pollset updater -----------------------------------

void multiplexer::do_shutdown() {
  // Note: calling apply_updates here is only safe because we know that the
  // pollset updater runs outside of the for-loop in run_once.
  CAF_LOG_DEBUG("initiate shutdown");
  shutting_down_ = true;
  apply_updates();
  // Skip the first manager (the pollset updater).
  for (size_t i = 1; i < managers_.size(); ++i) {
    auto& mgr = managers_[i];
    mgr->close_read();
    update_for(static_cast<ptrdiff_t>(i)).events &= ~input_mask;
  }
  apply_updates();
}

void multiplexer::do_discard(const socket_manager_ptr& mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  mgr->handle_error(sec::disposed);
  update_for(mgr.get()).events = 0;
}

void multiplexer::do_shutdown_reading(const socket_manager_ptr& mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  if (!shutting_down_ && !mgr->read_closed()) {
    mgr->close_read();
    update_for(mgr.get()).events &= ~input_mask;
  }
}

void multiplexer::do_shutdown_writing(const socket_manager_ptr& mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  if (!shutting_down_ && !mgr->write_closed()) {
    mgr->close_write();
    update_for(mgr.get()).events &= ~output_mask;
  }
}

void multiplexer::do_init(const socket_manager_ptr& mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  if (!shutting_down_) {
    error err;
    if (owner_)
      err = mgr->init(content(system().config()));
    else
      err = mgr->init(settings{});
    if (err) {
      CAF_LOG_DEBUG("mgr->init failed: " << err);
      // The socket manager should not register itself for any events if
      // initialization fails. Purge any state just in case.
      update_for(mgr.get()).events = 0;
    }
  }
}

} // namespace caf::net
