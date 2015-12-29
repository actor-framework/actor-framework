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

#define CAF_SUITE thread_specific
#include "caf/test/unit_test.hpp"

#include <thread>
#include <type_traits>

#include "caf/detail/thread_specific.hpp"

using caf::detail::thread_specific;

namespace {

struct test_class {
  test_class() {
    ++num_instances;
  }

  test_class(int init)
      : val(init) {
    ++num_instances;
  }

  ~test_class() {
    --num_instances;
  }

  int val = 0;

  static int num_instances;
};

int test_class::num_instances = 0;

// used to verify that TLS objects remain across function calls
test_class* func() {
  auto& obj = thread_specific<test_class>();
  return &obj;
}

} // namespace <anonymous>

CAF_TEST(lifetime) {
  CAF_REQUIRE(test_class::num_instances == 0);
  std::thread{[] {
    static_assert(std::is_same<
                    decltype(thread_specific<test_class>()),
                    test_class&>::value,
                  "thread_specific<>() wrong return type");
    auto& obj_0 = thread_specific<test_class>();
    auto& obj_1 = thread_specific<test_class>();
    CAF_CHECK(&obj_0 == &obj_1);
    CAF_CHECK(test_class::num_instances == 1);
  }}.join();
  CAF_REQUIRE(test_class::num_instances == 0);
  // verifies that TLS objects are lazy-initialized
  std::thread{[] {
    CAF_CHECK(test_class::num_instances == 0);
    static_cast<void>(thread_specific<test_class>());
    CAF_CHECK(test_class::num_instances == 1);
  }}.join();
}

CAF_TEST(thread_specificity) {
  // verifies that different threads have different objects
  std::thread{[] {
    auto& obj_0 = thread_specific<test_class>();
    std::thread{[&obj_0] {
      auto& obj_1 = thread_specific<test_class>();
      CAF_CHECK(&obj_0 != &obj_1);
    }}.join();
  }}.join();
  // verifies that TLS objects remain across function calls
  std::thread{[] {
    auto obj_ptr = func();
    CAF_CHECK(test_class::num_instances == 1);
    auto& obj = thread_specific<test_class>();
    CAF_CHECK(obj_ptr == &obj);
  }}.join();
}

CAF_TEST(tagging) {
  std::thread{[] {
    auto& obj_0 = thread_specific<test_class, int>();
    auto& obj_1 = thread_specific<test_class, double>();
    CAF_CHECK(test_class::num_instances == 2);
    CAF_CHECK(&obj_0 != &obj_1);
  }}.join();
}

CAF_TEST(init) {
  std::thread{[] {
    auto& obj = thread_specific<test_class>([](test_class& obj) {
      ++obj.val;
    });
    CAF_CHECK(obj.val == 1);
  }}.join();
}

CAF_TEST(uninit) {
  std::thread{[] {
    auto& obj = thread_specific<test_class>(nullptr,
      [](test_class& obj) {
        --obj.num_instances;
      }
    );
    ++obj.num_instances;
  }}.join();
  CAF_REQUIRE(test_class::num_instances == 0);
}

CAF_TEST(ctor_args) {
  std::thread{[] {
    auto& obj = thread_specific<test_class>(nullptr, nullptr, 7);
    CAF_CHECK(obj.val == 7);
  }}.join();
}
