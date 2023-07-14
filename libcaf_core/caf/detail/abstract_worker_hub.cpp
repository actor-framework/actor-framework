// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/abstract_worker_hub.hpp"

#include "caf/detail/abstract_worker.hpp"

namespace caf::detail {

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

} // namespace caf::detail
