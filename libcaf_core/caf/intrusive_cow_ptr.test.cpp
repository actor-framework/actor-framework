// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/intrusive_cow_ptr.hpp"

#include "caf/test/test.hpp"

#include "caf/config.hpp"

#include <filesystem>
#include <string>

using namespace std::literals;

using namespace caf;

namespace {

size_t class0_instances = 0;

size_t class0_copy_calls = 0;

size_t class1_instances = 0;

size_t class1_copy_calls = 0;

size_t custom_unshare_instances = 0;

size_t custom_unshare_unshare_calls = 0;

class class0;

class class1;

using class0ptr = intrusive_cow_ptr<class0>;

using class1ptr = intrusive_cow_ptr<class1>;

struct subtype_tag {};

class class0 : public ref_counted {
public:
  explicit class0(std::string value)
    : value(std::move(value)), subtype_(false) {
    ++class0_instances;
  }

  class0(const class0& other) : value(other.value), subtype_(other.subtype_) {
    if (!subtype_) {
      ++class0_instances;
    }
  }

  class0& operator=(const class0&) = delete;

  ~class0() noexcept override {
    if (!subtype_) {
      --class0_instances;
    }
  }

  bool is_subtype() const noexcept {
    return subtype_;
  }

  std::string value;

  virtual class0* copy() const {
    ++class0_copy_calls;
    return new class0(*this);
  }

protected:
  explicit class0(subtype_tag, std::string value)
    : value(std::move(value)), subtype_(true) {
    // nop
  }

private:
  bool subtype_;
};

class class1 : public class0 {
public:
  explicit class1(std::string val) : class0(subtype_tag{}, std::move(val)) {
    ++class1_instances;
  }

  class1(const class1& other) : class0(subtype_tag{}, other.value) {
    ++class1_instances;
  }

  class1& operator=(const class1&) = delete;

  ~class1() noexcept override {
    --class1_instances;
  }

  class1* copy() const override {
    ++class1_copy_calls;
    return new class1(*this);
  }
};

class custom_unshare : public ref_counted {
public:
  custom_unshare() {
    ++custom_unshare_instances;
  }

  custom_unshare(const custom_unshare& other) : value(other.value) {
    ++custom_unshare_instances;
  }

  custom_unshare(std::string value) : value(std::move(value)) {
    ++custom_unshare_instances;
  }

  ~custom_unshare() noexcept override {
    --custom_unshare_instances;
  }

  custom_unshare& operator=(const custom_unshare&) = delete;

  std::string value;
};

custom_unshare* intrusive_cow_ptr_unshare(custom_unshare*& ptr) {
  ++custom_unshare_unshare_calls;
  if (ptr->strong_reference_count() != 1) {
    ptr->deref();
    ptr = new custom_unshare(*ptr);
    return ptr;
  }
  return ptr;
}

using custom_unshare_ptr = intrusive_cow_ptr<custom_unshare>;

} // namespace

TEST("make") {
  SECTION("class0") {
    auto uut = class0ptr::make("hello"s);
    check_eq(class0_instances, 1u);
    check_eq(uut->strong_reference_count(), 1u);
    check_eq(uut->value, "hello");
  }
  SECTION("class1") {
    auto uut = class1ptr::make("hello"s);
    check_eq(class1_instances, 1u);
    check_eq(uut->strong_reference_count(), 1u);
    check_eq(uut->value, "hello");
  }
  SECTION("custom_unshare") {
    auto uut = custom_unshare_ptr::make("hello"s);
    check_eq(uut->strong_reference_count(), 1u);
    check_eq(uut->value, "hello");
  }
  check_eq(class0_instances, 0u);
  check_eq(class1_instances, 0u);
}

TEST("unshare") {
  SECTION("class0") {
    SECTION("calling unshare on uniquely referenced object does nothing") {
      auto uut1 = class0ptr::make("hello"s);
      check_eq(class0_instances, 1u);
      uut1.unshare();
      check_eq(class0_copy_calls, 0u);
      check_eq(class0_instances, 1u);
    }
    SECTION("calling unshare on a shared object copies the object") {
      auto uut1 = class0ptr::make("hello"s);
      check_eq(class0_instances, 1u);
      auto uut2 = uut1;
      check_eq(class0_instances, 1u);
      uut2.unshared().value = "world";
      check_eq(class0_instances, 2u);
      check_eq(uut1->value, "hello");
      check_eq(uut2->value, "world");
    }
  }
  SECTION("custom_unshare") {
    SECTION("calling unshare on uniquely referenced object does nothing") {
      auto uut1 = custom_unshare_ptr::make("hello"s);
      check_eq(custom_unshare_instances, 1u);
      check_eq(custom_unshare_unshare_calls, 0u);
      uut1.unshare();
      check_eq(custom_unshare_unshare_calls, 0u);
      check_eq(custom_unshare_instances, 1u);
    }
    SECTION("calling unshare on a shared object copies the object") {
      auto uut1 = custom_unshare_ptr::make("hello"s);
      check_eq(custom_unshare_instances, 1u);
      auto uut2 = uut1;
      check_eq(custom_unshare_instances, 1u);
      check_eq(custom_unshare_unshare_calls, 0u);
      uut2.unshared().value = "world";
      check_eq(custom_unshare_unshare_calls, 1u);
      check_eq(custom_unshare_instances, 2u);
      check_eq(uut1->value, "hello");
      check_eq(uut2->value, "world");
    }
  }
  SECTION("unsharing a pointer to the base class copies the derived object") {
    auto uut1 = class1ptr::make("hello"s);
    check_eq(class1_instances, 1u);
    auto uut2 = class0ptr{uut1};
    check_eq(class1_instances, 1u);
    uut2.unshared().value = "world";
    check_eq(class1_instances, 2u);
    check_eq(uut1->value, "hello");
    check_eq(uut2->value, "world");
  }
}

TEST("emplace") {
  SECTION("same type") {
    class0ptr uut;
    uut.emplace("hello"s);
    check_eq(uut->value, "hello");
    check_eq(uut->strong_reference_count(), 1u);
    check_eq(class0_instances, 1u);
  }
  SECTION("derived type") {
    class0ptr uut;
    uut.emplace<class1>("hello"s);
    check_eq(uut->value, "hello");
    check_eq(uut->strong_reference_count(), 1u);
    check_eq(class1_instances, 1u);
  }
}

TEST("upcast") {
  auto uut1 = class1ptr::make("hello"s);
  auto uut2 = uut1.upcast<class0>();
  check_eq(uut1.get(), uut2.get());
}

#ifdef CAF_ENABLE_RTTI
TEST("downcast") {
  SECTION("valid downcast") {
    auto uut1 = class0ptr{new class1("hello"s), adopt_ref};
    auto uut2 = uut1.downcast<class1>();
    check_eq(uut1.get(), uut2.get());
  }
  SECTION("invalid downcast") {
    auto uut1 = class0ptr::make("hello"s);
    auto uut2 = uut1.downcast<class1>();
    check_eq(uut2.get(), nullptr);
  }
}
#endif

TEST("swap") {
  auto uut1 = class0ptr::make("hello"s);
  auto uut2 = class0ptr::make("world"s);
  uut1.swap(uut2);
  check_eq(uut1->value, "world");
  check_eq(uut2->value, "hello");
}

TEST("compare") {
  SECTION("compare with nullptr") {
    SECTION("null COW pointer") {
      class0ptr uut;
      check_eq(uut, nullptr);
      check_ge(uut, nullptr);
      check_le(nullptr, uut);
    }
    SECTION("non-null COW pointer") {
      auto uut = class0ptr::make("hello"s);
      check_ne(uut, nullptr);
      check_ge(uut, nullptr);
      check_gt(uut, nullptr);
      check_le(nullptr, uut);
      check_lt(nullptr, uut);
    }
  }
  SECTION("compare with other COW pointer") {
    SECTION("both pointing to the same object") {
      auto uut1 = class0ptr::make("hello"s);
      auto uut2 = uut1;
      check_eq(uut1, uut2);
      check_ge(uut1, uut2);
      check_le(uut2, uut1);
    }
    SECTION("pointing to different objects") {
      auto uut1 = class0ptr::make("hello"s);
      auto uut2 = class0ptr::make("world"s);
      if (uut1.get() > uut2.get()) {
        uut1.swap(uut2);
      }
      check_ne(uut1, uut2);
      check_ge(uut2, uut1);
      check_gt(uut2, uut1);
      check_le(uut1, uut2);
      check_lt(uut1, uut2);
    }
  }
}
