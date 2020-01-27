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

#include <atomic>
#include <chrono>
#include <cstddef>

#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_clock.hpp"
#include "caf/actor_system.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/message.hpp"

namespace caf::scheduler {

/// A coordinator creates the workers, manages delayed sends and
/// the central printer instance for {@link aout}. It also forwards
/// sends from detached workers or non-actor threads to randomly
/// chosen workers.
class CAF_CORE_EXPORT abstract_coordinator : public actor_system::module {
public:
  enum utility_actor_id : size_t { printer_id, max_id };

  explicit abstract_coordinator(actor_system& sys);

  /// Returns a handle to the central printing actor.
  inline actor printer() const {
    return actor_cast<actor>(utility_actors_[printer_id]);
  }

  /// Returns the number of utility actors.
  inline size_t num_utility_actors() const {
    return utility_actors_.size();
  }

  /// Puts `what` into the queue of a randomly chosen worker.
  virtual void enqueue(resumable* what) = 0;

  inline actor_system& system() {
    return system_;
  }

  const actor_system_config& config() const;

  inline size_t max_throughput() const {
    return max_throughput_;
  }

  inline size_t num_workers() const {
    return num_workers_;
  }

  /// Returns `true` if this scheduler detaches its utility actors.
  virtual bool detaches_utility_actors() const;

  void start() override;

  void init(actor_system_config& cfg) override;

  id_t id() const override;

  void* subtype_ptr() override;

  static void cleanup_and_release(resumable*);

  virtual actor_clock& clock() noexcept = 0;

protected:
  void stop_actors();

  /// ID of the worker receiving the next enqueue (round-robin dispatch).
  std::atomic<size_t> next_worker_;

  /// Number of messages each actor is allowed to consume per resume.
  size_t max_throughput_;

  /// Configured number of workers.
  size_t num_workers_;

  /// Background workers, e.g., printer.
  std::array<actor, max_id> utility_actors_;

  /// Reference to the host system.
  actor_system& system_;
};

} // namespace caf::scheduler
