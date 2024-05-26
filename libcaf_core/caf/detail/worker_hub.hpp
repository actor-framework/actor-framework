// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/abstract_worker_hub.hpp"

namespace caf::detail {

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

} // namespace caf::detail
