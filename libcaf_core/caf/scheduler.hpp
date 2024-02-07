// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_clock.hpp"
#include "caf/actor_system.hpp"
#include "caf/detail/actor_local_printer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <cstddef>

namespace caf {

/// A coordinator creates the workers, manages delayed sends and
/// the central printer instance for {@link aout}. It also forwards
/// sends from detached workers or non-actor threads to randomly
/// chosen workers.
class CAF_CORE_EXPORT scheduler {
public:
  // -- factory functions ------------------------------------------------------
  static std::unique_ptr<scheduler> make_work_stealing(actor_system& sys);

  static std::unique_ptr<scheduler> make_work_sharing(actor_system& sys);

  // -- scheduler interface ----------------------------------------------------

  virtual ~scheduler();

  /// Puts `what` into the queue of a randomly chosen worker.
  virtual void enqueue(resumable* what) = 0;

  virtual actor_clock& clock() noexcept = 0;

  virtual void start();

  virtual void stop() = 0;

  // -- utility functions ------------------------------------------------------

  void init(actor_system_config& cfg);

  static void cleanup_and_release(resumable*);

  virtual detail::actor_local_printer_ptr printer_for(local_actor* self);

  // -- properties -------------------------------------------------------------

  static size_t default_thread_count() noexcept;

  actor_system& system() {
    return system_;
  }

  const actor_system_config& config() const {
    return system_.config();
  }

  size_t max_throughput() const {
    return max_throughput_;
  }

  size_t num_workers() const {
    return num_workers_;
  }

protected:
  explicit scheduler(actor_system& sys);

  void start_printer(caf::actor hdl) {
    system_.printer(std::move(hdl));
  }

  void stop_actors();

  /// Number of messages each actor is allowed to consume per resume.
  size_t max_throughput_;

  /// Configured number of workers.
  size_t num_workers_;

  /// Reference to the host system.
  actor_system& system_;
};

} // namespace caf
