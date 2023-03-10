// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE expected

#include "caf/expected.hpp"

#include "core-test.hpp"

#include "caf/sec.hpp"

#include <cassert>

using namespace caf;
using namespace std::literals;

namespace {

class counted_int : public ref_counted {
public:
  explicit counted_int(int initial_value) : value_(initial_value) {
    // nop
  }

  int value() const {
    return value_;
  }

  void value(int new_value) {
    value_ = new_value;
  }

private:
  int value_;
};

using counted_int_ptr = intrusive_ptr<counted_int>;

using e_int = expected<int>;
using e_str = expected<std::string>;
using e_void = expected<void>;
using e_iptr = expected<counted_int_ptr>;

} // namespace

TEST_CASE("expected reports its status via has_value() or operator bool()") {
  e_int x{42};
  CHECK(x);
  CHECK(x.has_value());
  e_int y{sec::runtime_error};
  CHECK(!y);
  CHECK(!y.has_value());
}

TEST_CASE("an expected exposed its value via value()") {
  auto i = make_counted<counted_int>(42);
  SUBCASE("mutable lvalue access") {
    auto ex = e_iptr{i};
    auto val = counted_int_ptr{ex.value()}; // must make a copy
    CHECK_EQ(val, i);
    if (CHECK(ex.has_value()))
      CHECK_EQ(ex.value(), i);
    e_void ev;
    ev.value(); // fancy no-op
  }
  SUBCASE("const lvalue access") {
    const auto ex = e_iptr{i};
    auto val = counted_int_ptr{ex.value()}; // must make a copy
    CHECK_EQ(val, i);
    if (CHECK(ex.has_value()))
      CHECK_EQ(ex.value(), i);
    const e_void ev;
    ev.value(); // fancy no-op
  }
  SUBCASE("mutable rvalue access") {
    auto ex = e_iptr{i};
    auto val = counted_int_ptr{std::move(ex).value()}; // must move the value
    CHECK_EQ(val, i);
    if (CHECK(ex.has_value()))
      CHECK_EQ(ex.value(), nullptr);
    e_void ev;
    std::move(ev).value(); // fancy no-op
  }
  SUBCASE("const rvalue access") {
    const auto ex = e_iptr{i};
    auto val = counted_int_ptr{std::move(ex).value()}; // must make a copy
    CHECK_EQ(val, i);
    if (CHECK(ex.has_value()))
      CHECK_EQ(ex.value(), i);
    const e_void ev;
    std::move(ev).value(); // fancy no-op
  }
#ifdef CAF_ENABLE_EXCEPTIONS
  SUBCASE("value() throws if has_value() would return false") {
    auto val = e_iptr{sec::runtime_error};
    const auto& cval = val;
    CHECK_THROWS_AS(val.value(), std::runtime_error);
    CHECK_THROWS_AS(std::move(val).value(), std::runtime_error);
    CHECK_THROWS_AS(cval.value(), std::runtime_error);
    CHECK_THROWS_AS(std::move(cval).value(), std::runtime_error);
  }
#endif
}

TEST_CASE("an expected exposed its value via operator*()") {
  auto i = make_counted<counted_int>(42);
  SUBCASE("mutable lvalue access") {
    auto ex = e_iptr{i};
    auto val = counted_int_ptr{*ex}; // must make a copy
    CHECK_EQ(val, i);
    if (CHECK(ex.has_value()))
      CHECK_EQ(*ex, i);
    e_void ev;
    *ev; // fancy no-op
  }
  SUBCASE("const lvalue access") {
    const auto ex = e_iptr{i};
    auto val = counted_int_ptr{*ex}; // must make a copy
    CHECK_EQ(val, i);
    if (CHECK(ex.has_value()))
      CHECK_EQ(*ex, i);
    const e_void ev;
    *ev; // fancy no-op
  }
  SUBCASE("mutable rvalue access") {
    auto ex = e_iptr{i};
    auto val = counted_int_ptr{*std::move(ex)}; // must move the value
    CHECK_EQ(val, i);
    if (CHECK(ex.has_value()))
      CHECK_EQ(*ex, nullptr);
    e_void ev;
    *std::move(ev); // fancy no-op
  }
  SUBCASE("const rvalue access") {
    const auto ex = e_iptr{i};
    auto val = counted_int_ptr{*std::move(ex)}; // must make a copy
    CHECK_EQ(val, i);
    if (CHECK(ex.has_value()))
      CHECK_EQ(*ex, i);
    const e_void ev;
    *std::move(ev); // fancy no-op
  }
}

TEST_CASE("an expected exposed its value via operator->()") {
  SUBCASE("mutable lvalue access") {
    auto val = e_str{std::in_place, "foo"};
    CHECK_EQ(val->c_str(), "foo"s);
    val->push_back('!');
    CHECK_EQ(val->c_str(), "foo!"s);
  }
  SUBCASE("const lvalue access") {
    const auto val = e_str{std::in_place, "foo"};
    CHECK_EQ(val->c_str(), "foo"s);
    using pointer_t = decltype(val.operator->());
    static_assert(std::is_same_v<pointer_t, const std::string*>);
  }
}

TEST_CASE("value_or() returns the stored value or a fallback") {
  auto i = make_counted<counted_int>(42);
  auto j = make_counted<counted_int>(24);
  SUBCASE("lvalue access with value") {
    auto val = e_iptr{i};
    auto k = counted_int_ptr{val.value_or(j)};
    CHECK_EQ(val, i);
    CHECK_EQ(k, i);
  }
  SUBCASE("lvalue access with error") {
    auto val = e_iptr{sec::runtime_error};
    auto k = counted_int_ptr{val.value_or(j)};
    CHECK_EQ(val, error{sec::runtime_error});
    CHECK_EQ(k, j);
  }
  SUBCASE("rvalue access with value") {
    auto val = e_iptr{i};
    auto k = counted_int_ptr{std::move(val).value_or(j)};
    CHECK_EQ(val, nullptr);
    CHECK_EQ(k, i);
  }
  SUBCASE("rvalue access with error") {
    auto val = e_iptr{sec::runtime_error};
    auto k = counted_int_ptr{std::move(val).value_or(j)};
    CHECK_EQ(val, error{sec::runtime_error});
    CHECK_EQ(k, j);
  }
}

TEST_CASE("emplace destroys an old value or error and constructs a new value") {
  SUBCASE("non-void value type") {
    e_int x{42};
    CHECK_EQ(x.value(), 42);
    x.emplace(23);
    CHECK_EQ(x.value(), 23);
    e_int y{sec::runtime_error};
    CHECK(!y);
    y.emplace(23);
    CHECK_EQ(y.value(), 23);
  }
  SUBCASE("void value type") {
    e_void x;
    CHECK(x.has_value());
    x.emplace();
    CHECK(x.has_value());
    e_void y{sec::runtime_error};
    CHECK(!y);
    y.emplace();
    CHECK(y);
  }
}

TEST_CASE("swap exchanges the content of two expected") {
  SUBCASE("lhs: value, rhs: value") {
    e_str lhs{std::in_place, "this is value 1"};
    e_str rhs{std::in_place, "this is value 2"};
    CHECK_EQ(lhs, "this is value 1"s);
    CHECK_EQ(rhs, "this is value 2"s);
    lhs.swap(rhs);
    CHECK_EQ(lhs, "this is value 2"s);
    CHECK_EQ(rhs, "this is value 1"s);
  }
  SUBCASE("lhs: value, rhs: error") {
    e_str lhs{std::in_place, "this is a value"};
    e_str rhs{sec::runtime_error};
    CHECK_EQ(lhs, "this is a value"s);
    CHECK_EQ(rhs, error{sec::runtime_error});
    lhs.swap(rhs);
    CHECK_EQ(lhs, error{sec::runtime_error});
    CHECK_EQ(rhs, "this is a value"s);
  }
  SUBCASE("lhs: error, rhs: value") {
    e_str lhs{sec::runtime_error};
    e_str rhs{std::in_place, "this is a value"};
    CHECK_EQ(lhs, error{sec::runtime_error});
    CHECK_EQ(rhs, "this is a value"s);
    lhs.swap(rhs);
    CHECK_EQ(lhs, "this is a value"s);
    CHECK_EQ(rhs, error{sec::runtime_error});
  }
  SUBCASE("lhs: error, rhs: error") {
    e_str lhs{sec::runtime_error};
    e_str rhs{sec::logic_error};
    CHECK_EQ(lhs, error{sec::runtime_error});
    CHECK_EQ(rhs, error{sec::logic_error});
    lhs.swap(rhs);
    CHECK_EQ(lhs, error{sec::logic_error});
    CHECK_EQ(rhs, error{sec::runtime_error});
  }
  SUBCASE("lhs: void, rhs: void") {
    e_void lhs;
    e_void rhs;
    CHECK(lhs);
    CHECK(rhs);
    lhs.swap(rhs); // fancy no-op
    CHECK(lhs);
    CHECK(rhs);
  }
  SUBCASE("lhs: void, rhs: error") {
    e_void lhs;
    e_void rhs{sec::runtime_error};
    CHECK(lhs);
    CHECK_EQ(rhs, error{sec::runtime_error});
    lhs.swap(rhs);
    CHECK_EQ(lhs, error{sec::runtime_error});
    CHECK(rhs);
  }
  SUBCASE("lhs: error, rhs: void") {
    e_void lhs{sec::runtime_error};
    e_void rhs;
    CHECK_EQ(lhs, error{sec::runtime_error});
    CHECK(rhs);
    lhs.swap(rhs);
    CHECK(lhs);
    CHECK_EQ(rhs, error{sec::runtime_error});
  }
  SUBCASE("lhs: error, rhs: error") {
    e_void lhs{sec::runtime_error};
    e_void rhs{sec::logic_error};
    CHECK_EQ(lhs, error{sec::runtime_error});
    CHECK_EQ(rhs, error{sec::logic_error});
    lhs.swap(rhs);
    CHECK_EQ(lhs, error{sec::logic_error});
    CHECK_EQ(rhs, error{sec::runtime_error});
  }
}

TEST_CASE("an expected can be compared to its expected type and errors") {
  SUBCASE("non-void value type") {
    e_int x{42};
    CHECK_EQ(x, 42);
    CHECK_NE(x, 24);
    CHECK_NE(x, make_error(sec::runtime_error));
    e_int y{sec::runtime_error};
    CHECK_NE(y, 42);
    CHECK_NE(y, 24);
    CHECK_EQ(y, make_error(sec::runtime_error));
    CHECK_NE(y, make_error(sec::logic_error));
  }
  SUBCASE("void value type") {
    e_void x;
    CHECK(x);
    e_void y{sec::runtime_error};
    CHECK_EQ(y, make_error(sec::runtime_error));
    CHECK_NE(y, make_error(sec::logic_error));
  }
}

TEST_CASE("two expected with the same value are equal") {
  SUBCASE("non-void value type") {
    e_int x{42};
    e_int y{42};
    CHECK_EQ(x, y);
    CHECK_EQ(y, x);
  }
  SUBCASE("void value type") {
    e_void x;
    e_void y;
    CHECK_EQ(x, y);
    CHECK_EQ(y, x);
  }
}

TEST_CASE("two expected with different values are unequal") {
  e_int x{42};
  e_int y{24};
  CHECK_NE(x, y);
  CHECK_NE(y, x);
}

TEST_CASE("an expected with value is not equal to an expected with an error") {
  SUBCASE("non-void value type") {
    // Use the same "underlying value" for both objects.
    e_int x{static_cast<int>(sec::runtime_error)};
    e_int y{sec::runtime_error};
    CHECK_NE(x, y);
    CHECK_NE(y, x);
  }
  SUBCASE("void value type") {
    e_void x;
    e_void y{sec::runtime_error};
    CHECK_NE(x, y);
    CHECK_NE(y, x);
  }
}

TEST_CASE("two expected with the same error are equal") {
  SUBCASE("non-void value type") {
    e_int x{sec::runtime_error};
    e_int y{sec::runtime_error};
    CHECK_EQ(x, y);
    CHECK_EQ(y, x);
  }
  SUBCASE("void value type") {
    e_void x{sec::runtime_error};
    e_void y{sec::runtime_error};
    CHECK_EQ(x, y);
    CHECK_EQ(y, x);
  }
}

TEST_CASE("two expected with different errors are unequal") {
  SUBCASE("non-void value type") {
    e_int x{sec::logic_error};
    e_int y{sec::runtime_error};
    CHECK_NE(x, y);
    CHECK_NE(y, x);
  }
  SUBCASE("void value type") {
    e_void x{sec::logic_error};
    e_void y{sec::runtime_error};
    CHECK_NE(x, y);
    CHECK_NE(y, x);
  }
}

TEST_CASE("expected is copyable") {
  SUBCASE("non-void value type") {
    SUBCASE("copy-constructible") {
      e_int x{42};
      e_int y{x};
      CHECK_EQ(x, y);
    }
    SUBCASE("copy-assignable") {
      e_int x{42};
      e_int y{0};
      CHECK_NE(x, y);
      y = x;
      CHECK_EQ(x, y);
    }
  }
  SUBCASE("void value type") {
    SUBCASE("copy-constructible") {
      e_void x;
      e_void y{x};
      CHECK_EQ(x, y);
    }
    SUBCASE("copy-assignable") {
      e_void x;
      e_void y;
      CHECK_EQ(x, y);
      y = x;
      CHECK_EQ(x, y);
    }
  }
}

TEST_CASE("expected is movable") {
  SUBCASE("non-void value type") {
    SUBCASE("move-constructible") {
      auto iptr = make_counted<counted_int>(42);
      CHECK_EQ(iptr->get_reference_count(), 1u);
      e_iptr x{std::in_place, iptr};
      e_iptr y{std::move(x)};
      CHECK_EQ(iptr->get_reference_count(), 2u);
      CHECK_NE(x, iptr);
      CHECK_EQ(y, iptr);
    }
    SUBCASE("move-assignable") {
      auto iptr = make_counted<counted_int>(42);
      CHECK_EQ(iptr->get_reference_count(), 1u);
      e_iptr x{std::in_place, iptr};
      e_iptr y{std::in_place, nullptr};
      CHECK_EQ(x, iptr);
      CHECK_NE(y, iptr);
      y = std::move(x);
      CHECK_EQ(iptr->get_reference_count(), 2u);
      CHECK_NE(x, iptr);
      CHECK_EQ(y, iptr);
    }
  }
  SUBCASE("non-void value type") {
    SUBCASE("move-constructible") {
      e_void x;
      e_void y{std::move(x)};
      CHECK_EQ(x, y);
    }
    SUBCASE("move-assignable") {
      e_void x;
      e_void y;
      CHECK_EQ(x, y);
      y = std::move(x);
      CHECK_EQ(x, y);
    }
  }
}

TEST_CASE("expected is convertible from none") {
  e_int x{none};
  if (CHECK(!x))
    CHECK(!x.error());
  e_void y{none};
  if (CHECK(!y))
    CHECK(!y.error());
}

TEST_CASE("and_then composes a chain of functions returning an expected") {
  SUBCASE("non-void value type") {
    auto inc = [](counted_int_ptr ptr) {
      ptr->value(ptr->value() + 1);
      return e_iptr{std::move(ptr)};
    };
    SUBCASE("and_then makes copies when called on a mutable lvalue") {
      auto i = make_counted<counted_int>(1);
      auto v1 = e_iptr{std::in_place, i};
      auto v2 = v1.and_then(inc);
      CHECK_EQ(v1, i);
      CHECK_EQ(v2, i);
      CHECK_EQ(i->value(), 2);
    }
    SUBCASE("and_then makes copies when called on a const lvalue") {
      auto i = make_counted<counted_int>(1);
      const auto v1 = e_iptr{std::in_place, i};
      const auto v2 = v1.and_then(inc);
      CHECK_EQ(v1, i);
      CHECK_EQ(v2, i);
      CHECK_EQ(i->value(), 2);
    }
    SUBCASE("and_then moves when called on a mutable rvalue") {
      auto i = make_counted<counted_int>(1);
      auto v1 = e_iptr{std::in_place, i};
      auto v2 = std::move(v1).and_then(inc);
      CHECK_EQ(v1, nullptr);
      CHECK_EQ(v2, i);
      CHECK_EQ(i->value(), 2);
    }
    SUBCASE("and_then makes copies when called on a const rvalue") {
      auto i = make_counted<counted_int>(1);
      const auto v1 = e_iptr{std::in_place, i};
      const auto v2 = std::move(v1).and_then(inc);
      CHECK_EQ(v1, i);
      CHECK_EQ(v2, i);
      CHECK_EQ(i->value(), 2);
    }
  }
  SUBCASE("void value type") {
    auto called = false;
    auto fn = [&called] {
      called = true;
      return e_void{};
    };
    SUBCASE("mutable lvalue") {
      called = false;
      auto v1 = e_void{};
      auto v2 = v1.and_then(fn);
      CHECK(called);
      CHECK_EQ(v1, v2);
    }
    SUBCASE("const lvalue") {
      called = false;
      const auto v1 = e_void{};
      const auto v2 = v1.and_then(fn);
      CHECK(called);
      CHECK_EQ(v1, v2);
    }
    SUBCASE("mutable rvalue") {
      called = false;
      auto v1 = e_void{};
      auto v2 = std::move(v1).and_then(fn);
      CHECK(called);
      CHECK_EQ(v1, v2);
    }
    SUBCASE("const rvalue") {
      called = false;
      const auto v1 = e_void{};
      const auto v2 = std::move(v1).and_then(fn);
      CHECK(called);
      CHECK_EQ(v1, v2);
    }
  }
}

TEST_CASE("and_then does nothing when called with an error") {
  SUBCASE("non-void value type") {
    auto inc = [](counted_int_ptr ptr) {
      ptr->value(ptr->value() + 1);
      return e_iptr{std::move(ptr)};
    };
    auto v1 = e_iptr{sec::runtime_error};
    auto v2 = v1.and_then(inc);                  // mutable lvalue
    const auto v3 = std::move(v2).and_then(inc); // mutable rvalue
    const auto v4 = v3.and_then(inc);            // const lvalue
    auto v5 = std::move(v4).and_then(inc);       // const rvalue
    CHECK_EQ(v1, error{sec::runtime_error});
    CHECK_EQ(v2, error{}); // has been moved-from
    CHECK_EQ(v3, error{sec::runtime_error});
    CHECK_EQ(v4, error{sec::runtime_error});
    CHECK_EQ(v5, error{sec::runtime_error});
  }
  SUBCASE("void value type") {
    auto fn = [] { return e_void{}; };
    auto v1 = e_void{sec::runtime_error};
    auto v2 = v1.and_then(fn);                  // mutable lvalue
    const auto v3 = std::move(v2).and_then(fn); // mutable rvalue
    const auto v4 = v3.and_then(fn);            // const lvalue
    auto v5 = std::move(v4).and_then(fn);       // const rvalue
    CHECK_EQ(v1, error{sec::runtime_error});
    CHECK_EQ(v2, error{}); // has been moved-from
    CHECK_EQ(v3, error{sec::runtime_error});
    CHECK_EQ(v4, error{sec::runtime_error});
    CHECK_EQ(v5, error{sec::runtime_error});
  }
}

TEST_CASE("transform applies a function to change the value") {
  SUBCASE("non-void value type") {
    auto inc = [](counted_int_ptr ptr) {
      return make_counted<counted_int>(ptr->value() + 1);
    };
    SUBCASE("transform makes copies when called on a mutable lvalue") {
      auto i = make_counted<counted_int>(1);
      auto v1 = e_iptr{std::in_place, i};
      auto v2 = v1.transform(inc);
      CHECK_EQ(i->value(), 1);
      CHECK_EQ(v1, i);
      if (CHECK(v2))
        CHECK_EQ((*v2)->value(), 2);
    }
    SUBCASE("transform makes copies when called on a const lvalue") {
      auto i = make_counted<counted_int>(1);
      const auto v1 = e_iptr{std::in_place, i};
      const auto v2 = v1.transform(inc);
      CHECK_EQ(i->value(), 1);
      CHECK_EQ(v1, i);
      if (CHECK(v2))
        CHECK_EQ((*v2)->value(), 2);
    }
    SUBCASE("transform moves when called on a mutable rvalue") {
      auto i = make_counted<counted_int>(1);
      auto v1 = e_iptr{std::in_place, i};
      auto v2 = std::move(v1).transform(inc);
      CHECK_EQ(i->value(), 1);
      CHECK_EQ(v1, nullptr);
      if (CHECK(v2))
        CHECK_EQ((*v2)->value(), 2);
    }
    SUBCASE("transform makes copies when called on a const rvalue") {
      auto i = make_counted<counted_int>(1);
      const auto v1 = e_iptr{std::in_place, i};
      const auto v2 = std::move(v1).transform(inc);
      CHECK_EQ(i->value(), 1);
      CHECK_EQ(v1, i);
      if (CHECK(v2))
        CHECK_EQ((*v2)->value(), 2);
    }
  }
  SUBCASE("void value type") {
    auto fn = [] { return 42; };
    SUBCASE("mutable lvalue") {
      auto v1 = e_void{};
      auto v2 = v1.transform(fn);
      CHECK_EQ(v2, 42);
    }
    SUBCASE("const lvalue") {
      const auto v1 = e_void{};
      const auto v2 = v1.transform(fn);
      CHECK_EQ(v2, 42);
    }
    SUBCASE("mutable rvalue") {
      auto v1 = e_void{};
      auto v2 = std::move(v1).transform(fn);
      CHECK_EQ(v2, 42);
    }
    SUBCASE("const rvalue") {
      const auto v1 = e_void{};
      const auto v2 = std::move(v1).transform(fn);
      CHECK_EQ(v2, 42);
    }
  }
}

TEST_CASE("transform does nothing when called with an error") {
  SUBCASE("non-void value type") {
    auto inc = [](counted_int_ptr ptr) {
      return make_counted<counted_int>(ptr->value() + 1);
    };
    auto v1 = e_iptr{sec::runtime_error};
    auto v2 = v1.transform(inc);                  // mutable lvalue
    const auto v3 = std::move(v2).transform(inc); // mutable rvalue
    const auto v4 = v3.transform(inc);            // const lvalue
    auto v5 = std::move(v4).transform(inc);       // const rvalue
    CHECK_EQ(v1, error{sec::runtime_error});
    CHECK_EQ(v2, error{}); // has been moved-from
    CHECK_EQ(v3, error{sec::runtime_error});
    CHECK_EQ(v4, error{sec::runtime_error});
    CHECK_EQ(v5, error{sec::runtime_error});
  }
  SUBCASE("void value type") {
    auto fn = [] {};
    auto v1 = e_void{sec::runtime_error};
    auto v2 = v1.transform(fn);                  // mutable lvalue
    const auto v3 = std::move(v2).transform(fn); // mutable rvalue
    const auto v4 = v3.transform(fn);            // const lvalue
    auto v5 = std::move(v4).transform(fn);       // const rvalue
    CHECK_EQ(v1, error{sec::runtime_error});
    CHECK_EQ(v2, error{}); // has been moved-from
    CHECK_EQ(v3, error{sec::runtime_error});
    CHECK_EQ(v4, error{sec::runtime_error});
    CHECK_EQ(v5, error{sec::runtime_error});
  }
}

namespace {

template <class T, bool WrapIntoExpected>
struct next_error_t {
  auto operator()(const error& err) const {
    assert(err.category() == type_id_v<sec>);
    auto code = err.code() + 1;
    if constexpr (WrapIntoExpected)
      return expected<T>{make_error(static_cast<sec>(code))};
    else
      return make_error(static_cast<sec>(code));
  }
  auto operator()(error&& err) const {
    assert(err.category() == type_id_v<sec>);
    auto code = err.code() + 1;
    err = make_error(static_cast<sec>(code));
    if constexpr (WrapIntoExpected)
      return expected<T>{std::move(err)};
    else
      return std::move(err);
  }
};

} // namespace

TEST_CASE("or_else may replace the error or set a default") {
  SUBCASE("non-void value type") {
    next_error_t<int, true> next_error;
    auto set_fallback = [](auto&& err) {
      if constexpr (std::is_same_v<decltype(err), error&&>) {
        // Just for testing that we get indeed an rvalue in the test.
        err = error{};
      }
      return e_int{42};
    };
    SUBCASE("or_else makes copies when called on a mutable lvalue") {
      auto v1 = e_int{sec::runtime_error};
      auto v2 = v1.or_else(next_error);
      CHECK_EQ(v1, sec::runtime_error);
      CHECK_EQ(v2, sec::remote_linking_failed);
      auto v3 = v2.or_else(set_fallback);
      CHECK_EQ(v2, sec::remote_linking_failed);
      CHECK_EQ(v3, 42);
    }
    SUBCASE("or_else makes copies when called on a const lvalue") {
      const auto v1 = e_int{sec::runtime_error};
      const auto v2 = v1.or_else(next_error);
      CHECK_EQ(v1, sec::runtime_error);
      CHECK_EQ(v2, sec::remote_linking_failed);
      const auto v3 = v2.or_else(set_fallback);
      CHECK_EQ(v2, sec::remote_linking_failed);
      CHECK_EQ(v3, 42);
    }
    SUBCASE("or_else moves when called on a mutable rvalue") {
      auto v1 = e_int{sec::runtime_error};
      auto v2 = std::move(v1).or_else(next_error);
      CHECK_EQ(v1, error{});
      CHECK_EQ(v2, sec::remote_linking_failed);
      auto v3 = std::move(v2).or_else(set_fallback);
      CHECK_EQ(v2, error{});
      CHECK_EQ(v3, 42);
    }
    SUBCASE("or_else makes copies when called on a const rvalue") {
      const auto v1 = e_int{sec::runtime_error};
      const auto v2 = std::move(v1).or_else(next_error);
      CHECK_EQ(v1, sec::runtime_error);
      CHECK_EQ(v2, sec::remote_linking_failed);
      const auto v3 = std::move(v2).or_else(set_fallback);
      CHECK_EQ(v2, sec::remote_linking_failed);
      CHECK_EQ(v3, 42);
    }
  }
  SUBCASE("void value type") {
    next_error_t<void, true> next_error;
    auto set_fallback = [](auto&& err) {
      if constexpr (std::is_same_v<decltype(err), error&&>) {
        // Just for testing that we get indeed an rvalue in the test.
        err = error{};
      }
      return e_void{};
    };
    SUBCASE("or_else makes copies when called on a mutable lvalue") {
      auto v1 = e_void{sec::runtime_error};
      auto v2 = v1.or_else(next_error);
      CHECK_EQ(v1, sec::runtime_error);
      CHECK_EQ(v2, sec::remote_linking_failed);
      auto v3 = v2.or_else(set_fallback);
      CHECK_EQ(v2, sec::remote_linking_failed);
      CHECK(v3);
    }
    SUBCASE("or_else makes copies when called on a const lvalue") {
      const auto v1 = e_void{sec::runtime_error};
      const auto v2 = v1.or_else(next_error);
      CHECK_EQ(v1, sec::runtime_error);
      CHECK_EQ(v2, sec::remote_linking_failed);
      const auto v3 = v2.or_else(set_fallback);
      CHECK_EQ(v2, sec::remote_linking_failed);
      CHECK(v3);
    }
    SUBCASE("or_else moves when called on a mutable rvalue") {
      auto v1 = e_void{sec::runtime_error};
      auto v2 = std::move(v1).or_else(next_error);
      CHECK_EQ(v1, error{});
      CHECK_EQ(v2, sec::remote_linking_failed);
      auto v3 = std::move(v2).or_else(set_fallback);
      CHECK_EQ(v2, error{});
      CHECK(v3);
    }
    SUBCASE("or_else makes copies when called on a const rvalue") {
      const auto v1 = e_void{sec::runtime_error};
      const auto v2 = std::move(v1).or_else(next_error);
      CHECK_EQ(v1, sec::runtime_error);
      CHECK_EQ(v2, sec::remote_linking_failed);
      const auto v3 = std::move(v2).or_else(set_fallback);
      CHECK_EQ(v2, sec::remote_linking_failed);
      CHECK(v3);
    }
  }
}

TEST_CASE("or_else leaves the expected unchanged when returning void") {
  SUBCASE("non-void value type") {
    auto i = 0;
    auto inc = [&i](auto&&) { ++i; };
    auto v1 = e_int{sec::runtime_error};
    auto v2 = v1.or_else(inc);                  // mutable lvalue
    const auto v3 = std::move(v2).or_else(inc); // mutable rvalue
    const auto v4 = v3.or_else(inc);            // const lvalue
    auto v5 = std::move(v4).or_else(inc);       // const rvalue
    CHECK_EQ(v1, error{sec::runtime_error});
    CHECK_EQ(v2, error{}); // has been moved-from
    CHECK_EQ(v3, error{sec::runtime_error});
    CHECK_EQ(v4, error{sec::runtime_error});
    CHECK_EQ(v5, error{sec::runtime_error});
    CHECK_EQ(i, 4);
  }
  SUBCASE("void value type") {
    auto i = 0;
    auto inc = [&i](auto&&) { ++i; };
    auto v1 = e_void{sec::runtime_error};
    auto v2 = v1.or_else(inc);                  // mutable lvalue
    const auto v3 = std::move(v2).or_else(inc); // mutable rvalue
    const auto v4 = v3.or_else(inc);            // const lvalue
    auto v5 = std::move(v4).or_else(inc);       // const rvalue
    CHECK_EQ(v1, error{sec::runtime_error});
    CHECK_EQ(v2, error{}); // has been moved-from
    CHECK_EQ(v3, error{sec::runtime_error});
    CHECK_EQ(v4, error{sec::runtime_error});
    CHECK_EQ(v5, error{sec::runtime_error});
    CHECK_EQ(i, 4);
  }
}

TEST_CASE("or_else does nothing when called with a value") {
  SUBCASE("non-void value type") {
    auto uh_oh = [](auto&&) { FAIL("uh-oh called!"); };
    auto i = make_counted<counted_int>(1);
    auto v1 = e_iptr{std::in_place, i};
    auto v2 = v1.or_else(uh_oh);                  // mutable lvalue
    const auto v3 = std::move(v2).or_else(uh_oh); // mutable rvalue
    const auto v4 = v3.or_else(uh_oh);            // const lvalue
    auto v5 = std::move(v4).or_else(uh_oh);       // const rvalue
    CHECK_EQ(v1, i);
    CHECK_EQ(v2, nullptr); // has been moved-from
    CHECK_EQ(v3, i);
    CHECK_EQ(v4, i);
    CHECK_EQ(v5, i);
  }
  SUBCASE("void value type") {
    auto uh_oh = [](auto&&) { FAIL("uh-oh called!"); };
    auto v1 = e_void{};
    auto v2 = v1.or_else(uh_oh);                  // mutable lvalue
    const auto v3 = std::move(v2).or_else(uh_oh); // mutable rvalue
    const auto v4 = v3.or_else(uh_oh);            // const lvalue
    auto v5 = std::move(v4).or_else(uh_oh);       // const rvalue
    CHECK(v1);
    CHECK(v2);
    CHECK(v3);
    CHECK(v4);
    CHECK(v5);
  }
}

TEST_CASE("transform_or may replace the error or set a default") {
  SUBCASE("non-void value type") {
    next_error_t<int, false> next_error;
    SUBCASE("transform_or makes copies when called on a mutable lvalue") {
      auto v1 = e_int{sec::runtime_error};
      auto v2 = v1.transform_or(next_error);
      CHECK_EQ(v1, sec::runtime_error);
      CHECK_EQ(v2, sec::remote_linking_failed);
    }
    SUBCASE("transform_or makes copies when called on a const lvalue") {
      const auto v1 = e_int{sec::runtime_error};
      const auto v2 = v1.transform_or(next_error);
      CHECK_EQ(v1, sec::runtime_error);
      CHECK_EQ(v2, sec::remote_linking_failed);
    }
    SUBCASE("transform_or moves when called on a mutable rvalue") {
      auto v1 = e_int{sec::runtime_error};
      auto v2 = std::move(v1).transform_or(next_error);
      CHECK_EQ(v1, error{});
      CHECK_EQ(v2, sec::remote_linking_failed);
    }
    SUBCASE("transform_or makes copies when called on a const rvalue") {
      const auto v1 = e_int{sec::runtime_error};
      const auto v2 = std::move(v1).transform_or(next_error);
      CHECK_EQ(v1, sec::runtime_error);
      CHECK_EQ(v2, sec::remote_linking_failed);
    }
  }
  SUBCASE("void value type") {
    next_error_t<void, false> next_error;
    SUBCASE("transform_or makes copies when called on a mutable lvalue") {
      auto v1 = e_void{sec::runtime_error};
      auto v2 = v1.transform_or(next_error);
      CHECK_EQ(v1, sec::runtime_error);
      CHECK_EQ(v2, sec::remote_linking_failed);
    }
    SUBCASE("transform_or makes copies when called on a const lvalue") {
      const auto v1 = e_void{sec::runtime_error};
      const auto v2 = v1.transform_or(next_error);
      CHECK_EQ(v1, sec::runtime_error);
      CHECK_EQ(v2, sec::remote_linking_failed);
    }
    SUBCASE("transform_or moves when called on a mutable rvalue") {
      auto v1 = e_void{sec::runtime_error};
      auto v2 = std::move(v1).transform_or(next_error);
      CHECK_EQ(v1, error{});
      CHECK_EQ(v2, sec::remote_linking_failed);
    }
    SUBCASE("transform_or makes copies when called on a const rvalue") {
      const auto v1 = e_void{sec::runtime_error};
      const auto v2 = std::move(v1).transform_or(next_error);
      CHECK_EQ(v1, sec::runtime_error);
      CHECK_EQ(v2, sec::remote_linking_failed);
    }
  }
}

TEST_CASE("transform_or does nothing when called with a value") {
  SUBCASE("non-void value type") {
    auto uh_oh = [](auto&&) {
      FAIL("uh-oh called!");
      return error{};
    };
    auto i = make_counted<counted_int>(1);
    auto v1 = e_iptr{std::in_place, i};
    auto v2 = v1.transform_or(uh_oh);                  // mutable lvalue
    const auto v3 = std::move(v2).transform_or(uh_oh); // mutable rvalue
    const auto v4 = v3.transform_or(uh_oh);            // const lvalue
    auto v5 = std::move(v4).transform_or(uh_oh);       // const rvalue
    CHECK_EQ(v1, i);
    CHECK_EQ(v2, nullptr); // has been moved-from
    CHECK_EQ(v3, i);
    CHECK_EQ(v4, i);
    CHECK_EQ(v5, i);
  }
  SUBCASE("void value type") {
    auto uh_oh = [](auto&&) {
      FAIL("uh-oh called!");
      return error{};
    };
    auto v1 = e_void{};
    auto v2 = v1.transform_or(uh_oh);                  // mutable lvalue
    const auto v3 = std::move(v2).transform_or(uh_oh); // mutable rvalue
    const auto v4 = v3.transform_or(uh_oh);            // const lvalue
    auto v5 = std::move(v4).transform_or(uh_oh);       // const rvalue
    CHECK(v1);
    CHECK(v2);
    CHECK(v3);
    CHECK(v4);
    CHECK(v5);
  }
}
