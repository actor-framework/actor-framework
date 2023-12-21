// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/ring_buffer.hpp"

#include "caf/test/test.hpp"

#include "caf/log/test.hpp"

using namespace caf;

using detail::ring_buffer;

namespace {

int pop(ring_buffer<int>& buf) {
  auto result = buf.front();
  buf.pop_front();
  return result;
}

TEST("push_back adds element") {
  ring_buffer<int> buf{3};
  log::test::debug("full capacity of ring buffer");
  for (int i = 1; i <= 3; ++i) {
    buf.push_back(i);
  }
  check_eq(buf.full(), true);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 1);
  log::test::debug("additional element after full capacity");
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

TEST("pop_front removes the oldest element") {
  ring_buffer<int> buf{3};
  log::test::debug("full capacity of ring buffer");
  for (int i = 1; i <= 3; ++i) {
    buf.push_back(i);
  }
  check_eq(buf.full(), true);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 1);
  log::test::debug("remove element from buffer");
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

TEST("circular buffer overwrites oldest element after it is full") {
  ring_buffer<int> buf{5};
  log::test::debug("full capacity of ring buffer");
  for (int i = 1; i <= 5; ++i) {
    buf.push_back(i);
  }
  check_eq(buf.full(), true);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 1);
  log::test::debug("add some elements into buffer");
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

TEST("pop_front removes the oldest element from the buffer") {
  ring_buffer<int> buf{5};
  log::test::debug("full capacity of ring buffer");
  for (int i = 1; i <= 5; ++i) {
    buf.push_back(i);
  }
  check_eq(buf.full(), true);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 1);
  log::test::debug("remove some element from buffer");
  buf.pop_front();
  check_eq(buf.full(), false);
  check_eq(buf.empty(), false);
  check_eq(buf.front(), 2);
  buf.pop_front();
  check_eq(buf.full(), false);
  check_eq(buf.front(), 3);
  log::test::debug("add some elements into buffer");
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

TEST("push_back does nothing for ring buffer with a capacity of 0") {
  ring_buffer<int> buf{0};
  log::test::debug("empty buffer is initialized");
  check_eq(buf.size(), 0u);
  for (int i = 1; i <= 3; ++i) {
    buf.push_back(i);
  }
  log::test::debug("buffer size after adding some elements");
  check_eq(buf.size(), 0u);
}

TEST("size() returns the number of elements in a buffer") {
  ring_buffer<int> buf{5};
  log::test::debug("empty buffer is initialized");
  check_eq(buf.size(), 0u);
  for (int i = 1; i <= 3; ++i) {
    buf.push_back(i);
  }
  log::test::debug("buffer size after adding some elements");
  check_eq(buf.size(), 3u);
}

TEST("ring-buffers are copiable") {
  ring_buffer<int> buf{5};
  log::test::debug("empty buffer is initialized");
  check_eq(buf.size(), 0u);
  for (int i = 1; i <= 3; ++i) {
    buf.push_back(i);
  }
  check_eq(buf.size(), 3u);
  SECTION("copy-assignment") {
    ring_buffer<int> new_buf{0};
    new_buf = buf;
    log::test::debug(
      "check size and elements of new_buf after copy-assignment");
    check_eq(new_buf.size(), 3u);
    check_eq(pop(new_buf), 1);
    check_eq(pop(new_buf), 2);
    check_eq(pop(new_buf), 3);
    check_eq(new_buf.empty(), true);
  }
  SECTION("copy constructor") {
    ring_buffer<int> new_buf{buf};
    log::test::debug(
      "check size and elements of new_buf after copy constructor");
    check_eq(new_buf.size(), 3u);
    check_eq(pop(new_buf), 1);
    check_eq(pop(new_buf), 2);
    check_eq(pop(new_buf), 3);
    check_eq(new_buf.empty(), true);
  }
  log::test::debug("check size and elements of buf after copy");
  check_eq(buf.size(), 3u);
  check_eq(pop(buf), 1);
  check_eq(pop(buf), 2);
  check_eq(pop(buf), 3);
  check_eq(buf.empty(), true);
}

} // namespace
