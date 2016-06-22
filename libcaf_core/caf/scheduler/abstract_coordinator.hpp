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

#ifndef CAF_SCHEDULER_ABSTRACT_COORDINATOR_HPP
#define CAF_SCHEDULER_ABSTRACT_COORDINATOR_HPP

#include <chrono>
#include <atomic>
#include <cstddef>

#include "caf/fwd.hpp"
#include "caf/atom.hpp"
#include "caf/actor.hpp"
#include "caf/message.hpp"
#include "caf/duration.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_system.hpp"

namespace caf {
namespace scheduler {

/// A coordinator creates the workers, manages delayed sends and
/// the central printer instance for {@link aout}. It also forwards
/// sends from detached workers or non-actor threads to randomly
/// chosen workers.
class abstract_coordinator : public actor_system::module {
public:
  explicit abstract_coordinator(actor_system& sys);

  /// Returns a handle to the central printing actor.
  actor printer() const;

  /// Puts `what` into the queue of a randomly chosen worker.
  virtual void enqueue(resumable* what) = 0;

  template <class Duration, class... Data>
  void delayed_send(Duration rel_time, strong_actor_ptr from,
                    strong_actor_ptr to, message_id mid, message data) {
    timer_->enqueue(nullptr, invalid_message_id,
                    make_message(duration{rel_time}, std::move(from),
                                 std::move(to), mid, std::move(data)),
                    nullptr);
  }

  inline actor_system& system() {
    return system_;
  }

  inline size_t max_throughput() const {
    return max_throughput_;
  }

  inline size_t num_workers() const {
    return num_workers_;
  }

  void start() override;

  void init(actor_system_config& cfg) override;

  id_t id() const override;

  void* subtype_ptr() override;

  static void cleanup_and_release(resumable*);

protected:
  void stop_actors();

  // ID of the worker receiving the next enqueue
  std::atomic<size_t> next_worker_;

  // number of messages each actor is allowed to consume per resume
  size_t max_throughput_;

  // configured number of workers
  size_t num_workers_;

  strong_actor_ptr timer_;
  strong_actor_ptr printer_;

  actor_system& system_;
};

} // namespace scheduler
} // namespace caf

#endif // CAF_SCHEDULER_ABSTRACT_COORDINATOR_HPP
