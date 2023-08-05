// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/intrusive/lifo_inbox.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

#include <memory>
#include <numeric>

using namespace caf;

using intrusive::inbox_result;

struct inode : intrusive::singly_linked<inode> {
  explicit inode(int x = 0) : value(x) {
    // nop
  }

  int value;
};

using inbox_type = intrusive::lifo_inbox<inode>;

std::string drain(inbox_type& xs) {
  std::vector<int> tmp;
  std::unique_ptr<inode> ptr{xs.take_head()};
  while (ptr != nullptr) {
    auto next = ptr->next;
    tmp.push_back(ptr->value);
    ptr.reset(inbox_type::promote(next));
  }
  return caf::deep_to_string(tmp);
}

TEST("a default default-constructed inbox is empty") {
  inbox_type uut;
  check(!uut.closed());
  check(!uut.blocked());
  check(uut.empty());
  check_eq(uut.take_head(), nullptr);
}

TEST("push_front adds elements to the front of the inbox") {
  inbox_type uut;
  check_eq(uut.emplace_front(1), inbox_result::success);
  check_eq(uut.emplace_front(2), inbox_result::success);
  check_eq(uut.emplace_front(3), inbox_result::success);
  check_eq(drain(uut), "[3, 2, 1]");
}

TEST("push_front discards elements if the inbox is closed") {
  inbox_type uut;
  uut.close();
  check(uut.closed());
  auto res = uut.emplace_front(0);
  check_eq(res, inbox_result::queue_closed);
}

TEST("push_front unblocks a blocked reader") {
  inbox_type uut;
  check(uut.try_block());
  check_eq(uut.emplace_front(1), inbox_result::unblocked_reader);
  check_eq(uut.emplace_front(2), inbox_result::success);
  check_eq(drain(uut), "[2, 1]");
}

CAF_TEST_MAIN()
