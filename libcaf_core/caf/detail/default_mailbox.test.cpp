// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/default_mailbox.hpp"

#include "caf/test/test.hpp"

#include <algorithm>
#include <functional>

using namespace caf;

namespace {

using ires = intrusive::inbox_result;

template <message_priority P = message_priority::normal>
auto make_int_msg(int value) {
  return make_mailbox_element(nullptr, make_message_id(P), make_message(value));
}

} // namespace

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

TEST("close discards all messages but counts only timestamped messages") {
  detail::default_mailbox uut;
  std::vector<message> payloads;
  auto enqueue_batch = [this, &uut, &payloads] {
    for (auto priority : {message_priority::normal, message_priority::high}) {
      for (auto timestamped : {false, true}) {
        payloads.emplace_back(make_message(static_cast<int>(payloads.size())));
        auto ptr = make_mailbox_element(nullptr, make_message_id(priority),
                                        payloads.back());
        if (timestamped) {
          ptr->set_enqueue_time();
        }
        check_eq(uut.push_back(std::move(ptr)), ires::success);
      }
    }
  };
  auto is_unique = [](const message& msg) { return msg.unique(); };
  // Note: size() fetches the first batch into the urgent and normal cached
  //       queues. The second batch remains in the inbox so that `close` needs
  //       to traverse the caches and the inbox.
  enqueue_batch();
  check_eq(uut.size(), 4u);
  enqueue_batch();
  // The mailbox holds references to the payloads until it is closed.
  check(std::all_of(payloads.begin(), payloads.end(), std::not_fn(is_unique)));
  check_eq(uut.close(error{}), 4u);
  check(uut.closed());
  // The mailbox may no longer hold references to the payloads.
  check(std::all_of(payloads.begin(), payloads.end(), is_unique));
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
