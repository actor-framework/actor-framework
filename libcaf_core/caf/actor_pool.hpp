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

#ifndef CAF_ACTOR_POOL_HPP
#define CAF_ACTOR_POOL_HPP

#include <vector>
#include <functional>

#include "caf/locks.hpp"
#include "caf/actor.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/mailbox_element.hpp"

#include "caf/detail/split_join.hpp"
#include "caf/detail/shared_spinlock.hpp"

namespace caf {

/// An actor poool is a lightweight abstraction for a set of workers.
/// The pool itself is an actor, meaning that it can be passed
/// around in an actor system to hide the actual set of workers.
///
/// After construction, new workers can be added via `{'SYS', 'PUT', actor}`
/// messages, e.g., `send(my_pool, sys_atom::value, put_atom::value, worker)`.
/// `{'SYS', 'DELETE', actor}` messages remove a worker from the set,
/// whereas `{'SYS', 'GET'}` returns a `vector<actor>` containing all workers.
///
/// Note that the pool *always*  sends exit messages to all of its workers
/// when forced to quit. The pool monitors all of its workers. Messages queued
/// up in a worker's mailbox are lost, i.e., the pool itself does not buffer
/// and resend messages. Advanced caching or resend strategies can be
/// implemented in a policy.
///
/// It is wort mentioning that the pool is *not* an event-based actor.
/// Neither does it live in its own thread. Messages are dispatched immediately
/// during the enqueue operation. Any user-defined policy thus has to dispatch
/// messages with as little overhead as possible, because the dispatching
/// runs in the context of the sender.
class actor_pool : public abstract_actor {
public:
  using uplock = upgrade_lock<detail::shared_spinlock>;
  using actor_vec = std::vector<actor>;
  using factory = std::function<actor ()>;
  using policy = std::function<void (uplock&, const actor_vec&,
                                     mailbox_element_ptr&, execution_unit*)>;

  /// Returns a simple round robin dispatching policy.
  static policy round_robin();

  /// Returns a broadcast dispatching policy.
  static policy broadcast();

  /// Returns a random dispatching policy.
  static policy random();

  /// Returns a split/join dispatching policy. The function object `sf`
  /// distributes a work item to all workers (split step) and the function
  /// object `jf` joins individual results into a single one with `init`
  /// as initial value of the operation.
  /// @tparam T Result type of the join step.
  /// @tparam Join Function object with signature `void (T&, message&)`.
  /// @tparam Split Function object with signature
  ///               `void (vector<pair<actor, message>>&, message&)`. The first
  ///               argument is a mapping from actors (workers) to tasks
  ///               (messages). The second argument is the input message.
  ///               The default split policy broadcasts the work item to all
  ///               workers.
  template <class T, class Join, class Split = detail::nop_split>
  static policy split_join(Join jf, Split sf = Split(), T init = T()) {
    using impl = detail::split_join<T, Split, Join>;
    return impl{std::move(init), std::move(sf), std::move(jf)};
  }

  ~actor_pool();

  /// Returns an actor pool without workers using the dispatch policy `pol`.
  static actor make(policy pol);

  /// Returns an actor pool with `n` workers created by the factory
  /// function `fac` using the dispatch policy `pol`.
  static actor make(size_t n, factory fac, policy pol);

  void enqueue(const actor_addr& sender, message_id mid,
               message content, execution_unit* host) override;

  void enqueue(mailbox_element_ptr what, execution_unit* host) override;

  actor_pool();

private:
  bool filter(upgrade_lock<detail::shared_spinlock>&, const actor_addr& sender,
              message_id mid, const message& content, execution_unit* host);

  // call without workers_mtx_ held
  void quit();

  detail::shared_spinlock workers_mtx_;
  std::vector<actor> workers_;
  policy policy_;
  uint32_t planned_reason_;
};

} // namespace caf

#endif // CAF_ACTOR_POOL_HPP
