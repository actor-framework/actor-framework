// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/intrusive_ptr.hpp"

#include "caf/test/test.hpp"

// this test doesn't verify thread-safety of intrusive_ptr
// however, it is thread safe since it uses atomic operations only

#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"

#include <cstddef>
#include <vector>

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

void check_class_instances() {
  auto& this_test = test::runnable::current();
  this_test.check_eq(class0_instances, 0);
  this_test.check_eq(class1_instances, 0);
}

} // namespace

TEST("make_counted") {
  {
    auto p = make_counted<class0>();
    check_eq(class0_instances, 1);
    check(p->unique());
  }
  check_class_instances();
}

TEST("reset") {
  {
    class0ptr p;
    p.reset(new class0, false);
    check_eq(class0_instances, 1);
    check(p->unique());
  }
  check_class_instances();
}

TEST("get_test_rc") {
  {
    class0ptr p1;
    p1 = get_test_rc();
    class0ptr p2 = p1;
    check_eq(class0_instances, 1);
    check_eq(p1->unique(), false);
  }
  check_class_instances();
}

TEST("list") {
  {
    std::vector<class0ptr> pl;
    pl.push_back(get_test_ptr());
    pl.push_back(get_test_rc());
    pl.push_back(pl.front()->create());
    check(pl.front()->unique());
    check_eq(class0_instances, 3);
  }
  check_class_instances();
}

TEST("full_test") {
  {
    auto p1 = make_counted<class0>();
    check_eq(p1->is_subtype(), false);
    check_eq(p1->unique(), true);
    check_eq(class0_instances, 1);
    check_eq(class1_instances, 0);
    p1.reset(new class1, false);
    check_eq(p1->is_subtype(), true);
    check_eq(p1->unique(), true);
    check_eq(class0_instances, 0);
    check_eq(class1_instances, 1);
    auto p2 = make_counted<class1>();
    p1 = p2;
    check_eq(p1->unique(), false);
    check_eq(class0_instances, 0);
    check_eq(class1_instances, 1);
    check_eq(p1, static_cast<class0*>(p2.get()));
  }
  check_class_instances();
}
