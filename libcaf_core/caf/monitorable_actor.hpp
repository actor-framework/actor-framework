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

#ifndef CAF_MONITORABLE_ACTOR_HPP
#define CAF_MONITORABLE_ACTOR_HPP

#include <set>
#include <mutex>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <type_traits>
#include <condition_variable>

#include "caf/type_nr.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/mailbox_element.hpp"

#include "caf/detail/type_traits.hpp"
#include "caf/detail/functor_attachable.hpp"

namespace caf {

/// Base class for all actor implementations.
class monitorable_actor : public abstract_actor {
public:
  /// Returns an implementation-dependent name for logging purposes, which
  /// is only valid as long as the actor is running. The default
  /// implementation simply returns "actor".
  virtual const char* name() const;

  void attach(attachable_ptr ptr) override;

  size_t detach(const attachable::token& what) override;

  // -- linking and monitoring -------------------------------------------------

  /// Links this actor to `x`.
  void link_to(const actor_addr& x) {
    auto ptr = actor_cast<strong_actor_ptr>(x);
    if (! ptr || ptr->get() == this)
      return;
    link_impl(establish_link_op, ptr->get());
  }

  /// Links this actor to `x`.
  template <class ActorHandle>
  void link_to(const ActorHandle& x) {
    auto ptr = actor_cast<abstract_actor*>(x);
    if (! ptr || ptr == this)
      return;
    link_impl(establish_link_op, ptr);
  }

  /// Unlinks this actor from `x`.
  void unlink_from(const actor_addr& x) {
    auto ptr = actor_cast<strong_actor_ptr>(x);
    if (! ptr || ptr->get() == this)
      return;
    link_impl(remove_link_op, ptr->get());
  }

  /// Links this actor to `x`.
  template <class ActorHandle>
  void unlink_from(const ActorHandle& x) {
    auto ptr = actor_cast<abstract_actor*>(x);
    if (! ptr || ptr == this)
      return;
    link_impl(remove_link_op, ptr);
  }

  /// @cond PRIVATE

  /// Called by the runtime system to perform cleanup actions for this actor.
  /// Subtypes should always call this member function when overriding it.
  /// This member function is thread-safe, and if the actor has already exited
  /// upon invocation, nothing is done. The return value of this member
  /// function is ignored by scheduled actors.
  virtual bool cleanup(error&& reason, execution_unit* host);

  /// @endcond

protected:
  /// Allows subclasses to add additional cleanup code to the
  /// critical secion in `cleanup`. This member function is
  /// called inside of a critical section.
  virtual void on_cleanup();

  /// Sends a response message if `what` is a request.
  void bounce(mailbox_element_ptr& what);

  /// Sends a response message if `what` is a request.
  void bounce(mailbox_element_ptr& what, const error& err);

  /// Creates a new actor instance.
  explicit monitorable_actor(actor_config& cfg);

  /****************************************************************************
   *                 here be dragons: end of public interface                 *
   ****************************************************************************/

  bool link_impl(linking_operation op, abstract_actor* x) override;

  bool establish_link_impl(abstract_actor* other);

  bool remove_link_impl(abstract_actor* other);

  bool establish_backlink_impl(abstract_actor* other);

  bool remove_backlink_impl(abstract_actor* other);

  // precondition: `mtx_` is acquired
  inline void attach_impl(attachable_ptr& ptr) {
    ptr->next.swap(attachables_head_);
    attachables_head_.swap(ptr);
  }

  // precondition: `mtx_` is acquired
  static size_t detach_impl(const attachable::token& what,
                            attachable_ptr& ptr,
                            bool stop_on_first_hit = false,
                            bool dry_run = false);

  // handles only `exit_msg` and `sys_atom` messages;
  // returns true if the message is handled
  bool handle_system_message(mailbox_element& node, execution_unit* context,
                             bool trap_exit);

  // handles `exit_msg`, `sys_atom` messages, and additionally `down_msg`
  // with `down_msg_handler`; returns true if the message is handled
  template <class F>
  bool handle_system_message(mailbox_element& x, execution_unit* context,
                             bool trap_exit, F& down_msg_handler) {
    auto& content = x.content();
    if (content.type_token() == make_type_token<down_msg>()) {
      if (content.shared()) {
        auto vptr = content.copy(0);
        down_msg_handler(vptr->get_mutable_as<down_msg>());
      } else {
        down_msg_handler(content.get_mutable_as<down_msg>(0));
      }
      return true;
    }
    return handle_system_message(x, context, trap_exit);
  }

  // Calls `fun` with exclusive access to an actor's state.
  template <class F>
  auto exclusive_critical_section(F fun) -> decltype(fun()) {
    std::unique_lock<std::mutex> guard{mtx_};
    return fun();
  }

  template <class F>
  auto shared_critical_section(F fun) -> decltype(fun()) {
    std::unique_lock<std::mutex> guard{mtx_};
    return fun();
  }

  // is protected by `mtx_` in actors that are not scheduled, but
  // can be accessed without lock for event-based and blocking actors
  error fail_state_;

  // guards access to exit_state_, attachables_, links_,
  // and enqueue operations if actor is thread-mapped
  mutable std::mutex mtx_;

  // only used in blocking and thread-mapped actors
  mutable std::condition_variable cv_;

  // attached functors that are executed on cleanup (monitors, links, etc)
  attachable_ptr attachables_head_;

 /// @endcond
};

std::string to_string(abstract_actor::linking_operation op);

} // namespace caf

#endif // CAF_MONITORABLE_ACTOR_HPP
