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

#include "caf/config.hpp"

#define CAF_SUITE intrusive_ptr
#include "caf/test/unit_test.hpp"

// this test dosn't verify thread-safety of intrusive_ptr
// however, it is thread safe since it uses atomic operations only

#include <vector>
#include <cstddef>

#include "caf/ref_counted.hpp"
#include "caf/make_counted.hpp"
#include "caf/intrusive_ptr.hpp"

using namespace caf;

namespace {

int class0_instances = 0;
int class1_instances = 0;

class class0;
class class1;

using class0ptr = intrusive_ptr<class0>;
using class1ptr = intrusive_ptr<class1>;

class class0 : public ref_counted {
public:
  explicit class0(bool subtype = false) : subtype_(subtype) {
    if (!subtype) {
      ++class0_instances;
    }
  }

  ~class0() override {
    if (!subtype_) {
      --class0_instances;
    }
  }

  bool is_subtype() const {
    return subtype_;
  }

  virtual class0ptr create() const {
    return make_counted<class0>();
  }

private:
  bool subtype_;
};

class class1 : public class0 {
public:
  class1() : class0(true) {
    ++class1_instances;
  }

  ~class1() override {
    --class1_instances;
  }

  class0ptr create() const override {
    return make_counted<class1>();
  }
};

class0ptr get_test_rc() {
  return make_counted<class0>();
}

class0ptr get_test_ptr() {
  return get_test_rc();
}

struct fixture {
  ~fixture() {
    CAF_CHECK_EQUAL(class0_instances, 0);
    CAF_CHECK_EQUAL(class1_instances, 0);
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(atom_tests, fixture)

CAF_TEST(make_counted) {
  auto p = make_counted<class0>();
  CAF_CHECK_EQUAL(class0_instances, 1);
  CAF_CHECK(p->unique());
}

CAF_TEST(reset) {
  class0ptr p;
  p.reset(new class0, false);
  CAF_CHECK_EQUAL(class0_instances, 1);
  CAF_CHECK(p->unique());
}

CAF_TEST(get_test_rc) {
  class0ptr p1;
  p1 = get_test_rc();
  class0ptr p2 = p1;
  CAF_CHECK_EQUAL(class0_instances, 1);
  CAF_CHECK_EQUAL(p1->unique(), false);
}

CAF_TEST(list) {
  std::vector<class0ptr> pl;
  pl.push_back(get_test_ptr());
  pl.push_back(get_test_rc());
  pl.push_back(pl.front()->create());
  CAF_CHECK(pl.front()->unique());
  CAF_CHECK_EQUAL(class0_instances, 3);
}

CAF_TEST(full_test) {
  auto p1 = make_counted<class0>();
  CAF_CHECK_EQUAL(p1->is_subtype(), false);
  CAF_CHECK_EQUAL(p1->unique(), true);
  CAF_CHECK_EQUAL(class0_instances, 1);
  CAF_CHECK_EQUAL(class1_instances, 0);
  p1.reset(new class1, false);
  CAF_CHECK_EQUAL(p1->is_subtype(), true);
  CAF_CHECK_EQUAL(p1->unique(), true);
  CAF_CHECK_EQUAL(class0_instances, 0);
  CAF_CHECK_EQUAL(class1_instances, 1);
  auto p2 = make_counted<class1>();
  p1 = p2;
  CAF_CHECK_EQUAL(p1->unique(), false);
  CAF_CHECK_EQUAL(class0_instances, 0);
  CAF_CHECK_EQUAL(class1_instances, 1);
  CAF_CHECK_EQUAL(p1, static_cast<class0*>(p2.get()));
}

CAF_TEST_FIXTURE_SCOPE_END()
