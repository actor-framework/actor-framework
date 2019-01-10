/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/message.hpp"

#define CAF_SUITE match

#include "caf/test/unit_test.hpp"

#include <functional>

#include "caf/make_type_erased_tuple_view.hpp"
#include "caf/message_builder.hpp"
#include "caf/message_handler.hpp"
#include "caf/rtti_pair.hpp"

using namespace caf;
using namespace std;

using hi_atom = atom_constant<atom("hi")>;
using ho_atom = atom_constant<atom("ho")>;

namespace {

struct fixture {
  using array_type = std::array<bool, 4>;

  fixture() {
    reset();
  }

  void reset() {
    for (auto& x : invoked)
      x = false;
  }

  template <class... Ts>
  ptrdiff_t invoke(message_handler expr, Ts... xs) {
    auto msg1 = make_message(xs...);
    auto msg2 = message_builder{}.append_all(xs...).move_to_message();
    auto msg3 = make_type_erased_tuple_view(xs...);
    CAF_CHECK_EQUAL(to_string(msg1), to_string(msg2));
    CAF_CHECK_EQUAL(to_string(msg1), to_string(msg3));
    CAF_CHECK_EQUAL(msg1.type_token(), msg2.type_token());
    CAF_CHECK_EQUAL(msg1.type_token(), msg3.type_token());
    std::vector<std::string> msg1_types;
    std::vector<std::string> msg2_types;
    std::vector<std::string> msg3_types;
    for (size_t i = 0; i < msg1.size(); ++i) {
      msg1_types.push_back(to_string(msg1.type(i)));
      msg2_types.push_back(to_string(msg2.type(i)));
      msg3_types.push_back(to_string(msg3.type(i)));
    }
    CAF_CHECK_EQUAL(msg1_types, msg2_types);
    CAF_CHECK_EQUAL(msg1_types, msg3_types);
    set<ptrdiff_t> results;
    process(results, expr, msg1, msg2, msg3);
    if (results.size() > 1) {
      CAF_ERROR("different results reported: " << deep_to_string(results));
      return -2;
    }
    return *results.begin();
  }

  void process(std::set<ptrdiff_t>&, message_handler&) {
    // end of recursion
  }

  template <class T, class... Ts>
  void process(std::set<ptrdiff_t>& results, message_handler& expr,
               T& x, Ts&... xs) {
    expr(x);
    results.insert(invoked_res());
    reset();
    process(results, expr, xs...);
  }

  ptrdiff_t invoked_res() {
    auto first = begin(invoked);
    auto last = end(invoked);
    auto i = find(first, last, true);
    if (i != last) {
      CAF_REQUIRE_EQUAL(count(i, last, true), 1u);
      return distance(first, i);
    }
    return -1;
  }

  array_type invoked;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(atom_constants_tests, fixture)

CAF_TEST(atom_constants) {
  message_handler expr{
    [&](hi_atom) {
      invoked[0] = true;
    },
    [&](ho_atom) {
      invoked[1] = true;
    }
  };
  CAF_CHECK_EQUAL(invoke(expr, atom_value{ok_atom::value}), -1);
  CAF_CHECK_EQUAL(invoke(expr, atom_value{hi_atom::value}), 0);
  CAF_CHECK_EQUAL(invoke(expr, atom_value{ho_atom::value}), 1);
}

CAF_TEST(manual_matching) {
  using foo_atom = atom_constant<atom("foo")>;
  using bar_atom = atom_constant<atom("bar")>;
  auto msg1 = make_message(foo_atom::value, 42);
  auto msg2 = make_message(bar_atom::value, 42);
  CAF_MESSAGE("check individual message elements");
  CAF_CHECK((msg1.match_element<int>(1)));
  CAF_CHECK((msg2.match_element<int>(1)));
  CAF_CHECK((msg1.match_element<foo_atom>(0)));
  CAF_CHECK((!msg2.match_element<foo_atom>(0)));
  CAF_CHECK((!msg1.match_element<bar_atom>(0)));
  CAF_CHECK((msg2.match_element<bar_atom>(0)));
  CAF_MESSAGE("check matching whole tuple");
  CAF_CHECK((msg1.match_elements<foo_atom, int>()));
  CAF_CHECK(!(msg2.match_elements<foo_atom, int>()));
  CAF_CHECK(!(msg1.match_elements<bar_atom, int>()));
  CAF_CHECK((msg2.match_elements<bar_atom, int>()));
  CAF_CHECK((msg1.match_elements<atom_value, int>()));
  CAF_CHECK((msg2.match_elements<atom_value, int>()));
  CAF_CHECK(!(msg1.match_elements<atom_value, double>()));
  CAF_CHECK(!(msg2.match_elements<atom_value, double>()));
  CAF_CHECK(!(msg1.match_elements<atom_value, int, int>()));
  CAF_CHECK(!(msg2.match_elements<atom_value, int, int>()));
}

CAF_TEST_FIXTURE_SCOPE_END()
