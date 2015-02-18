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
#include <mutex>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <exception>
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/node_id.hpp"
#include "caf/attachable.hpp"
#include "caf/message_id.hpp"
#include "caf/exit_reason.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/abstract_channel.hpp"

#include "caf/detail/type_traits.hpp"
#include "caf/detail/functor_attachable.hpp"

namespace caf {

/**
 * A unique actor ID.
 * @relates abstract_actor
 */
using actor_id = uint32_t;

/**
 * Denotes an ID that is never used by an actor.
 */
constexpr actor_id invalid_actor_id = 0;

using abstract_actor_ptr = intrusive_ptr<abstract_actor>;

/**
 * Base class for all actor implementations.
 */
class abstract_actor : public abstract_channel {
 public:
  /**
   * Attaches `ptr` to this actor. The actor will call `ptr->detach(...)` on
   * exit, or immediately if it already finished execution.
   */
  void attach(attachable_ptr ptr);

  /**
   * Convenience function that attaches the functor `f` to this actor. The
   * actor executes `f()` on exit or immediatley if it is not running.
   */
  template <class F>
  void attach_functor(F f) {
    attach(attachable_ptr{new detail::functor_attachable<F>(std::move(f))});
  }

  /**
   * Returns the logical actor address.
   */
  actor_addr address() const;

  /**
   * Detaches the first attached object that matches `what`.
   */
  size_t detach(const attachable::token& what);

  /**
   * Links this actor to `whom`.
   */
  inline void link_to(const actor_addr& whom) {
    link_impl(establish_link_op, whom);
  }

  /**
   * Links this actor to `whom`.
   */
  template <class ActorHandle>
  void link_to(const ActorHandle& whom) {
    link_to(whom.address());
  }

  /**
   * Unlinks this actor from `whom`.
   */
  inline void unlink_from(const actor_addr& other) {
    link_impl(remove_link_op, other);
  }

  /**
   * Unlinks this actor from `whom`.
   */
  template <class ActorHandle>
  void unlink_from(const ActorHandle& other) {
    unlink_from(other.address());
  }

  /**
   * Establishes a link relation between this actor and `other`
   * and returns whether the operation succeeded.
   */
  inline bool establish_backlink(const actor_addr& other) {
    return link_impl(establish_backlink_op, other);
  }

  /**
   * Removes the link relation between this actor and `other`
   * and returns whether the operation succeeded.
   */
  inline bool remove_backlink(const actor_addr& other) {
    return link_impl(remove_backlink_op, other);
  }

  /**
   * Returns the unique ID of this actor.
   */
  inline uint32_t id() const {
    return m_id;
  }

  /**
   * Returns the actor's exit reason or
   * `exit_reason::not_exited` if it's still alive.
   */
  inline uint32_t exit_reason() const {
    return m_exit_reason;
  }

  /**
   * Returns the set of accepted messages types as strings or
   * an empty set if this actor is untyped.
   */
  virtual std::set<std::string> message_types() const;

  /**
   * Returns the execution unit currently used by this actor.
   * @warning not thread safe
   */
  inline execution_unit* host() const {
    return m_host;
  }

 protected:
  /**
   * Creates a non-proxy instance.
   */
  abstract_actor(size_t initial_ref_count = 0);

  /**
   * Creates a proxy instance for a proxy running on `nid`.
   */
  abstract_actor(actor_id aid, node_id nid, size_t initial_ref_count = 0);

  /**
   * Called by the runtime system to perform cleanup actions for this actor.
   * Subtypes should always call this member function when overriding it.
   */
  void cleanup(uint32_t reason);

  /**
   * Returns `exit_reason() != exit_reason::not_exited`.
   */
  inline bool exited() const {
    return exit_reason() != exit_reason::not_exited;
  }

  /**
   * Sets the execution unit for this actor.
   */
  inline void host(execution_unit* new_host) {
    m_host = new_host;
  }

  /****************************************************************************
   *                 here be dragons: end of public interface                 *
   ****************************************************************************/

 public:
  /** @cond PRIVATE */

  enum linking_operation {
    establish_link_op,
    establish_backlink_op,
    remove_link_op,
    remove_backlink_op
  };

  enum actor_state_flag {
    //                                used by ...
    trap_exit_flag         = 0x01, // local_actor
    has_timeout_flag       = 0x02, // mixin::single_timeout
    is_registered_flag     = 0x04, // no_resume, resumable, and scoped_actor
    is_initialized_flag    = 0x08, // event-based actors
    is_blocking_flag       = 0x10  // blocking_actor
  };

  inline void set_flag(bool enable_flag, actor_state_flag mask) {
    m_flags = enable_flag ? m_flags | static_cast<int>(mask)
                          : m_flags & ~static_cast<int>(mask);
  }

  inline bool get_flag(actor_state_flag mask) const {
    return static_cast<bool>(m_flags & static_cast<int>(mask));
  }

  inline bool has_timeout() const {
    return get_flag(has_timeout_flag);
  }

  inline void has_timeout(bool value) {
    set_flag(value, has_timeout_flag);
  }

  inline bool is_registered() const {
    return get_flag(is_registered_flag);
  }

  void is_registered(bool value);

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

 protected:
  virtual bool link_impl(linking_operation op, const actor_addr& other);

  bool establish_link_impl(const actor_addr& other);

  bool remove_link_impl(const actor_addr& other);

  bool establish_backlink_impl(const actor_addr& other);

  bool remove_backlink_impl(const actor_addr& other);

  inline void attach_impl(attachable_ptr& ptr) {
    ptr->next.swap(m_attachables_head);
    m_attachables_head.swap(ptr);
  }

  static size_t detach_impl(const attachable::token& what,
                            attachable_ptr& ptr,
                            bool stop_on_first_hit = false,
                            bool dry_run = false);

  // Tries to run a custom exception handler for `eptr`.
  optional<uint32_t> handle(const std::exception_ptr& eptr);

  // cannot be changed after construction
  const actor_id m_id;

  // initially set to exit_reason::not_exited
  std::atomic<uint32_t> m_exit_reason;

  // guards access to m_exit_reason, m_attachables, and m_links
  mutable std::mutex m_mtx;

  // attached functors that are executed on cleanup (monitors, links, etc)
  attachable_ptr m_attachables_head;

  // identifies the execution unit this actor is currently executed by
  execution_unit* m_host;

  /** @endcond */
};

} // namespace caf

#endif // CAF_ABSTRACT_ACTOR_HPP
