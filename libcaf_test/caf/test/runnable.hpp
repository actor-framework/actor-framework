// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/test/approx.hpp"
#include "caf/test/binary_predicate.hpp"
#include "caf/test/block.hpp"
#include "caf/test/block_type.hpp"
#include "caf/test/context.hpp"
#include "caf/test/fwd.hpp"
#include "caf/test/reporter.hpp"
#include "caf/test/requirement_failed.hpp"

#include "caf/callback.hpp"
#include "caf/config_value.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/detail/format.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/test_export.hpp"
#include "caf/expected.hpp"
#include "caf/format_string_with_location.hpp"
#include "caf/fwd.hpp"
#include "caf/log/level.hpp"
#include "caf/raise_error.hpp"
#include "caf/telemetry/label_view.hpp"

#include <concepts>
#include <source_location>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace caf::detail {

template <class T>
concept metric_predicate_value
  = std::is_floating_point_v<T>
    || (std::is_integral_v<T> && std::is_signed_v<T>);

template <class T>
struct metric_predicate_signature_oracle;

template <std::integral T>
struct metric_predicate_signature_oracle<T> {
  using type = bool(int64_t);
};

template <std::floating_point T>
struct metric_predicate_signature_oracle<T> {
  using type = bool(double);
};

template <class T>
using metric_predicate_signature =
  typename metric_predicate_signature_oracle<T>::type;

template <class T0, class T1>
concept safely_int_comparable
  = !(std::is_integral_v<T0> && std::is_integral_v<T1>)
    || (std::is_signed_v<T0> == std::is_signed_v<T1>);

template <class T>
std::string test_stringify(const T& value) {
  if constexpr (std::is_same_v<T, std::nullptr_t>) {
    return "null";
  } else if constexpr (std::is_convertible_v<T, std::string>) {
    return std::string{value};
  } else {
    return deep_to_string(value);
  }
}

template <class... Ts, class Array, size_t... Is>
auto test_convert_all(Array& arr, std::index_sequence<Is...>, bool& ok) {
  auto result = std::make_tuple(get_as<Ts>(arr[Is])...);
  ok = (std::get<Is>(result).has_value() && ...);
  return result;
}

template <class... Ts, size_t... Is>
auto test_unbox_all(std::tuple<Ts...>& vals, std::index_sequence<Is...>) {
  return std::make_tuple(*std::move(std::get<Is>(vals))...);
}

} // namespace caf::detail

namespace caf::test {

/// A runnable definition of a test case or scenario.
class CAF_TEST_EXPORT runnable {
public:
  struct runnable_state;

  friend class runnable_with_examples;
  friend class runner;

  /// Creates a new runnable.
  /// @param ctx The test context.
  /// @param description A description of the test or scenario.
  /// @param root_type The type of the root block.
  /// @param loc The source location of the test or scenario.
  runnable(context_ptr ctx, std::string_view description, block_type root_type,
           const std::source_location& loc = std::source_location::current());

  virtual ~runnable();

  /// Sets the current metric registry.
  void current_metric_registry(const telemetry::metric_registry* ptr);

  /// Returns the current metric registry.
  [[nodiscard]] const telemetry::metric_registry*
  current_metric_registry() const noexcept;

  /// Sets the poll interval for checks against the metric registry.
  void metric_registry_poll_interval(timespan interval);

  /// Returns the poll interval for checks against the metric registry.
  [[nodiscard]] timespan metric_registry_poll_interval() const noexcept;

  /// Sets the timeout for checks against the metric registry.
  void metric_registry_poll_timeout(timespan timeout);

  /// Returns the timeout for checks against the metric registry.
  [[nodiscard]] timespan metric_registry_poll_timeout() const noexcept;

  /// Records a failure with the given message and aborts the test by raising
  /// requirement_failed.
  template <class... Ts>
  [[noreturn]] void fail(format_string_with_location fwl, Ts&&... xs) {
    auto msg = detail::format(fwl.value, std::forward<Ts>(xs)...);
    reporter::instance().fail(msg, fwl.location);
    requirement_failed::raise(fwl.location);
  }

  /// Checks whether `lhs` and `rhs` are equal.
  template <class T0, class T1>
  bool check_eq(const T0& lhs, const T1& rhs,
                const std::source_location& location
                = std::source_location::current()) {
    static_assert(detail::safely_int_comparable<T0, T1>,
                  "comparing signed and unsigned integers is unsafe");
    static_assert(!std::is_floating_point_v<T0>
                    || !std::is_floating_point_v<T1>,
                  "comparing floating points using '==' is unsafe, use approx");
    if (lhs == rhs) {
      reporter::instance().pass(location);
      return true;
    }
    reporter::instance().fail(binary_predicate::eq, detail::test_stringify(lhs),
                              detail::test_stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` and `rhs` are unequal.
  template <class T0, class T1>
  bool check_ne(const T0& lhs, const T1& rhs,
                const std::source_location& location
                = std::source_location::current()) {
    static_assert(detail::safely_int_comparable<T0, T1>,
                  "comparing signed and unsigned integers is unsafe");
    static_assert(!std::is_floating_point_v<T0>
                    || !std::is_floating_point_v<T1>,
                  "comparing floating points using '!=' is unsafe, use approx");
    if (lhs != rhs) {
      reporter::instance().pass(location);
      return true;
    }
    reporter::instance().fail(binary_predicate::ne, detail::test_stringify(lhs),
                              detail::test_stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` is less than `rhs`.
  template <class T0, class T1>
  bool check_lt(const T0& lhs, const T1& rhs,
                const std::source_location& location
                = std::source_location::current()) {
    static_assert(detail::safely_int_comparable<T0, T1>,
                  "comparing signed and unsigned integers is unsafe");
    if (lhs < rhs) {
      reporter::instance().pass(location);
      return true;
    }
    reporter::instance().fail(binary_predicate::lt, detail::test_stringify(lhs),
                              detail::test_stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` is less than or equal to `rhs`.
  template <class T0, class T1>
  bool check_le(const T0& lhs, const T1& rhs,
                const std::source_location& location
                = std::source_location::current()) {
    static_assert(detail::safely_int_comparable<T0, T1>,
                  "comparing signed and unsigned integers is unsafe");
    if (lhs <= rhs) {
      reporter::instance().pass(location);
      return true;
    }
    reporter::instance().fail(binary_predicate::le, detail::test_stringify(lhs),
                              detail::test_stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` is greater than `rhs`.
  template <class T0, class T1>
  bool check_gt(const T0& lhs, const T1& rhs,
                const std::source_location& location
                = std::source_location::current()) {
    static_assert(detail::safely_int_comparable<T0, T1>,
                  "comparing signed and unsigned integers is unsafe");
    if (lhs > rhs) {
      reporter::instance().pass(location);
      return true;
    }
    reporter::instance().fail(binary_predicate::gt, detail::test_stringify(lhs),
                              detail::test_stringify(rhs), location);
    return false;
  }

  /// Checks whether `lhs` is greater than or equal to `rhs`.
  template <class T0, class T1>
  bool check_ge(const T0& lhs, const T1& rhs,
                const std::source_location& location
                = std::source_location::current()) {
    static_assert(detail::safely_int_comparable<T0, T1>,
                  "comparing signed and unsigned integers is unsafe");
    if (lhs >= rhs) {
      reporter::instance().pass(location);
      return true;
    }
    reporter::instance().fail(binary_predicate::ge, detail::test_stringify(lhs),
                              detail::test_stringify(rhs), location);
    return false;
  }

  /// Checks whether `value` is `true`.
  bool check(bool value, const std::source_location& location
                         = std::source_location::current());

  /// Checks whether `str` matches the regular expression `rx`.
  bool check_matches(std::string_view str, std::string_view rx,
                     const std::source_location& location
                     = std::source_location::current());

  /// Checks whether `what` holds a value.
  template <class T>
  bool check_has_value(const std::optional<T>& what,
                       const std::source_location& location
                       = std::source_location::current()) {
    if (what.has_value()) {
      reporter::instance().pass(location);
      return true;
    }
    auto msg = detail::format("std::optional<T> is empty");
    reporter::instance().fail(msg, location);
    return false;
  }

  /// Checks whether `what` holds a value.
  template <class T>
  bool
  check_has_value(const expected<T>& what, const std::source_location& location
                                           = std::source_location::current()) {
    if (what.has_value()) {
      reporter::instance().pass(location);
      return true;
    }
    auto msg = detail::format("expected<T> contains an error: {}",
                              what.error());
    reporter::instance().fail(msg, location);
    return false;
  }

  /// Checks whether the metric has the given value or reaches the given value
  /// within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  template <std::integral ValueType>
  bool check_metric_eq(std::string_view prefix, std::string_view name,
                       ValueType value,
                       const std::source_location& location
                       = std::source_location::current()) {
    using signature = detail::metric_predicate_signature<ValueType>;
    auto fn = [value](auto other) { return other == value; };
    callback_ref_impl<decltype(fn), signature> pred{fn};
    return do_check_metric(prefix, name, {}, pred, location);
  }

  /// Checks whether the metric has the given value or reaches the given value
  /// within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  template <std::integral ValueType>
  bool check_metric_eq(std::string_view prefix, std::string_view name,
                       std::vector<telemetry::label_view> labels,
                       ValueType value,
                       const std::source_location& location
                       = std::source_location::current()) {
    using signature = detail::metric_predicate_signature<ValueType>;
    auto fn = [value](auto other) { return other == value; };
    callback_ref_impl<decltype(fn), signature> pred{fn};
    return do_check_metric(prefix, name, labels, pred, location);
  }

  /// Checks whether the metric has the given value or reaches the given value
  /// within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  bool check_metric_eq(std::string_view prefix, std::string_view name,
                       approx<double> value,
                       const std::source_location& location
                       = std::source_location::current()) {
    auto fn = [value](double other) { return other == value; };
    callback_ref_impl<decltype(fn), bool(double)> pred{fn};
    return do_check_metric(prefix, name, {}, pred, location);
  }

  /// Checks whether the metric has the given value or reaches the given value
  /// within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  bool check_metric_eq(std::string_view prefix, std::string_view name,
                       std::vector<telemetry::label_view> labels,
                       approx<double> value,
                       const std::source_location& location
                       = std::source_location::current()) {
    auto fn = [value](double other) { return other == value; };
    callback_ref_impl<decltype(fn), bool(double)> pred{fn};
    return do_check_metric(prefix, name, labels, pred, location);
  }

  /// Checks whether the metric has a value not equal to the given value or
  /// reaches such a value within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  template <std::integral ValueType>
  bool check_metric_ne(std::string_view prefix, std::string_view name,
                       ValueType value,
                       const std::source_location& location
                       = std::source_location::current()) {
    using signature = detail::metric_predicate_signature<ValueType>;
    auto fn = [value](auto other) { return other != value; };
    callback_ref_impl<decltype(fn), signature> pred{fn};
    return do_check_metric(prefix, name, {}, pred, location);
  }

  /// Checks whether the metric has a value not equal to the given value or
  /// reaches such a value within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  template <std::integral ValueType>
  bool check_metric_ne(std::string_view prefix, std::string_view name,
                       std::vector<telemetry::label_view> labels,
                       ValueType value,
                       const std::source_location& location
                       = std::source_location::current()) {
    using signature = detail::metric_predicate_signature<ValueType>;
    auto fn = [value](auto other) { return other != value; };
    callback_ref_impl<decltype(fn), signature> pred{fn};
    return do_check_metric(prefix, name, labels, pred, location);
  }

  /// Checks whether the metric has a value not equal to the given value (within
  /// an epsilon) or reaches such a value within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  bool check_metric_ne(std::string_view prefix, std::string_view name,
                       approx<double> value,
                       const std::source_location& location
                       = std::source_location::current()) {
    auto fn = [value](double other) { return other != value; };
    callback_ref_impl<decltype(fn), bool(double)> pred{fn};
    return do_check_metric(prefix, name, {}, pred, location);
  }

  /// Checks whether the metric has a value not equal to the given value (within
  /// an epsilon) or reaches such a value within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  bool check_metric_ne(std::string_view prefix, std::string_view name,
                       std::vector<telemetry::label_view> labels,
                       approx<double> value,
                       const std::source_location& location
                       = std::source_location::current()) {
    auto fn = [value](double other) { return other != value; };
    callback_ref_impl<decltype(fn), bool(double)> pred{fn};
    return do_check_metric(prefix, name, labels, pred, location);
  }

  /// Checks whether the metric has a value less than the given value or
  /// reaches such a value within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  bool check_metric_lt(std::string_view prefix, std::string_view name,
                       ValueType value,
                       const std::source_location& location
                       = std::source_location::current()) {
    using signature = detail::metric_predicate_signature<ValueType>;
    auto fn = [value](auto other) { return other < value; };
    callback_ref_impl<decltype(fn), signature> pred{fn};
    return do_check_metric(prefix, name, {}, pred, location);
  }

  /// Checks whether the metric has a value less than the given value or
  /// reaches such a value within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  bool check_metric_lt(std::string_view prefix, std::string_view name,
                       std::vector<telemetry::label_view> labels,
                       ValueType value,
                       const std::source_location& location
                       = std::source_location::current()) {
    using signature = detail::metric_predicate_signature<ValueType>;
    auto fn = [value](auto other) { return other < value; };
    callback_ref_impl<decltype(fn), signature> pred{fn};
    return do_check_metric(prefix, name, labels, pred, location);
  }

  /// Checks whether the metric has a value less than or equal to the given
  /// value or reaches such a value within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  bool check_metric_le(std::string_view prefix, std::string_view name,
                       ValueType value,
                       const std::source_location& location
                       = std::source_location::current()) {
    using signature = detail::metric_predicate_signature<ValueType>;
    auto fn = [value](auto other) { return other <= value; };
    callback_ref_impl<decltype(fn), signature> pred{fn};
    return do_check_metric(prefix, name, {}, pred, location);
  }

  /// Checks whether the metric has a value less than or equal to the given
  /// value or reaches such a value within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  bool check_metric_le(std::string_view prefix, std::string_view name,
                       std::vector<telemetry::label_view> labels,
                       ValueType value,
                       const std::source_location& location
                       = std::source_location::current()) {
    using signature = detail::metric_predicate_signature<ValueType>;
    auto fn = [value](auto other) { return other <= value; };
    callback_ref_impl<decltype(fn), signature> pred{fn};
    return do_check_metric(prefix, name, labels, pred, location);
  }

  /// Checks whether the metric has a value greater than the given value or
  /// reaches such a value within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  bool check_metric_gt(std::string_view prefix, std::string_view name,
                       ValueType value,
                       const std::source_location& location
                       = std::source_location::current()) {
    using signature = detail::metric_predicate_signature<ValueType>;
    auto fn = [value](auto other) { return other > value; };
    callback_ref_impl<decltype(fn), signature> pred{fn};
    return do_check_metric(prefix, name, {}, pred, location);
  }

  /// Checks whether the metric has a value greater than the given value or
  /// reaches such a value within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  bool check_metric_gt(std::string_view prefix, std::string_view name,
                       std::vector<telemetry::label_view> labels,
                       ValueType value,
                       const std::source_location& location
                       = std::source_location::current()) {
    using signature = detail::metric_predicate_signature<ValueType>;
    auto fn = [value](auto other) { return other > value; };
    callback_ref_impl<decltype(fn), signature> pred{fn};
    return do_check_metric(prefix, name, labels, pred, location);
  }

  /// Checks whether the metric has a value greater than or equal to the given
  /// value or reaches such a value within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  bool check_metric_ge(std::string_view prefix, std::string_view name,
                       ValueType value,
                       const std::source_location& location
                       = std::source_location::current()) {
    using signature = detail::metric_predicate_signature<ValueType>;
    auto fn = [value](auto other) { return other >= value; };
    callback_ref_impl<decltype(fn), signature> pred{fn};
    return do_check_metric(prefix, name, {}, pred, location);
  }

  /// Checks whether the metric has a value greater than or equal to the given
  /// value or reaches such a value within the configured timeout.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  bool check_metric_ge(std::string_view prefix, std::string_view name,
                       std::vector<telemetry::label_view> labels,
                       ValueType value,
                       const std::source_location& location
                       = std::source_location::current()) {
    using signature = detail::metric_predicate_signature<ValueType>;
    auto fn = [value](auto other) { return other >= value; };
    callback_ref_impl<decltype(fn), signature> pred{fn};
    return do_check_metric(prefix, name, labels, pred, location);
  }

  /// Evaluates whether `lhs` and `rhs` are equal and fails otherwise.
  template <class T0, class T1>
  void require_eq(const T0& lhs, const T1& rhs,
                  const std::source_location& location
                  = std::source_location::current()) {
    if (!check_eq(lhs, rhs, location))
      requirement_failed::raise(location);
  }

  /// Evaluates whether `lhs` and `rhs` are unequal and fails otherwise.
  template <class T0, class T1>
  void require_ne(const T0& lhs, const T1& rhs,
                  const std::source_location& location
                  = std::source_location::current()) {
    if (!check_ne(lhs, rhs, location))
      requirement_failed::raise(location);
  }

  /// Evaluates whether `lhs` is less than `rhs` and fails otherwise
  template <class T0, class T1>
  void require_lt(const T0& lhs, const T1& rhs,
                  const std::source_location& location
                  = std::source_location::current()) {
    if (!check_lt(lhs, rhs, location))
      requirement_failed::raise(location);
  }

  /// Evaluates whether `lhs` is less than or equal to `rhs` and fails
  /// otherwise.
  template <class T0, class T1>
  void require_le(const T0& lhs, const T1& rhs,
                  const std::source_location& location
                  = std::source_location::current()) {
    if (!check_le(lhs, rhs, location))
      requirement_failed::raise(location);
  }

  /// Evaluates whether `lhs` is greater than `rhs` and fails otherwise.
  template <class T0, class T1>
  void require_gt(const T0& lhs, const T1& rhs,
                  const std::source_location& location
                  = std::source_location::current()) {
    if (!check_gt(lhs, rhs, location))
      requirement_failed::raise(location);
  }

  /// Evaluates whether `lhs` is greater than or equal to `rhs` and fails
  /// otherwise.
  template <class T0, class T1>
  void require_ge(const T0& lhs, const T1& rhs,
                  const std::source_location& location
                  = std::source_location::current()) {
    if (!check_ge(lhs, rhs, location))
      requirement_failed::raise(location);
  }

  /// Evaluates whether `value` is `true` and fails otherwise.
  void require(bool value, const std::source_location& location
                           = std::source_location::current());

  /// Requires that `what` holds a value. Fails otherwise.
  template <class T>
  void require_has_value(const std::optional<T>& what,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_has_value(what, location)) {
      requirement_failed::raise(location);
    }
  }

  /// Requires that `what` holds a value. Fails otherwise.
  template <class T>
  void require_has_value(const expected<T>& what,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_has_value(what, location)) {
      requirement_failed::raise(location);
    }
  }

  /// Requires the metric to have the given value or reach it within the
  /// configured timeout. Fails otherwise.
  /// @pre `current_metric_registry() != nullptr`
  template <std::integral ValueType>
  void require_metric_eq(std::string_view prefix, std::string_view name,
                         ValueType value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_eq(prefix, name, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have the given value or reach it within the
  /// configured timeout. Fails otherwise.
  /// @pre `current_metric_registry() != nullptr`
  template <std::integral ValueType>
  void require_metric_eq(std::string_view prefix, std::string_view name,
                         std::vector<telemetry::label_view> labels,
                         ValueType value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_eq(prefix, name, labels, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have the given value or reach it within the
  /// configured timeout. Fails otherwise.
  /// @pre `current_metric_registry() != nullptr`
  void require_metric_eq(std::string_view prefix, std::string_view name,
                         approx<double> value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_eq(prefix, name, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have the given value or reach it within the
  /// configured timeout. Fails otherwise.
  /// @pre `current_metric_registry() != nullptr`
  void require_metric_eq(std::string_view prefix, std::string_view name,
                         std::vector<telemetry::label_view> labels,
                         approx<double> value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_eq(prefix, name, labels, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have a value not equal to the given value or
  /// reach such a value within the configured timeout. Fails otherwise.
  /// @pre `current_metric_registry() != nullptr`
  template <std::integral ValueType>
  void require_metric_ne(std::string_view prefix, std::string_view name,
                         ValueType value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_ne(prefix, name, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have a value not equal to the given value or
  /// reach such a value within the configured timeout. Fails otherwise.
  /// @pre `current_metric_registry() != nullptr`
  template <std::integral ValueType>
  void require_metric_ne(std::string_view prefix, std::string_view name,
                         std::vector<telemetry::label_view> labels,
                         ValueType value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_ne(prefix, name, labels, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have a value not equal to the given value
  /// (within an epsilon) or reach such a value within the configured
  /// timeout. Fails otherwise.
  /// @pre `current_metric_registry() != nullptr`
  void require_metric_ne(std::string_view prefix, std::string_view name,
                         approx<double> value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_ne(prefix, name, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have a value not equal to the given value
  /// (within an epsilon) or reach such a value within the configured
  /// timeout. Fails otherwise.
  /// @pre `current_metric_registry() != nullptr`
  void require_metric_ne(std::string_view prefix, std::string_view name,
                         std::vector<telemetry::label_view> labels,
                         approx<double> value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_ne(prefix, name, labels, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have a value less than the given value or reach
  /// such a value within the configured timeout. Fails otherwise.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  void require_metric_lt(std::string_view prefix, std::string_view name,
                         ValueType value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_lt(prefix, name, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have a value less than the given value or reach
  /// such a value within the configured timeout. Fails otherwise.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  void require_metric_lt(std::string_view prefix, std::string_view name,
                         std::vector<telemetry::label_view> labels,
                         ValueType value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_lt(prefix, name, labels, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have a value less than or equal to the given
  /// value or reach such a value within the configured timeout. Fails
  /// otherwise.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  void require_metric_le(std::string_view prefix, std::string_view name,
                         ValueType value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_le(prefix, name, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have a value less than or equal to the given
  /// value or reach such a value within the configured timeout. Fails
  /// otherwise.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  void require_metric_le(std::string_view prefix, std::string_view name,
                         std::vector<telemetry::label_view> labels,
                         ValueType value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_le(prefix, name, labels, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have a value greater than the given value or
  /// reach such a value within the configured timeout. Fails otherwise.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  void require_metric_gt(std::string_view prefix, std::string_view name,
                         ValueType value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_gt(prefix, name, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have a value greater than the given value or
  /// reach such a value within the configured timeout. Fails otherwise.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  void require_metric_gt(std::string_view prefix, std::string_view name,
                         std::vector<telemetry::label_view> labels,
                         ValueType value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_gt(prefix, name, labels, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have a value greater than or equal to the given
  /// value or reach such a value within the configured timeout. Fails
  /// otherwise.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  void require_metric_ge(std::string_view prefix, std::string_view name,
                         ValueType value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_ge(prefix, name, value, location))
      requirement_failed::raise(location);
  }

  /// Requires the metric to have a value greater than or equal to the given
  /// value or reach such a value within the configured timeout. Fails
  /// otherwise.
  /// @pre `current_metric_registry() != nullptr`
  template <detail::metric_predicate_value ValueType>
  void require_metric_ge(std::string_view prefix, std::string_view name,
                         std::vector<telemetry::label_view> labels,
                         ValueType value,
                         const std::source_location& location
                         = std::source_location::current()) {
    if (!check_metric_ge(prefix, name, labels, value, location))
      requirement_failed::raise(location);
  }

  template <class T>
  T unbox(const expected<T>& what, const std::source_location& location
                                   = std::source_location::current()) {
    require_has_value(what, location);
    return *what;
  }

  template <class T>
  T unbox(const std::optional<T>& what, const std::source_location& location
                                        = std::source_location::current()) {
    require_has_value(what, location);
    return *what;
  }

  /// Returns the `runnable` instance that is currently running.
  static runnable& current();

  block& current_block();

  template <class Expr>
  void should_fail(Expr&& expr, const std::source_location& location
                                = std::source_location::current()) {
    auto& rep = reporter::instance();
    auto lvl = rep.verbosity(log::level::quiet);
    auto before = rep.test_stats();
    {
      detail::scope_guard lvl_guard([&rep, lvl]() noexcept { //
        rep.verbosity(lvl);
      });
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
      cfg_vals[index] = test_context().parameter(params[index]);
    auto ok = false;
    auto vals = detail::test_convert_all<Ts...>(
      cfg_vals, std::index_sequence_for<Ts...>{}, ok);
    if (!ok)
      CAF_RAISE_ERROR(std::logic_error,
                      "block_parameters: conversion(s) failed");
    if constexpr (sizeof...(Ts) == 1)
      return std::move(*std::get<0>(vals));
    else
      return detail::test_unbox_all(vals, std::index_sequence_for<Ts...>{});
  }

  /// Returns the index of the current block in the list of block parameters.
  /// When running an outline, the index corresponds to the current example.
  size_t block_parameters_index() const {
    return test_context().example_id;
  }

#ifdef CAF_ENABLE_EXCEPTIONS

  /// Checks whether `expr()` throws an exception of type `Exception`.
  template <class Exception = void, class Expr>
  void check_throws(Expr&& expr, const std::source_location& location
                                 = std::source_location::current()) {
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
  void
  should_fail_with_exception(Expr&& expr, const std::source_location& location
                                          = std::source_location::current()) {
    auto& rep = reporter::instance();
    auto before = rep.test_stats();
    auto lvl = rep.verbosity(log::level::quiet);
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

  /// Returns the test context.
  caf::test::context& test_context() const noexcept;

  /// Returns the description of the runnable.
  std::string_view test_description() const noexcept;

  /// Returns the source location of the runnable.
  const std::source_location& test_location() const noexcept;

private:
  bool do_check_metric(std::string_view prefix, std::string_view name,
                       std::span<const telemetry::label_view> labels,
                       callback<bool(int64_t)>& pred,
                       const std::source_location& location);

  bool do_check_metric(std::string_view prefix, std::string_view name,
                       std::span<const telemetry::label_view> labels,
                       callback<bool(double)>& pred,
                       const std::source_location& location);

  void run_next_test_branch();

  virtual void run_next_test_branch_init();

  virtual void run_next_test_branch_impl() = 0;

  std::unique_ptr<runnable_state> test_state_;
};

} // namespace caf::test
