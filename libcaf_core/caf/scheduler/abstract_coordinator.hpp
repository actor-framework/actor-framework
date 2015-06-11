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
#include "caf/channel.hpp"
#include "caf/message.hpp"
#include "caf/duration.hpp"
#include "caf/actor_addr.hpp"

namespace caf {
namespace scheduler {

/// A coordinator creates the workers, manages delayed sends and
/// the central printer instance for {@link aout}. It also forwards
/// sends from detached workers or non-actor threads to randomly
/// chosen workers.
class abstract_coordinator {
public:
  friend class detail::singletons;

  explicit abstract_coordinator(size_t num_worker_threads);

  virtual ~abstract_coordinator();

  /// Returns a handle to the central printing actor.
  inline actor printer() const {
    return printer_;
  }

  /// Puts `what` into the queue of a randomly chosen worker.
  virtual void enqueue(resumable* what) = 0;

  template <class Duration, class... Data>
  void delayed_send(Duration rel_time, actor_addr from, channel to,
                    message_id mid, message data) {
    timer_->enqueue(invalid_actor_addr, invalid_message_id,
                     make_message(duration{rel_time}, std::move(from),
                                  std::move(to), mid, std::move(data)),
                     nullptr);
  }

  inline size_t num_workers() const {
    return num_workers_;
  }

protected:
  abstract_coordinator();

  virtual void initialize();

  virtual void stop() = 0;

  void stop_actors();

  // Creates a default instance.
  static abstract_coordinator* create_singleton();

  inline void dispose() {
    delete this;
  }

  actor timer_;
  actor printer_;

  // ID of the worker receiving the next enqueue
  std::atomic<size_t> next_worker_;

  size_t num_workers_;
};

} // namespace scheduler
} // namespace caf

#endif // CAF_SCHEDULER_ABSTRACT_COORDINATOR_HPP
