// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <atomic>
#include <condition_variable>
#include <forward_list>
#include <mutex>
#include <thread>

#include "caf/fwd.hpp"

namespace caf::detail {

class private_thread_pool {
public:
  struct node {
    virtual ~node();
    node* next = nullptr;
    // Called by the private thread pool to stop the node. Regular nodes should
    // return true. Returning false signals the thread pool to shut down.
    virtual bool stop() = 0;
  };

  explicit private_thread_pool(actor_system* sys) : sys_(sys), running_(0) {
    // nop
  }

  void start();

  void stop();

  void run_loop();

  private_thread* acquire();

  void release(private_thread*);

  size_t running() const noexcept;

private:
  std::pair<node*, size_t> dequeue();

  actor_system* sys_;
  std::thread loop_;
  mutable std::mutex mtx_;
  std::condition_variable cv_;
  node* head_ = nullptr;
  size_t running_;
};

} // namespace caf::detail
