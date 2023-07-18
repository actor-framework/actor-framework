// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace caf::detail {

/// A central place where workers return to after finishing a task. A hub
/// supports any number of workers that call `push`, but only a single master
/// that calls `pop`. The hub takes ownership of all workers. Workers register
/// at the hub during construction and get destroyed when the hub gets
/// destroyed.
class CAF_CORE_EXPORT abstract_worker_hub {
public:
  // -- constructors, destructors, and assignment operators --------------------

  abstract_worker_hub();

  virtual ~abstract_worker_hub();

  // -- synchronization --------------------------------------------------------

  /// Waits until all workers are back at the hub.
  void await_workers();

protected:
  // -- worker management ------------------------------------------------------

  /// Adds a new worker to the hub.
  void push_new(abstract_worker* ptr);

  /// Returns a worker to the hub.
  void push_returning(abstract_worker* ptr);

  /// Tries to retrieve a worker from the hub.
  /// @returns the next available worker (in LIFO order) or `nullptr` if the
  ///          hub is currently empty.
  abstract_worker* pop_impl();

  /// Checks which worker would `pop` currently return.
  /// @returns the next available worker (in LIFO order) or `nullptr` if the
  ///          hub is currently empty.
  abstract_worker* peek_impl();

  // -- member variables -------------------------------------------------------

  std::atomic<abstract_worker*> head_;

  std::atomic<size_t> running_;

  std::mutex mtx_;

  std::condition_variable cv_;
};

} // namespace caf::detail
