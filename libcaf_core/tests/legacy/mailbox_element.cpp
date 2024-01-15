// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE mailbox_element

#include "caf/mailbox_element.hpp"

#include "caf/all.hpp"

#include "core-test.hpp"

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

CAF_TEST(empty_message) {
  auto m1 = make_mailbox_element(nullptr, make_message_id(), make_message());
  CHECK(m1->mid.is_async());
  CHECK(m1->mid.category() == message_id::normal_message_category);
  CHECK(m1->content().empty());
}

CAF_TEST(non_empty_message) {
  auto m1 = make_mailbox_element(nullptr, make_message_id(),
                                 make_message(1, 2, 3));
  CHECK(m1->mid.is_async());
  CHECK(m1->mid.category() == message_id::normal_message_category);
  CHECK(!m1->content().empty());
  CHECK_EQ((fetch<int, int>(*m1)), std::nullopt);
  CHECK_EQ((fetch<int, int, int>(*m1)), make_tuple(1, 2, 3));
}

CAF_TEST(tuple) {
  auto m1 = make_mailbox_element(nullptr, make_message_id(), 1, 2, 3);
  CHECK(m1->mid.is_async());
  CHECK(m1->mid.category() == message_id::normal_message_category);
  CHECK(!m1->content().empty());
  CHECK_EQ((fetch<int, int>(*m1)), std::nullopt);
  CHECK_EQ((fetch<int, int, int>(*m1)), make_tuple(1, 2, 3));
}

CAF_TEST(high_priority) {
  auto m1 = make_mailbox_element(nullptr,
                                 make_message_id(message_priority::high), 42);
  CHECK(m1->mid.category() == message_id::urgent_message_category);
}
