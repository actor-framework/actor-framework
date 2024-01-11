// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/default_mailbox.hpp"

#include "caf/test/test.hpp"

using namespace caf;

namespace {

using ires = intrusive::inbox_result;

template <message_priority P = message_priority::normal>
auto make_int_msg(int value) {
  return make_mailbox_element(nullptr, make_message_id(P), make_message(value));
}

TEST("a default-constructed mailbox is empty") {
  detail::default_mailbox uut;
  check(!uut.closed());
  check(!uut.blocked());
  check_eq(uut.size(), 0u);
  check_eq(uut.pop_front(), nullptr);
  check_eq(uut.size(), 0u);
  check(uut.try_block());
  check(!uut.try_block());
}

TEST("the first item unblocks the mailbox") {
  detail::default_mailbox uut;
  check(uut.try_block());
  check_eq(uut.push_back(make_int_msg(1)), ires::unblocked_reader);
  check_eq(uut.push_back(make_int_msg(2)), ires::success);
}

TEST("a closed mailbox no longer accepts new messages") {
  detail::default_mailbox uut;
  uut.close(error{});
  check_eq(uut.push_back(make_int_msg(1)), ires::queue_closed);
}

TEST("urgent messages are processed first") {
  detail::default_mailbox uut;
  check_eq(uut.push_back(make_int_msg(1)), ires::success);
  check_eq(uut.push_back(make_int_msg(2)), ires::success);
  check_eq(uut.push_back(make_int_msg<message_priority::high>(3)),
           ires::success);
  std::vector<message> results;
  for (auto ptr = uut.pop_front(); ptr != nullptr; ptr = uut.pop_front()) {
    results.emplace_back(ptr->content());
  }
  if (check_eq(results.size(), 3u)) {
    check_eq(results[0].get_as<int>(0), 3);
    check_eq(results[1].get_as<int>(0), 1);
    check_eq(results[2].get_as<int>(0), 2);
  }
}

TEST("calling push_front inserts messages at the beginning") {
  detail::default_mailbox uut;
  check_eq(uut.push_back(make_int_msg(1)), ires::success);
  check_eq(uut.push_back(make_int_msg(2)), ires::success);
  uut.push_front(make_int_msg(3));
  uut.push_front(make_int_msg(4));
  std::vector<message> results;
  for (auto ptr = uut.pop_front(); ptr != nullptr; ptr = uut.pop_front()) {
    results.emplace_back(ptr->content());
  }
  if (check_eq(results.size(), 4u)) {
    check_eq(results[0].get_as<int>(0), 4);
    check_eq(results[1].get_as<int>(0), 3);
    check_eq(results[2].get_as<int>(0), 1);
    check_eq(results[3].get_as<int>(0), 2);
  }
}

} // namespace
