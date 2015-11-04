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

#include "caf/config.hpp"

// exclude this suite; advanced match cases are currently not supported on MSVC
#ifndef CAF_WINDOWS

#define CAF_SUITE match
#include "caf/test/unit_test.hpp"

#include <functional>

#include "caf/on.hpp"
#include "caf/announce.hpp"
#include "caf/shutdown.hpp"
#include "caf/message_builder.hpp"
#include "caf/message_handler.hpp"

using namespace caf;
using namespace std;

using hi_atom = atom_constant<atom("hi")>;
using ho_atom = atom_constant<atom("ho")>;

function<maybe<string>(const string&)> starts_with(const string& s) {
  return [=](const string& str) -> maybe<string> {
    if (str.size() > s.size() && str.compare(0, s.size(), s) == 0) {
      auto res = str.substr(s.size());
      return res;
    }
    return none;
  };
}

maybe<int> toint(const string& str) {
  char* endptr = nullptr;
  int result = static_cast<int>(strtol(str.c_str(), &endptr, 10));
  if (endptr != nullptr && *endptr == '\0') {
    return result;
  }
  return none;
}

namespace {

bool s_invoked[] = {false, false, false, false};

} // namespace <anonymous>

void reset() {
  fill(begin(s_invoked), end(s_invoked), false);
}

void fill_mb(message_builder&) {
  // end of recursion
}

template <class T, class... Ts>
void fill_mb(message_builder& mb, const T& x, const Ts&... xs) {
  fill_mb(mb.append(x), xs...);
}

template <class... Ts>
ptrdiff_t invoked(message_handler expr, const Ts&... xs) {
  vector<message> msgs;
  msgs.push_back(make_message(xs...));
  message_builder mb;
  fill_mb(mb, xs...);
  msgs.push_back(mb.to_message());
  set<ptrdiff_t> results;
  for (auto& msg : msgs) {
    expr(msg);
    auto first = begin(s_invoked);
    auto last = end(s_invoked);
    auto i = find(begin(s_invoked), last, true);
    results.insert(i != last && count(i, last, true) == 1 ? distance(first, i)
                                                          : -1);
    reset();
  }
  if (results.size() > 1) {
    CAF_TEST_ERROR("make_message() yielded a different result than "
                   "message_builder(...).to_message()");
    return -2;
  }
  return *results.begin();
}

function<void()> f(int idx) {
  return [=] {
    s_invoked[idx] = true;
  };
}

CAF_TEST(atom_constants) {
  auto expr = on(hi_atom::value) >> f(0);
  CAF_CHECK_EQUAL(invoked(expr, hi_atom::value), 0);
  CAF_CHECK_EQUAL(invoked(expr, ho_atom::value), -1);
  CAF_CHECK_EQUAL(invoked(expr, hi_atom::value, hi_atom::value), -1);
  message_handler expr2{
    [](hi_atom) {
      s_invoked[0] = true;
    },
    [](ho_atom) {
      s_invoked[1] = true;
    }
  };
  CAF_CHECK_EQUAL(invoked(expr2, ok_atom::value), -1);
  CAF_CHECK_EQUAL(invoked(expr2, hi_atom::value), 0);
  CAF_CHECK_EQUAL(invoked(expr2, ho_atom::value), 1);
}

CAF_TEST(guards_called) {
  bool guard_called = false;
  auto guard = [&](int arg) -> maybe<int> {
    guard_called = true;
    return arg;
  };
  auto expr = on(guard) >> f(0);
  CAF_CHECK_EQUAL(invoked(expr, 42), 0);
  CAF_CHECK_EQUAL(guard_called, true);
}

CAF_TEST(forwarding_optionals) {
  auto expr = (
    on(starts_with("--")) >> [](const string& str) {
      CAF_CHECK_EQUAL(str, "help");
      s_invoked[0] = true;
    }
  );
  CAF_CHECK_EQUAL(invoked(expr, "--help"), 0);
  CAF_CHECK_EQUAL(invoked(expr, "-help"), -1);
  CAF_CHECK_EQUAL(invoked(expr, "--help", "--help"), -1);
  CAF_CHECK_EQUAL(invoked(expr, 42), -1);
}

CAF_TEST(projections) {
  auto expr = (
    on(toint) >> [](int i) {
      CAF_CHECK_EQUAL(i, 42);
      s_invoked[0] = true;
    }
  );
  CAF_CHECK_EQUAL(invoked(expr, "42"), 0);
  CAF_CHECK_EQUAL(invoked(expr, "42f"), -1);
  CAF_CHECK_EQUAL(invoked(expr, "42", "42"), -1);
  CAF_CHECK_EQUAL(invoked(expr, 42), -1);
}

struct wrapped_int {
  int value;
};

inline bool operator==(const wrapped_int& lhs, const wrapped_int& rhs) {
  return lhs.value == rhs.value;
}

CAF_TEST(arg_match_pattern) {
  announce<wrapped_int>("wrapped_int", &wrapped_int::value);
  auto expr = on(42, arg_match) >> [](int i) {
    s_invoked[0] = true;
    CAF_CHECK_EQUAL(i, 1);
  };
  CAF_CHECK_EQUAL(invoked(expr, 42, 1.f), -1);
  CAF_CHECK_EQUAL(invoked(expr, 42), -1);
  CAF_CHECK_EQUAL(invoked(expr, 1, 42), -1);
  CAF_CHECK_EQUAL(invoked(expr, 42, 1), 0);
  auto expr2 = on("-a", arg_match) >> [](const string& value) {
    s_invoked[0] = true;
    CAF_CHECK_EQUAL(value, "b");
  };
  CAF_CHECK_EQUAL(invoked(expr2, "b", "-a"), -1);
  CAF_CHECK_EQUAL(invoked(expr2, "-a"), -1);
  CAF_CHECK_EQUAL(invoked(expr2, "-a", "b"), 0);
  auto expr3 = on(wrapped_int{42}, arg_match) >> [](wrapped_int i) {
    s_invoked[0] = true;
    CAF_CHECK_EQUAL(i.value, 1);
  };
  CAF_CHECK_EQUAL(invoked(expr3, wrapped_int{42}, 1.f), -1);
  CAF_CHECK_EQUAL(invoked(expr3, 42), -1);
  CAF_CHECK_EQUAL(invoked(expr3, wrapped_int{1}, wrapped_int{42}), -1);
  CAF_CHECK_EQUAL(invoked(expr3, 42, 1), -1);
  CAF_CHECK_EQUAL(invoked(expr3, wrapped_int{42}, wrapped_int{1}), 0);
}

#endif // CAF_WINDOWS
