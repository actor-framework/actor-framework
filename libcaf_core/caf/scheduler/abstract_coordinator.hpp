// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_clock.hpp"
#include "caf/actor_system.hpp"
#include "caf/detail/actor_local_printer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/message.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>

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
  actor printer() const {
    return actor_cast<actor>(utility_actors_[printer_id]);
  }

  virtual detail::actor_local_printer_ptr printer_for(local_actor* self);

  /// Returns the number of utility actors.
  size_t num_utility_actors() const {
    return utility_actors_.size();
  }

  /// Puts `what` into the queue of a randomly chosen worker.
  virtual void enqueue(resumable* what) = 0;

  actor_system& system() {
    return system_;
  }

  const actor_system_config& config() const;

  size_t max_throughput() const {
    return max_throughput_;
  }

  size_t num_workers() const {
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

  static size_t default_thread_count() noexcept;

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
