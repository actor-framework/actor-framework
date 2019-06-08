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

#include "caf/io/basp/worker_hub.hpp"

#include "caf/io/basp/worker.hpp"

namespace caf {
namespace io {
namespace basp {

// -- constructors, destructors, and assignment operators ----------------------

worker_hub::worker_hub() : head_(nullptr), running_(0) {
  // nop
}

worker_hub::~worker_hub() {
  await_workers();
  auto head = head_.load();
  while (head != nullptr) {
    auto next = head->next_.load();
    delete head;
    head = next;
  }
}

// -- properties ---------------------------------------------------------------

void worker_hub::add_new_worker(message_queue& queue, proxy_registry& proxies) {
  auto ptr = new worker(*this, queue, proxies);
  auto next = head_.load();
  for (;;) {
    ptr->next_ = next;
    if (head_.compare_exchange_strong(next, ptr))
      return;
  }
}

void worker_hub::push(pointer ptr) {
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

worker_hub::pointer worker_hub::pop() {
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

worker_hub::pointer worker_hub::peek() {
  return head_.load();
}

void worker_hub::await_workers() {
  std::unique_lock<std::mutex> guard{mtx_};
  while (running_ != 0)
    cv_.wait(guard);
}

} // namespace basp
} // namespace io
} // namespace caf
