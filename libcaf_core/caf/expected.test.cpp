// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/expected.hpp"

#include "caf/test/test.hpp"

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

// NOLINTBEGIN(bugprone-use-after-move)

TEST("expected reports its status via has_value() or operator bool()") {
  e_int x{42};
  check(static_cast<bool>(x));
  check(x.has_value());
  e_int y{sec::runtime_error};
  check(!y);
  check(!y.has_value());
}

TEST("an expected exposed its value via value()") {
  auto i = make_counted<counted_int>(42);
  SECTION("mutable lvalue access") {
    auto ex = e_iptr{i};
    auto val = counted_int_ptr{ex.value()}; // must make a copy
    check_eq(val, i);
    if (check(ex.has_value()))
      check_eq(ex.value(), i);
    e_void ev;
    ev.value(); // fancy no-op
  }
  SECTION("const lvalue access") {
    const auto ex = e_iptr{i};
    auto val = counted_int_ptr{ex.value()}; // must make a copy
    check_eq(val, i);
    if (check(ex.has_value()))
      check_eq(ex.value(), i);
    const e_void ev;
    ev.value(); // fancy no-op
  }
  SECTION("mutable rvalue access") {
    auto ex = e_iptr{i};
    auto val = counted_int_ptr{std::move(ex).value()}; // must move the value
    check_eq(val, i);
    if (check(ex.has_value()))
      check_eq(ex.value(), nullptr);
    e_void ev;
    std::move(ev).value(); // fancy no-op
  }
  SECTION("const rvalue access") {
    const auto ex = e_iptr{i};
    auto val = counted_int_ptr{std::move(ex).value()}; // must make a copy
    check_eq(val, i);
    if (check(ex.has_value()))
      check_eq(ex.value(), i);
    const e_void ev;
    std::move(ev).value(); // fancy no-op
  }
#ifdef CAF_ENABLE_EXCEPTIONS
  SECTION("value() throws if has_value() would return false") {
    auto val = e_iptr{sec::runtime_error};
    const auto& cval = val;
    check_throws<std::runtime_error>([&] { val.value(); });
    check_throws<std::runtime_error>([&] { std::move(val).value(); });
    check_throws<std::runtime_error>([&] { cval.value(); });
    check_throws<std::runtime_error>([&] { std::move(cval).value(); });
  }
#endif
}

TEST("an expected exposed its value via operator*()") {
  auto i = make_counted<counted_int>(42);
  SECTION("mutable lvalue access") {
    auto ex = e_iptr{i};
    auto val = counted_int_ptr{*ex}; // must make a copy
    check_eq(val, i);
    if (check(ex.has_value()))
      check_eq(*ex, i);
    e_void ev;
    *ev; // fancy no-op
  }
  SECTION("const lvalue access") {
    const auto ex = e_iptr{i};
    auto val = counted_int_ptr{*ex}; // must make a copy
    check_eq(val, i);
    if (check(ex.has_value()))
      check_eq(*ex, i);
    const e_void ev;
    *ev; // fancy no-op
  }
  SECTION("mutable rvalue access") {
    auto ex = e_iptr{i};
    auto val = counted_int_ptr{*std::move(ex)}; // must move the value
    check_eq(val, i);
    if (check(ex.has_value()))
      check_eq(*ex, nullptr);
    e_void ev;
    *std::move(ev); // fancy no-op
  }
  SECTION("const rvalue access") {
    const auto ex = e_iptr{i};
    auto val = counted_int_ptr{*std::move(ex)}; // must make a copy
    check_eq(val, i);
    if (check(ex.has_value()))
      check_eq(*ex, i);
    const e_void ev;
    *std::move(ev); // fancy no-op
  }
}

TEST("an expected exposed its value via operator->()") {
  SECTION("mutable lvalue access") {
    auto val = e_str{std::in_place, "foo"};
    check_eq(val->c_str(), "foo"s);
    val->push_back('!');
    check_eq(val->c_str(), "foo!"s);
  }
  SECTION("const lvalue access") {
    const auto val = e_str{std::in_place, "foo"};
    check_eq(val->c_str(), "foo"s);
    using pointer_t = decltype(val.operator->());
    static_assert(std::is_same_v<pointer_t, const std::string*>);
  }
}

TEST("value_or() returns the stored value or a fallback") {
  auto i = make_counted<counted_int>(42);
  auto j = make_counted<counted_int>(24);
  SECTION("lvalue access with value") {
    auto val = e_iptr{i};
    auto k = counted_int_ptr{val.value_or(j)};
    check_eq(val, i);
    check_eq(k, i);
  }
  SECTION("lvalue access with error") {
    auto val = e_iptr{sec::runtime_error};
    auto k = counted_int_ptr{val.value_or(j)};
    check_eq(val, error{sec::runtime_error});
    check_eq(k, j);
  }
  SECTION("rvalue access with value") {
    auto val = e_iptr{i};
    auto k = counted_int_ptr{std::move(val).value_or(j)};
    check_eq(val, nullptr);
    check_eq(k, i);
  }
  SECTION("rvalue access with error") {
    auto val = e_iptr{sec::runtime_error};
    auto k = counted_int_ptr{std::move(val).value_or(j)};
    check_eq(val, error{sec::runtime_error});
    check_eq(k, j);
  }
}

TEST("emplace destroys an old value or error and constructs a new value") {
  SECTION("non-void value type") {
    e_int x{42};
    check_eq(x.value(), 42);
    x.emplace(23);
    check_eq(x.value(), 23);
    e_int y{sec::runtime_error};
    check(!y);
    y.emplace(23);
    check_eq(y.value(), 23);
  }
  SECTION("void value type") {
    e_void x;
    check(x.has_value());
    x.emplace();
    check(x.has_value());
    e_void y{sec::runtime_error};
    check(!y);
    y.emplace();
    check(static_cast<bool>(y));
  }
}

TEST("swap exchanges the content of two expected") {
  SECTION("lhs: value, rhs: value") {
    e_str lhs{std::in_place, "this is value 1"};
    e_str rhs{std::in_place, "this is value 2"};
    check_eq(lhs, "this is value 1"s);
    check_eq(rhs, "this is value 2"s);
    lhs.swap(rhs);
    check_eq(lhs, "this is value 2"s);
    check_eq(rhs, "this is value 1"s);
  }
  SECTION("lhs: value, rhs: error") {
    e_str lhs{std::in_place, "this is a value"};
    e_str rhs{sec::runtime_error};
    check_eq(lhs, "this is a value"s);
    check_eq(rhs, error{sec::runtime_error});
    lhs.swap(rhs);
    check_eq(lhs, error{sec::runtime_error});
    check_eq(rhs, "this is a value"s);
  }
  SECTION("lhs: error, rhs: value") {
    e_str lhs{sec::runtime_error};
    e_str rhs{std::in_place, "this is a value"};
    check_eq(lhs, error{sec::runtime_error});
    check_eq(rhs, "this is a value"s);
    lhs.swap(rhs);
    check_eq(lhs, "this is a value"s);
    check_eq(rhs, error{sec::runtime_error});
  }
  SECTION("lhs: error, rhs: error") {
    e_str lhs{sec::runtime_error};
    e_str rhs{sec::logic_error};
    check_eq(lhs, error{sec::runtime_error});
    check_eq(rhs, error{sec::logic_error});
    lhs.swap(rhs);
    check_eq(lhs, error{sec::logic_error});
    check_eq(rhs, error{sec::runtime_error});
  }
  SECTION("lhs: void, rhs: void") {
    e_void lhs;
    e_void rhs;
    check(static_cast<bool>(lhs));
    check(static_cast<bool>(rhs));
    lhs.swap(rhs); // fancy no-op
    check(static_cast<bool>(lhs));
    check(static_cast<bool>(rhs));
  }
  SECTION("lhs: void, rhs: error") {
    e_void lhs;
    e_void rhs{sec::runtime_error};
    check(static_cast<bool>(lhs));
    check_eq(rhs, error{sec::runtime_error});
    lhs.swap(rhs);
    check_eq(lhs, error{sec::runtime_error});
    check(static_cast<bool>(rhs));
  }
  SECTION("lhs: error, rhs: void") {
    e_void lhs{sec::runtime_error};
    e_void rhs;
    check_eq(lhs, error{sec::runtime_error});
    check(static_cast<bool>(rhs));
    lhs.swap(rhs);
    check(static_cast<bool>(lhs));
    check_eq(rhs, error{sec::runtime_error});
  }
  SECTION("lhs: error, rhs: error") {
    e_void lhs{sec::runtime_error};
    e_void rhs{sec::logic_error};
    check_eq(lhs, error{sec::runtime_error});
    check_eq(rhs, error{sec::logic_error});
    lhs.swap(rhs);
    check_eq(lhs, error{sec::logic_error});
    check_eq(rhs, error{sec::runtime_error});
  }
}

TEST("an expected can be compared to its expected type and errors") {
  SECTION("non-void value type") {
    e_int x{42};
    check_eq(x, 42);
    check_ne(x, 24);
    check_ne(x, make_error(sec::runtime_error));
    e_int y{sec::runtime_error};
    check_ne(y, 42);
    check_ne(y, 24);
    check_eq(y, make_error(sec::runtime_error));
    check_ne(y, make_error(sec::logic_error));
  }
  SECTION("void value type") {
    e_void x;
    check(static_cast<bool>(x));
    e_void y{sec::runtime_error};
    check_eq(y, make_error(sec::runtime_error));
    check_ne(y, make_error(sec::logic_error));
  }
}

TEST("two expected with the same value are equal") {
  SECTION("non-void value type") {
    e_int x{42};
    e_int y{42};
    check_eq(x, y);
    check_eq(y, x);
  }
  SECTION("void value type") {
    e_void x;
    e_void y;
    check_eq(x, y);
    check_eq(y, x);
  }
}

TEST("two expected with different values are unequal") {
  e_int x{42};
  e_int y{24};
  check_ne(x, y);
  check_ne(y, x);
}

TEST("an expected with value is not equal to an expected with an error") {
  SECTION("non-void value type") {
    // Use the same "underlying value" for both objects.
    e_int x{static_cast<int>(sec::runtime_error)};
    e_int y{sec::runtime_error};
    check_ne(x, y);
    check_ne(y, x);
  }
  SECTION("void value type") {
    e_void x;
    e_void y{sec::runtime_error};
    check_ne(x, y);
    check_ne(y, x);
  }
}

TEST("two expected with the same error are equal") {
  SECTION("non-void value type") {
    e_int x{sec::runtime_error};
    e_int y{sec::runtime_error};
    check_eq(x, y);
    check_eq(y, x);
  }
  SECTION("void value type") {
    e_void x{sec::runtime_error};
    e_void y{sec::runtime_error};
    check_eq(x, y);
    check_eq(y, x);
  }
}

TEST("two expected with different errors are unequal") {
  SECTION("non-void value type") {
    e_int x{sec::logic_error};
    e_int y{sec::runtime_error};
    check_ne(x, y);
    check_ne(y, x);
  }
  SECTION("void value type") {
    e_void x{sec::logic_error};
    e_void y{sec::runtime_error};
    check_ne(x, y);
    check_ne(y, x);
  }
}

TEST("expected is copyable") {
  SECTION("non-void value type") {
    SECTION("copy-constructible") {
      e_int x{42};
      e_int y{x};
      check_eq(x, y);
    }
    SECTION("copy-assignable") {
      e_int x{42};
      e_int y{0};
      check_ne(x, y);
      y = x;
      check_eq(x, y);
    }
  }
  SECTION("void value type") {
    SECTION("copy-constructible") {
      e_void x;
      e_void y{x};
      check_eq(x, y);
    }
    SECTION("copy-assignable") {
      e_void x;
      e_void y;
      check_eq(x, y);
      y = x;
      check_eq(x, y);
    }
  }
}

TEST("expected is movable") {
  SECTION("non-void value type") {
    SECTION("move-constructible") {
      auto iptr = make_counted<counted_int>(42);
      check_eq(iptr->get_reference_count(), 1u);
      e_iptr x{std::in_place, iptr};
      e_iptr y{std::move(x)};
      check_eq(iptr->get_reference_count(), 2u);
      check_ne(x, iptr);
      check_eq(y, iptr);
    }
    SECTION("move-assignable") {
      auto iptr = make_counted<counted_int>(42);
      check_eq(iptr->get_reference_count(), 1u);
      e_iptr x{std::in_place, iptr};
      e_iptr y{std::in_place, nullptr};
      check_eq(x, iptr);
      check_ne(y, iptr);
      y = std::move(x);
      check_eq(iptr->get_reference_count(), 2u);
      check_ne(x, iptr);
      check_eq(y, iptr);
    }
  }
  SECTION("non-void value type") {
    SECTION("move-constructible") {
      e_void x;
      e_void y{std::move(x)};
      check_eq(x, y);
    }
    SECTION("move-assignable") {
      e_void x;
      e_void y;
      check_eq(x, y);
      y = std::move(x);
      check_eq(x, y);
    }
  }
}

TEST("expected is convertible from none") {
  e_int x{none};
  if (check(!x))
    check(!x.error());
  e_void y{none};
  if (check(!y))
    check(!y.error());
}

TEST("and_then composes a chain of functions returning an expected") {
  SECTION("non-void value type") {
    auto inc = [](counted_int_ptr ptr) {
      ptr->value(ptr->value() + 1);
      return e_iptr{std::move(ptr)};
    };
    SECTION("and_then makes copies when called on a mutable lvalue") {
      auto i = make_counted<counted_int>(1);
      auto v1 = e_iptr{std::in_place, i};
      auto v2 = v1.and_then(inc);
      check_eq(v1, i);
      check_eq(v2, i);
      check_eq(i->value(), 2);
    }
    SECTION("and_then makes copies when called on a const lvalue") {
      auto i = make_counted<counted_int>(1);
      const auto v1 = e_iptr{std::in_place, i};
      const auto v2 = v1.and_then(inc);
      check_eq(v1, i);
      check_eq(v2, i);
      check_eq(i->value(), 2);
    }
    SECTION("and_then moves when called on a mutable rvalue") {
      auto i = make_counted<counted_int>(1);
      auto v1 = e_iptr{std::in_place, i};
      auto v2 = std::move(v1).and_then(inc);
      check_eq(v1, nullptr);
      check_eq(v2, i);
      check_eq(i->value(), 2);
    }
    SECTION("and_then makes copies when called on a const rvalue") {
      auto i = make_counted<counted_int>(1);
      const auto v1 = e_iptr{std::in_place, i};
      const auto v2 = std::move(v1).and_then(inc);
      check_eq(v1, i);
      check_eq(v2, i);
      check_eq(i->value(), 2);
    }
  }
  SECTION("void value type") {
    auto called = false;
    auto fn = [&called] {
      called = true;
      return e_void{};
    };
    SECTION("mutable lvalue") {
      called = false;
      auto v1 = e_void{};
      auto v2 = v1.and_then(fn);
      check(called);
      check_eq(v1, v2);
    }
    SECTION("const lvalue") {
      called = false;
      const auto v1 = e_void{};
      const auto v2 = v1.and_then(fn);
      check(called);
      check_eq(v1, v2);
    }
    SECTION("mutable rvalue") {
      called = false;
      auto v1 = e_void{};
      auto v2 = std::move(v1).and_then(fn);
      check(called);
      check_eq(v1, v2);
    }
    SECTION("const rvalue") {
      called = false;
      const auto v1 = e_void{};
      const auto v2 = std::move(v1).and_then(fn);
      check(called);
      check_eq(v1, v2);
    }
  }
}

TEST("and_then does nothing when called with an error") {
  SECTION("non-void value type") {
    auto inc = [](counted_int_ptr ptr) {
      ptr->value(ptr->value() + 1);
      return e_iptr{std::move(ptr)};
    };
    auto v1 = e_iptr{sec::runtime_error};
    auto v2 = v1.and_then(inc);                  // mutable lvalue
    const auto v3 = std::move(v2).and_then(inc); // mutable rvalue
    const auto v4 = v3.and_then(inc);            // const lvalue
    auto v5 = std::move(v4).and_then(inc);       // const rvalue
    check_eq(v1, error{sec::runtime_error});
    check_eq(v2, error{}); // has been moved-from
    check_eq(v3, error{sec::runtime_error});
    check_eq(v4, error{sec::runtime_error});
    check_eq(v5, error{sec::runtime_error});
  }
  SECTION("void value type") {
    auto fn = [] { return e_void{}; };
    auto v1 = e_void{sec::runtime_error};
    auto v2 = v1.and_then(fn);                  // mutable lvalue
    const auto v3 = std::move(v2).and_then(fn); // mutable rvalue
    const auto v4 = v3.and_then(fn);            // const lvalue
    auto v5 = std::move(v4).and_then(fn);       // const rvalue
    check_eq(v1, error{sec::runtime_error});
    check_eq(v2, error{}); // has been moved-from
    check_eq(v3, error{sec::runtime_error});
    check_eq(v4, error{sec::runtime_error});
    check_eq(v5, error{sec::runtime_error});
  }
}

TEST("transform applies a function to change the value") {
  SECTION("non-void value type") {
    auto inc = [](counted_int_ptr ptr) {
      return make_counted<counted_int>(ptr->value() + 1);
    };
    SECTION("transform makes copies when called on a mutable lvalue") {
      auto i = make_counted<counted_int>(1);
      auto v1 = e_iptr{std::in_place, i};
      auto v2 = v1.transform(inc);
      check_eq(i->value(), 1);
      check_eq(v1, i);
      if (check(static_cast<bool>(v2)))
        check_eq((*v2)->value(), 2);
    }
    SECTION("transform makes copies when called on a const lvalue") {
      auto i = make_counted<counted_int>(1);
      const auto v1 = e_iptr{std::in_place, i};
      const auto v2 = v1.transform(inc);
      check_eq(i->value(), 1);
      check_eq(v1, i);
      if (check(static_cast<bool>(v2)))
        check_eq((*v2)->value(), 2);
    }
    SECTION("transform moves when called on a mutable rvalue") {
      auto i = make_counted<counted_int>(1);
      auto v1 = e_iptr{std::in_place, i};
      auto v2 = std::move(v1).transform(inc);
      check_eq(i->value(), 1);
      check_eq(v1, nullptr);
      if (check(static_cast<bool>(v2)))
        check_eq((*v2)->value(), 2);
    }
    SECTION("transform makes copies when called on a const rvalue") {
      auto i = make_counted<counted_int>(1);
      const auto v1 = e_iptr{std::in_place, i};
      const auto v2 = std::move(v1).transform(inc);
      check_eq(i->value(), 1);
      check_eq(v1, i);
      if (check(static_cast<bool>(v2)))
        check_eq((*v2)->value(), 2);
    }
  }
  SECTION("void value type") {
    auto fn = [] { return 42; };
    SECTION("mutable lvalue") {
      auto v1 = e_void{};
      auto v2 = v1.transform(fn);
      check_eq(v2, 42);
    }
    SECTION("const lvalue") {
      const auto v1 = e_void{};
      const auto v2 = v1.transform(fn);
      check_eq(v2, 42);
    }
    SECTION("mutable rvalue") {
      auto v1 = e_void{};
      auto v2 = std::move(v1).transform(fn);
      check_eq(v2, 42);
    }
    SECTION("const rvalue") {
      const auto v1 = e_void{};
      const auto v2 = std::move(v1).transform(fn);
      check_eq(v2, 42);
    }
  }
}

TEST("transform does nothing when called with an error") {
  SECTION("non-void value type") {
    auto inc = [](counted_int_ptr ptr) {
      return make_counted<counted_int>(ptr->value() + 1);
    };
    auto v1 = e_iptr{sec::runtime_error};
    auto v2 = v1.transform(inc);                  // mutable lvalue
    const auto v3 = std::move(v2).transform(inc); // mutable rvalue
    const auto v4 = v3.transform(inc);            // const lvalue
    auto v5 = std::move(v4).transform(inc);       // const rvalue
    check_eq(v1, error{sec::runtime_error});
    check_eq(v2, error{}); // has been moved-from
    check_eq(v3, error{sec::runtime_error});
    check_eq(v4, error{sec::runtime_error});
    check_eq(v5, error{sec::runtime_error});
  }
  SECTION("void value type") {
    auto fn = [] {};
    auto v1 = e_void{sec::runtime_error};
    auto v2 = v1.transform(fn);                  // mutable lvalue
    const auto v3 = std::move(v2).transform(fn); // mutable rvalue
    const auto v4 = v3.transform(fn);            // const lvalue
    auto v5 = std::move(v4).transform(fn);       // const rvalue
    check_eq(v1, error{sec::runtime_error});
    check_eq(v2, error{}); // has been moved-from
    check_eq(v3, error{sec::runtime_error});
    check_eq(v4, error{sec::runtime_error});
    check_eq(v5, error{sec::runtime_error});
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

TEST("or_else may replace the error or set a default") {
  SECTION("non-void value type") {
    next_error_t<int, true> next_error;
    auto set_fallback = [](auto&& err) {
      if constexpr (std::is_same_v<decltype(err), error&&>) {
        // Just for testing that we get indeed an rvalue in the test.
        err = error{};
      }
      return e_int{42};
    };
    SECTION("or_else makes copies when called on a mutable lvalue") {
      auto v1 = e_int{sec::runtime_error};
      auto v2 = v1.or_else(next_error);
      check_eq(v1, sec::runtime_error);
      check_eq(v2, sec::remote_linking_failed);
      auto v3 = v2.or_else(set_fallback);
      check_eq(v2, sec::remote_linking_failed);
      check_eq(v3, 42);
    }
    SECTION("or_else makes copies when called on a const lvalue") {
      const auto v1 = e_int{sec::runtime_error};
      const auto v2 = v1.or_else(next_error);
      check_eq(v1, sec::runtime_error);
      check_eq(v2, sec::remote_linking_failed);
      const auto v3 = v2.or_else(set_fallback);
      check_eq(v2, sec::remote_linking_failed);
      check_eq(v3, 42);
    }
    SECTION("or_else moves when called on a mutable rvalue") {
      auto v1 = e_int{sec::runtime_error};
      auto v2 = std::move(v1).or_else(next_error);
      check_eq(v1, error{});
      check_eq(v2, sec::remote_linking_failed);
      auto v3 = std::move(v2).or_else(set_fallback);
      check_eq(v2, error{});
      check_eq(v3, 42);
    }
    SECTION("or_else makes copies when called on a const rvalue") {
      const auto v1 = e_int{sec::runtime_error};
      const auto v2 = std::move(v1).or_else(next_error);
      check_eq(v1, sec::runtime_error);
      check_eq(v2, sec::remote_linking_failed);
      const auto v3 = std::move(v2).or_else(set_fallback);
      check_eq(v2, sec::remote_linking_failed);
      check_eq(v3, 42);
    }
  }
  SECTION("void value type") {
    next_error_t<void, true> next_error;
    auto set_fallback = [](auto&& err) {
      if constexpr (std::is_same_v<decltype(err), error&&>) {
        // Just for testing that we get indeed an rvalue in the test.
        err = error{};
      }
      return e_void{};
    };
    SECTION("or_else makes copies when called on a mutable lvalue") {
      auto v1 = e_void{sec::runtime_error};
      auto v2 = v1.or_else(next_error);
      check_eq(v1, sec::runtime_error);
      check_eq(v2, sec::remote_linking_failed);
      auto v3 = v2.or_else(set_fallback);
      check_eq(v2, sec::remote_linking_failed);
      check(static_cast<bool>(v3));
    }
    SECTION("or_else makes copies when called on a const lvalue") {
      const auto v1 = e_void{sec::runtime_error};
      const auto v2 = v1.or_else(next_error);
      check_eq(v1, sec::runtime_error);
      check_eq(v2, sec::remote_linking_failed);
      const auto v3 = v2.or_else(set_fallback);
      check_eq(v2, sec::remote_linking_failed);
      check(static_cast<bool>(v3));
    }
    SECTION("or_else moves when called on a mutable rvalue") {
      auto v1 = e_void{sec::runtime_error};
      auto v2 = std::move(v1).or_else(next_error);
      check_eq(v1, error{});
      check_eq(v2, sec::remote_linking_failed);
      auto v3 = std::move(v2).or_else(set_fallback);
      check_eq(v2, error{});
      check(static_cast<bool>(v3));
    }
    SECTION("or_else makes copies when called on a const rvalue") {
      const auto v1 = e_void{sec::runtime_error};
      const auto v2 = std::move(v1).or_else(next_error);
      check_eq(v1, sec::runtime_error);
      check_eq(v2, sec::remote_linking_failed);
      const auto v3 = std::move(v2).or_else(set_fallback);
      check_eq(v2, sec::remote_linking_failed);
      check(static_cast<bool>(v3));
    }
  }
}

TEST("or_else leaves the expected unchanged when returning void") {
  SECTION("non-void value type") {
    auto i = 0;
    auto inc = [&i](auto&&) { ++i; };
    auto v1 = e_int{sec::runtime_error};
    auto v2 = v1.or_else(inc);                  // mutable lvalue
    const auto v3 = std::move(v2).or_else(inc); // mutable rvalue
    const auto v4 = v3.or_else(inc);            // const lvalue
    auto v5 = std::move(v4).or_else(inc);       // const rvalue
    check_eq(v1, error{sec::runtime_error});
    check_eq(v2, error{}); // has been moved-from
    check_eq(v3, error{sec::runtime_error});
    check_eq(v4, error{sec::runtime_error});
    check_eq(v5, error{sec::runtime_error});
    check_eq(i, 4);
  }
  SECTION("void value type") {
    auto i = 0;
    auto inc = [&i](auto&&) { ++i; };
    auto v1 = e_void{sec::runtime_error};
    auto v2 = v1.or_else(inc);                  // mutable lvalue
    const auto v3 = std::move(v2).or_else(inc); // mutable rvalue
    const auto v4 = v3.or_else(inc);            // const lvalue
    auto v5 = std::move(v4).or_else(inc);       // const rvalue
    check_eq(v1, error{sec::runtime_error});
    check_eq(v2, error{}); // has been moved-from
    check_eq(v3, error{sec::runtime_error});
    check_eq(v4, error{sec::runtime_error});
    check_eq(v5, error{sec::runtime_error});
    check_eq(i, 4);
  }
}

TEST("or_else does nothing when called with a value") {
  SECTION("non-void value type") {
    auto uh_oh = [this](auto&&) { fail("uh-oh called!"); };
    auto i = make_counted<counted_int>(1);
    auto v1 = e_iptr{std::in_place, i};
    auto v2 = v1.or_else(uh_oh);                  // mutable lvalue
    const auto v3 = std::move(v2).or_else(uh_oh); // mutable rvalue
    const auto v4 = v3.or_else(uh_oh);            // const lvalue
    auto v5 = std::move(v4).or_else(uh_oh);       // const rvalue
    check_eq(v1, i);
    check_eq(v2, nullptr); // has been moved-from
    check_eq(v3, i);
    check_eq(v4, i);
    check_eq(v5, i);
  }
  SECTION("void value type") {
    auto uh_oh = [this](auto&&) { fail("uh-oh called!"); };
    auto v1 = e_void{};
    auto v2 = v1.or_else(uh_oh);                  // mutable lvalue
    const auto v3 = std::move(v2).or_else(uh_oh); // mutable rvalue
    const auto v4 = v3.or_else(uh_oh);            // const lvalue
    auto v5 = std::move(v4).or_else(uh_oh);       // const rvalue
    check(static_cast<bool>(v1));
    check(static_cast<bool>(v2));
    check(static_cast<bool>(v3));
    check(static_cast<bool>(v4));
    check(static_cast<bool>(v5));
  }
}

TEST("transform_or may replace the error or set a default") {
  SECTION("non-void value type") {
    next_error_t<int, false> next_error;
    SECTION("transform_or makes copies when called on a mutable lvalue") {
      auto v1 = e_int{sec::runtime_error};
      auto v2 = v1.transform_or(next_error);
      check_eq(v1, sec::runtime_error);
      check_eq(v2, sec::remote_linking_failed);
    }
    SECTION("transform_or makes copies when called on a const lvalue") {
      const auto v1 = e_int{sec::runtime_error};
      const auto v2 = v1.transform_or(next_error);
      check_eq(v1, sec::runtime_error);
      check_eq(v2, sec::remote_linking_failed);
    }
    SECTION("transform_or moves when called on a mutable rvalue") {
      auto v1 = e_int{sec::runtime_error};
      auto v2 = std::move(v1).transform_or(next_error);
      check_eq(v1, error{});
      check_eq(v2, sec::remote_linking_failed);
    }
    SECTION("transform_or makes copies when called on a const rvalue") {
      const auto v1 = e_int{sec::runtime_error};
      const auto v2 = std::move(v1).transform_or(next_error);
      check_eq(v1, sec::runtime_error);
      check_eq(v2, sec::remote_linking_failed);
    }
  }
  SECTION("void value type") {
    next_error_t<void, false> next_error;
    SECTION("transform_or makes copies when called on a mutable lvalue") {
      auto v1 = e_void{sec::runtime_error};
      auto v2 = v1.transform_or(next_error);
      check_eq(v1, sec::runtime_error);
      check_eq(v2, sec::remote_linking_failed);
    }
    SECTION("transform_or makes copies when called on a const lvalue") {
      const auto v1 = e_void{sec::runtime_error};
      const auto v2 = v1.transform_or(next_error);
      check_eq(v1, sec::runtime_error);
      check_eq(v2, sec::remote_linking_failed);
    }
    SECTION("transform_or moves when called on a mutable rvalue") {
      auto v1 = e_void{sec::runtime_error};
      auto v2 = std::move(v1).transform_or(next_error);
      check_eq(v1, error{});
      check_eq(v2, sec::remote_linking_failed);
    }
    SECTION("transform_or makes copies when called on a const rvalue") {
      const auto v1 = e_void{sec::runtime_error};
      const auto v2 = std::move(v1).transform_or(next_error);
      check_eq(v1, sec::runtime_error);
      check_eq(v2, sec::remote_linking_failed);
    }
  }
}

TEST("transform_or does nothing when called with a value") {
  SECTION("non-void value type") {
    auto uh_oh = [this](auto&&) {
      fail("uh-oh called!");
      return error{};
    };
    auto i = make_counted<counted_int>(1);
    auto v1 = e_iptr{std::in_place, i};
    auto v2 = v1.transform_or(uh_oh);                  // mutable lvalue
    const auto v3 = std::move(v2).transform_or(uh_oh); // mutable rvalue
    const auto v4 = v3.transform_or(uh_oh);            // const lvalue
    auto v5 = std::move(v4).transform_or(uh_oh);       // const rvalue
    check_eq(v1, i);
    check_eq(v2, nullptr); // has been moved-from
    check_eq(v3, i);
    check_eq(v4, i);
    check_eq(v5, i);
  }
  SECTION("void value type") {
    auto uh_oh = [this](auto&&) {
      fail("uh-oh called!");
      return error{};
    };
    auto v1 = e_void{};
    auto v2 = v1.transform_or(uh_oh);                  // mutable lvalue
    const auto v3 = std::move(v2).transform_or(uh_oh); // mutable rvalue
    const auto v4 = v3.transform_or(uh_oh);            // const lvalue
    auto v5 = std::move(v4).transform_or(uh_oh);       // const rvalue
    check(static_cast<bool>(v1));
    check(static_cast<bool>(v2));
    check(static_cast<bool>(v3));
    check(static_cast<bool>(v4));
    check(static_cast<bool>(v5));
  }
}

// NOLINTEND(bugprone-use-after-move)
