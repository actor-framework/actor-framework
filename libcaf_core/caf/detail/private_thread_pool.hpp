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
