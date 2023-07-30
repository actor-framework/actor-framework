// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/ring_buffer.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

using namespace caf;

using detail::ring_buffer;

TEST("push_back adds element") {
  ring_buffer<int> buf(3);
  info("full capacity of ring buffer");
  for (int i = 1; i <= 3; ++i) {
    buf.push_back(i);
  }
  check_eq(buf.full(), true);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 1);
  info("additional element after full capacity");
  buf.push_back(4);
  check_eq(buf.full(), true);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 2);
  buf.push_back(5);
  check_eq(buf.full(), true);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 3);
  buf.push_back(6);
  check_eq(buf.full(), true);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 4);
}

TEST("pop_front removes element") {
  ring_buffer<int> buf(3);
  info("full capacity of ring buffer");
  for (int i = 1; i <= 3; ++i) {
    buf.push_back(i);
  }
  check_eq(buf.full(), true);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 1);
  info("remove element from buffer");
  buf.pop_front();
  check_eq(buf.full(), false);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 2);
  buf.pop_front();
  check_eq(buf.full(), false);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 3);
  buf.pop_front();
  check_eq(buf.empty(), true);
}

TEST("circular buffer functions properly") {
  ring_buffer<int> buf(5);
  info("full capacity of ring buffer");
  for (int i = 1; i <= 5; ++i) {
    buf.push_back(i);
  }
  check_eq(buf.full(), true);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 1);
  info("remove some element from buffer");
  buf.pop_front();
  check_eq(buf.full(), false);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 2);
  buf.pop_front();
  check_eq(buf.full(), false);
  check_eq(buf.front(), 3);
  info("add some elements into buffer");
  buf.push_back(6);
  buf.push_back(7);
  check_eq(buf.full(), true);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 3);
  buf.push_back(8);
  check_eq(buf.full(), true);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 4);
  buf.push_back(9);
  check_eq(buf.full(), true);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 5);
}

TEST("buffer size is calculated correctly") {
  ring_buffer<int> buf(5);
  info("empty buffer is initialized");
  check_eq(buf.size(), 0u);
  for (int i = 1; i <= 3; ++i) {
    buf.push_back(i);
  }
  info("buffer size after adding some elements");
  check_eq(buf.size(), 3u);
}

CAF_TEST_MAIN()
