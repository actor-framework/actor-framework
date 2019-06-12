/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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
#include <condition_variable>
#include <mutex>

#include "caf/fwd.hpp"
#include "caf/io/basp/fwd.hpp"

namespace caf {
namespace io {
namespace basp {

/// A central place where BASP workers return to after finishing a task. A hub
/// supports any number of workers that call `push`, but only a single master
/// that calls `pop`. The hub takes ownership of all workers. Workers register
/// at the hub during construction and get destroyed when the hub gets
/// destroyed.
class worker_hub {
public:
  // -- member types -----------------------------------------------------------

  using pointer = worker*;

  // -- constructors, destructors, and assignment operators --------------------

  worker_hub();

  ~worker_hub();

  // -- properties -------------------------------------------------------------

  /// Creates a new worker and adds it to the hub.
  void add_new_worker(message_queue&, proxy_registry&);

  /// Add a worker to the hub.
  void push(pointer ptr);

  /// Get a worker from the hub.
  /// @returns the next available worker (in LIFO order) or `nullptr` if the
  ///          hub is currently empty.
  pointer pop();

  /// Check which worker would `pop` currently return.
  /// @returns the next available worker (in LIFO order) or `nullptr` if the
  ///          hub is currently empty.
  pointer peek();

  /// Waits until all workers are back at the hub.
  void await_workers();

private:
  // -- member variables -------------------------------------------------------

  std::atomic<pointer> head_;

  std::atomic<size_t> running_;

  std::mutex mtx_;

  std::condition_variable cv_;
};

} // namespace basp
} // namespace io
} // namespace caf
