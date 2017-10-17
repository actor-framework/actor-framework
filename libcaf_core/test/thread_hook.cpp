/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

// this suite tests whether actors terminate as expect in several use cases
#define CAF_SUITE thread_hook

#include "caf/all.hpp"
#include "caf/test/unit_test.hpp"

using namespace caf;

namespace {

class test_thread_hooks : public thread_hook {
  using atomic_cnt = std::atomic<size_t>;
public:
  test_thread_hooks(size_t assumed_init, size_t assumed_thread_started,
                    size_t assumed_thread_termintes_cb)
    : count_init_{0},
      count_thread_started_{0},
      count_thread_terminates_{0},
      assumed_init_{assumed_init},
      assumed_thread_started_{assumed_thread_started},
      assumed_thread_termintes_cb_{assumed_thread_termintes_cb} {
    // nop
  }
  virtual ~test_thread_hooks() {
    CAF_CHECK_EQUAL(count_init_, assumed_init_);
    CAF_CHECK_EQUAL(count_thread_started_, assumed_thread_started_);
    CAF_CHECK_EQUAL(count_thread_terminates_, assumed_thread_termintes_cb_);
  }
  virtual void init(actor_system&) {
    count_init_.fetch_add(1, std::memory_order_relaxed);
  }
  virtual void thread_started() {
    count_thread_started_.fetch_add(1, std::memory_order_relaxed);
  }
  virtual void thread_terminates() {
    count_thread_terminates_.fetch_add(1, std::memory_order_relaxed);
  }
private:
  atomic_cnt count_init_;
  atomic_cnt count_thread_started_;
  atomic_cnt count_thread_terminates_;
  atomic_cnt assumed_init_;
  atomic_cnt assumed_thread_started_;
  atomic_cnt assumed_thread_termintes_cb_;
};

} // namespace <anonymous>

CAF_TEST(test_no_args) {
  actor_system_config cfg{};
  struct a : public thread_hook {
    virtual void init(actor_system&) {}
    virtual void thread_started() {}
    virtual void thread_terminates() {}
  };
  cfg.add_thread_hook<a>();
}

CAF_TEST(test_no_system) {
  actor_system_config cfg{};
  cfg.add_thread_hook<test_thread_hooks>(0,0,0);
}

CAF_TEST(test_system_no_actor) {
  actor_system_config cfg{};
  size_t assumed_threads = 2 + cfg.scheduler_max_threads;
#ifdef CAF_LOG_LEVEL
  assumed_threads += 1; // If logging is enabled, we have a additional thread.
#endif
  cfg.add_thread_hook<test_thread_hooks>(1, assumed_threads, assumed_threads);
  actor_system system{cfg};
}
