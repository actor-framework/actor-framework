/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_ABSTRACT_ACTOR_HPP
#define CAF_ABSTRACT_ACTOR_HPP

#include <set>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/node_id.hpp"
#include "caf/attachable.hpp"
#include "caf/message_id.hpp"
#include "caf/exit_reason.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/execution_unit.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/abstract_channel.hpp"

#include "caf/detail/disposer.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/functor_attachable.hpp"

namespace caf {

/// A unique actor ID.
/// @relates abstract_actor
using actor_id = uint64_t;

/// Denotes an ID that is never used by an actor.
constexpr actor_id invalid_actor_id = 0;

/// Base class for all actor implementations.
class abstract_actor : public abstract_channel {
public:
  // allow placement new (only)
  inline void* operator new(std::size_t, void* ptr) {
    return ptr;
  }

  actor_control_block* ctrl() const;

  virtual ~abstract_actor();

  /// Cleans up any remaining state before the destructor is called.
  /// This function makes sure it is safe to call virtual functions
  /// in sub classes before destroying the object, because calling
  /// virtual function in the destructor itself is not safe. Any override
  /// implementation is required to call `super::destroy()` at the end.
  virtual void on_destroy();

  void enqueue(strong_actor_ptr sender, message_id mid,
               message content, execution_unit* host) override;

  /// Enqueues a new message wrapped in a `mailbox_element` to the actor.
  /// This `enqueue` variant allows to define forwarding chains.
  virtual void enqueue(mailbox_element_ptr what, execution_unit* host) = 0;

  /// Attaches `ptr` to this actor. The actor will call `ptr->detach(...)` on
  /// exit, or immediately if it already finished execution.
  virtual void attach(attachable_ptr ptr) = 0;

  /// Convenience function that attaches the functor `f` to this actor. The
  /// actor executes `f()` on exit or immediatley if it is not running.
  template <class F>
  void attach_functor(F f) {
    attach(attachable_ptr{new detail::functor_attachable<F>(std::move(f))});
  }

  /// Returns the logical actor address.
  actor_addr address() const;

  /// Detaches the first attached object that matches `what`.
  virtual size_t detach(const attachable::token& what) = 0;

  /// Establishes a link relation between this actor and `x`
  /// and returns whether the operation succeeded.
  inline bool establish_backlink(abstract_actor* x) {
    return link_impl(establish_backlink_op, x);
  }

  /// Removes the link relation between this actor and `x`
  /// and returns whether the operation succeeded.
  inline bool remove_backlink(abstract_actor* x) {
    return link_impl(remove_backlink_op, x);
  }

  /// Returns the set of accepted messages types as strings or
  /// an empty set if this actor is untyped.
  virtual std::set<std::string> message_types() const;

  /// Returns the ID of this actor.
  actor_id id() const noexcept;

  /// Returns the node this actor is living on.
  node_id node() const noexcept;

  /// Returns the system that created this actor (or proxy).
  actor_system& home_system() const noexcept;

  /****************************************************************************
   *                 here be dragons: end of public interface                 *
   ****************************************************************************/

  /// @cond PRIVATE

  template <class... Ts>
  void eq_impl(message_id mid, strong_actor_ptr sender,
               execution_unit* ctx, Ts&&... xs) {
    enqueue(make_mailbox_element(std::move(sender), mid,
                                 {}, std::forward<Ts>(xs)...),
            ctx);
  }

  enum linking_operation {
    establish_link_op,
    establish_backlink_op,
    remove_link_op,
    remove_backlink_op
  };

  // flags storing runtime information                     used by ...
  static constexpr int has_timeout_flag       = 0x0004; // single_timeout
  static constexpr int is_registered_flag     = 0x0008; // (several actors)
  static constexpr int is_initialized_flag    = 0x0010; // event-based actors
  static constexpr int is_blocking_flag       = 0x0020; // blocking_actor
  static constexpr int is_detached_flag       = 0x0040; // local_actor
  static constexpr int is_priority_aware_flag = 0x0080; // local_actor
  static constexpr int is_serializable_flag   = 0x0100; // local_actor
  static constexpr int is_migrated_from_flag  = 0x0200; // local_actor
  static constexpr int has_used_aout_flag     = 0x0400; // local_actor
  static constexpr int is_terminated_flag     = 0x0800; // local_actor
  static constexpr int is_cleaned_up_flag     = 0x1000; // monitorable_actor

  inline void set_flag(bool enable_flag, int mask) {
    auto x = flags();
    flags(enable_flag ? x | mask : x & ~mask);
  }

  inline bool get_flag(int mask) const {
    return static_cast<bool>(flags() & mask);
  }

  inline bool has_timeout() const {
    return get_flag(has_timeout_flag);
  }

  inline void has_timeout(bool value) {
    set_flag(value, has_timeout_flag);
  }

  inline bool is_initialized() const {
    return get_flag(is_initialized_flag);
  }

  inline void is_initialized(bool value) {
    set_flag(value, is_initialized_flag);
  }

  inline bool is_blocking() const {
    return get_flag(is_blocking_flag);
  }

  inline void is_blocking(bool value) {
    set_flag(value, is_blocking_flag);
  }

  inline bool is_detached() const {
    return get_flag(is_detached_flag);
  }

  inline void is_detached(bool value) {
    set_flag(value, is_detached_flag);
  }

  inline bool is_priority_aware() const {
    return get_flag(is_priority_aware_flag);
  }

  inline void is_priority_aware(bool value) {
    set_flag(value, is_priority_aware_flag);
  }

  inline bool is_serializable() const {
    return get_flag(is_serializable_flag);
  }

  inline void is_serializable(bool value) {
    set_flag(value, is_serializable_flag);
  }

  inline bool is_migrated_from() const {
    return get_flag(is_migrated_from_flag);
  }

  inline void is_migrated_from(bool value) {
    set_flag(value, is_migrated_from_flag);
  }

  inline bool is_registered() const {
    return get_flag(is_registered_flag);
  }

  void is_registered(bool value);

  inline bool is_actor_decorator() const {
    return static_cast<bool>(flags() & is_actor_decorator_mask);
  }

  inline bool is_terminated() const {
    return get_flag(is_terminated_flag);
  }

  inline void is_terminated(bool value) {
    set_flag(value, is_terminated_flag);
  }

  inline bool is_cleaned_up() const {
    return get_flag(is_cleaned_up_flag);
  }

  inline void is_cleaned_up(bool value) {
    set_flag(value, is_cleaned_up_flag);
  }

  virtual bool link_impl(linking_operation op, abstract_actor* other) = 0;

 /// @endcond

protected:
  /// Creates a new actor instance.
  explicit abstract_actor(actor_config& cfg);

private:
  // prohibit copies, assigments, and heap allocations
  void* operator new(size_t);
  void* operator new[](size_t);
  abstract_actor(const abstract_actor&) = delete;
  abstract_actor& operator=(const abstract_actor&) = delete;
};

std::string to_string(abstract_actor::linking_operation op);

} // namespace caf

#endif // CAF_ABSTRACT_ACTOR_HPP
