// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/mailbox_element.hpp"

#include "caf/test/test.hpp"

#include "caf/all.hpp"

#include <string>
#include <tuple>
#include <vector>

using std::make_tuple;
using std::string;
using std::tuple;
using std::vector;

using namespace caf;

namespace {

template <class... Ts>
std::optional<tuple<Ts...>> fetch(const message& x) {
  if (auto view = make_const_typed_message_view<Ts...>(x))
    return to_tuple(view);
  return std::nullopt;
}

template <class... Ts>
std::optional<tuple<Ts...>> fetch(const mailbox_element& x) {
  return fetch<Ts...>(x.content());
}

} // namespace

TEST("empty_message") {
  auto m1 = make_mailbox_element(nullptr, make_message_id(), make_message());
  check(m1->mid.is_async());
  check(m1->mid.category() == message_id::normal_message_category);
  check(m1->content().empty());
}

TEST("non_empty_message") {
  auto m1 = make_mailbox_element(nullptr, make_message_id(),
                                 make_message(1, 2, 3));
  check(m1->mid.is_async());
  check(m1->mid.category() == message_id::normal_message_category);
  check(!m1->content().empty());
  check_eq((fetch<int, int>(*m1)), std::nullopt);
  check_eq((fetch<int, int, int>(*m1)), make_tuple(1, 2, 3));
}

TEST("tuple") {
  auto m1 = make_mailbox_element(nullptr, make_message_id(), 1, 2, 3);
  check(m1->mid.is_async());
  check(m1->mid.category() == message_id::normal_message_category);
  check(!m1->content().empty());
  check_eq((fetch<int, int>(*m1)), std::nullopt);
  check_eq((fetch<int, int, int>(*m1)), make_tuple(1, 2, 3));
}

TEST("high_priority") {
  auto m1 = make_mailbox_element(nullptr,
                                 make_message_id(message_priority::high), 42);
  check(m1->mid.category() == message_id::urgent_message_category);
}
