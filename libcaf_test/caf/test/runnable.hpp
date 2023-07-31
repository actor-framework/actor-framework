// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/binary_predicate.hpp"
#include "caf/test/block_type.hpp"
#include "caf/test/fwd.hpp"
#include "caf/test/reporter.hpp"

#include "caf/config.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/detail/format.hpp"
#include "caf/detail/source_location.hpp"
#include "caf/detail/test_export.hpp"

#include <string_view>

namespace caf::test {

namespace {

template <class T0, class T1>
inline void assert_integral_comparison(const T0& lhs, const T1& rhs) {
  if (std::is_integral_v<T0> && std::is_integral_v<T1>) {
    static_assert(std::is_signed_v<T0> == std::is_signed_v<T1>,
                  "comparing signed and unsigned integers is unsafe");
  }
}

}

/// A runnable definition of a test case or scenario.
class CAF_TEST_EXPORT runnable {
public:
  /// Creates a new runnable.
  /// @param ctx The test context.
  /// @param description A description of the test or scenario.
  /// @param root_type The type of the root block.
  /// @param loc The source location of the test or scenario.
  runnable(context_ptr ctx, std::string_view description, block_type root_type,
           const detail::source_location& loc
           = detail::source_location::current())
    : ctx_(std::move(ctx)),
      description_(description),
      root_type_(root_type),
      loc_(loc) {
    // nop
  }

  virtual ~runnable();

  /// Runs the next branch of the test.
  void run();

  /// Generates a message with the INFO severity level.
  template <class... Ts>
  void info(detail::format_string_with_location fwl, Ts&&... xs) {
    if constexpr (sizeof...(Ts) > 0) {
      auto msg = detail::format(fwl.value, std::forward<Ts>(xs)...);
      reporter::instance->info(msg, fwl.location);
    } else {
      reporter::instance->info(fwl.value, fwl.location);
    }
  }

  /// Checks whether `lhs` and `rhs` are equal.
  template <class T0, class T1>
  bool check_eq(const T0& lhs, const T1& rhs,
                const detail::source_location& location
                = detail::source_location::current()) {
    assert_integral_comparison(lhs, rhs);
    if (lhs == rhs) {
      reporter::instance->pass(location);
      return true;
    }
    reporter::instance->fail(binary_predicate::eq, stringify(lhs),
                             stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` and `rhs` are unequal.
  template <class T0, class T1>
  bool check_ne(const T0& lhs, const T1& rhs,
                const detail::source_location& location
                = detail::source_location::current()) {
    assert_integral_comparison(lhs, rhs);
    if (lhs != rhs) {
      reporter::instance->pass(location);
      return true;
    }
    reporter::instance->fail(binary_predicate::ne, stringify(lhs),
                             stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` is less than `rhs`.
  template <class T0, class T1>
  bool check_lt(const T0& lhs, const T1& rhs,
                const detail::source_location& location
                = detail::source_location::current()) {
    assert_integral_comparison(lhs, rhs);
    if (lhs < rhs) {
      reporter::instance->pass(location);
      return true;
    }
    reporter::instance->fail(binary_predicate::lt, stringify(lhs),
                             stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` less than or equal to `rhs`.
  template <class T0, class T1>
  bool check_le(const T0& lhs, const T1& rhs,
                const detail::source_location& location
                = detail::source_location::current()) {
    assert_integral_comparison(lhs, rhs);
    if (lhs <= rhs) {
      reporter::instance->pass(location);
      return true;
    }
    reporter::instance->fail(binary_predicate::le, stringify(lhs),
                             stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` is greater than `rhs`.
  template <class T0, class T1>
  bool check_gt(const T0& lhs, const T1& rhs,
                const detail::source_location& location
                = detail::source_location::current()) {
    assert_integral_comparison(lhs, rhs);
    if (lhs > rhs) {
      reporter::instance->pass(location);
      return true;
    }
    reporter::instance->fail(binary_predicate::gt, stringify(lhs),
                             stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` greater than or equal to `rhs`.
  template <class T0, class T1>
  bool check_ge(const T0& lhs, const T1& rhs,
                const detail::source_location& location
                = detail::source_location::current()) {
    assert_integral_comparison(lhs, rhs);
    if (lhs >= rhs) {
      reporter::instance->pass(location);
      return true;
    }
    reporter::instance->fail(binary_predicate::ge, stringify(lhs),
                             stringify(rhs), location);
    return false;
  }

  bool check(bool value, const detail::source_location& location
                         = detail::source_location::current());

  block& current_block();

#ifdef CAF_ENABLE_EXCEPTIONS
  template <class Exception = void, class CodeBlock>
  void check_throws(CodeBlock&& fn, const detail::source_location& location
                                    = detail::source_location::current()) {
    if constexpr (std::is_same_v<Exception, void>) {
      try {
        fn();
      } catch (...) {
        reporter::instance->pass(location);
        return;
      }
      reporter::instance->fail("throws", location);
    } else {
      try {
        fn();
      } catch (const Exception&) {
        reporter::instance->pass(location);
        return;
      } catch (...) {
        reporter::instance->fail("throws Exception", location);
        return;
      }
      reporter::instance->fail("throws Exception", location);
    }
  }
#endif

  template <class T>
  void should_fail(T expr, const caf::detail::source_location& location
                           = caf::detail::source_location::current()) {
    auto* rep = caf::test::reporter::instance;
    auto before = rep->test_stats();
    expr();
    auto after = rep->test_stats();
    check_eq(before.passed, after.passed, location);
    if (check_eq(before.failed + 1, after.failed, location))
      rep->test_stats({before.passed + 1, before.failed});
  }

protected:
  context_ptr ctx_;
  std::string_view description_;
  block_type root_type_;
  detail::source_location loc_;

private:
  template <class T>
  std::string stringify(const T& value) {
    if constexpr (std::is_convertible_v<T, std::string>) {
      return std::string{value};
    } else {
      return caf::deep_to_string(value);
    }
  }

  virtual void do_run() = 0;
};

} // namespace caf::test
