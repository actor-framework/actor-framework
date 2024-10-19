// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/reporter.hpp"

#include "caf/test/binary_predicate.hpp"
#include "caf/test/block.hpp"
#include "caf/test/block_type.hpp"
#include "caf/test/context.hpp"

#include "caf/detail/format.hpp"
#include "caf/detail/log_level.hpp"
#include "caf/detail/log_level_map.hpp"
#include "caf/local_actor.hpp"
#include "caf/log/event.hpp"
#include "caf/log/level.hpp"
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
  using difference_type = int;
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

  default_reporter() {
    // Install lower-case log level names for more consistent output.
    log_level_names_.set("error", log::level::error);
    log_level_names_.set("warning", log::level::warning);
    log_level_names_.set("info", log::level::info);
    log_level_names_.set("debug", log::level::debug);
    log_level_names_.set("trace", log::level::trace);
  }

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
    using std::chrono::duration_cast;
    auto elapsed = clock_type::now() - start_time_;
    detail::format_to(colored(),
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
      detail::format_to(colored(), "  $B(Failed Suites):\n");
      for (auto name : failed_suites_)
        detail::format_to(colored(), "  - $R({})\n", name);
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
    using std::chrono::duration_cast;
    total_stats_ += suite_stats_;
    if (suite_stats_.failed > 0)
      failed_suites_.push_back(name);
    else if (level_ < log::level::debug)
      return;
    auto elapsed = clock_type::now() - suite_start_time_;
    detail::format_to(colored(),
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
      detail::format_to(colored(), "  $B(Failed tests):\n");
      for (auto name : failed_tests_)
        detail::format_to(colored(), "  - $R({})\n", name);
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
    if (current_ctx_ == nullptr)
      CAF_RAISE_ERROR(std::logic_error, "begin_test was not called");
    if (current_suite_ != "$") {
      detail::format_to(colored(), "$C(Suite): $0{}\n", current_suite_);
      indent_ = 2;
    } else {
      indent_ = 0;
    }
    live_ = true;
    for (auto* frame : current_ctx_->call_stack)
      begin_step(frame);
  }

  void begin_step(block* ptr) override {
    if (!live_)
      return;
    if (indent_ > 0 && is_extension(ptr->type()))
      indent_ -= 2;
    detail::format_to(colored(), "{0:{1}}$C({2}): $0{3}\n", ' ', indent_,
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
    test_stats_.passed++;
    if (level_ < log::level::debug)
      return;
    set_live();
    detail::format_to(colored(), "{0:{1}}$G(pass) $C({2}):$Y({3})$0\n", ' ',
                      indent_, location.file_name(), location.line());
  }

  void fail(binary_predicate type, std::string_view lhs, std::string_view rhs,
            const detail::source_location& location) override {
    test_stats_.failed++;
    if (level_ < log::level::error)
      return;
    set_live();
    detail::format_to(colored(),
                      "{0:{1}}$R({2}): lhs {3} rhs\n"
                      "{0:{1}}  loc: $C({4}):$Y({5})$0\n"
                      "{0:{1}}  lhs: {6}\n"
                      "{0:{1}}  rhs: {7}\n",
                      ' ', indent_, log_level_names_[log::level::error],
                      str(negate(type)), location.file_name(), location.line(),
                      lhs, rhs);
  }

  void fail(std::string_view arg,
            const detail::source_location& location) override {
    test_stats_.failed++;
    if (level_ < log::level::error)
      return;
    set_live();
    detail::format_to(colored(),
                      "{0:{1}}$R({2}): check failed\n"
                      "{0:{1}}    loc: $C({3}):$Y({4})$0\n"
                      "{0:{1}}  check: {5}\n",
                      ' ', indent_, log_level_names_[log::level::error],
                      location.file_name(), location.line(), arg);
  }

  void unhandled_exception(std::string_view msg) override {
    test_stats_.failed++;
    if (level_ < log::level::error)
      return;
    set_live();
    if (current_ctx_ == nullptr || current_ctx_->unwind_stack.empty()) {
      detail::format_to(colored(),
                        "{0:{1}}$R(unhandled exception): abort test run\n"
                        "{0:{1}}  loc: $R(unknown)$0\n"
                        "{0:{1}}  msg: {2}\n",
                        ' ', indent_, msg);
    } else {
      auto& location = current_ctx_->unwind_stack.front()->location();
      detail::format_to(colored(),
                        "{0:{1}}$R(unhandled exception): abort test run\n"
                        "{0:{1}}  loc: in block starting at $C({2}):$Y({3})$0\n"
                        "{0:{1}}  msg: {4}\n",
                        ' ', indent_, location.file_name(), location.line(),
                        msg);
    }
  }

  void unhandled_exception(std::string_view msg,
                           const detail::source_location& location) override {
    test_stats_.failed++;
    if (level_ < log::level::error)
      return;
    set_live();
    detail::format_to(colored(),
                      "{0:{1}}$R(unhandled exception): abort test run\n"
                      "{0:{1}}  loc: $C({2}):$Y({3})$0\n"
                      "{0:{1}}  msg: {4}\n",
                      ' ', indent_, location.file_name(), location.line(), msg);
  }

  void print(const log::event& event) override {
    if (level_ < event.level())
      return;
    set_live();
    detail::format_to(colored(),
                      "{0:{1}}${2}({3}):\n"
                      "{0:{1}}  loc: $C({4}):$Y({5})$0\n"
                      "{0:{1}}  msg: {6}\n",
                      ' ', indent_, color_by_log_level(event.level()),
                      log_level_names_[event.level()], event.file_name(),
                      event.line_number(), event.message());
    for (const auto& field : event.fields())
      do_print(field);
  }

  void print_actor_output(local_actor* self, std::string_view msg) override {
    if (level_ < log::level::info)
      return;
    set_live();
    detail::format_to(colored(),
                      "{0:{1}}$M({2}):\n"
                      "{0:{1}}  src: $0{3} [ID {4}]\n"
                      "{0:{1}}  msg: {5}\n",
                      ' ', indent_, log_level_names_[log::level::info],
                      self->name(), self->id(), msg);
  }

  unsigned verbosity() const noexcept override {
    return level_;
  }

  unsigned verbosity(unsigned level) noexcept override {
    auto result = level_;
    level_ = level;
    return result;
  }

  std::vector<std::string> log_component_filter() const override {
    return log_component_filter_;
  }

  void log_component_filter(std::vector<std::string> new_filter) override {
    log_component_filter_ = std::move(new_filter);
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
  void do_print(const log::event::field& field) {
    auto fn = [this, &field](const auto& value) {
      using value_t = std::decay_t<decltype(value)>;
      if constexpr (std::is_same_v<value_t, std::nullopt_t>) {
        detail::format_to(plain(), "{0:{1}}  {2}: null\n", ' ', indent_,
                          field.key);
      } else if constexpr (std::is_same_v<value_t, log::event::field_list>) {
        detail::format_to(plain(), "{0:{1}}  {2}:\n", ' ', indent_, field.key);
        indent_ += 2;
        for (const auto& nested_field : value)
          do_print(nested_field);
        indent_ -= 2;
      } else {
        detail::format_to(plain(), "{0:{1}}  {2}: {3}\n", ' ', indent_,
                          field.key, value);
      }
    };
    std::visit(fn, field.value);
  }

  static char color_by_log_level(unsigned level) {
    if (level >= log::level::debug)
      return 'B';
    if (level >= log::level::info)
      return 'M';
    if (level >= log::level::warning)
      return 'Y';
    return 'R';
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
  stats test_stats_;

  /// Stores statistics for the current suite.
  stats suite_stats_;

  /// Stores statistics for all suites.
  stats total_stats_;

  /// Counts the number of test suites.
  size_t num_suites_ = 0;

  /// Stores the time point when the test runner started.
  time_point start_time_;

  /// Stores the time point when the current test suite started.
  time_point suite_start_time_;

  /// Configures the verbosity of the reporter.
  unsigned level_ = log::level::info;

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

  /// Maps log levels to their names.
  detail::log_level_map log_level_names_;

  /// Stores the current log component filter.
  std::vector<std::string> log_component_filter_;
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

namespace {

/// A logger implementation that delegates to the test reporter.
class reporter_logger : public logger, public detail::atomic_ref_counted {
public:
  /// Increases the reference count of the coordinated.
  void ref_logger() const noexcept final {
    this->ref();
  }

  /// Decreases the reference count of the coordinated and destroys the object
  /// if necessary.
  void deref_logger() const noexcept final {
    this->deref();
  }

  // -- constructors, destructors, and assignment operators --------------------

  reporter_logger() {
    filter_ = reporter::instance().log_component_filter();
  }

  // -- logging ----------------------------------------------------------------

  /// Writes an entry to the event-queue of the logger.
  /// @threadsafe
  void do_log(log::event_ptr&& event) override {
    // We omit fields such as component and actor ID. When not filtering
    // non-test log messages, we add these fields to the message in order to be
    // able to distinguish between different actors and components.
    if (event->component() != "caf.test") {
      auto enriched = detail::format("[{}, aid: {}] {}", event->component(),
                                     logger::thread_local_aid(),
                                     event->message());
      auto enriched_event = event->with_message(enriched, log::keep_timestamp);
      reporter::instance().print(enriched_event);
      return;
    }
    reporter::instance().print(event);
  }

  /// Returns whether the logger is configured to accept input for given
  /// component and log level.
  bool accepts(unsigned level, std::string_view component) override {
    return level <= reporter::instance().verbosity()
           && !std::any_of(filter_.begin(), filter_.end(),
                           [component](const std::string& excluded) {
                             return component == excluded;
                           });
  }

  // -- initialization ---------------------------------------------------------

  /// Allows the logger to read its configuration from the actor system config.
  void init(const actor_system_config&) override {
    // nop
  }

  // -- event handling ---------------------------------------------------------

  /// Starts any background threads needed by the logger.
  void start() override {
    // nop
  }

  /// Stops all background threads of the logger.
  void stop() override {
    // nop
  }

private:
  std::vector<std::string> filter_;
};

} // namespace

intrusive_ptr<logger> reporter::make_logger() {
  return make_counted<reporter_logger>();
}

} // namespace caf::test
