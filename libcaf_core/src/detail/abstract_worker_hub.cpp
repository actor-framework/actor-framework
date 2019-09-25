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

#include "caf/detail/abstract_worker_hub.hpp"

#include "caf/detail/abstract_worker.hpp"

namespace caf {
namespace detail {

// -- constructors, destructors, and assignment operators ----------------------

abstract_worker_hub::abstract_worker_hub() : head_(nullptr), running_(0) {
  // nop
}

abstract_worker_hub::~abstract_worker_hub() {
  await_workers();
  auto head = head_.load();
  while (head != nullptr) {
    auto next = head->next_.load();
    head->intrusive_ptr_release_impl();
    head = next;
  }
}

// -- synchronization ----------------------------------------------------------

void abstract_worker_hub::await_workers() {
  std::unique_lock<std::mutex> guard{mtx_};
  while (running_ != 0)
    cv_.wait(guard);
}

// -- worker management --------------------------------------------------------

void abstract_worker_hub::push_new(abstract_worker* ptr) {
  auto next = head_.load();
  for (;;) {
    ptr->next_ = next;
    if (head_.compare_exchange_strong(next, ptr))
      return;
  }
}

void abstract_worker_hub::push_returning(abstract_worker* ptr) {
  auto next = head_.load();
  for (;;) {
    ptr->next_ = next;
    if (head_.compare_exchange_strong(next, ptr)) {
      if (--running_ == 0) {
        std::unique_lock<std::mutex> guard{mtx_};
        cv_.notify_all();
      }
      return;
    }
  }
}

abstract_worker* abstract_worker_hub::pop_impl() {
  auto result = head_.load();
  if (result == nullptr)
    return nullptr;
  for (;;) {
    auto next = result->next_.load();
    if (head_.compare_exchange_strong(result, next)) {
      if (result != nullptr)
        ++running_;
      return result;
    }
  }
}

abstract_worker* abstract_worker_hub::peek_impl() {
  return head_.load();
}

} // namespace detail
} // namespace caf
