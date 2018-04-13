/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <vector>
#include <functional>

#include "caf/locks.hpp"
#include "caf/actor.hpp"
#include "caf/execution_unit.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/monitorable_actor.hpp"

#include "caf/detail/split_join.hpp"
#include "caf/detail/shared_spinlock.hpp"

namespace caf {

/// An actor poool is a lightweight abstraction for a set of workers.
/// The pool itself is an actor, meaning that it can be passed
/// around in an actor system to hide the actual set of workers.
///
/// After construction, new workers can be added via `{'SYS', 'PUT', actor}`
/// messages, e.g., `send(my_pool, sys_atom::value, put_atom::value, worker)`.
/// `{'SYS', 'DELETE', actor}` messages remove a specific worker from the set,
/// `{'SYS', 'DELETE'}` removes all workers, and `{'SYS', 'GET'}` returns a
/// `vector<actor>` containing all workers.
///
/// Note that the pool *always*  sends exit messages to all of its workers
/// when forced to quit. The pool monitors all of its workers. Messages queued
/// up in a worker's mailbox are lost, i.e., the pool itself does not buffer
/// and resend messages. Advanced caching or resend strategies can be
/// implemented in a policy.
///
/// It is worth mentioning that the pool is *not* an event-based actor.
/// Neither does it live in its own thread. Messages are dispatched immediately
/// during the enqueue operation. Any user-defined policy thus has to dispatch
/// messages with as little overhead as possible, because the dispatching
/// runs in the context of the sender.
/// @experimental
class actor_pool : public monitorable_actor {
public:
  using uplock = upgrade_lock<detail::shared_spinlock>;
  using actor_vec = std::vector<actor>;
  using factory = std::function<actor ()>;
  using policy = std::function<void (actor_system&, uplock&, const actor_vec&,
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

  ~actor_pool() override;

  /// Returns an actor pool without workers using the dispatch policy `pol`.
  static actor make(execution_unit* eu, policy pol);

  /// Returns an actor pool with `n` workers created by the factory
  /// function `fac` using the dispatch policy `pol`.
  static actor make(execution_unit* eu, size_t num_workers, const factory& fac, policy pol);

  void enqueue(mailbox_element_ptr what, execution_unit* eu) override;

  actor_pool(actor_config& cfg);

  void on_destroy() override;

protected:
  void on_cleanup(const error& reason) override;

private:
  bool filter(upgrade_lock<detail::shared_spinlock>&,
              const strong_actor_ptr& sender, message_id mid,
              message_view& mv, execution_unit* eu);

  // call without workers_mtx_ held
  void quit(execution_unit* host);

  detail::shared_spinlock workers_mtx_;
  std::vector<actor> workers_;
  policy policy_;
  exit_reason planned_reason_;
};

} // namespace caf

