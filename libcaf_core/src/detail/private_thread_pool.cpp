/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2021 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/private_thread_pool.hpp"

#include "caf/actor_system.hpp"
#include "caf/config.hpp"
#include "caf/detail/private_thread.hpp"

namespace caf::detail {

private_thread_pool::node::~node() {
  // nop
}

void private_thread_pool::start() {
  loop_ = sys_->launch_thread("caf.pool", [this] { run_loop(); });
}

void private_thread_pool::stop() {
  struct shutdown_helper : node {
    bool stop() override {
      return false;
    }
  };
  auto ptr = new shutdown_helper;
  {
    std::unique_lock guard{mtx_};
    ++running_;
    ptr->next = head_;
    head_ = ptr;
    cv_.notify_all();
  }
  loop_.join();
}

void private_thread_pool::run_loop() {
  bool shutting_down = false;
  for (;;) {
    auto [ptr, remaining] = dequeue();
    CAF_ASSERT(ptr != nullptr);
    if (!ptr->stop())
      shutting_down = true;
    delete ptr;
    if (remaining == 0 && shutting_down)
      return;
  }
}

private_thread* private_thread_pool::acquire() {
  {
    std::unique_lock guard{mtx_};
    ++running_;
  }
#ifdef CAF_ENABLE_EXCEPTIONS
  try {
    return private_thread::launch(sys_);
  } catch (...) {
    {
      std::unique_lock guard{mtx_};
      --running_;
    }
    throw;
  }
#else
  return private_thread::launch(sys_);
#endif
}

void private_thread_pool::release(private_thread* ptr) {
  std::unique_lock guard{mtx_};
  ptr->next = head_;
  head_ = ptr;
  cv_.notify_all();
}

size_t private_thread_pool::running() const noexcept {
  std::unique_lock guard{mtx_};
  return running_;
}

std::pair<private_thread_pool::node*, size_t> private_thread_pool::dequeue() {
  std::unique_lock guard{mtx_};
  while (head_ == nullptr)
    cv_.wait(guard);
  auto ptr = head_;
  head_ = ptr->next;
  auto remaining = --running_;
  return {ptr, remaining};
}

} // namespace caf::detail
