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

#define CAF_SUITE either
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

using foo = typed_actor<replies_to<int>::with_either<int>::or_else<float>>;

foo::behavior_type my_foo() {
  return {
    [](int arg) -> either<int>::or_else<float> {
      if (arg == 42) {
        return 42;
      }
      return static_cast<float>(arg);
    }
  };
}

CAF_TEST(either) {
  auto f1 = []() -> either<int>::or_else<float> {
    return 42;
  };
  auto f2 = []() -> either<int>::or_else<float> {
    return 42.f;
  };
  auto f3 = [](bool flag) -> either<int, int>::or_else<float, float> {
    if (flag) {
      return {1, 2};
    }
    return {3.f, 4.f};
  };
  f1();
  f2();
  f3(true);
  f3(false);
  either<int>::or_else<float> x1{4};
  either<int>::or_else<float> x2{4.f};
  auto mf = spawn_typed(my_foo);
  {
    scoped_actor self;
    self->sync_send(mf, 42).await(
      [](int val) {
        CAF_CHECK_EQUAL(val, 42);
      },
      [](float) {
        CAF_TEST_ERROR("expected an integer");
      }
    );
    self->sync_send(mf, 10).await(
      [](int) {
        CAF_TEST_ERROR("expected a float");
      },
      [](float val) {
        CAF_CHECK_EQUAL(val, 10.f);
      }
    );
  }
  shutdown();
}
