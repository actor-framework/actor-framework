#include <list>
#include <cstddef>

#include "test.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"
#include "caf/detail/make_counted.hpp"

using namespace caf;

namespace {

int class0_instances = 0;
int class1_instances = 0;

struct class0 : ref_counted {
  class0(bool subtype = false) : m_subtype(subtype) {
    if (!subtype) {
      ++class0_instances;
    }
  }

  ~class0() {
    if (!m_subtype) {
      --class0_instances;
    }
  }

  bool is_subtype() const {
    return m_subtype;
  }

  virtual class0* create() const {
    return new class0;
  }

  bool m_subtype;
};

struct class1 : class0 {
  class1() : class0(true) {
    ++class1_instances;
  }

  ~class1() {
    --class1_instances;
  }

  class1* create() const override {
    return new class1;
  }
};

using class0_ptr = intrusive_ptr<class0>;
using class1_ptr = intrusive_ptr<class1>;

class0* get_test_rc() {
  return new class0;
}

class0_ptr get_test_ptr() {
  return get_test_rc();
}

} // namespace <anonymous>

int main() {
  // this test dosn't test thread-safety of intrusive_ptr
  // however, it is thread safe since it uses atomic operations only

  CAF_TEST(test_intrusive_ptr);
  {
    auto p = detail::make_counted<class0>();
    CAF_CHECK_EQUAL(class0_instances, 1);
    CAF_CHECK(p->unique());
  }
  CAF_CHECK_EQUAL(class0_instances, 0);
  CAF_CHECK_EQUAL(class1_instances, 0);
  {
    class0_ptr p;
    p = new class0;
    CAF_CHECK_EQUAL(class0_instances, 1);
    CAF_CHECK(p->unique());
  }
  CAF_CHECK_EQUAL(class0_instances, 0);
  CAF_CHECK_EQUAL(class1_instances, 0);
  {
    class0_ptr p1;
    p1 = get_test_rc();
    class0_ptr p2 = p1;
    CAF_CHECK_EQUAL(class0_instances, 1);
    CAF_CHECK_EQUAL(p1->unique(), false);
  }
  CAF_CHECK_EQUAL(class0_instances, 0);
  CAF_CHECK_EQUAL(class1_instances, 0);
  {
    std::list<class0_ptr> pl;
    pl.push_back(get_test_ptr());
    pl.push_back(get_test_rc());
    pl.push_back(pl.front()->create());
    CAF_CHECK(pl.front()->unique());
    CAF_CHECK_EQUAL(class0_instances, 3);
  }
  CAF_CHECK_EQUAL(class0_instances, 0);
  CAF_CHECK_EQUAL(class1_instances, 0);
  {
    auto p1 = detail::make_counted<class0>();
    CAF_CHECK_EQUAL(p1->is_subtype(), false);
    CAF_CHECK_EQUAL(p1->unique(), true);
    CAF_CHECK_EQUAL(class0_instances, 1);
    CAF_CHECK_EQUAL(class1_instances, 0);
    p1.reset(new class1);
    CAF_CHECK_EQUAL(p1->is_subtype(), true);
    CAF_CHECK_EQUAL(p1->unique(), true);
    CAF_CHECK_EQUAL(class0_instances, 0);
    CAF_CHECK_EQUAL(class1_instances, 1);
    auto p2 = detail::make_counted<class1>();
    p1 = p2;
    CAF_CHECK_EQUAL(p1->unique(), false);
    CAF_CHECK_EQUAL(class0_instances, 0);
    CAF_CHECK_EQUAL(class1_instances, 1);
    CAF_CHECK(p1 == p2);
  }
  CAF_CHECK_EQUAL(class0_instances, 0);
  CAF_CHECK_EQUAL(class1_instances, 0);
  return CAF_TEST_RESULT();
}
