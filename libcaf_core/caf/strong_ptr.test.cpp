// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/strong_ptr.hpp"

#include "caf/test/test.hpp"

#include "caf/detail/build_config.hpp"
#include "caf/detail/control_block_ref_count.hpp"
#include "caf/detail/memory_interface.hpp"

#include <cstddef>

CAF_PUSH_DEPRECATED_WARNING

using namespace caf;

namespace {

int class0_instances = 0;
int class1_instances = 0;

class class0;
class class1;

class ctrl_block {
public:
  static constexpr auto memory_interface
    = detail::memory_interface::aligned_alloc_and_free;

  static constexpr size_t allocation_size = CAF_CACHE_LINE_SIZE;

  static constexpr size_t alignment = CAF_CACHE_LINE_SIZE;

  using managed_type = class0;

  static class0* managed(ctrl_block* ptr) noexcept {
    return reinterpret_cast<class0*>(reinterpret_cast<intptr_t>(ptr)
                                     + allocation_size);
  }

  static ctrl_block* from(const class0* ptr) noexcept {
    return reinterpret_cast<ctrl_block*>(reinterpret_cast<intptr_t>(ptr)
                                         - allocation_size);
  }

  void ref() noexcept {
    ref_count_.inc_strong();
  }

  void deref() noexcept {
    ref_count_.dec_strong(this);
  }

  void ref_weak() noexcept {
    ref_count_.inc_weak();
  }

  void deref_weak() noexcept {
    ref_count_.dec_weak(this);
  }

  bool upgrade_weak() noexcept {
    return ref_count_.upgrade_weak();
  }

  size_t strong_reference_count() const noexcept {
    return ref_count_.strong_reference_count();
  }

  size_t weak_reference_count() const noexcept {
    return ref_count_.weak_reference_count();
  }

  bool delete_managed_object() noexcept;

private:
  detail::control_block_ref_count ref_count_;
};

using class0ptr = strong_ptr<class0>;
using class1ptr = strong_ptr<class1>;

class class0 {
public:
  using control_block_type = ctrl_block;

  explicit class0(bool subtype = false) : subtype_(subtype) {
    if (!subtype) {
      ++class0_instances;
    }
  }

  virtual ~class0() noexcept {
    if (!subtype_) {
      --class0_instances;
    }
  }

  bool is_subtype() const {
    return subtype_;
  }

  virtual class0ptr create() const;

private:
  bool subtype_;
};

bool ctrl_block::delete_managed_object() noexcept {
  auto* ptr = managed(this);
  ptr->~class0();
  return true;
}

class class1 : public class0 {
public:
  class1() : class0(true) {
    ++class1_instances;
  }

  ~class1() override {
    --class1_instances;
  }

  class0ptr create() const override;
};

template <class T>
strong_ptr<T> make_uut() {
  static constexpr size_t alloc_size = ctrl_block::allocation_size + sizeof(T);
  auto* mem = detail::aligned_alloc(ctrl_block::alignment, alloc_size);
  new (mem) ctrl_block();
  auto* obj_mem = reinterpret_cast<std::byte*>(mem)
                  + ctrl_block::allocation_size;
  return {new (obj_mem) T(), adopt_ref};
}

class0ptr class0::create() const {
  return make_uut<class0>();
}

class0ptr class1::create() const {
  return make_uut<class1>();
}

static_assert(class0ptr::is_base_ptr_type);

static_assert(!class1ptr::is_base_ptr_type);

} // namespace

TEST("make_uut") {
  {
    auto ptr = make_uut<class0>();
    check_eq(class0_instances, 1);
    check_eq(ptr.control_block()->strong_reference_count(), 1u);
  }
  check_eq(class0_instances, 0);
}

TEST("reset") {
  SECTION("no arguments") {
    auto ptr = make_uut<class0>();
    ptr.reset();
    check_eq(class0_instances, 0);
    check_eq(ptr.get(), nullptr);
  }
  SECTION("passing pointer and adopt_ref") {
    class0ptr ptr;
    auto ptr2 = make_uut<class0>();
    ptr.reset(ptr2.release(), adopt_ref);
    check_eq(class0_instances, 1);
    check_eq(ptr.control_block()->strong_reference_count(), 1u);
  }
  check_eq(class0_instances, 0);
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
  {
    auto ptr = make_uut<class0>();
    auto ptr2 = class0ptr{ptr.get(), add_ref};
    check_eq(class0_instances, 1);
    check_eq(ptr2.control_block()->strong_reference_count(), 2u);
  }
  // ptr released one ref, but raw still holds its initial ref
  check_eq(class0_instances, 0);
}

TEST("construction with adopt_ref") {
  {
    auto ptr = make_uut<class0>();
    auto ptr2 = class0ptr{ptr.release(), adopt_ref};
    check_eq(class0_instances, 1);
    check_eq(ptr2.control_block()->strong_reference_count(), 1u);
  }
  check_eq(class0_instances, 0);
}

TEST("move constructor") {
  {
    auto p1 = make_uut<class0>();
    check_eq(class0_instances, 1);
    class0ptr p2{std::move(p1)};
    check_eq(p1.get(), nullptr);
    check_ne(p2.get(), nullptr);
    check_eq(p2.control_block()->strong_reference_count(), 1u);
    check_eq(class0_instances, 1);
  }
  check_eq(class0_instances, 0);
}

TEST("copy constructor") {
  {
    auto p1 = make_uut<class0>();
    check_eq(p1.control_block()->strong_reference_count(), 1u);
    class0ptr p2{p1};
    check_eq(p1.get(), p2.get());
    check_eq(p1.control_block()->strong_reference_count(), 2u);
    check_eq(p2.control_block()->strong_reference_count(), 2u);
    check_eq(class0_instances, 1);
  }
  check_eq(class0_instances, 0);
}

TEST("converting constructor from derived type") {
  {
    auto derived = make_uut<class1>();
    check_eq(class1_instances, 1);
    class0ptr base{std::move(derived)};
    check_eq(derived.get(), nullptr);
    check(base->is_subtype());
    check_eq(base.control_block()->strong_reference_count(), 1u);
    check_eq(class1_instances, 1);
  }
  check_eq(class1_instances, 0);
}

TEST("swap") {
  {
    auto p1 = make_uut<class0>();
    auto p2 = make_uut<class0>();
    auto* raw1 = p1.get();
    auto* raw2 = p2.get();
    check_eq(class0_instances, 2);
    p1.swap(p2);
    check_eq(p1.get(), raw2);
    check_eq(p2.get(), raw1);
    check_eq(p1.control_block()->strong_reference_count(), 1u);
    check_eq(p2.control_block()->strong_reference_count(), 1u);
  }
  check_eq(class0_instances, 0);
}

TEST("release") {
  SECTION("from non-null pointer") {
    auto ptr = make_uut<class0>();
    auto* raw = ptr.get();
    check_eq(class0_instances, 1);
    auto* released = ptr.release();
    check_eq(released, raw);
    check_eq(ptr.get(), nullptr);
    check_eq(class0_instances, 1);
    ctrl_block::from(released)->deref();
  }
  SECTION("from null pointer") {
    class0ptr ptr;
    auto* released = ptr.release();
    check_eq(released, nullptr);
  }
  check_eq(class0_instances, 0);
}

TEST("assignment from nullptr") {
  {
    auto ptr = make_uut<class0>();
    check_eq(class0_instances, 1);
    ptr = nullptr;
    check_eq(ptr.get(), nullptr);
    check_eq(class0_instances, 0);
  }
}

TEST("move assignment") {
  {
    auto p1 = make_uut<class0>();
    auto p2 = make_uut<class0>();
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
    auto p1 = make_uut<class0>();
    auto p2 = make_uut<class0>();
    check_eq(class0_instances, 2);
    p1 = p2;
    check_eq(p1.get(), p2.get());
    check_eq(p1.control_block()->strong_reference_count(), 2u);
    check_eq(class0_instances, 1);
  }
  SECTION("self-assignment") {
    auto p1 = make_uut<class0>();
    auto* raw = p1.get();
    auto& p1_ref = p1;
    p1 = p1_ref;
    check_eq(p1.get(), raw);
    check_eq(p1.control_block()->strong_reference_count(), 1u);
    check_eq(class0_instances, 1);
  }
  check_eq(class0_instances, 0);
}

TEST("pointer access operators") {
  auto ptr = make_uut<class0>();
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
    auto ptr = make_uut<class0>();
    check(!!ptr);
    check_eq(static_cast<bool>(ptr), true);
    if (!ptr) {
      fail("non-null pointer should convert to true");
    }
  }
}

TEST("compare") {
  auto p1 = make_uut<class0>();
  auto p2 = make_uut<class0>();
  SECTION("compare with raw pointer") {
    check_eq(p1.compare(p1.get()), 0);
    check_ne(p1.compare(p2.get()), 0);
  }
  SECTION("compare with strong_ptr") {
    check_eq(p1.compare(p1), 0);
    check_ne(p1.compare(p2), 0);
  }
  SECTION("compare with nullptr_t") {
    class0ptr null_ptr;
    check_eq(null_ptr.compare(nullptr), 0);
    check_ne(p1.compare(nullptr), 0);
  }
}

#ifdef CAF_ENABLE_RTTI
TEST("downcast") {
  SECTION("successful downcast") {
    class0ptr base = make_uut<class1>();
    auto derived = base.downcast<class1>();
    check_ne(derived.get(), nullptr);
    check_eq(base.control_block()->strong_reference_count(), 2u);
    check_eq(derived.control_block()->strong_reference_count(), 2u);
    check_eq(class1_instances, 1);
  }
  SECTION("failed downcast") {
    auto base = make_uut<class0>();
    auto derived = base.downcast<class1>();
    check_eq(derived.get(), nullptr);
    check_eq(base.control_block()->strong_reference_count(), 1u);
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
#endif

TEST("upcast") {
  SECTION("lvalue upcast adds reference") {
    auto derived = make_uut<class1>();
    class0ptr base = derived.upcast<class0>();
    check_ne(base.get(), nullptr);
    check_eq(derived.control_block()->strong_reference_count(), 2u);
    check_eq(base.get(), derived.get());
    check_eq(class1_instances, 1);
  }
  SECTION("rvalue upcast moves ownership") {
    auto derived = make_uut<class1>();
    auto* raw = derived.get();
    class0ptr base = std::move(derived).upcast<class0>();
    check_eq(derived.get(), nullptr);
    check_eq(base.get(), raw);
    check_eq(base.control_block()->strong_reference_count(), 1u);
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
  auto valid_ptr = make_uut<class0>();
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
  auto p1 = make_uut<class0>();
  auto p2 = make_uut<class0>();
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

TEST("comparison operators between strong pointers") {
  auto p1 = make_uut<class0>();
  auto p2 = make_uut<class0>();
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
  class0ptr base = make_uut<class0>();
  class1ptr derived = make_uut<class1>();
  // Should compile and compare correctly
  check(base != derived);
  check(!(base == derived));
}

TEST("to_string") {
  auto ptr = make_uut<class0>();
  auto str = to_string(ptr);
  // Should be a hex representation of the pointer
  check(!str.empty());
  // Null pointer should also work
  class0ptr null_ptr;
  auto null_str = to_string(null_ptr);
  check(!null_str.empty());
}

CAF_POP_WARNINGS
