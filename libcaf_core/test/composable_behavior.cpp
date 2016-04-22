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

#define CAF_SUITE composable_behaviors
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace std;
using namespace caf;

namespace {

using i3_actor = typed_actor<replies_to<int, int, int>::with<int>>;

using d_actor = typed_actor<replies_to<double>::with<double, double>>;

using foo_actor = i3_actor::extend_with<d_actor>;

class foo_actor_state : public composable_behavior<foo_actor> {
public:
  result<int> operator()(int x, int y, int z) override {
    return x + y + z;
  }

  result<double, double> operator()(double x) override {
    return {x, x};
  }
};

class i3_actor_state : public composable_behavior<i3_actor> {
public:
  result<int> operator()(int x, int y, int z) override {
    return x + y + z;
  }
};

class d_actor_state : public composable_behavior<d_actor> {
public:
  result<double, double> operator()(double x) override {
    return {x, x};
  }
};

class i3_actor_state2 : public composable_behavior<i3_actor> {
public:
  result<int> operator()(int x, int y, int z) override {
    return x * (y * z);
  }
};

struct foo_actor_state2 : composed_behavior<i3_actor_state2, i3_actor_state, d_actor_state> {
  result<int> operator()(int x, int y, int z) override {
    return x - y - z;
  }
};

using add_atom = atom_constant<atom("Add")>;
using get_name_atom = atom_constant<atom("GetName")>;

// "base" interface
using named_actor = typed_actor<replies_to<get_name_atom>::with<std::string>>;

// a simple dictionary
using dict = named_actor::extend<replies_to<get_atom, std::string>::with<std::string>,
                                 replies_to<put_atom, std::string, std::string>::with<void>>;

// a simple calculator
using calc = named_actor::extend<replies_to<add_atom, int, int>::with<int>>;

class dict_state : public composable_behavior<dict> {
public:
  result<std::string> operator()(get_name_atom) override {
    return "dictionary";
  }

  result<std::string> operator()(get_atom, param<std::string> key) override {
    auto i = values_.find(key);
    if (i == values_.end())
      return "";
    return i->second;
  }

  result<void> operator()(put_atom, param<std::string> key,
                          param<std::string> value) override {
    values_.emplace(key.move(), value.move());
    return unit;
  }

protected:
  std::unordered_map<std::string, std::string> values_;
};

class calc_state : public composable_behavior<calc> {
public:
  result<std::string> operator()(get_name_atom) override {
    return "calculator";
  }

  result<int> operator()(add_atom, int x, int y) override {
    return x + y;
  }
};

class dict_calc_state : public composed_behavior<dict_state, calc_state> {
public:
  // composed_behavior<...> will mark this operator pure virtual, because
  // of conflicting declarations in dict_state and calc_state
  result<std::string> operator()(get_name_atom) override {
    return "calculating dictionary";
  }
};

} // namespace <anonymous>

CAF_TEST(composable_behaviors) {
  actor_system sys;
  //auto x1 = sys.spawn<stateful_impl<foo_actor_state>>();
  auto x1 = sys.spawn<foo_actor_state>();
  scoped_actor self{sys};
  self->request(x1, infinite, 1, 2, 4).receive(
    [](int y) {
      CAF_CHECK_EQUAL(y, 7);
    }
  );
  self->send_exit(x1, exit_reason::kill);
  //auto x2 = sys.spawn<stateful_impl<composed_behavior<i3_actor_state, d_actor_state>>>();
  auto x2 = sys.spawn<composed_behavior<i3_actor_state, d_actor_state>>();
  self->request(x2, infinite, 1, 2, 4).receive(
    [](int y) {
      CAF_CHECK_EQUAL(y, 7);
    }
  );
  self->request(x2, infinite, 1.0).receive(
    [](double y1, double y2) {
      CAF_CHECK_EQUAL(y1, 1.0);
      CAF_CHECK_EQUAL(y1, y2);
    }
  );
  self->send_exit(x2, exit_reason::kill);
  //auto x3 = sys.spawn<stateful_impl<foo_actor_state2>>();
  auto x3 = sys.spawn<foo_actor_state2>();
  self->request(x3, infinite, 1, 2, 4).receive(
    [](int y) {
      CAF_CHECK_EQUAL(y, -5);
    }
  );
  self->send_exit(x3, exit_reason::kill);
  //auto x4 = sys.spawn<stateful_impl<dict_calc_state>>();
  auto x4 = sys.spawn<dict_calc_state>();
  self->request(x4, infinite, add_atom::value, 10, 20).receive(
    [](int y) {
      CAF_CHECK_EQUAL(y, 30);
    }
  );
  self->send_exit(x4, exit_reason::kill);
}
