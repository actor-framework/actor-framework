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

#define CAF_SUITE fifo_inbox

#include <memory>

#include "caf/test/unit_test.hpp"

#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/intrusive/singly_linked.hpp"

using namespace caf;
using namespace caf::intrusive;

namespace {

struct inode : singly_linked<inode> {
  int value;
  inode(int x = 0) : value(x) {
    // nop
  }
};

std::string to_string(const inode& x) {
  return std::to_string(x.value);
}

struct inode_policy {
  using mapped_type = inode;

  using task_size_type = int;

  using deficit_type = int;

  using deleter_type = std::default_delete<mapped_type>;

  using unique_pointer = std::unique_ptr<mapped_type, deleter_type>;

  using queue_type = drr_queue<inode_policy>;

  static constexpr task_size_type task_size(const inode&) noexcept {
    return 1;
  }
};

using inbox_type = fifo_inbox<inode_policy>;

struct fixture {
  inode_policy policy;
  inbox_type inbox{policy};

  void fill(inbox_type&) {
    // nop
  }

  template <class T, class... Ts>
  void fill(inbox_type& i, T x, Ts... xs) {
    i.emplace_back(x);
    fill(i, xs...);
  }

  std::string fetch() {
    std::string result;
    auto f = [&](inode& x) {
      result += to_string(x);
      return task_result::resume;
    };
    inbox.new_round(1000, f);
    return result;
  }

  std::string close_and_fetch() {
    std::string result;
    auto f = [&](inode& x) {
      result += to_string(x);
      return task_result::resume;
    };
    inbox.close();
    inbox.queue().new_round(1000, f);
    return result;
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(fifo_inbox_tests, fixture)

CAF_TEST(default_constructed) {
  CAF_REQUIRE_EQUAL(inbox.empty(), true);
}

CAF_TEST(push_front) {
  fill(inbox, 1, 2, 3);
  CAF_REQUIRE_EQUAL(close_and_fetch(), "123");
  CAF_REQUIRE_EQUAL(inbox.closed(), true);
}

CAF_TEST(push_after_close) {
  inbox.close();
  auto res = inbox.push_back(new inode(0));
  CAF_REQUIRE_EQUAL(res, inbox_result::queue_closed);
}

CAF_TEST(unblock) {
  CAF_REQUIRE_EQUAL(inbox.try_block(), true);
  auto res = inbox.push_back(new inode(0));
  CAF_REQUIRE_EQUAL(res, inbox_result::unblocked_reader);
  res = inbox.push_back(new inode(1));
  CAF_REQUIRE_EQUAL(res, inbox_result::success);
  CAF_REQUIRE_EQUAL(close_and_fetch(), "01");
}

CAF_TEST(await) {
  std::mutex mx;
  std::condition_variable cv;
  std::thread t{[&] {
    inbox.synchronized_emplace_back(mx, cv, 1);
  }};
  inbox.synchronized_await(mx, cv);
  CAF_REQUIRE_EQUAL(close_and_fetch(), "1");
  t.join();
}

CAF_TEST(timed_await) {
  std::mutex mx;
  std::condition_variable cv;
  auto tout = std::chrono::system_clock::now();
  tout += std::chrono::microseconds(1);
  auto res = inbox.synchronized_await(mx, cv, tout);
  CAF_REQUIRE_EQUAL(res, false);
  fill(inbox, 1);
  res = inbox.synchronized_await(mx, cv, tout);
  CAF_REQUIRE_EQUAL(res, true);
  CAF_CHECK_EQUAL(fetch(), "1");
  tout += std::chrono::hours(1000);
  std::thread t{[&] {
    inbox.synchronized_emplace_back(mx, cv, 2);
  }};
  res = inbox.synchronized_await(mx, cv, tout);
  CAF_REQUIRE_EQUAL(res, true);
  CAF_REQUIRE_EQUAL(close_and_fetch(), "2");
  t.join();
}
CAF_TEST_FIXTURE_SCOPE_END()
