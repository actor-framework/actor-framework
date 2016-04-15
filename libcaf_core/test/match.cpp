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

#define CAF_SUITE match
#include "caf/test/unit_test.hpp"

#include <functional>

#include "caf/message_builder.hpp"
#include "caf/message_handler.hpp"

using namespace caf;
using namespace std;

using hi_atom = atom_constant<atom("hi")>;
using ho_atom = atom_constant<atom("ho")>;

function<optional<string>(const string&)> starts_with(const string& s) {
  return [=](const string& str) -> optional<string> {
    if (str.size() > s.size() && str.compare(0, s.size(), s) == 0)
      return str.substr(s.size());
    return none;
  };
}

optional<int> toint(const string& str) {
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
    CAF_ERROR("make_message() yielded a different result than "
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
  message_handler expr{
    [](hi_atom) {
      s_invoked[0] = true;
    },
    [](ho_atom) {
      s_invoked[1] = true;
    }
  };
  CAF_CHECK_EQUAL(invoked(expr, ok_atom::value), -1);
  CAF_CHECK_EQUAL(invoked(expr, hi_atom::value), 0);
  CAF_CHECK_EQUAL(invoked(expr, ho_atom::value), 1);
}
