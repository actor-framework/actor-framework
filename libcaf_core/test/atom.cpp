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

#include "caf/config.hpp"

#define CAF_SUITE atom
#include "caf/test/unit_test.hpp"

#include <string>

#include "caf/all.hpp"

using namespace caf;

namespace {

constexpr auto s_foo = atom("FooBar");

using a_atom = atom_constant<atom("a")>;
using b_atom = atom_constant<atom("b")>;
using c_atom = atom_constant<atom("c")>;
using abc_atom = atom_constant<atom("abc")>;
using def_atom = atom_constant<atom("def")>;
using foo_atom = atom_constant<atom("foo")>;

struct fixture {
  fixture() : system(cfg) {
    // nop
  }

  actor_system_config cfg;
  actor_system system;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(atom_tests, fixture)

CAF_TEST(basics) {
  // check if there are leading bits that distinguish "zzz" and "000 "
  CAF_CHECK_NOT_EQUAL(atom("zzz"), atom("000 "));
  // check if there are leading bits that distinguish "abc" and " abc"
  CAF_CHECK_NOT_EQUAL(atom("abc"), atom(" abc"));
  // 'illegal' characters are mapped to whitespaces
  CAF_CHECK_EQUAL(atom("   "), atom("@!?"));
  // check to_string impl
  CAF_CHECK_EQUAL(to_string(s_foo), "FooBar");
}

struct send_to_self {
  explicit send_to_self(blocking_actor* self) : self_(self) {
    // nop
  }
  template <class... Ts>
  void operator()(Ts&&... xs) {
    self_->send(self_, std::forward<Ts>(xs)...);
  }
  blocking_actor* self_;
};

CAF_TEST(receive_atoms) {
  scoped_actor self{system};
  send_to_self f{self.ptr()};
  f(foo_atom::value, static_cast<uint32_t>(42));
  f(abc_atom::value, def_atom::value, "cstring");
  f(1.f);
  f(a_atom::value, b_atom::value, c_atom::value, 23.f);
  bool matched_pattern[3] = {false, false, false};
  int i = 0;
  CAF_MESSAGE("start receive loop");
  for (i = 0; i < 3; ++i) {
    self->receive(
      [&](foo_atom, uint32_t value) {
        matched_pattern[0] = true;
        CAF_CHECK_EQUAL(value, 42u);
      },
      [&](abc_atom, def_atom, const std::string& str) {
        matched_pattern[1] = true;
        CAF_CHECK_EQUAL(str, "cstring");
      },
      [&](a_atom, b_atom, c_atom, float value) {
        matched_pattern[2] = true;
        CAF_CHECK_EQUAL(value, 23.f);
      }
    );
  }
  CAF_CHECK(matched_pattern[0] && matched_pattern[1] && matched_pattern[2]);
  self->receive(
    [](float) {
      // erase float message
    }
  );
  atom_value x = atom("abc");
  atom_value y = abc_atom::value;
  CAF_CHECK_EQUAL(x, y);
  auto msg = make_message(atom("abc"));
  self->send(self, msg);
  self->receive(
    [](abc_atom) {
      CAF_MESSAGE("received 'abc'");
    }
  );
}

using testee = typed_actor<replies_to<abc_atom>::with<int>>;

testee::behavior_type testee_impl(testee::pointer self) {
  return {
    [=](abc_atom) {
      self->quit();
      return 42;
    }
  };
}

CAF_TEST(request_atom_constants) {
  scoped_actor self{system};
  auto tst = system.spawn(testee_impl);
  self->request(tst, infinite, abc_atom::value).receive(
    [](int i) {
      CAF_CHECK_EQUAL(i, 42);
    },
    [&](error& err) {
      CAF_FAIL("err: " << system.render(err));
    }
  );
}

CAF_TEST(runtime_conversion) {
  CAF_CHECK_EQUAL(atom("foo"), atom_from_string("foo"));
  CAF_CHECK_EQUAL(atom(""), atom_from_string("tooManyCharacters"));
}

CAF_TEST_FIXTURE_SCOPE_END()
