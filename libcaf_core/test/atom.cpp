/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#define CAF_SUITE atom
#include "caf/test/unit_test.hpp"

#include <string>

#include "caf/all.hpp"

using namespace caf;

namespace {

constexpr auto s_foo = atom("FooBar");
using abc_atom = atom_constant<atom("abc")>;

} // namespace <anonymous>

CAF_TEST(basics) {
  // check if there are leading bits that distinguish "zzz" and "000 "
  CAF_CHECK(atom("zzz") != atom("000 "));
  // check if there are leading bits that distinguish "abc" and " abc"
  CAF_CHECK(atom("abc") != atom(" abc"));
  // 'illegal' characters are mapped to whitespaces
  CAF_CHECK_EQUAL(atom("   "), atom("@!?"));
  // check to_string impl.
  CAF_CHECK_EQUAL(to_string(s_foo), "FooBar");
}

struct send_to_self {
  send_to_self(blocking_actor* self) : m_self(self) {
    // nop
  }
  template <class... Ts>
  void operator()(Ts&&... xs) {
    m_self->send(m_self, std::forward<Ts>(xs)...);
  }
  blocking_actor* m_self;
};

CAF_TEST(receive_atoms) {
  scoped_actor self;
  send_to_self f{self.get()};
  f(atom("foo"), static_cast<uint32_t>(42));
  f(atom("abc"), atom("def"), "cstring");
  f(1.f);
  f(atom("a"), atom("b"), atom("c"), 23.f);
  bool matched_pattern[3] = {false, false, false};
  int i = 0;
  CAF_MESSAGE("start receive loop");
  for (i = 0; i < 3; ++i) {
    self->receive(
      on(atom("foo"), arg_match) >> [&](uint32_t value) {
        matched_pattern[0] = true;
        CAF_CHECK_EQUAL(value, 42);
      },
      on(atom("abc"), atom("def"), arg_match) >> [&](const std::string& str) {
        matched_pattern[1] = true;
        CAF_CHECK_EQUAL(str, "cstring");
      },
      on(atom("a"), atom("b"), atom("c"), arg_match) >> [&](float value) {
        matched_pattern[2] = true;
        CAF_CHECK_EQUAL(value, 23.f);
      });
  }
  CAF_CHECK(matched_pattern[0] && matched_pattern[1] && matched_pattern[2]);
  self->receive(
    // "erase" message { atom("b"), atom("a"), atom("c"), 23.f }
    others >> [] {
      CAF_MESSAGE("drain mailbox");
    },
    after(std::chrono::seconds(0)) >> [] {
      CAF_TEST_ERROR("mailbox empty");
    }
  );
  atom_value x = atom("abc");
  atom_value y = abc_atom::value;
  CAF_CHECK_EQUAL(x, y);
  auto msg = make_message(abc_atom());
  self->send(self, msg);
  self->receive(
    on(atom("abc")) >> [] {
      CAF_MESSAGE("received 'abc'");
    },
    others >> [] {
      CAF_TEST_ERROR("unexpected message");
    }
  );
}

using testee = typed_actor<replies_to<abc_atom>::with<int>>;

testee::behavior_type testee_impl(testee::pointer self) {
  return {
    [=](abc_atom) {
      CAF_MESSAGE("received abc_atom");
      self->quit();
      return 42;
    }
  };
}

CAF_TEST(sync_send_atom_constants) {
  scoped_actor self;
  auto tst = spawn_typed(testee_impl);
  self->sync_send(tst, abc_atom::value).await(
    [](int i) {
      CAF_CHECK_EQUAL(i, 42);
    }
  );
}
