// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/reporter.hpp"

#include "caf/test/binary_predicate.hpp"
#include "caf/test/block.hpp"
#include "caf/test/block_type.hpp"
#include "caf/test/context.hpp"

#include "caf/detail/format.hpp"
#include "caf/detail/log_level.hpp"
#include "caf/local_actor.hpp"
#include "caf/raise_error.hpp"
#include "caf/term.hpp"

#include <iostream>

namespace caf::test {

reporter::~reporter() {
  // nop
}

namespace {

/// Implements a mini-DSL for colored output:
/// - $R(red text)
/// - $G(green text)
/// - $B(blue text)
/// - $Y(yellow text)
/// - $M(magenta text)
/// - $C(cyan text)
/// - $0 turns off coloring completely (enter verbatim mode)
struct colorizing_iterator {
  using difference_type = void;
  using value_type = void;
  using pointer = void;
  using reference = void;
  using iterator_category = std::output_iterator_tag;

  enum mode_t {
    normal,
    read_color,
    escape,
    color,
    off,
    off_read_color,
    off_escape,
    off_color,
    verbatim,
  };
  mode_t mode = normal;
  std::ostream* out;
  void put(char c) {
    switch (mode) {
      case normal:
        if (c == '$')
          mode = read_color;
        else
          out->put(c);
        break;
      case read_color:
        out->flush();
        switch (c) {
          case 'R':
            *out << term::red;
            break;
          case 'G':
            *out << term::green;
            break;
          case 'B':
            *out << term::blue;
            break;
          case 'Y':
            *out << term::yellow;
            break;
          case 'M':
            *out << term::magenta;
            break;
          case 'C':
            *out << term::cyan;
            break;
          case '0':
            mode = verbatim;
            return;
          default:
            CAF_RAISE_ERROR("invalid color code");
            break;
        }
        mode = escape;
        break;
      case escape:
        if (c != '(')
          CAF_RAISE_ERROR("expected ( after color code");
        mode = color;
        break;
      case color:
        if (c == ')') {
          out->flush();
          *out << term::reset;
          mode = normal;
          break;
        }
        out->put(c);
        break;
      case off:
        if (c == '$')
          mode = off_read_color;
        else
          out->put(c);
        break;
      case off_read_color:
        out->flush();
        switch (c) {
          case '0':
            mode = verbatim;
            return;
          default:
            mode = off_escape;
            return;
        }
        break;
      case off_escape:
        if (c != '(')
          CAF_RAISE_ERROR("expected ( after color code");
        mode = off_color;
        break;
      case off_color:
        if (c == ')') {
          out->flush();
          mode = off;
          break;
        }
        out->put(c);
        break;
      default: // verbatim
        out->put(c);
    }
  }

  colorizing_iterator& operator++() {
    return *this;
  }

  colorizing_iterator operator++(int) {
    return *this;
  }

  colorizing_iterator& operator*() {
    return *this;
  }

  colorizing_iterator& operator=(char c) {
    put(c);
    return *this;
  }
};

class default_reporter : public reporter {
public:
  using clock_type = std::chrono::steady_clock;

  using time_point = clock_type::time_point;

  using fractional_seconds = std::chrono::duration<double>;

  bool success() const noexcept override {
    return total_stats_.failed == 0;
  }

  void start() override {
    start_time_ = clock_type::now();
  }

  auto plain() {
    return std::ostream_iterator<char>{std::cout};
  }

  auto colored() {
    auto state = no_colors_ ? colorizing_iterator::off
                            : colorizing_iterator::normal;
    return colorizing_iterator{state, &std::cout};
  }

  void stop() override {
    using detail::format_to;
    using std::chrono::duration_cast;
    auto elapsed = clock_type::now() - start_time_;
    format_to(colored(),
              "$B(Summary):\n"
              "  $B(Time):   $Y({0:.3f}s)\n"
              "  $B(Suites): ${1}({2} / {3})\n"
              "  $B(Checks): ${1}({4} / {5})\n"
              "  $B(Status): ${1}({6})\n",
              duration_cast<fractional_seconds>(elapsed).count(), // {0}
              total_stats_.failed > 0 ? 'R' : 'G',                // {1}
              num_suites_ - failed_suites_.size(),                // {2}
              num_suites_,                                        // {3}
              total_stats_.passed,                                // {4}
              total_stats_.total(),                               // {5}
              total_stats_.failed > 0 ? "failed" : "passed");     // {6}
    if (!failed_suites_.empty()) {
      format_to(colored(), "  $B(Failed Suites):\n");
      for (auto name : failed_suites_)
        format_to(colored(), "  - $R({})\n", name);
    }
    std::cout << std::endl;
  }

  void begin_suite(std::string_view name) override {
    failed_tests_.clear();
    current_suite_ = name;
    suite_start_time_ = clock_type::now();
    ++num_suites_;
    suite_stats_ = {0, 0};
  }

  void end_suite(std::string_view name) override {
    using detail::format_to;
    using std::chrono::duration_cast;
    total_stats_ += suite_stats_;
    if (suite_stats_.failed > 0)
      failed_suites_.push_back(name);
    else if (level_ < CAF_LOG_LEVEL_DEBUG)
      return;
    auto elapsed = clock_type::now() - suite_start_time_;
    format_to(colored(),
              "$B(Suite Summary): $C({0})\n"
              "  $B(Time):   $Y({1:.3f}s)\n"
              "  $B(Checks): ${2}({3} / {4})\n"
              "  $B(Status): ${2}({5})\n",
              name != "$" ? name : "(default suite)",             // {0}
              duration_cast<fractional_seconds>(elapsed).count(), // {1}
              suite_stats_.failed > 0 ? 'R' : 'G',                // {2}
              suite_stats_.passed,                                // {3}
              suite_stats_.total(),                               // {4}
              suite_stats_.failed > 0 ? "failed" : "passed");     // {5}
    if (!failed_tests_.empty()) {
      format_to(colored(), "  $B(Failed tests):\n");
      for (auto name : failed_tests_)
        format_to(colored(), "  - $R({})\n", name);
    }
    std::cout << std::endl;
  }

  void begin_test(context_ptr state, std::string_view name) override {
    live_ = false;
    test_stats_ = {0, 0};
    current_test_ = name;
    current_ctx_ = std::move(state);
  }

  void end_test() override {
    if (test_stats_.failed > 0)
      failed_tests_.push_back(current_test_);
    suite_stats_ += test_stats_;
    current_ctx_.reset();
    if (live_)
      std::cout << std::endl;
  }

  void set_live() {
    if (live_)
      return;
    using detail::format_to;
    if (current_ctx_ == nullptr)
      CAF_RAISE_ERROR(std::logic_error, "begin_test was not called");
    if (current_suite_ != "$") {
      format_to(colored(), "$C(Suite): $0{}\n", current_suite_);
      indent_ = 2;
    } else {
      indent_ = 0;
    }
    live_ = true;
    for (auto* frame : current_ctx_->call_stack)
      begin_step(frame);
  }

  void begin_step(block* ptr) override {
    using detail::format_to;
    if (!live_)
      return;
    if (indent_ > 0 && is_extension(ptr->type()))
      indent_ -= 2;
    format_to(colored(), "{0:{1}}$C({2}): $0{3}\n", ' ', indent_,
              as_prefix(ptr->type()), ptr->description());
    indent_ += 2;
  }

  void end_step(block* ptr) override {
    if (!live_)
      return;
    if (indent_ == 0)
      CAF_RAISE_ERROR("unbalanced (begin|end)_step calls");
    if (!is_extension(ptr->type()))
      indent_ -= 2;
  }

  void pass(const detail::source_location& location) override {
    using detail::format_to;
    test_stats_.passed++;
    if (level_ < CAF_LOG_LEVEL_DEBUG)
      return;
    set_live();
    format_to(colored(), "{0:{1}}$G(pass) $C({2}):$Y({3})$0\n", ' ', indent_,
              location.file_name(), location.line());
  }

  void fail(binary_predicate type, std::string_view lhs, std::string_view rhs,
            const detail::source_location& location) override {
    using detail::format_to;
    test_stats_.failed++;
    if (level_ < CAF_LOG_LEVEL_ERROR)
      return;
    set_live();
    format_to(colored(),
              "{0:{1}}$R(error): lhs {2} rhs\n"
              "{0:{1}}  loc: $C({3}):$Y({4})$0\n"
              "{0:{1}}  lhs: {5}\n"
              "{0:{1}}  rhs: {6}\n",
              ' ', indent_, str(negate(type)), location.file_name(),
              location.line(), lhs, rhs);
  }

  void fail(std::string_view arg,
            const detail::source_location& location) override {
    using detail::format_to;
    test_stats_.failed++;
    if (level_ < CAF_LOG_LEVEL_ERROR)
      return;
    set_live();
    format_to(colored(),
              "{0:{1}}$R(error): check failed\n"
              "{0:{1}}    loc: $C({2}):$Y({3})$0\n"
              "{0:{1}}  check: {4}\n",
              ' ', indent_, location.file_name(), location.line(), arg);
  }

  void unhandled_exception(std::string_view msg) override {
    using detail::format_to;
    test_stats_.failed++;
    if (level_ < CAF_LOG_LEVEL_ERROR)
      return;
    set_live();
    if (current_ctx_ == nullptr || current_ctx_->unwind_stack.empty()) {
      format_to(colored(),
                "{0:{1}}$R(unhandled exception): abort test run\n"
                "{0:{1}}  loc: $R(unknown)$0\n"
                "{0:{1}}  msg: {2}\n",
                ' ', indent_, msg);
    } else {
      auto& location = current_ctx_->unwind_stack.front()->location();
      format_to(colored(),
                "{0:{1}}$R(unhandled exception): abort test run\n"
                "{0:{1}}  loc: in block starting at $C({2}):$Y({3})$0\n"
                "{0:{1}}  msg: {4}\n",
                ' ', indent_, location.file_name(), location.line(), msg);
    }
  }

  void unhandled_exception(std::string_view msg,
                           const detail::source_location& location) override {
    using detail::format_to;
    test_stats_.failed++;
    if (level_ < CAF_LOG_LEVEL_ERROR)
      return;
    set_live();
    format_to(colored(),
              "{0:{1}}$R(unhandled exception): abort test run\n"
              "{0:{1}}  loc: $C({2}):$Y({3})$0\n"
              "{0:{1}}  msg: {4}\n",
              ' ', indent_, location.file_name(), location.line(), msg);
  }

  void print_info(std::string_view msg,
                  const detail::source_location& location) override {
    print_impl(CAF_LOG_LEVEL_INFO, 'M', "info", msg, location);
  }

  void print_error(std::string_view msg,
                   const detail::source_location& location) override {
    print_impl(CAF_LOG_LEVEL_ERROR, 'R', "error", msg, location);
  }

  void print_debug(std::string_view msg,
                   const detail::source_location& location) override {
    print_impl(CAF_LOG_LEVEL_DEBUG, 'B', "debug", msg, location);
  }

  void print_warning(std::string_view msg,
                     const detail::source_location& location) override {
    print_impl(CAF_LOG_LEVEL_WARNING, 'Y', "warning", msg, location);
  }

  void print_actor_output(local_actor* self, std::string_view msg) override {
    using detail::format_to;
    if (level_ < CAF_LOG_LEVEL_INFO)
      return;
    set_live();
    format_to(colored(),
              "{0:{1}}$M(info):\n"
              "{0:{1}}  src: $0{2} [ID {3}]\n"
              "{0:{1}}  msg: {4}\n",
              ' ', indent_, self->name(), self->id(), msg);
  }

  unsigned verbosity(unsigned level) override {
    auto result = level_;
    level_ = level;
    return result;
  }

  unsigned verbosity() override {
    return level_;
  }

  void no_colors(bool new_value) override {
    no_colors_ = new_value;
  }

  stats test_stats() override {
    return test_stats_;
  }

  void test_stats(stats new_value) override {
    test_stats_ = new_value;
  }

  stats suite_stats() override {
    return suite_stats_;
  }

  stats total_stats() override {
    return total_stats_;
  }

private:
  void print_impl(unsigned level, char color_code, std::string_view level_name,
                  std::string_view msg,
                  const detail::source_location& location) {
    using detail::format_to;
    if (level_ < level)
      return;
    set_live();
    format_to(colored(),
              "{0:{1}}${2}({3}):\n"
              "{0:{1}}  loc: $C({4}):$Y({5})$0\n"
              "{0:{1}}  msg: {6}\n",
              ' ', indent_, color_code, level_name, location.file_name(),
              location.line(), msg);
  }

  void print_indent(size_t indent) {
    for (size_t i = 0; i < indent; ++i)
      std::cout << ' ';
  }

  void print_indent() {
    print_indent(indent_);
  }

  /// Configures the number of spaces to print before each line.
  size_t indent_ = 0;

  /// Stores statistics for the current test.
  stats test_stats_ = {0, 0};

  /// Stores statistics for the current suite.
  stats suite_stats_ = {0, 0};

  /// Stores statistics for all suites.
  stats total_stats_ = {0, 0};

  /// Counts the number of test suites.
  size_t num_suites_ = 0;

  /// Stores the time point when the test runner started.
  time_point start_time_;

  /// Stores the time point when the current test suite started.
  time_point suite_start_time_;

  /// Configures the verbosity of the reporter.
  unsigned level_ = CAF_LOG_LEVEL_INFO;

  /// Configures whether we render text without colors.
  bool no_colors_ = false;

  /// Stores the names of failed test suites.
  std::vector<std::string_view> failed_suites_;

  /// Stores the names of failed tests in a suite.
  std::vector<std::string_view> failed_tests_;

  /// Stores whether we render the current test as live. We start off with
  /// `false` and switch to `true` as soon as the current test generates any
  /// output.
  bool live_ = false;

  /// Stores the name of the current test suite.
  std::string_view current_suite_;

  /// Stores the name of the current test suite.
  std::string_view current_test_;

  /// Stores the state for the current test.
  context_ptr current_ctx_;
};

reporter* global_instance;

} // namespace

reporter& reporter::instance() {
  if (global_instance == nullptr)
    CAF_RAISE_ERROR("no reporter instance available");
  return *global_instance;
}

void reporter::instance(reporter* ptr) {
  global_instance = ptr;
}

std::unique_ptr<reporter> reporter::make_default() {
  return std::make_unique<default_reporter>();
}

} // namespace caf::test
