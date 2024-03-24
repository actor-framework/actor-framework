// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/socket_manager.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/socket_event_layer.hpp"

#include "caf/action.hpp"
#include "caf/actor_system.hpp"
#include "caf/detail/assert.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/flow/coordinated.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/fwd.hpp"
#include "caf/log/net.hpp"
#include "caf/make_counted.hpp"
#include "caf/sec.hpp"

#include <type_traits>

namespace caf::net {

namespace {

class socket_manager_impl : public socket_manager {
public:
  // -- constructors, destructors, and assignment operators --------------------

  /// @pre `handle != invalid_socket`
  /// @pre `mpx!= nullptr`
  socket_manager_impl(multiplexer* mpx, event_handler_ptr handler)
    : fd_(handler->handle()),
      mpx_(mpx),
      handler_(std::move(handler)),
      disposed_(false) {
    CAF_ASSERT(fd_ != invalid_socket);
    CAF_ASSERT(mpx_ != nullptr);
    CAF_ASSERT(handler_ != nullptr);
  }

  ~socket_manager_impl() override {
    // Note: may not call cleanup since it calls the multiplexer via
    // deregister().
    handler_.reset();
    if (fd_) {
      close(fd_);
      fd_ = invalid_socket;
    }
    if (!cleanup_listeners_.empty()) {
      for (auto& f : cleanup_listeners_)
        mpx_->schedule(std::move(f));
      cleanup_listeners_.clear();
    }
  }

  socket_manager_impl(const socket_manager_impl&) = delete;

  socket_manager_impl& operator=(const socket_manager_impl&) = delete;

  // -- properties -------------------------------------------------------------

  /// Returns the handle for the managed socket.
  socket handle() const override {
    return fd_;
  }

  /// @private
  void handle(socket new_handle) override {
    fd_ = new_handle;
  }

  /// Returns a reference to the hosting @ref actor_system instance.
  actor_system& system() noexcept override {
    CAF_ASSERT(mpx_ != nullptr);
    return mpx_->system();
  }

  /// Returns the owning @ref multiplexer instance.
  multiplexer& mpx() noexcept override {
    return *mpx_;
  }

  /// Returns the owning @ref multiplexer instance.
  const multiplexer& mpx() const noexcept override {
    return *mpx_;
  }

  /// Returns a pointer to the owning @ref multiplexer instance.
  multiplexer* mpx_ptr() noexcept override {
    return mpx_;
  }

  /// Returns a pointer to the owning @ref multiplexer instance.
  const multiplexer* mpx_ptr() const noexcept override {
    return mpx_;
  }

  /// Queries whether the manager is registered for reading.
  bool is_reading() const noexcept override {
    return mpx_->is_reading(this);
  }

  /// Queries whether the manager is registered for writing.
  bool is_writing() const noexcept override {
    return mpx_->is_writing(this);
  }

  // -- event loop management --------------------------------------------------

  /// Registers the manager for read operations.
  void register_reading() override {
    mpx_->register_reading(this);
  }

  /// Registers the manager for write operations.
  void register_writing() override {
    mpx_->register_writing(this);
  }

  /// Deregisters the manager from read operations.
  void deregister_reading() override {
    mpx_->deregister_reading(this);
  }

  /// Deregisters the manager from write operations.
  void deregister_writing() override {
    mpx_->deregister_writing(this);
  }

  /// Deregisters the manager from both read and write operations.
  void deregister() override {
    mpx_->deregister(this);
  }

  /// Schedules a call to `fn` on the multiplexer when this socket manager
  /// cleans up its state.
  /// @note Must be called before `start`.
  void add_cleanup_listener(action fn) override {
    cleanup_listeners_.push_back(std::move(fn));
  }

  // -- callbacks for the handler ----------------------------------------------

  /// Schedules a call to `do_handover` on the handler.
  void schedule_handover() override {
    deregister();
    mpx_->schedule_fn([ptr = strong_this()] {
      event_handler_ptr next;
      if (ptr->handler_->do_handover(next)) {
        ptr->handler_.swap(next);
      }
    });
  }

  /// Shuts down this socket manager.
  void shutdown() override {
    auto lg = log::net::trace("");
    if (!shutting_down_) {
      shutting_down_ = true;
      dispose();
    } else {
      // This usually only happens after disposing the manager if the handler
      // still had data to send.
      mpx_->schedule_fn([ptr = strong_this()] { //
        ptr->cleanup();
      });
    }
  }

  // -- callbacks for the multiplexer ------------------------------------------

  /// Starts the manager and its all of its processing layers.
  error start() override {
    auto lg = log::net::trace("");
    if (auto err = nonblocking(fd_, true)) {
      log::net::error("failed to set nonblocking flag in socket: {}", err);
      handler_->abort(err);
      cleanup();
      return err;
    }
    if (auto err = handler_->start(this); err) {
      log::net::debug("failed to initialize handler: {}", err);
      cleanup();
      return err;
    }
    run_delayed_actions();
    return none;
  }

  /// Called whenever the socket received new data.
  void handle_read_event() override {
    if (handler_) {
      handler_->handle_read_event();
      run_delayed_actions();
      return;
    }
    deregister();
  }

  /// Called whenever the socket is allowed to send data.
  void handle_write_event() override {
    if (handler_) {
      handler_->handle_write_event();
      run_delayed_actions();
      return;
    }
    deregister();
  }

  /// Called when the remote side becomes unreachable due to an error or after
  /// calling @ref dispose.
  /// @param code The error code as reported by the operating system or
  ///             @ref sec::disposed.
  void handle_error(sec code) override {
    auto lg = log::net::trace("");
    if (!disposed_)
      disposed_ = true;
    if (handler_) {
      if (!shutting_down_) {
        handler_->abort(make_error(code));
        shutting_down_ = true;
        run_delayed_actions();
      }
      if (code == sec::disposed && !handler_->finalized()) {
        // When disposing the manger, the transport is still allowed to send
        // any pending data and it will call shutdown() later to trigger
        // cleanup().
        deregister_reading();
      } else {
        cleanup();
      }
    }
  }

  // -- implementation of coordinator ------------------------------------------

  void ref_execution_context() const noexcept override {
    ref();
  }

  void deref_execution_context() const noexcept override {
    deref();
  }

  void schedule(action what) override {
    mpx_->schedule_fn([ptr = strong_this(), f = std::move(what)]() mutable { //
      ptr->exec(f);
    });
  }

  void watch(disposable what) override {
    watched_.push_back(std::move(what));
  }

  void release_later(flow::coordinated_ptr& child) override {
    trash_.push_back(std::move(child));
  }

  steady_time_point steady_time() override {
    return std::chrono::steady_clock::now();
  }

  void delay(action what) override {
    delayed_.push_back(std::move(what));
  }

  disposable delay_until(steady_time_point abs_time, action what) override {
    auto callback = make_single_shot_action(
      [ptr = strong_this(), f = std::move(what)]() mutable { ptr->exec(f); });
    mpx_->schedule(abs_time, callback);
    return std::move(callback).as_disposable();
  }

  // -- implementation of disposable_impl --------------------------------------

  void dispose() override {
    bool expected = false;
    if (disposed_.compare_exchange_strong(expected, true)) {
      mpx_->schedule_fn([ptr = strong_this()] { //
        ptr->handle_error(sec::disposed);
      });
    }
  }

  bool disposed() const noexcept override {
    return disposed_.load();
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

  // -- disambiguation ---------------------------------------------------------

private:
  // -- utility functions ------------------------------------------------------

  void exec(action& f) {
    f.run();
    run_delayed_actions();
  }

  void run_delayed_actions() override {
    while (!delayed_.empty()) {
      auto next = std::move(delayed_.front());
      delayed_.pop_front();
      next.run();
    }
    trash_.clear();
    disposable::erase_disposed(watched_);
  }

  void cleanup() {
    deregister();
    handler_.reset();
    if (fd_) {
      close(fd_);
      fd_ = invalid_socket;
    }
    if (!cleanup_listeners_.empty()) {
      for (auto& f : cleanup_listeners_)
        mpx_->schedule(std::move(f));
      cleanup_listeners_.clear();
    }
  }

  intrusive_ptr<socket_manager_impl> strong_this() {
    return intrusive_ptr<socket_manager_impl>{this};
  }

  // -- member variables -------------------------------------------------------

  /// Stores the socket file descriptor. The socket manager automatically closes
  /// the socket in its destructor.
  socket fd_;

  /// Points to the multiplexer that executes this manager. Note: we do not need
  /// to increase the reference count for the multiplexer, because the
  /// multiplexer owns all managers in the sense that calling any member
  /// function on a socket manager may not occur if the actor system has shut
  /// down (and the multiplexer is part of the actor system).
  multiplexer* mpx_;

  /// Stores the event handler that operators on the socket file descriptor.
  event_handler_ptr handler_;

  /// Stores whether `shutdown` has been called.
  bool shutting_down_ = false;

  /// Stores whether the manager has been either explicitly disposed or shut
  /// down by demand of the application.
  std::atomic<bool> disposed_;

  /// Callbacks to run when calling `cleanup`.
  std::vector<action> cleanup_listeners_;

  /// Stores watched disposables.
  std::vector<disposable> watched_;

  /// Stores actions that should run at the next opportunity.
  std::deque<action> delayed_;

  /// Stores flow children that should be released at the next opportunity.
  std::vector<flow::coordinated_ptr> trash_;
};

} // namespace

// -- factories ----------------------------------------------------------------

socket_manager_ptr socket_manager::make(multiplexer* mpx,
                                        event_handler_ptr handler) {
  CAF_ASSERT(mpx != nullptr);
  return make_counted<socket_manager_impl>(std::move(mpx), std::move(handler));
}

// -- free functions -----------------------------------------------------------

void intrusive_ptr_add_ref(socket_manager* ptr) noexcept {
  ptr->ref();
}

void intrusive_ptr_release(socket_manager* ptr) noexcept {
  ptr->deref();
}

} // namespace caf::net
