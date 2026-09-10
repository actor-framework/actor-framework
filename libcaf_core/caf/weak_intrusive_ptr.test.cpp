// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/weak_intrusive_ptr.hpp"

#include "caf/test/test.hpp"

#include "caf/adopt_ref.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/detail/control_block_ref_count.hpp"
#include "caf/intrusive_ptr.hpp"

#include <cstddef>
#include <memory_resource>
#include <new>
#include <string>

using namespace std::literals;

using namespace caf;

namespace {

class class0;

class class1;

class class2;

template <class Managed>
class custom_control_block {
public:
  virtual ~custom_control_block() noexcept {
    // nop
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

  virtual void destroy_managed() noexcept = 0;

  virtual void delete_this() noexcept = 0;

  virtual Managed* managed() noexcept = 0;

protected:
  detail::control_block_ref_count ref_count_;
};

template <class T, class Base = T>
class custom_control_block_impl final : public custom_control_block<Base> {
public:
  using super = custom_control_block<Base>;

  template <class... Args>
  custom_control_block_impl(std::pmr::memory_resource* res, Args&&... args)
    : resource_(res) {
    new (&storage_.managed) T(this, std::forward<Args>(args)...);
  }

  ~custom_control_block_impl() noexcept override {
    // nop; managed object destroyed in destroy_managed()
  }

  void destroy_managed() noexcept override {
    storage_.managed.~T();
  }

  void delete_this() noexcept override {
    auto* res = resource_;
    this->~custom_control_block_impl();
    res->deallocate(this, sizeof(custom_control_block_impl),
                    alignof(custom_control_block_impl));
  }

  T* managed() noexcept override {
    return &storage_.managed;
  }

private:
  struct storage {
    union {
      T managed;
    };

    storage() noexcept {
      // nop
    }

    ~storage() noexcept {
      // nop
    }
  };

  storage storage_;
  std::pmr::memory_resource* resource_;
};

using class0_ctrl = custom_control_block<class0>;

using class2_ctrl = custom_control_block<class2>;

size_t class0_instances = 0;

size_t class1_instances = 0;

} // namespace

namespace caf {

template <>
struct weak_intrusive_ptr_traits<class0_ctrl> {
  using managed_type = class0;
  using control_block_type = class0_ctrl;
};

template <>
struct weak_intrusive_ptr_traits<class2_ctrl> {
  using managed_type = class2;
  using control_block_type = class2_ctrl;
};

} // namespace caf

namespace {

using class0_ptr = intrusive_ptr<class0_ctrl>;

using class2_ptr = intrusive_ptr<class2_ctrl>;

using class0_weak_ptr = weak_intrusive_ptr<class0_ctrl>;

using class2_weak_ptr = weak_intrusive_ptr<class2_ctrl>;

class class0 {
public:
  using control_block_type = custom_control_block<class0>;

  explicit class0(control_block_type* ctrl, std::string val)
    : value(std::move(val)), ctrl_(ctrl), subtype_(false) {
    ++class0_instances;
  }

  explicit class0(control_block_type* ctrl, bool subtype = false)
    : ctrl_(ctrl), subtype_(subtype) {
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

  control_block_type* ctrl() const noexcept {
    return ctrl_;
  }

  void ref() const noexcept {
    ctrl_->ref();
  }

  void deref() const noexcept {
    ctrl_->deref();
  }

  std::string value;

private:
  control_block_type* ctrl_;
  bool subtype_;
};

class class1 : public class0 {
public:
  explicit class1(control_block_type* ctrl) : class0(ctrl, true) {
    ++class1_instances;
  }

  ~class1() override {
    --class1_instances;
  }
};

template <class T, class... Args>
auto make_custom(Args&&... args) {
  static_assert(std::is_same_v<T, class0> || std::is_same_v<T, class1>);
  using block = custom_control_block_impl<T, class0>;
  auto* res = std::pmr::get_default_resource();
  auto* mem = res->allocate(sizeof(block), alignof(block));
  auto* ctrl = new (mem) block(res, std::forward<Args>(args)...);
  return intrusive_ptr<class0_ctrl>(ctrl, adopt_ref);
}

} // namespace

// Comparable to itself.
static_assert(detail::has_compare_overload<class0_weak_ptr, class0_weak_ptr>);

// Weak pointers are comparable to intrusive pointers.
static_assert(detail::has_compare_overload<class0_weak_ptr, class0_ptr>);

// class2 pointers are not comparable.
static_assert(!detail::has_compare_overload<class0_weak_ptr, class2_weak_ptr>);

// class2 pointers are not comparable.
static_assert(!detail::has_compare_overload<class0_weak_ptr, class2_ptr>);

// class2 pointers are not comparable.
static_assert(!detail::has_compare_overload<class2_weak_ptr, class0_weak_ptr>);

// class2 pointers are not comparable.
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
  SECTION("from null strong pointer") {
    class0_ptr ptr;
    auto wptr = class0_weak_ptr{ptr};
    check_eq(wptr, nullptr);
    check(!wptr);
  }
  SECTION("from managed type") {
    using ptr_t = intrusive_ptr<class0>;
    auto ptr = ptr_t{make_custom<class0>()->managed(), add_ref};
    check_eq(class0_instances, 1u);
    check_eq(ptr->ctrl()->strong_reference_count(), 1u);
    check_eq(ptr->ctrl()->weak_reference_count(), 1u);
    {
      auto wptr = class0_weak_ptr{ptr};
      check_eq(ptr->ctrl()->strong_reference_count(), 1u);
      check_eq(ptr->ctrl()->weak_reference_count(), 2u);
    }
    check_eq(ptr->ctrl()->strong_reference_count(), 1u);
    check_eq(ptr->ctrl()->weak_reference_count(), 1u);
  }
  SECTION("from control block type") {
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
  auto wptr = class0_weak_ptr{ptr.get(), add_ref};
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 2u);
}

TEST("construction with adopt_ref") {
  auto ptr = make_custom<class0>();
  auto wptr1 = class0_weak_ptr{ptr};
  check_eq(ptr->strong_reference_count(), 1u);
  check_eq(ptr->weak_reference_count(), 2u);
  auto* ctrl = ptr.get();
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

TEST("converting constructor from derived managed type") {
  auto ptr = make_custom<class1>();
  auto derived = class0_weak_ptr{ptr};
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
  check_eq(ptr1->managed()->value, "foo");
  check_eq(ptr2->managed()->value, "bar");
  auto wptr1 = class0_weak_ptr{ptr1};
  auto wptr2 = class0_weak_ptr{ptr2};
  wptr1.swap(wptr2);
  check_eq(wptr1.lock()->managed()->value, "bar"s);
  check_eq(wptr2.lock()->managed()->value, "foo"s);
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
  auto* ctrl1 = ptr1.get();
  auto* ctrl2 = ptr2.get();
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
    check_eq(wptr1.compare(ptr1.get()), 0);
    check_ne(wptr1.compare(ptr2.get()), 0);
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
  auto wptr = class0_weak_ptr{ptr};
  auto strong = wptr.lock();
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

TEST("comparison operators with derived managed type") {
  SECTION("different objects") {
    class0_weak_ptr derived{make_custom<class1>()};
    class0_weak_ptr base{make_custom<class0>()};
    check_ne(base.ctrl(), derived.ctrl());
  }
  SECTION("same objects") {
    auto strong = make_custom<class1>();
    class0_weak_ptr derived{strong};
    class0_weak_ptr base{strong};
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
  auto ptr1 = make_custom<class0>();
  check_eq(ptr1->strong_reference_count(), 1u);
  check_eq(ptr1->weak_reference_count(), 1u);
  auto wptr1 = class0_weak_ptr{ptr1.get(), add_ref};
  check_eq(ptr1->strong_reference_count(), 1u);
  check_eq(ptr1->weak_reference_count(), 2u);
  auto wptr2 = class0_weak_ptr{ptr1};
  check_eq(ptr1->strong_reference_count(), 1u);
  check_eq(ptr1->weak_reference_count(), 3u);
  check_eq(ptr1.get(), wptr1.ctrl());
  check_eq(wptr1.ctrl(), wptr2.ctrl());
  check_eq(wptr1, wptr2);
  check_eq(wptr1.hash(), wptr2.hash());
  // have wptr2 point to a different object
  auto ptr2 = make_custom<class0>();
  wptr2 = ptr2;
  check_ne(wptr1.ctrl(), wptr2.ctrl());
}

TEST("weak and strong pointers use the same ordering") {
  std::vector<class0_ptr> ptrs;
  ptrs.push_back(make_custom<class0>());
  ptrs.push_back(make_custom<class0>());
  ptrs.push_back(make_custom<class0>());
  std::sort(ptrs.begin(), ptrs.end());
  check_lt(ptrs[0], ptrs[1]);
  check_lt(ptrs[1], ptrs[2]);
  std::vector<class0_weak_ptr> wptrs;
  for (auto& ptr : ptrs) {
    wptrs.emplace_back(ptr);
  }
  check_lt(wptrs[0], wptrs[1]);
  check_lt(wptrs[1], wptrs[2]);
}

TEST("to_string") {
  auto ptr1 = make_custom<class0>();
  auto ptr2 = class0_ptr{ptr1.get(), add_ref};
  auto wptr = class0_weak_ptr{ptr2};
  check_eq(ptr1.get(), wptr.ctrl());
  check_eq(ptr2.get(), wptr.ctrl());
  check_eq(ptr2.hash(), wptr.hash());
  check_eq(to_string(ptr2), to_string(wptr));
}
