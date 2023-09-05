// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/binary_predicate.hpp"
#include "caf/test/block.hpp"
#include "caf/test/block_type.hpp"
#include "caf/test/context.hpp"
#include "caf/test/fwd.hpp"
#include "caf/test/reporter.hpp"
#include "caf/test/requirement_failed.hpp"

#include "caf/config.hpp"
#include "caf/config_value.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/detail/format.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/source_location.hpp"
#include "caf/detail/test_export.hpp"
#include "caf/expected.hpp"
#include "caf/raise_error.hpp"

#include <string_view>

namespace caf::test {

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
  virtual void run();

  /// Generates a message with the INFO severity level.
  template <class... Ts>
  [[noreturn]] void fail(detail::format_string_with_location fwl, Ts&&... xs) {
    auto msg = detail::format(fwl.value, std::forward<Ts>(xs)...);
    reporter::instance().fail(msg, fwl.location);
    requirement_failed::raise(fwl.location);
  }

  /// Generates a message with the INFO severity level.
  template <class... Ts>
  void print_info(detail::format_string_with_location fwl, Ts&&... xs) {
    auto msg = detail::format(fwl.value, std::forward<Ts>(xs)...);
    reporter::instance().print_info(msg, fwl.location);
  }

  /// Generates a message with the DEBUG severity level.
  template <class... Ts>
  void print_debug(detail::format_string_with_location fwl, Ts&&... xs) {
    auto msg = detail::format(fwl.value, std::forward<Ts>(xs)...);
    reporter::instance().print_debug(msg, fwl.location);
  }

  /// Generates a message with the WARNING severity level.
  template <class... Ts>
  void print_warning(detail::format_string_with_location fwl, Ts&&... xs) {
    auto msg = detail::format(fwl.value, std::forward<Ts>(xs)...);
    reporter::instance().print_warning(msg, fwl.location);
  }

  /// Generates a message with the ERROR severity level.
  template <class... Ts>
  void print_error(detail::format_string_with_location fwl, Ts&&... xs) {
    auto msg = detail::format(fwl.value, std::forward<Ts>(xs)...);
    reporter::instance().print_error(msg, fwl.location);
  }

  /// Checks whether `lhs` and `rhs` are equal.
  template <class T0, class T1>
  bool check_eq(const T0& lhs, const T1& rhs,
                const detail::source_location& location
                = detail::source_location::current()) {
    assert_save_comparison<T0, T1>();
    if (lhs == rhs) {
      reporter::instance().pass(location);
      return true;
    }
    reporter::instance().fail(binary_predicate::eq, stringify(lhs),
                              stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` and `rhs` are unequal.
  template <class T0, class T1>
  bool check_ne(const T0& lhs, const T1& rhs,
                const detail::source_location& location
                = detail::source_location::current()) {
    assert_save_comparison<T0, T1>();
    if (lhs != rhs) {
      reporter::instance().pass(location);
      return true;
    }
    reporter::instance().fail(binary_predicate::ne, stringify(lhs),
                              stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` is less than `rhs`.
  template <class T0, class T1>
  bool check_lt(const T0& lhs, const T1& rhs,
                const detail::source_location& location
                = detail::source_location::current()) {
    assert_save_comparison<T0, T1>();
    if (lhs < rhs) {
      reporter::instance().pass(location);
      return true;
    }
    reporter::instance().fail(binary_predicate::lt, stringify(lhs),
                              stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` less than or equal to `rhs`.
  template <class T0, class T1>
  bool check_le(const T0& lhs, const T1& rhs,
                const detail::source_location& location
                = detail::source_location::current()) {
    assert_save_comparison<T0, T1>();
    if (lhs <= rhs) {
      reporter::instance().pass(location);
      return true;
    }
    reporter::instance().fail(binary_predicate::le, stringify(lhs),
                              stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` is greater than `rhs`.
  template <class T0, class T1>
  bool check_gt(const T0& lhs, const T1& rhs,
                const detail::source_location& location
                = detail::source_location::current()) {
    assert_save_comparison<T0, T1>();
    if (lhs > rhs) {
      reporter::instance().pass(location);
      return true;
    }
    reporter::instance().fail(binary_predicate::gt, stringify(lhs),
                              stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` greater than or equal to `rhs`.
  template <class T0, class T1>
  bool check_ge(const T0& lhs, const T1& rhs,
                const detail::source_location& location
                = detail::source_location::current()) {
    assert_save_comparison<T0, T1>();
    if (lhs >= rhs) {
      reporter::instance().pass(location);
      return true;
    }
    reporter::instance().fail(binary_predicate::ge, stringify(lhs),
                              stringify(rhs), location);
    return false;
  }

  /// Checks whether `value` is `true`.
  bool check(bool value, const detail::source_location& location
                         = detail::source_location::current());

  /// Evaluates whether `lhs` and `rhs` are equal and fails otherwise.
  template <class T0, class T1>
  void require_eq(const T0& lhs, const T1& rhs,
                  const detail::source_location& location
                  = detail::source_location::current()) {
    if (!check_eq(lhs, rhs, location))
      requirement_failed::raise(location);
  }

  /// Evaluates whether `lhs` and `rhs` are unequal and fails otherwise.
  template <class T0, class T1>
  void require_ne(const T0& lhs, const T1& rhs,
                  const detail::source_location& location
                  = detail::source_location::current()) {
    if (!check_ne(lhs, rhs, location))
      requirement_failed::raise(location);
  }

  /// Evaluates whether `lhs` is less than `rhs` and fails otherwise
  template <class T0, class T1>
  void require_lt(const T0& lhs, const T1& rhs,
                  const detail::source_location& location
                  = detail::source_location::current()) {
    if (!check_lt(lhs, rhs, location))
      requirement_failed::raise(location);
  }

  /// Evaluates whether `lhs` less than or equal to `rhs` and fails otherwise.
  template <class T0, class T1>
  void require_le(const T0& lhs, const T1& rhs,
                  const detail::source_location& location
                  = detail::source_location::current()) {
    if (!check_le(lhs, rhs, location))
      requirement_failed::raise(location);
  }

  /// Evaluates whether `lhs` is greater than `rhs` and fails otherwise.
  template <class T0, class T1>
  void require_gt(const T0& lhs, const T1& rhs,
                  const detail::source_location& location
                  = detail::source_location::current()) {
    if (!check_gt(lhs, rhs, location))
      requirement_failed::raise(location);
  }

  /// Evaluates whether `lhs` greater than or equal to `rhs` and fails
  /// otherwise.
  template <class T0, class T1>
  void require_ge(const T0& lhs, const T1& rhs,
                  const detail::source_location& location
                  = detail::source_location::current()) {
    if (!check_ge(lhs, rhs, location))
      requirement_failed::raise(location);
  }

  /// Evaluates whether `value` is `true` and fails otherwise.
  void require(bool value, const detail::source_location& location
                           = detail::source_location::current());

  /// Returns the `runnable` instance that is currently running.
  static runnable& current();

  block& current_block();

  template <class Expr>
  void should_fail(Expr&& expr, const caf::detail::source_location& location
                                = caf::detail::source_location::current()) {
    auto& rep = reporter::instance();
    auto lvl = rep.verbosity(CAF_LOG_LEVEL_QUIET);
    auto before = rep.test_stats();
    {
      auto lvl_guard = detail::make_scope_guard([&] { rep.verbosity(lvl); });
      expr();
    }
    auto after = rep.test_stats();
    auto passed_count_ok = before.passed == after.passed;
    auto failed_count_ok = before.failed + 1 == after.failed;
    if (passed_count_ok && failed_count_ok) {
      reporter::instance().pass(location);
      rep.test_stats({before.passed + 1, before.failed});
    } else {
      reporter::instance().fail("nested check should fail", location);
      rep.test_stats({before.passed, before.failed + 1});
    }
  }

  template <class... Ts>
  auto block_parameters() {
    static_assert(sizeof...(Ts) != 0);
    const auto& params = current_block().parameter_names();
    if (params.size() != sizeof...(Ts))
      CAF_RAISE_ERROR(std::logic_error,
                      "block_parameters: invalid number of parameters");
    std::array<config_value, sizeof...(Ts)> cfg_vals;
    for (size_t index = 0; index < sizeof...(Ts); ++index)
      cfg_vals[index] = ctx_->parameter(params[index]);
    auto ok = false;
    auto vals = convert_all<Ts...>(cfg_vals, std::index_sequence_for<Ts...>{},
                                   ok);
    if (!ok)
      CAF_RAISE_ERROR(std::logic_error,
                      "block_parameters: conversion(s) failed");
    if constexpr (sizeof...(Ts) == 1)
      return std::move(*std::get<0>(vals));
    else
      return unbox_all(vals, std::index_sequence_for<Ts...>{});
  }

#ifdef CAF_ENABLE_EXCEPTIONS

  /// Checks whether `expr()` throws an exception of type `Exception`.
  template <class Exception = void, class Expr>
  void check_throws(Expr&& expr, const detail::source_location& location
                                 = detail::source_location::current()) {
    if constexpr (std::is_same_v<Exception, void>) {
      try {
        expr();
      } catch (...) {
        reporter::instance().pass(location);
        return;
      }
      reporter::instance().fail("throws", location);
    } else {
      try {
        expr();
      } catch (const Exception&) {
        reporter::instance().pass(location);
        return;
      } catch (...) {
        reporter::instance().fail("throws Exception", location);
        return;
      }
      reporter::instance().fail("throws Exception", location);
    }
  }

  /// Checks whether `expr()` throws an exception of type `Exception` and
  /// increases the failure count.
  template <class Exception = void, class Expr>
  void should_fail_with_exception(Expr&& expr,
                                  const caf::detail::source_location& location
                                  = caf::detail::source_location::current()) {
    auto& rep = reporter::instance();
    auto before = rep.test_stats();
    auto lvl = rep.verbosity(CAF_LOG_LEVEL_QUIET);
    auto caught = false;
    if constexpr (std::is_same_v<Exception, void>) {
      try {
        expr();
      } catch (...) {
        caught = true;
      }
    } else {
      try {
        expr();
      } catch (const Exception&) {
        caught = true;
      } catch (...) {
        // TODO: print error message
      }
    }
    rep.verbosity(lvl);
    auto after = rep.test_stats();
    auto passed_count_ok = before.passed == after.passed;
    auto failed_count_ok = before.failed + 1 == after.failed;
    if (caught && passed_count_ok && failed_count_ok) {
      reporter::instance().pass(location);
      rep.test_stats({before.passed + 1, before.failed});
    } else {
      if (!caught) {
        reporter::instance().fail("nested check should throw an Exception",
                                  location);
      } else if (!passed_count_ok || !failed_count_ok) {
        reporter::instance().fail("nested check should fail", location);
      }
      rep.test_stats({before.passed, before.failed + 1});
    }
  }

#endif

protected:
  void call_do_run();

  context_ptr ctx_;
  std::string_view description_;
  block_type root_type_;
  detail::source_location loc_;

private:
  template <class... Ts, class Array, size_t... Is>
  auto convert_all(Array& arr, std::index_sequence<Is...>, bool& ok) {
    auto result = std::make_tuple(get_as<Ts>(arr[Is])...);
    ok = (std::get<Is>(result).has_value() && ...);
    return result;
  }

  template <class... Ts, size_t... Is>
  auto unbox_all(std::tuple<Ts...>& vals, std::index_sequence<Is...>) {
    return std::make_tuple(*std::move(std::get<Is>(vals))...);
  }

  template <class T0, class T1>
  static void assert_save_comparison() {
    if constexpr (std::is_integral_v<T0> && std::is_integral_v<T1>) {
      static_assert(std::is_signed_v<T0> == std::is_signed_v<T1>,
                    "comparing signed and unsigned integers is unsafe");
    }
  }

  template <class T>
  static auto stringify(const T& value) {
    if constexpr (std::is_convertible_v<T, std::string>) {
      return std::string{value};
    } else {
      return caf::deep_to_string(value);
    }
  }

  virtual void do_run() = 0;
};

} // namespace caf::test
