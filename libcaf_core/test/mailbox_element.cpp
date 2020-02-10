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

#define CAF_SUITE mailbox_element

#include <string>
#include <tuple>
#include <vector>

#include "caf/all.hpp"

#include "caf/test/unit_test.hpp"

using std::make_tuple;
using std::string;
using std::tuple;
using std::vector;

using namespace caf;

namespace {

template <class... Ts>
optional<tuple<Ts...>> fetch(const type_erased_tuple& x) {
  if (!x.match_elements<Ts...>())
    return none;
  return x.get_as_tuple<Ts...>();
}

template <class... Ts>
optional<tuple<Ts...>> fetch(const message& x) {
  return fetch<Ts...>(x.content());
}

template <class... Ts>
optional<tuple<Ts...>> fetch(const mailbox_element& x) {
  return fetch<Ts...>(x.content());
}

} // namespace

CAF_TEST(empty_message) {
  auto m1 = make_mailbox_element(nullptr, make_message_id(),
                                 no_stages, make_message());
  CAF_CHECK(m1->mid.is_async());
  CAF_CHECK(m1->mid.category() == message_id::normal_message_category);
  CAF_CHECK(m1->content().empty());
}

CAF_TEST(non_empty_message) {
  auto m1 = make_mailbox_element(nullptr, make_message_id(),
                                 no_stages, make_message(1, 2, 3));
  CAF_CHECK(m1->mid.is_async());
  CAF_CHECK(m1->mid.category() == message_id::normal_message_category);
  CAF_CHECK(!m1->content().empty());
  CAF_CHECK_EQUAL((fetch<int, int>(*m1)), none);
  CAF_CHECK_EQUAL((fetch<int, int, int>(*m1)), make_tuple(1, 2, 3));
}

CAF_TEST(tuple) {
  auto m1 = make_mailbox_element(nullptr, make_message_id(),
                                 no_stages, 1, 2, 3);
  CAF_CHECK(m1->mid.is_async());
  CAF_CHECK(m1->mid.category() == message_id::normal_message_category);
  CAF_CHECK(!m1->content().empty());
  CAF_CHECK_EQUAL((fetch<int, int>(*m1)), none);
  CAF_CHECK_EQUAL((fetch<int, int, int>(*m1)), make_tuple(1, 2, 3));
}

CAF_TEST(high_priority) {
  auto m1 = make_mailbox_element(nullptr,
                                 make_message_id(message_priority::high),
                                 no_stages, 42);
  CAF_CHECK(m1->mid.category() == message_id::urgent_message_category);
}

CAF_TEST(upstream_msg_static) {
  auto m1 = make_mailbox_element(nullptr, make_message_id(), no_stages,
                                 make<upstream_msg::drop>({0, 0}, nullptr));
  CAF_CHECK(m1->mid.category() == message_id::upstream_message_category);
}

CAF_TEST(upstream_msg_dynamic) {
  auto m1 = make_mailbox_element(
    nullptr, make_message_id(), no_stages,
    make_message(make<upstream_msg::drop>({0, 0}, nullptr)));
  CAF_CHECK(m1->mid.category() == message_id::upstream_message_category);
}

CAF_TEST(downstream_msg_static) {
  auto m1 = make_mailbox_element(nullptr, make_message_id(), no_stages,
                                 make<downstream_msg::close>({0, 0}, nullptr));
  CAF_CHECK(m1->mid.category() == message_id::downstream_message_category);
}

CAF_TEST(downstream_msg_dynamic) {
  auto m1 = make_mailbox_element(
    nullptr, make_message_id(), no_stages,
    make_message(make<downstream_msg::close>({0, 0}, nullptr)));
  CAF_CHECK(m1->mid.category() == message_id::downstream_message_category);
}
