// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/private_thread_pool.hpp"

#include "caf/actor_system.hpp"
#include "caf/config.hpp"
#include "caf/detail/private_thread.hpp"
#include "caf/thread_owner.hpp"

namespace caf::detail {

private_thread_pool::node::~node() {
  // nop
}

void private_thread_pool::start() {
  loop_ = sys_->launch_thread("caf.pool", thread_owner::pool,
                              [this] { run_loop(); });
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
