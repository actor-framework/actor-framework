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

#include "caf/detail/abstract_worker_hub.hpp"

namespace caf {
namespace detail {

template <class Worker>
class worker_hub : public abstract_worker_hub {
public:
  // -- member types -----------------------------------------------------------

  using super = abstract_worker_hub;

  using worker_type = Worker;

  // -- worker management ------------------------------------------------------

  /// Creates a new worker and adds it to the hub.
  template <class... Ts>
  void add_new_worker(Ts&&... xs) {
    super::push_new(new worker_type(*this, std::forward<Ts>(xs)...));
  }

  /// Returns a worker to the hub.
  void push(worker_type* ptr) {
    super::push_returning(ptr);
  }

  /// Gets a worker from the hub.
  /// @returns the next available worker (in LIFO order) or `nullptr` if the
  ///          hub is currently empty.
  worker_type* pop() {
    return static_cast<worker_type*>(super::pop_impl());
  }

  /// Checks which worker would `pop` currently return.
  /// @returns the next available worker (in LIFO order) or `nullptr` if the
  ///          hub is currently empty.
  worker_type* peek() {
    return static_cast<worker_type*>(super::peek_impl());
  }
};

} // namespace detail
} // namespace caf
