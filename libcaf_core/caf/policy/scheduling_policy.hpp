/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_SCHEDULING_POLICY_HPP
#define CAF_SCHEDULING_POLICY_HPP

#include "caf/fwd.hpp"

namespace caf {

namespace policy {

enum class timed_fetch_result {
  no_message,
  indeterminate,
  success

};

/**
 * The scheduling_policy <b>concept</b> class. Please note that this
 * class is <b>not</b> implemented. It only explains the all
 * required member function and their behavior for any scheduling policy.
 */
class scheduling_policy {

 public:

  /**
   * This type can be set freely by any implementation and is
   * used by callers to pass the result of `init_timeout` back to
   * `fetch_messages`.
   */
  using timeout_type = int;

  /**
   * Fetches new messages from the actor's mailbox and feeds them
   * to the given callback. The member function returns `false` if
   * no message was read, `true` otherwise.
   *
   * In case this function returned `false,` the policy also sets the state
   * of the actor to blocked. Any caller must evaluate the return value and
   * act properly in case of a returned `false,` i.e., it must <b>not</b>
   * atttempt to call any further function on the actor, since it might be
   * already in the pipe for re-scheduling.
   */
  template <class Actor, typename F>
  bool fetch_messages(Actor* self, F cb);

  /**
   * Tries to fetch new messages from the actor's mailbox and to feeds
   * them to the given callback. The member function returns `false`
   * if no message was read, `true` otherwise.
   *
   * This member function does not have any side-effect other than removing
   * messages from the actor's mailbox.
   */
  template <class Actor, typename F>
  bool try_fetch_messages(Actor* self, F cb);

  /**
   * Tries to fetch new messages before a timeout occurs. The message
   * can either return `success,` or `no_message,`
   * or `indeterminate`. The latter occurs for cooperative scheduled
   * operations and means that timeouts are signaled using
   * special-purpose messages. In this case, clients have to simply
   * wait for the arriving message.
   */
  template <class Actor, typename F>
  timed_fetch_result fetch_messages(Actor* self, F cb, timeout_type abs_time);

  /**
   * Enqueues given message to the actor's mailbox and take any
   * steps to resume the actor if it's currently blocked.
   */
  template <class Actor>
  void enqueue(Actor* self, const actor_addr& sender, message_id mid,
         message& msg, execution_unit* host);

  /**
   * Starts the given actor either by launching a thread or enqueuing
   * it to the cooperative scheduler's job queue.
   */
  template <class Actor>
  void launch(Actor* self);

};

} // namespace policy
} // namespace caf

#endif // CAF_SCHEDULING_POLICY_HPP
