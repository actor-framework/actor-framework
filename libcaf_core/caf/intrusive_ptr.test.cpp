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

size_t class0_instances = 0;

size_t class1_instances = 0;

size_t class2_instances = 0;

class class0;

class class1;

class class2;

using class0_ptr = intrusive_ptr<class0>;

using class1_ptr = intrusive_ptr<class1>;

using class2_ptr = intrusive_ptr<class2>;

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

  virtual class0_ptr create() const {
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

  class0_ptr create() const override {
    return make_counted<class1>();
  }
};

void intrusive_ptr_add_ref(const class2*);

void intrusive_ptr_release(const class2*);

class class2 {
public:
  friend void intrusive_ptr_add_ref(const class2*);

  friend void intrusive_ptr_release(const class2*);

  class2() {
    ++class2_instances;
  }

  class2(const class2& other) : value(other.value) {
    ++class2_instances;
  }

  explicit class2(std::string value) : value(std::move(value)) {
    ++class2_instances;
  }

  ~class2() noexcept {
    --class2_instances;
  }

  class2& operator=(const class2&) = delete;

  std::string value;

private:
  mutable detail::atomic_ref_count ref_count_;
};

void intrusive_ptr_add_ref(const class2* ptr) {
  ptr->ref_count_.inc();
}

void intrusive_ptr_release(const class2* ptr) {
  ptr->ref_count_.dec(ptr);
}

} // namespace

TEST("make_counted") {
  {
    auto ptr = make_counted<class0>();
    check_eq(class0_instances, 1u);
    check_eq(ptr->strong_reference_count(), 1u);
  }
  check_eq(class0_instances, 0u);
}

TEST("reset") {
  SECTION("no arguments") {
    auto ptr = make_counted<class0>();
    ptr.reset();
    check_eq(class0_instances, 0u);
    check_eq(ptr.get(), nullptr);
  }
  SECTION("passing pointer and adopt_ref") {
    class0_ptr ptr;
    ptr.reset(new class0, adopt_ref);
    check_eq(class0_instances, 1u);
    check_eq(ptr->strong_reference_count(), 1u);
  }
  SECTION("passing pointer and false") {
    class0_ptr ptr;
    ptr.reset(new class0, false);
    check_eq(class0_instances, 1u);
    check_eq(ptr->strong_reference_count(), 1u);
  }
  SECTION("passing pointer and add_ref") {
    auto* raw_ptr = new class0;
    class0_ptr ptr;
    ptr.reset(raw_ptr, add_ref);
    check_eq(class0_instances, 1u);
    raw_ptr->deref();
    check_eq(ptr->strong_reference_count(), 1u);
  }
  SECTION("passing pointer and true") {
    auto* raw_ptr = new class0;
    class0_ptr ptr;
    ptr.reset(raw_ptr, true);
    check_eq(class0_instances, 1u);
    raw_ptr->deref();
    check_eq(ptr->strong_reference_count(), 1u);
  }
  check_eq(class0_instances, 0u);
}

TEST("default and nullptr construction") {
  SECTION("default constructor creates null pointer") {
    class0_ptr ptr;
    check_eq(ptr.get(), nullptr);
    check(!ptr);
    check_eq(static_cast<bool>(ptr), false);
  }
  SECTION("nullptr_t constructor creates null pointer") {
    class0_ptr ptr{nullptr};
    check_eq(ptr.get(), nullptr);
    check(!ptr);
  }
  check_eq(class0_instances, 0u);
}

TEST("construction with add_ref") {
  auto* raw = new class0;
  {
    class0_ptr ptr{raw, add_ref};
    check_eq(class0_instances, 1u);
    check_eq(ptr->strong_reference_count(), 2u);
  }
  // ptr released one ref, but raw still holds its initial ref
  raw->deref();
  check_eq(class0_instances, 0u);
}

TEST("construction with adopt_ref") {
  {
    class0_ptr ptr{new class0, adopt_ref};
    check_eq(class0_instances, 1u);
    check_eq(ptr->strong_reference_count(), 1u);
  }
  check_eq(class0_instances, 0u);
}

TEST("move constructor") {
  {
    auto ptr1 = make_counted<class0>();
    check_eq(class0_instances, 1u);
    class0_ptr ptr2{std::move(ptr1)};
    check_eq(ptr1.get(), nullptr);
    check_ne(ptr2.get(), nullptr);
    check_eq(ptr2->strong_reference_count(), 1u);
    check_eq(class0_instances, 1u);
  }
  check_eq(class0_instances, 0u);
}

TEST("copy constructor") {
  {
    auto ptr1 = make_counted<class0>();
    check_eq(ptr1->strong_reference_count(), 1u);
    class0_ptr ptr2{ptr1};
    check_eq(ptr1.get(), ptr2.get());
    check_eq(ptr1->strong_reference_count(), 2u);
    check_eq(ptr2->strong_reference_count(), 2u);
    check_eq(class0_instances, 1u);
  }
  check_eq(class0_instances, 0u);
}

TEST("converting constructor from derived type") {
  {
    auto derived = make_counted<class1>();
    check_eq(class1_instances, 1u);
    class0_ptr base{std::move(derived)};
    check_eq(derived.get(), nullptr);
    check(base->is_subtype());
    check_eq(base->strong_reference_count(), 1u);
    check_eq(class1_instances, 1u);
  }
  check_eq(class1_instances, 0u);
}

TEST("swap") {
  {
    auto ptr1 = make_counted<class0>();
    auto ptr2 = make_counted<class0>();
    auto* raw1 = ptr1.get();
    auto* raw2 = ptr2.get();
    check_eq(class0_instances, 2u);
    ptr1.swap(ptr2);
    check_eq(ptr1.get(), raw2);
    check_eq(ptr2.get(), raw1);
    check_eq(ptr1->strong_reference_count(), 1u);
    check_eq(ptr2->strong_reference_count(), 1u);
  }
  check_eq(class0_instances, 0u);
}

TEST("release") {
  SECTION("from non-null pointer") {
    auto ptr = make_counted<class0>();
    auto* raw = ptr.get();
    check_eq(class0_instances, 1u);
    auto* released = ptr.release();
    check_eq(released, raw);
    check_eq(ptr.get(), nullptr);
    check_eq(class0_instances, 1u);
    released->deref();
  }
  SECTION("from null pointer") {
    class0_ptr ptr;
    auto* released = ptr.release();
    check_eq(released, nullptr);
  }
  check_eq(class0_instances, 0u);
}

TEST("emplace") {
  {
    class0_ptr ptr1;
    ptr1.emplace();
    check_ne(ptr1.get(), nullptr);
    check_eq(ptr1->strong_reference_count(), 1u);
    check_eq(class0_instances, 1u);
    // calling emplace again replaces the object
    auto ptr2 = ptr1;
    ptr1.emplace();
    check_ne(ptr1.get(), ptr2.get());
    check_eq(ptr1->strong_reference_count(), 1u);
    check_eq(class0_instances, 2u);
  }
  check_eq(class0_instances, 0u);
}

TEST("assignment from nullptr") {
  {
    auto ptr = make_counted<class0>();
    check_eq(class0_instances, 1u);
    ptr = nullptr;
    check_eq(ptr.get(), nullptr);
    check_eq(class0_instances, 0u);
  }
}

TEST("move assignment") {
  {
    auto ptr1 = make_counted<class0>();
    {
      auto ptr2 = make_counted<class0>();
      check_eq(class0_instances, 2u);
      ptr1 = std::move(ptr2);
    }
    check_eq(class0_instances, 1u);
  }
  check_eq(class0_instances, 0u);
}

TEST("copy assignment") {
  SECTION("from non-null to non-null") {
    auto ptr1 = make_counted<class0>();
    auto ptr2 = make_counted<class0>();
    check_eq(class0_instances, 2u);
    ptr1 = ptr2;
    check_eq(ptr1.get(), ptr2.get());
    check_eq(ptr1->strong_reference_count(), 2u);
    check_eq(class0_instances, 1u);
  }
  SECTION("self-assignment") {
    auto ptr1 = make_counted<class0>();
    auto* raw = ptr1.get();
    auto& ptr1_ref = ptr1;
    ptr1 = ptr1_ref;
    check_eq(ptr1.get(), raw);
    check_eq(ptr1->strong_reference_count(), 1u);
    check_eq(class0_instances, 1u);
  }
  check_eq(class0_instances, 0u);
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
    class0_ptr ptr;
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
  auto ptr1 = make_counted<class0>();
  auto ptr2 = make_counted<class0>();
  SECTION("compare with raw pointer") {
    check_eq(ptr1.compare(ptr1.get()), 0);
    check_ne(ptr1.compare(ptr2.get()), 0);
  }
  SECTION("compare with intrusive_ptr") {
    check_eq(ptr1.compare(ptr1), 0);
    check_ne(ptr1.compare(ptr2), 0);
  }
  SECTION("compare with nullptr_t") {
    class0_ptr null_ptr;
    check_eq(null_ptr.compare(nullptr), 0);
    check_ne(ptr1.compare(nullptr), 0);
  }
}

#ifdef CAF_ENABLE_RTTI
TEST("downcast") {
  SECTION("successful downcast") {
    class0_ptr base = make_counted<class1>();
    auto derived = base.downcast<class1>();
    check_ne(derived.get(), nullptr);
    check_eq(base->strong_reference_count(), 2u);
    check_eq(derived->strong_reference_count(), 2u);
    check_eq(class1_instances, 1u);
  }
  SECTION("failed downcast") {
    auto base = make_counted<class0>();
    auto derived = base.downcast<class1>();
    check_eq(derived.get(), nullptr);
    check_eq(base->strong_reference_count(), 1u);
    check_eq(class0_instances, 1u);
  }
  SECTION("downcast from null") {
    class0_ptr base;
    auto derived = base.downcast<class1>();
    check_eq(derived.get(), nullptr);
  }
  check_eq(class0_instances, 0u);
  check_eq(class1_instances, 0u);
}
#endif

TEST("upcast") {
  SECTION("lvalue upcast adds reference") {
    auto derived = make_counted<class1>();
    class0_ptr base = derived.upcast<class0>();
    check_ne(base.get(), nullptr);
    check_eq(derived->strong_reference_count(), 2u);
    check_eq(base.get(), derived.get());
    check_eq(class1_instances, 1u);
  }
  SECTION("rvalue upcast moves ownership") {
    auto derived = make_counted<class1>();
    auto* raw = derived.get();
    class0_ptr base = std::move(derived).upcast<class0>();
    check_eq(derived.get(), nullptr);
    check_eq(base.get(), raw);
    check_eq(base->strong_reference_count(), 1u);
    check_eq(class1_instances, 1u);
  }
  SECTION("upcast from null") {
    class1_ptr derived;
    class0_ptr base = derived.upcast<class0>();
    check_eq(base.get(), nullptr);
  }
  check_eq(class1_instances, 0u);
}

TEST("comparison operators with nullptr") {
  class0_ptr null_ptr;
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
  auto ptr1 = make_counted<class0>();
  auto ptr2 = make_counted<class0>();
  SECTION("operator== with raw pointer") {
    check(ptr1 == ptr1.get());
    check(ptr1.get() == ptr1);
    check(!(ptr1 == ptr2.get()));
    check(!(ptr2.get() == ptr1));
  }
  SECTION("operator!= with raw pointer") {
    check(!(ptr1 != ptr1.get()));
    check(!(ptr1.get() != ptr1));
    check(ptr1 != ptr2.get());
    check(ptr2.get() != ptr1);
  }
}

TEST("comparison operators between intrusive pointers") {
  auto ptr1 = make_counted<class0>();
  auto ptr2 = make_counted<class0>();
  class0_ptr ptr1_copy = ptr1;
  SECTION("operator==") {
    check(ptr1 == ptr1);
    check(ptr1 == ptr1_copy);
    check(!(ptr1 == ptr2));
  }
  SECTION("operator!=") {
    check(!(ptr1 != ptr1));
    check(!(ptr1 != ptr1_copy));
    check(ptr1 != ptr2);
  }
  SECTION("operator< provides ordering") {
    // Just verify it compiles and provides consistent ordering
    auto less_result = ptr1 < ptr2;
    auto greater_result = ptr2 < ptr1;
    // exactly one should be true, or they're equal (which they're not)
    check(less_result != greater_result);
  }
}

TEST("comparison operators with derived type pointers") {
  SECTION("different objects") {
    class1_ptr derived = make_counted<class1>();
    class0_ptr base = make_counted<class0>();
    check_ne(base, derived);
  }
  SECTION("same objects") {
    class1_ptr derived = make_counted<class1>();
    class0_ptr base = derived;
    check_eq(base, derived);
  }
}

TEST("to_string") {
  SECTION("non-null pointer") {
    auto ptr = make_counted<class0>();
    auto str = to_string(ptr);
    check(!str.empty());
  }
  SECTION("null pointer") {
    class0_ptr ptr;
    auto str = to_string(ptr);
    check(!str.empty());
  }
}

TEST("intrusive_ptr with free functions") {
  auto ptr1 = make_counted<class2>("foo");
  check_eq(class2_instances, 1u);
  auto ptr2 = ptr1;
  check_eq(class2_instances, 1u);
  ptr2.emplace("bar");
  check_eq(class2_instances, 2u);
  check_eq(ptr1->value, "foo");
  check_eq(ptr2->value, "bar");
}

CAF_POP_WARNINGS
