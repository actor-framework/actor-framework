// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/weak_intrusive_ptr.hpp"

#include "caf/test/test.hpp"

#include "caf/adopt_ref.hpp"
#include "caf/config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/control_block_ref_count.hpp"
#include "caf/detail/control_block_traits.hpp"
#include "caf/detail/memory_interface.hpp"
#include "caf/intrusive_ptr.hpp"

#include <cstddef>
#include <string>

using namespace std::literals;

CAF_PUSH_DEPRECATED_WARNING

using namespace caf;

namespace {

template <class T>
class custom_control_block {
public:
  /// Specifies the memory interface used to allocate the actor control block.
  static constexpr auto memory_interface
    = detail::memory_interface::aligned_alloc_and_free;

  static constexpr size_t allocation_size = CAF_CACHE_LINE_SIZE;

  static constexpr size_t alignment = CAF_CACHE_LINE_SIZE;

  using managed_type = T;

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

private:
  detail::control_block_ref_count ref_count_;
};

template <class T, class... Args>
intrusive_ptr<T> make_custom(Args&&... args) {
  using managed_type = typename T::managed_type;
  using control_block_type = custom_control_block<managed_type>;
  using traits = detail::control_block_traits<control_block_type>;
  auto* mem = traits::template allocate<T>();
  traits::construct_ctrl(mem);
  auto* ptr
    = traits::template construct_managed<T>(mem, std::forward<Args>(args)...);
  return intrusive_ptr<T>(ptr, adopt_ref);
}

size_t class0_instances = 0;

size_t class1_instances = 0;

class class0;

class class1;

class class2;

} // namespace

namespace caf {

template <>
struct weak_intrusive_ptr_traits<custom_control_block<class0>> {
  using managed_type = class0;
  using control_block_type = custom_control_block<class0>;
};

template <>
struct weak_intrusive_ptr_traits<class0> {
  using managed_type = class0;
  using control_block_type = custom_control_block<class0>;
};

template <>
struct weak_intrusive_ptr_traits<class1> {
  using managed_type = class0;
  using control_block_type = custom_control_block<class0>;
};

template <>
struct weak_intrusive_ptr_traits<class2> {
  using managed_type = class2;
  using control_block_type = custom_control_block<class2>;
};

} // namespace caf

namespace {

using class0_ptr = intrusive_ptr<class0>;

using class1_ptr = intrusive_ptr<class1>;

using class2_ptr = intrusive_ptr<class2>;

using class0_weak_ptr = weak_intrusive_ptr<class0>;

using class1_weak_ptr = weak_intrusive_ptr<class1>;

using class2_weak_ptr = weak_intrusive_ptr<class2>;

class class0 {
public:
  using managed_type = class0;

  using control_block_type = custom_control_block<class0>;

  explicit class0(std::string val) : value(std::move(val)), subtype_(false) {
    ++class0_instances;
  }

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

  void ref() const noexcept {
    ctrl()->ref();
  }

  /// Decrements the strong reference count of this actor.
  void deref() const noexcept {
    ctrl()->deref();
  }

  size_t strong_reference_count() const noexcept {
    return ctrl()->strong_reference_count();
  }

  size_t weak_reference_count() const noexcept {
    return ctrl()->weak_reference_count();
  }

  custom_control_block<class0>* ctrl() const noexcept {
    using traits = detail::control_block_traits<custom_control_block<class0>>;
    return traits::ctrl_ptr(this);
  }

  virtual class0_ptr create() const {
    return make_custom<class0>();
  }

  std::string value;

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
    return make_custom<class1>();
  }
};

class class2 {};

} // namespace

// Comparable to itself.
static_assert(detail::has_compare_overload<class0_weak_ptr, class0_weak_ptr>);

// Pointers between base and derived types are comparable.
static_assert(detail::has_compare_overload<class1_weak_ptr, class0_weak_ptr>);

// Pointers between base and derived types are comparable.
static_assert(detail::has_compare_overload<class0_weak_ptr, class1_weak_ptr>);

// Weak pointers are comparable to intrusive pointers.
static_assert(detail::has_compare_overload<class0_weak_ptr, class0_ptr>);

// Weak pointers are comparable to intrusive pointers.
static_assert(detail::has_compare_overload<class0_weak_ptr, class1_ptr>);

// Weak pointers are comparable to intrusive pointers.
static_assert(detail::has_compare_overload<class1_weak_ptr, class0_ptr>);

// Unrelated pointers are not comparable.
static_assert(!detail::has_compare_overload<class0_weak_ptr, class2_weak_ptr>);

// Unrelated pointers are not comparable.
static_assert(!detail::has_compare_overload<class0_weak_ptr, class2_ptr>);

// Unrelated pointers are not comparable.
static_assert(!detail::has_compare_overload<class2_weak_ptr, class0_weak_ptr>);

// Unrelated pointers are not comparable.
static_assert(!detail::has_compare_overload<class2_weak_ptr, class0_ptr>);

TEST("default constructor") {
  class0_weak_ptr ptr;
  check_eq(ptr, nullptr);
  check(!ptr);
}

TEST("construction from nullptr") {
  class0_weak_ptr ptr{nullptr};
  check_eq(ptr, nullptr);
  check(!ptr);
}

TEST("construction from intrusive_ptr") {
  auto ptr = make_custom<class0>();
  check_eq(class0_instances, 1u);
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 1u);
  {
    auto wptr = class0_weak_ptr{ptr};
    check_eq(ptr->strong_reference_count(), 1u);
    check_eq(ptr->weak_reference_count(), 2u);
  }
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 1u);
}

TEST("weak pointers can promote to strong pointers") {
  auto ptr1 = make_custom<class0>();
  check_eq(class0_instances, 1u);
  check_eq(ptr1->strong_reference_count(), 1u);
  check_eq(ptr1->weak_reference_count(), 1u);
  auto wptr = class0_weak_ptr{ptr1};
  check_eq(ptr1->strong_reference_count(), 1u);
  check_eq(ptr1->weak_reference_count(), 2u);
  auto ptr2 = wptr.lock();
  check_eq(ptr1->strong_reference_count(), 2u);
  check_eq(ptr1->weak_reference_count(), 2u);
}

TEST("weak pointers can expire") {
  auto ptr = make_custom<class0>();
  check_eq(class0_instances, 1u);
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 1u);
  auto wptr = class0_weak_ptr{ptr};
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 2u);
  ptr.reset();
  check_eq(class0_instances, 0u);
  check_eq(wptr.lock(), nullptr);
}

TEST("reset") {
  auto ptr = make_custom<class0>();
  auto wptr = class0_weak_ptr{ptr};
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 2u);
  wptr.reset();
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 1u);
  check_eq(wptr, nullptr);
}

TEST("construction with add_ref") {
  auto ptr = make_custom<class0>();
  auto wptr = class0_weak_ptr{ptr->ctrl(), add_ref};
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 2u);
}

TEST("construction with adopt_ref") {
  auto ptr = make_custom<class0>();
  auto wptr1 = class0_weak_ptr{ptr};
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 2u);
  auto* ctrl = ptr->ctrl();
  ctrl->ref_weak();
  auto wptr2 = class0_weak_ptr{ctrl, adopt_ref};
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 3u);
}

TEST("move constructor") {
  auto ptr = make_custom<class0>();
  auto wptr1 = class0_weak_ptr{ptr};
  auto wptr2 = class0_weak_ptr{std::move(wptr1)};
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 2u);
}

TEST("copy constructor") {
  auto ptr = make_custom<class0>();
  auto wptr1 = class0_weak_ptr{ptr};
  auto wptr2 = class0_weak_ptr{wptr1};
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 3u);
}

TEST("converting constructor from derived type") {
  auto ptr = make_custom<class1>();
  auto derived = class1_weak_ptr{ptr};
  auto base = class0_weak_ptr{std::move(derived)};
  check_eq(derived, nullptr);
  check_ne(base, nullptr);
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 2u);
  check_eq(class0_instances, 0u);
  check_eq(class1_instances, 1u);
}

TEST("swap") {
  auto ptr1 = make_custom<class0>("foo"s);
  auto ptr2 = make_custom<class0>("bar"s);
  check_eq(ptr1->value, "foo");
  check_eq(ptr2->value, "bar");
  auto wptr1 = class0_weak_ptr{ptr1};
  auto wptr2 = class0_weak_ptr{ptr2};
  wptr1.swap(wptr2);
  check_eq(wptr1.lock()->value, "bar"s);
  check_eq(wptr2.lock()->value, "foo"s);
}

TEST("assignment from nullptr") {
  auto ptr = make_custom<class0>();
  check_eq(class0_instances, 1u);
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 1u);
  auto wptr = class0_weak_ptr{ptr};
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 2u);
  wptr = nullptr;
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 1u);
}

TEST("move assignment") {
  auto ptr1 = make_custom<class0>();
  auto ptr2 = make_custom<class0>();
  check_eq(class0_instances, 2u);
  auto wptr1 = class0_weak_ptr{ptr1};
  auto wptr2 = class0_weak_ptr{ptr2};
  auto* ctrl1 = ptr1->ctrl();
  auto* ctrl2 = ptr2->ctrl();
  wptr1 = std::move(wptr2);
  check_eq(wptr2.ctrl(), ctrl1);
  check_eq(wptr1.ctrl(), ctrl2);
  check_eq(ptr1->weak_reference_count(), 2u);
  check_eq(ptr2->weak_reference_count(), 2u);
}

TEST("copy assignment") {
  SECTION("from non-null to non-null") {
    auto ptr1 = make_custom<class0>();
    auto ptr2 = make_custom<class0>();
    check_eq(class0_instances, 2u);
    auto wptr1 = class0_weak_ptr{ptr1};
    auto wptr2 = class0_weak_ptr{ptr2};
    wptr1 = wptr2;
    check_eq(wptr1.ctrl(), wptr2.ctrl());
    check_eq(ptr1->weak_reference_count(), 1u);
    check_eq(ptr2->weak_reference_count(), 3u);
  }
  SECTION("self-assignment") {
    auto ptr = make_custom<class0>();
    auto wptr = class0_weak_ptr{ptr};
    auto& wptr_ref = wptr;
    auto* ctrl = wptr.ctrl();
    wptr = wptr_ref;
    check_eq(wptr.ctrl(), ctrl);
    check_eq(ptr->weak_reference_count(), 2u);
  }
}

TEST("boolean conversion") {
  SECTION("null pointer is false") {
    class0_weak_ptr wptr;
    check(!wptr);
    check_eq(static_cast<bool>(wptr), false);
  }
  SECTION("non-null pointer is true") {
    auto ptr = make_custom<class0>();
    auto wptr = class0_weak_ptr{ptr};
    check_eq(static_cast<bool>(wptr), true);
  }
}

TEST("compare") {
  auto ptr1 = make_custom<class0>();
  auto ptr2 = make_custom<class0>();
  auto wptr1 = class0_weak_ptr{ptr1};
  auto wptr2 = class0_weak_ptr{ptr2};
  SECTION("compare with raw pointer") {
    check_eq(wptr1.compare(ptr1->ctrl()), 0);
    check_ne(wptr1.compare(ptr2->ctrl()), 0);
  }
  SECTION("compare with intrusive_ptr") {
    check_eq(wptr1.compare(ptr1), 0);
    check_ne(wptr1.compare(ptr2), 0);
  }
  SECTION("compare with weak_intrusive_ptr") {
    check_eq(wptr1.compare(wptr1), 0);
    check_ne(wptr1.compare(wptr2), 0);
  }
  SECTION("compare with nullptr_t") {
    class0_weak_ptr null_wptr;
    check_eq(null_wptr.compare(nullptr), 0);
    check_ne(wptr1.compare(nullptr), 0);
  }
}

TEST("lock returns same object when alive") {
  auto ptr = make_custom<class1>();
  auto derived_wptr = class1_weak_ptr{ptr};
  auto strong = derived_wptr.lock();
  check_ne(strong.get(), nullptr);
  check_eq(strong.get(), ptr.get());
  check_eq(ptr->strong_reference_count(), 2u);
}

TEST("comparison operators with nullptr") {
  class0_weak_ptr null_ptr;
  auto strong = make_custom<class0>();
  auto valid_ptr = class0_weak_ptr{strong};
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

TEST("comparison operators between weak pointers") {
  auto ptr1 = make_custom<class0>();
  auto ptr2 = make_custom<class0>();
  class0_weak_ptr ptr1_copy = class0_weak_ptr{ptr1};
  auto wptr1 = class0_weak_ptr{ptr1};
  auto wptr2 = class0_weak_ptr{ptr2};
  SECTION("operator==") {
    check(wptr1 == wptr1);
    check(wptr1 == ptr1_copy);
    check(!(wptr1 == wptr2));
  }
  SECTION("operator!=") {
    check(!(wptr1 != wptr1));
    check(!(wptr1 != ptr1_copy));
    check(wptr1 != wptr2);
  }
  SECTION("operator< provides ordering") {
    auto less_result = wptr1 < wptr2;
    auto greater_result = wptr2 < wptr1;
    check(less_result != greater_result);
  }
}

TEST("comparison operators with derived type weak pointers") {
  SECTION("different objects") {
    class1_weak_ptr derived = class1_weak_ptr{make_custom<class1>()};
    class0_weak_ptr base = class0_weak_ptr{make_custom<class0>()};
    check_ne(base.ctrl(), derived.ctrl());
  }
  SECTION("same objects") {
    auto strong = make_custom<class1>();
    class1_weak_ptr derived{strong};
    class0_ptr upcasted = strong;
    class0_weak_ptr base{upcasted};
    check_eq(base.ctrl(), derived.ctrl());
  }
}

TEST("hash") {
  auto ptr = make_custom<class0>();
  auto wptr1 = class0_weak_ptr{ptr};
  auto wptr2 = class0_weak_ptr{ptr};
  check_eq(wptr1.hash(), wptr2.hash());
}

TEST("weak pointer may point to the control block explicitly") {
  using ptr_type = weak_intrusive_ptr<custom_control_block<class0>>;
  auto ptr1 = make_custom<class0>();
  check_eq(ptr1->strong_reference_count(), 1u);
  check_eq(ptr1->weak_reference_count(), 1u);
  auto wptr1 = ptr_type{ptr1};
  check_eq(ptr1->strong_reference_count(), 1u);
  check_eq(ptr1->weak_reference_count(), 2u);
  auto wptr2 = class0_weak_ptr{ptr1};
  check_eq(ptr1->strong_reference_count(), 1u);
  check_eq(ptr1->weak_reference_count(), 3u);
  check_eq(ptr1, wptr1);
  check_eq(wptr1, ptr1);
  check_eq(ptr1, wptr2);
  check_eq(wptr2, ptr1);
  check_eq(wptr1, wptr2);
  check_eq(wptr2, wptr1);
  check_eq(wptr1.hash(), wptr2.hash());
  // have wptr2 point to a different object
  auto ptr2 = make_custom<class0>();
  wptr2 = ptr2;
  check_ne(ptr1, wptr2);
  check_ne(wptr2, ptr1);
  check_ne(wptr1, wptr2);
  check_ne(wptr2, wptr1);
}

TEST("weak and strong pointers preserve ordering") {
  std::vector<class0_ptr> ptrs;
  ptrs.push_back(make_custom<class0>());
  ptrs.push_back(make_custom<class0>());
  ptrs.push_back(make_custom<class0>());
  std::sort(ptrs.begin(), ptrs.end());
  std::vector<class0_weak_ptr> wptrs;
  for (auto& ptr : ptrs) {
    wptrs.emplace_back(ptr);
  }
  check_lt(wptrs[0], wptrs[1]);
  check_lt(wptrs[1], wptrs[2]);
}

TEST("to_string") {
  auto ptr1 = make_custom<class0>();
  auto ptr2 = intrusive_ptr<custom_control_block<class0>>{ptr1->ctrl()};
  auto wptr = weak_intrusive_ptr<custom_control_block<class0>>{ptr2};
  check_eq(ptr1, wptr);
  check_eq(wptr, ptr1);
  check_eq(ptr2, wptr);
  check_eq(wptr, ptr2);
  check_eq(ptr2.hash(), wptr.hash());
  check_eq(to_string(ptr2), to_string(wptr));
}

CAF_POP_WARNINGS
