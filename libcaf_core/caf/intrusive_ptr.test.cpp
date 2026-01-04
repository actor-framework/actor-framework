// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/intrusive_ptr.hpp"

#include "caf/test/test.hpp"

#include "caf/config.hpp"

// this test doesn't verify thread-safety of intrusive_ptr
// however, it is thread safe since it uses atomic operations only

#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"

#include <cstddef>
#include <vector>

CAF_PUSH_DEPRECATED_WARNING

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

} // namespace

TEST("make_counted") {
  {
    auto p = make_counted<class0>();
    check_eq(class0_instances, 1);
    check(p->unique());
  }
  check_eq(class0_instances, 0);
}

TEST("reset") {
  SECTION("no arguments") {
    auto ptr = make_counted<class0>();
    ptr.reset();
    check_eq(class0_instances, 0);
    check_eq(ptr.get(), nullptr);
  }
  SECTION("passing pointer and adopt_ref") {
    class0ptr ptr;
    ptr.reset(new class0, adopt_ref);
    check_eq(class0_instances, 1);
    check(ptr->unique());
  }
  SECTION("passing pointer and false") {
    class0ptr ptr;
    ptr.reset(new class0, false);
    check_eq(class0_instances, 1);
    check(ptr->unique());
  }
  SECTION("passing pointer and add_ref") {
    auto* raw_ptr = new class0;
    class0ptr ptr;
    ptr.reset(raw_ptr, add_ref);
    check_eq(class0_instances, 1);
    raw_ptr->deref();
    check(ptr->unique());
  }
  SECTION("passing pointer and true") {
    auto* raw_ptr = new class0;
    class0ptr ptr;
    ptr.reset(raw_ptr, true);
    check_eq(class0_instances, 1);
    raw_ptr->deref();
    check(ptr->unique());
  }
  check_eq(class0_instances, 0);
}

TEST("list") {
  {
    std::vector<class0ptr> pl;
    pl.push_back(make_counted<class0>());
    pl.push_back(make_counted<class0>());
    pl.push_back(pl.front()->create());
    check(pl.front()->unique());
    check_eq(class0_instances, 3);
  }
  check_eq(class0_instances, 0);
}

TEST("full_test") {
  {
    auto p1 = make_counted<class0>();
    check_eq(p1->is_subtype(), false);
    check_eq(p1->unique(), true);
    check_eq(class0_instances, 1);
    check_eq(class1_instances, 0);
    p1.reset(new class1, adopt_ref);
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
  check_eq(class0_instances, 0);
  check_eq(class1_instances, 0);
}

TEST("default and nullptr construction") {
  SECTION("default constructor creates null pointer") {
    class0ptr ptr;
    check_eq(ptr.get(), nullptr);
    check(!ptr);
    check_eq(static_cast<bool>(ptr), false);
  }
  SECTION("nullptr_t constructor creates null pointer") {
    class0ptr ptr{nullptr};
    check_eq(ptr.get(), nullptr);
    check(!ptr);
  }
  check_eq(class0_instances, 0);
}

TEST("construction with add_ref") {
  auto* raw = new class0;
  {
    class0ptr ptr{raw, add_ref};
    check_eq(class0_instances, 1);
    check(!ptr->unique());
  }
  // ptr released one ref, but raw still holds its initial ref
  raw->deref();
  check_eq(class0_instances, 0);
}

TEST("construction with adopt_ref") {
  {
    class0ptr ptr{new class0, adopt_ref};
    check_eq(class0_instances, 1);
    check(ptr->unique());
  }
  check_eq(class0_instances, 0);
}

TEST("move constructor") {
  {
    auto p1 = make_counted<class0>();
    check_eq(class0_instances, 1);
    class0ptr p2{std::move(p1)};
    check_eq(p1.get(), nullptr);
    check_ne(p2.get(), nullptr);
    check(p2->unique());
    check_eq(class0_instances, 1);
  }
  check_eq(class0_instances, 0);
}

TEST("copy constructor") {
  {
    auto p1 = make_counted<class0>();
    check(p1->unique());
    class0ptr p2{p1};
    check_eq(p1.get(), p2.get());
    check(!p1->unique());
    check(!p2->unique());
    check_eq(class0_instances, 1);
  }
  check_eq(class0_instances, 0);
}

TEST("converting constructor from derived type") {
  {
    auto derived = make_counted<class1>();
    check_eq(class1_instances, 1);
    class0ptr base{std::move(derived)};
    check_eq(derived.get(), nullptr);
    check(base->is_subtype());
    check(base->unique());
    check_eq(class1_instances, 1);
  }
  check_eq(class1_instances, 0);
}

TEST("swap") {
  {
    auto p1 = make_counted<class0>();
    auto p2 = make_counted<class0>();
    auto* raw1 = p1.get();
    auto* raw2 = p2.get();
    check_eq(class0_instances, 2);
    p1.swap(p2);
    check_eq(p1.get(), raw2);
    check_eq(p2.get(), raw1);
    check(p1->unique());
    check(p2->unique());
  }
  check_eq(class0_instances, 0);
}

TEST("detach") {
  SECTION("from non-null pointer") {
    auto ptr = make_counted<class0>();
    auto* raw = ptr.get();
    check_eq(class0_instances, 1);
    auto* detached = ptr.detach();
    check_eq(detached, raw);
    check_eq(ptr.get(), nullptr);
    check_eq(class0_instances, 1);
    detached->deref();
  }
  SECTION("from null pointer") {
    class0ptr ptr;
    auto* detached = ptr.detach();
    check_eq(detached, nullptr);
  }
  check_eq(class0_instances, 0);
}

TEST("release is alias for detach") {
  {
    auto ptr = make_counted<class0>();
    auto* raw = ptr.get();
    auto* released = ptr.release();
    check_eq(released, raw);
    check_eq(ptr.get(), nullptr);
    released->deref();
  }
  check_eq(class0_instances, 0);
}

TEST("emplace") {
  {
    class0ptr ptr;
    ptr.emplace();
    check_ne(ptr.get(), nullptr);
    check(ptr->unique());
    check_eq(class0_instances, 1);
    // emplace again replaces the object
    auto* old_raw = ptr.get();
    ptr.emplace();
    check_ne(ptr.get(), old_raw);
    check(ptr->unique());
    check_eq(class0_instances, 1);
  }
  check_eq(class0_instances, 0);
}

TEST("assignment from nullptr") {
  {
    auto ptr = make_counted<class0>();
    check_eq(class0_instances, 1);
    ptr = nullptr;
    check_eq(ptr.get(), nullptr);
    check_eq(class0_instances, 0);
  }
}

TEST("move assignment") {
  {
    auto p1 = make_counted<class0>();
    auto p2 = make_counted<class0>();
    auto* raw2 = p2.get();
    check_eq(class0_instances, 2);
    p1 = std::move(p2);
    // p1's old object is released, p2's object is moved to p1
    // p2 now holds p1's old object (swap semantics)
    check_eq(p1.get(), raw2);
    check_eq(class0_instances, 2);
  }
  check_eq(class0_instances, 0);
}

TEST("copy assignment") {
  SECTION("from non-null to non-null") {
    auto p1 = make_counted<class0>();
    auto p2 = make_counted<class0>();
    check_eq(class0_instances, 2);
    p1 = p2;
    check_eq(p1.get(), p2.get());
    check(!p1->unique());
    check_eq(class0_instances, 1);
  }
  SECTION("self-assignment") {
    auto p1 = make_counted<class0>();
    auto* raw = p1.get();
    auto& p1_ref = p1;
    p1 = p1_ref;
    check_eq(p1.get(), raw);
    check(p1->unique());
    check_eq(class0_instances, 1);
  }
  check_eq(class0_instances, 0);
}

TEST("pointer access operators") {
  auto ptr = make_counted<class0>();
  SECTION("get returns raw pointer") {
    check_ne(ptr.get(), nullptr);
  }
  SECTION("operator-> returns raw pointer") {
    check_eq(ptr.operator->(), ptr.get());
    check_eq(ptr->is_subtype(), false);
  }
  SECTION("operator* returns reference") {
    check_eq(&(*ptr), ptr.get());
  }
}

TEST("boolean conversion") {
  SECTION("null pointer is false") {
    class0ptr ptr;
    check(!ptr);
    check_eq(static_cast<bool>(ptr), false);
    if (ptr) {
      fail("null pointer should not convert to true");
    }
  }
  SECTION("non-null pointer is true") {
    auto ptr = make_counted<class0>();
    check(!!ptr);
    check_eq(static_cast<bool>(ptr), true);
    if (!ptr) {
      fail("non-null pointer should convert to true");
    }
  }
}

TEST("compare") {
  auto p1 = make_counted<class0>();
  auto p2 = make_counted<class0>();
  SECTION("compare with raw pointer") {
    check_eq(p1.compare(p1.get()), 0);
    check_ne(p1.compare(p2.get()), 0);
  }
  SECTION("compare with intrusive_ptr") {
    check_eq(p1.compare(p1), 0);
    check_ne(p1.compare(p2), 0);
  }
  SECTION("compare with nullptr_t") {
    class0ptr null_ptr;
    check_eq(null_ptr.compare(nullptr), 0);
    check_ne(p1.compare(nullptr), 0);
  }
}

TEST("downcast") {
  SECTION("successful downcast") {
    class0ptr base = make_counted<class1>();
    auto derived = base.downcast<class1>();
    check_ne(derived.get(), nullptr);
    check(!base->unique());
    check(!derived->unique());
    check_eq(class1_instances, 1);
  }
  SECTION("failed downcast") {
    auto base = make_counted<class0>();
    auto derived = base.downcast<class1>();
    check_eq(derived.get(), nullptr);
    check(base->unique());
    check_eq(class0_instances, 1);
  }
  SECTION("downcast from null") {
    class0ptr base;
    auto derived = base.downcast<class1>();
    check_eq(derived.get(), nullptr);
  }
  check_eq(class0_instances, 0);
  check_eq(class1_instances, 0);
}

TEST("upcast") {
  SECTION("lvalue upcast adds reference") {
    auto derived = make_counted<class1>();
    class0ptr base = derived.upcast<class0>();
    check_ne(base.get(), nullptr);
    check(!derived->unique());
    check_eq(base.get(), derived.get());
    check_eq(class1_instances, 1);
  }
  SECTION("rvalue upcast moves ownership") {
    auto derived = make_counted<class1>();
    auto* raw = derived.get();
    class0ptr base = std::move(derived).upcast<class0>();
    check_eq(derived.get(), nullptr);
    check_eq(base.get(), raw);
    check(base->unique());
    check_eq(class1_instances, 1);
  }
  SECTION("upcast from null") {
    class1ptr derived;
    class0ptr base = derived.upcast<class0>();
    check_eq(base.get(), nullptr);
  }
  check_eq(class1_instances, 0);
}

TEST("comparison operators with nullptr") {
  class0ptr null_ptr;
  auto valid_ptr = make_counted<class0>();
  SECTION("operator== with nullptr") {
    check(null_ptr == nullptr);
    check(nullptr == null_ptr);
    check(!(valid_ptr == nullptr));
    check(!(nullptr == valid_ptr));
  }
  SECTION("operator!= with nullptr") {
    check(!(null_ptr != nullptr));
    check(!(nullptr != null_ptr));
    check(valid_ptr != nullptr);
    check(nullptr != valid_ptr);
  }
}

TEST("comparison operators with raw pointer") {
  auto p1 = make_counted<class0>();
  auto p2 = make_counted<class0>();
  SECTION("operator== with raw pointer") {
    check(p1 == p1.get());
    check(p1.get() == p1);
    check(!(p1 == p2.get()));
    check(!(p2.get() == p1));
  }
  SECTION("operator!= with raw pointer") {
    check(!(p1 != p1.get()));
    check(!(p1.get() != p1));
    check(p1 != p2.get());
    check(p2.get() != p1);
  }
}

TEST("comparison operators between intrusive_ptrs") {
  auto p1 = make_counted<class0>();
  auto p2 = make_counted<class0>();
  class0ptr p1_copy = p1;
  SECTION("operator==") {
    check(p1 == p1);
    check(p1 == p1_copy);
    check(!(p1 == p2));
  }
  SECTION("operator!=") {
    check(!(p1 != p1));
    check(!(p1 != p1_copy));
    check(p1 != p2);
  }
  SECTION("operator< provides ordering") {
    // Just verify it compiles and provides consistent ordering
    auto less_result = p1 < p2;
    auto greater_result = p2 < p1;
    // exactly one should be true, or they're equal (which they're not)
    check(less_result != greater_result);
  }
}

TEST("comparison operators with derived type pointers") {
  class0ptr base = make_counted<class0>();
  class1ptr derived = make_counted<class1>();
  // Should compile and compare correctly
  check(base != derived);
  check(!(base == derived));
}

TEST("to_string") {
  auto ptr = make_counted<class0>();
  auto str = to_string(ptr);
  // Should be a hex representation of the pointer
  check(!str.empty());
  // Null pointer should also work
  class0ptr null_ptr;
  auto null_str = to_string(null_ptr);
  check(!null_str.empty());
}

CAF_POP_WARNINGS
