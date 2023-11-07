// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/fwd.hpp"

#include "caf/detail/log_level.hpp"
#include "caf/detail/source_location.hpp"
#include "caf/detail/test_export.hpp"
#include "caf/format_string_with_location.hpp"
#include "caf/fwd.hpp"
#include "caf/logger.hpp"

#include <cstddef>
#include <string_view>

namespace caf::test {

/// Observes the execution of test suites and reports the results.
class CAF_TEST_EXPORT reporter {
public:
  struct stats {
    size_t passed = 0;
    size_t failed = 0;

    size_t total() const noexcept {
      return passed + failed;
    }

    stats& operator+=(const stats& other) {
      passed += other.passed;
      failed += other.failed;
      return *this;
    }
  };

  virtual ~reporter();

  virtual bool success() const noexcept = 0;

  virtual void start() = 0;

  virtual void stop() = 0;

  virtual void begin_suite(std::string_view name) = 0;

  virtual void end_suite(std::string_view name) = 0;

  virtual void begin_test(context_ptr state, std::string_view) = 0;

  virtual void end_test() = 0;

  virtual void begin_step(block* ptr) = 0;

  virtual void end_step(block* ptr) = 0;

  virtual void pass(const detail::source_location& location) = 0;

  /// Reports a failed check with a binary predicate.
  virtual void fail(binary_predicate type, std::string_view lhs,
                    std::string_view rhs,
                    const detail::source_location& location)
    = 0;

  /// Reports a failed check (unary predicate).
  virtual void
  fail(std::string_view arg, const detail::source_location& location)
    = 0;

  virtual void unhandled_exception(std::string_view msg) = 0;

  virtual void unhandled_exception(std::string_view msg,
                                   const detail::source_location& location)
    = 0;

  /// Prints a message to the output stream if `verbosity() >= level`.
  virtual void print(const logger::context& ctx, std::string_view msg) = 0;

  /// Generates a message with the DEBUG severity level.
  template <class... Ts>
  void print_debug(format_string_with_location fwl, Ts&&... xs) {
    auto ctx = logger::context::make(CAF_LOG_LEVEL_DEBUG, "caf.test",
                                     fwl.location);
    print(ctx, detail::format(fwl.value, std::forward<Ts>(xs)...));
  }

  /// Generates a message with the INFO severity level.
  template <class... Ts>
  void print_info(format_string_with_location fwl, Ts&&... xs) {
    auto ctx = logger::context::make(CAF_LOG_LEVEL_INFO, "caf.test",
                                     fwl.location);
    print(ctx, detail::format(fwl.value, std::forward<Ts>(xs)...));
  }

  /// Generates a message with the WARNING severity level.
  template <class... Ts>
  void print_warning(format_string_with_location fwl, Ts&&... xs) {
    auto ctx = logger::context::make(CAF_LOG_LEVEL_WARNING, "caf.test",
                                     fwl.location);
    print(ctx, detail::format(fwl.value, std::forward<Ts>(xs)...));
  }

  /// Generates a message with the ERROR severity level.
  template <class... Ts>
  void print_error(format_string_with_location fwl, Ts&&... xs) {
    auto ctx = logger::context::make(CAF_LOG_LEVEL_ERROR, "caf.test",
                                     fwl.location);
    print(ctx, detail::format(fwl.value, std::forward<Ts>(xs)...));
  }

  virtual void print_actor_output(local_actor* self, std::string_view msg) = 0;

  /// Sets the verbosity level of the reporter and returns the previous value.
  virtual unsigned verbosity(unsigned level) noexcept = 0;

  /// returns the current verbosity level.
  virtual unsigned verbosity() const noexcept = 0;

  /// Sets whether the reporter disables colored output even when writing to a
  /// TTY.
  virtual void no_colors(bool new_value) = 0;

  /// Returns statistics for the current test.
  virtual stats test_stats() = 0;

  /// Overrides the statistics for the current test.
  virtual void test_stats(stats) = 0;

  /// Returns statistics for the current suite.
  virtual stats suite_stats() = 0;

  /// Returns statistics for the entire run.
  virtual stats total_stats() = 0;

  /// Returns the registered reporter instance.
  static reporter& instance();

  /// Sets the reporter instance for the current test run.
  static void instance(reporter* ptr);

  /// Creates a default reporter that writes to the standard output.
  static std::unique_ptr<reporter> make_default();
};

} // namespace caf::test
