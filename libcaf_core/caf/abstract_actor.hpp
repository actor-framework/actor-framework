// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/attachable.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/functor_attachable.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/exit_reason.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_id.hpp"
#include "caf/node_id.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

namespace caf::io::basp {

template <class>
class remote_message_handler;

} // namespace caf::io::basp

namespace caf {

CAF_CORE_EXPORT void intrusive_ptr_release(actor_control_block*);

/// A unique actor ID.
/// @relates abstract_actor
using actor_id = uint64_t;

/// Denotes an ID that is never used by an actor.
constexpr actor_id invalid_actor_id = 0;

/// Base class for all actor implementations.
class CAF_CORE_EXPORT abstract_actor {
public:
  // -- friends ----------------------------------------------------------------

  template <class T>
  friend class actor_storage;

  template <class>
  friend class caf::io::basp::remote_message_handler;

  friend CAF_CORE_EXPORT void intrusive_ptr_release(actor_control_block*);

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~abstract_actor();

  abstract_actor(const abstract_actor&) = delete;

  abstract_actor& operator=(const abstract_actor&) = delete;

  // -- attachables ------------------------------------------------------------

  /// Attaches `ptr` to this actor. The actor will call `ptr->detach(...)` on
  /// exit, or immediately if it already finished execution.
  void attach(attachable_ptr ptr);

  /// Convenience function that attaches the functor `f` to this actor. The
  /// actor executes `f()` on exit or immediately if it is not running.
  template <class F>
  void attach_functor(F f) {
    attach(attachable_ptr{new detail::functor_attachable<F>(std::move(f))});
  }

  /// Detaches the first attached object that matches `what`.
  size_t detach(const attachable::token& what);

  // -- linking ----------------------------------------------------------------

  /// Links this actor to `other`.
  void link_to(const actor_addr& other);

  /// Links this actor to `other`.
  template <class ActorHandle>
  void link_to(const ActorHandle& other) {
    if (other) {
      if (auto* ptr = other.get()->get(); ptr != this)
        add_link(other.get()->get());
    }
  }

  /// Unlinks this actor from `addr`.
  void unlink_from(const actor_addr& other);

  /// Links this actor to `hdl`.
  template <class ActorHandle>
  void unlink_from(const ActorHandle& other) {
    if (other) {
      if (auto* ptr = other.get()->get(); ptr != this)
        remove_link(other.get()->get());
    }
  }

  // -- properties -------------------------------------------------------------

  /// Returns an implementation-dependent name for logging purposes, which
  /// is only valid as long as the actor is running. The default
  /// implementation simply returns "actor".
  virtual const char* name() const = 0;

  /// Returns the set of accepted messages types as strings or
  /// an empty set if this actor is untyped.
  virtual std::set<std::string> message_types() const;

  /// Returns the ID of this actor.
  actor_id id() const noexcept;

  /// Returns the node this actor is living on.
  node_id node() const noexcept;

  /// Returns the system that created this actor (or proxy).
  actor_system& home_system() const noexcept;

  /// Returns the control block for this actor.
  actor_control_block* ctrl() const;

  /// Returns the logical actor address.
  actor_addr address() const noexcept;

  // -- messaging --------------------------------------------------------------

  /// Enqueues a new message wrapped in a `mailbox_element` to the actor.
  /// This `enqueue` variant allows to define forwarding chains.
  /// @returns `true` if the message has added to the mailbox, `false`
  ///          otherwise. In the latter case, the actor terminated and the
  ///          message has been dropped. Once this function returns `false`, it
  ///          returns `false` for all future invocations.
  /// @note The returned value is purely informational and may be used to
  ///       discard actor handles early. Messages may still get dropped later
  ///       even if this function returns `true`. In particular when dealing
  ///       with remote actors.
  virtual bool enqueue(mailbox_element_ptr what, scheduler* sched) = 0;

  /// Called by the testing DSL to peek at the next element in the mailbox. Do
  /// not call this function in production code! The default implementation
  /// always returns `nullptr`.
  /// @returns A pointer to the next mailbox element or `nullptr` if the
  ///          mailbox is empty or the actor does not have a mailbox.
  virtual mailbox_element* peek_at_next_mailbox_element();

  /// Called by the runtime system to perform cleanup actions for this actor.
  /// Subtypes should always call this member function when overriding it.
  /// This member function is thread-safe, and if the actor has already exited
  /// upon invocation, nothing is done. The return value of this member
  /// function is ignored by scheduled actors.
  bool cleanup(error&& reason, scheduler* sched);

  // -- here be dragons: end of public interface -------------------------------

  /// @cond

  /// Indicates that the actor system shall not wait for this actor to
  /// finish execution on shutdown.
  static constexpr int is_hidden_flag = 0b0000'0000'0001;

  /// Indicates that the actor is registered at the actor system.
  static constexpr int is_registered_flag = 0b0000'0000'0010;

  /// Indicates that the actor has been initialized.
  static constexpr int is_initialized_flag = 0b0000'0000'0100;

  /// Indicates that the actor uses blocking message handlers.
  static constexpr int is_blocking_flag = 0b0000'0000'1000;

  /// Indicates that the actor runs in its own thread.
  static constexpr int is_detached_flag = 0b0000'0001'0000;

  /// Indicates that the actor collects metrics.
  static constexpr int collects_metrics_flag = 0b0000'0010'0000;

  /// Indicates that the actor has used aout at least once and thus needs to
  /// call `flush` when shutting down.
  static constexpr int has_used_aout_flag = 0b0000'0100'0000;

  /// Indicates that the actor has terminated and waits for its destructor to
  /// run.
  static constexpr int is_terminated_flag = 0b0000'1000'0000;

  /// Indicates that the actor is currently shutting down and thus may no longer
  /// set a new behavior.
  static constexpr int is_shutting_down_flag = 0b0001'0000'0000;

  /// Indicates that the actor is currently inactive.
  static constexpr int is_inactive_flag = 0b0010'0000'0000;

  void setf(int flag) {
    auto x = flags();
    flags(x | flag);
  }

  void unsetf(int flag) {
    auto x = flags();
    flags(x & ~flag);
  }

  bool getf(int flag) const {
    return (flags() & flag) != 0;
  }

  /// Sets `is_registered_flag` and calls `system().registry().inc_running()`.
  void register_at_system();

  /// Unsets `is_registered_flag` and calls `system().registry().dec_running()`.
  void unregister_from_system();

  /// Calls `fun` with exclusive access to an actor's state.
  template <class F>
  auto exclusive_critical_section(F fun) -> decltype(fun()) {
    std::unique_lock<std::mutex> guard{mtx_};
    return fun();
  }

  /// Calls `fun` with readonly access to an actor's state.
  template <class F>
  auto shared_critical_section(F fun) -> decltype(fun()) {
    std::unique_lock<std::mutex> guard{mtx_};
    return fun();
  }

  /// Calls `fun` with exclusive access to the state of both `p1` and `p2`. This
  /// function guarantees that the order of acquiring the locks is always
  /// identical, independently from the order of `p1` and `p2`.
  template <class F>
  static auto joined_exclusive_critical_section(abstract_actor* p1,
                                                abstract_actor* p2, F fun)
    -> decltype(fun()) {
    // Make sure to allocate locks always in the same order by starting on the
    // actor with the lowest address.
    CAF_ASSERT(p1 != p2 && p1 != nullptr && p2 != nullptr);
    if (p1 < p2) {
      std::unique_lock<std::mutex> guard1{p1->mtx_};
      std::unique_lock<std::mutex> guard2{p2->mtx_};
      return fun();
    }
    std::unique_lock<std::mutex> guard1{p2->mtx_};
    std::unique_lock<std::mutex> guard2{p1->mtx_};
    return fun();
  }

  /// @endcond

protected:
  // -- callbacks --------------------------------------------------------------

  /// Called on actor if the last strong reference to it expired without
  /// a prior call to `quit(exit_reason::not_exited)`.
  /// @pre `getf(is_terminated_flag) == false`
  /// @post `getf(is_terminated_flag) == true`
  virtual void on_unreachable();

  /// Called from `cleanup` to perform extra cleanup actions for this actor.
  virtual void on_cleanup(const error& reason);

  // -- properties -------------------------------------------------------------

  // note: *both* operations use relaxed memory order, this is because
  // only the actor itself is granted write access while all access
  // from other actors or threads is always read-only; further, only
  // flags that are considered constant after an actor has launched are
  // read by others, i.e., there is no acquire/release semantic between
  // setting and reading flags
  int flags() const {
    return flags_.load(std::memory_order_relaxed);
  }

  void flags(int new_value) {
    flags_.store(new_value, std::memory_order_relaxed);
  }

  /// Checks whether this actor has terminated.
  bool is_terminated() const noexcept {
    return getf(is_terminated_flag);
  }

  // -- constructors, destructors, and assignment operators --------------------

  explicit abstract_actor(actor_config& cfg);

  // -- attachables ------------------------------------------------------------

  // precondition: `mtx_` is acquired
  void attach_impl(attachable_ptr& ptr);

  // precondition: `mtx_` is acquired
  size_t detach_impl(const attachable::token& what, bool stop_on_hit = false,
                     bool dry_run = false);

  // -- linking ----------------------------------------------------------------

  /// Causes the actor to establish a link to `other`.
  void add_link(abstract_actor* other);

  /// Causes the actor to remove any established link to `other`.
  void remove_link(abstract_actor* other);

  /// Adds an entry to `other` to the link table of this actor.
  /// @warning Must be called inside a critical section, i.e.,
  ///          while holding `mtx_`.
  virtual bool add_backlink(abstract_actor* other);

  /// Removes an entry to `other` from the link table of this actor.
  /// @warning Must be called inside a critical section, i.e.,
  ///          while holding `mtx_`.
  virtual bool remove_backlink(abstract_actor* other);

  // -- member variables -------------------------------------------------------

  /// Holds several state and type flags.
  std::atomic<int> flags_;

  /// Guards members that may be subject to concurrent access . For example,
  /// `exit_state_`, `attachables_`, and `links_`.
  mutable std::mutex mtx_;

  /// Allows blocking actors to actively wait for incoming messages.
  mutable std::condition_variable cv_;

  /// Stores the user-defined exit reason if this actor has finished execution.
  error fail_state_;

  /// Points to the first attachable in the linked list of attachables (if any).
  attachable_ptr attachables_head_;

private:
  /// Forces the actor to close its mailbox and drop all messages. The only
  /// place calling this member function is
  /// `intrusive_ptr_release(actor_control_block*)` before calling
  /// `on_unreachable`.
  virtual void force_close_mailbox() = 0;
};

} // namespace caf
