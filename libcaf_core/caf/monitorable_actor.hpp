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
#include <exception>
#include <type_traits>
#include <condition_variable>

#include "caf/abstract_actor.hpp"

#include "caf/detail/type_traits.hpp"
#include "caf/detail/functor_attachable.hpp"

namespace caf {

/// Base class for all actor implementations.
class monitorable_actor : public abstract_actor {
public:
  void attach(attachable_ptr ptr) override;

  size_t detach(const attachable::token& what) override;

  /// @cond PRIVATE

  // Returns `exit_reason_ != exit_reason::not_exited`.
  inline bool exited() const {
    return exit_reason_ != exit_reason::not_exited;
  }

  /// @endcond

protected:
  /// Creates a new actor instance.
  explicit monitorable_actor(actor_config& cfg);

  /// Creates a new actor instance.
  monitorable_actor(actor_system* sys, actor_id aid, node_id nid);

  /// Creates a new actor instance.
  monitorable_actor(actor_system* sys, actor_id aid, node_id nid, int flags);

  /// Called by the runtime system to perform cleanup actions for this actor.
  /// Subtypes should always call this member function when overriding it.
  virtual void cleanup(exit_reason reason, execution_unit* host);

  /****************************************************************************
   *                 here be dragons: end of public interface                 *
   ****************************************************************************/

  bool link_impl(linking_operation op, const actor_addr& other) override;

  bool establish_link_impl(const actor_addr& other);

  bool remove_link_impl(const actor_addr& other);

  bool establish_backlink_impl(const actor_addr& other);

  bool remove_backlink_impl(const actor_addr& other);

  // Tries to run a custom exception handler for `eptr`.
  maybe<exit_reason> handle(const std::exception_ptr& eptr);

  inline void attach_impl(attachable_ptr& ptr) {
    ptr->next.swap(attachables_head_);
    attachables_head_.swap(ptr);
  }

  static size_t detach_impl(const attachable::token& what,
                            attachable_ptr& ptr,
                            bool stop_on_first_hit = false,
                            bool dry_run = false);

  // initially set to exit_reason::not_exited
  std::atomic<exit_reason> exit_reason_;

  // guards access to exit_reason_, attachables_, links_,
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
