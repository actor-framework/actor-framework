// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/runnable.hpp"

#include "caf/test/block_type.hpp"
#include "caf/test/outline.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/detail/format.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/telemetry/metric_registry.hpp"

#include <chrono>
#include <regex>

namespace caf::test {

namespace {

thread_local runnable* current_runnable;

std::string render_label_name(std::string_view prefix, std::string_view name,
                              std::span<const telemetry::label_view> labels) {
  std::string result = detail::format("{}.{}", prefix, name);
  if (!labels.empty()) {
    result += "{";
    for (const auto& label : labels) {
      result += label.name();
      result += "=";
      result += label.value();
      result += ",";
    }
    result.back() = '}';
  }
  return result;
}

} // namespace

struct runnable::runnable_state {
  runnable_state(context_ptr ctx, std::string_view description,
                 block_type root_type, const std::source_location& loc)
    : ctx_(ctx), description_(description), root_type_(root_type), loc_(loc) {
    // nop
  }

  context_ptr ctx_;
  std::string_view description_;
  block_type root_type_;
  std::source_location loc_;
  const telemetry::metric_registry* current_registry_ = nullptr;
  timespan registry_poll_interval_ = std::chrono::milliseconds{10};
  timespan registry_poll_timeout_ = std::chrono::seconds{1};
};

runnable::runnable(context_ptr ctx, std::string_view description,
                   block_type root_type, const std::source_location& loc) {
  if (!ctx) {
    CAF_RAISE_ERROR(std::invalid_argument, "context cannot be null");
  }
  test_state_.reset(
    new runnable_state(std::move(ctx), description, root_type, loc));
  current_runnable = this;
}

runnable::~runnable() {
  // nop
}

/// Sets the current metric registry.
void runnable::current_metric_registry(const telemetry::metric_registry* ptr) {
  test_state_->current_registry_ = ptr;
}

const telemetry::metric_registry*
runnable::current_metric_registry() const noexcept {
  return test_state_->current_registry_;
}

void runnable::metric_registry_poll_interval(timespan interval) {
  if (interval.count() <= 0) {
    CAF_RAISE_ERROR(std::invalid_argument, "interval must be positive");
  }
  test_state_->registry_poll_interval_ = interval;
}

timespan runnable::metric_registry_poll_interval() const noexcept {
  return test_state_->registry_poll_interval_;
}

void runnable::metric_registry_poll_timeout(timespan timeout) {
  if (timeout.count() <= 0) {
    CAF_RAISE_ERROR(std::invalid_argument, "timeout must be positive");
  }
  test_state_->registry_poll_timeout_ = timeout;
}

timespan runnable::metric_registry_poll_timeout() const noexcept {
  return test_state_->registry_poll_timeout_;
}

caf::test::context& runnable::test_context() const noexcept {
  return *test_state_->ctx_;
}

std::string_view runnable::test_description() const noexcept {
  return test_state_->description_;
}

const std::source_location& runnable::test_location() const noexcept {
  return test_state_->loc_;
}

void runnable::run_next_test_branch() {
  current_runnable = this;
  detail::scope_guard run_guard{[]() noexcept { current_runnable = nullptr; }};
  run_next_test_branch_init();
  switch (test_state_->root_type_) {
    case block_type::outline:
      if (auto guard = test_state_->ctx_
                         ->get<outline>(0, test_state_->description_,
                                        test_state_->loc_)
                         ->commit()) {
        run_next_test_branch_impl();
        return;
      }
      CAF_RAISE_ERROR(std::logic_error,
                      "failed to select the root block for the outline");
      break;
    case block_type::scenario:
      if (auto guard = test_state_->ctx_
                         ->get<scenario>(0, test_state_->description_,
                                         test_state_->loc_)
                         ->commit()) {
        run_next_test_branch_impl();
        return;
      }
      CAF_RAISE_ERROR(std::logic_error,
                      "failed to select the root block for the scenario");
      break;
    case block_type::test:
      if (auto guard = test_state_->ctx_
                         ->get<test>(0, test_state_->description_,
                                     test_state_->loc_)
                         ->commit()) {
        run_next_test_branch_impl();
        return;
      }
      CAF_RAISE_ERROR(std::logic_error,
                      "failed to select the root block for the test");
      break;
    default:
      CAF_RAISE_ERROR(std::logic_error, "invalid root type");
  }
}

void runnable::run_next_test_branch_init() {
  // customization point for runnable_with_examples
}

bool runnable::check(bool value, const std::source_location& location) {
  if (value) {
    reporter::instance().pass(location);
  } else {
    reporter::instance().fail("should be true", location);
  }
  return value;
}

bool runnable::check_matches(std::string_view str, std::string_view rx,
                             const std::source_location& location) {
  auto regex = std::regex{rx.begin(), rx.end()};
  if (std::regex_match(str.begin(), str.end(), regex)) {
    reporter::instance().pass(location);
    return true;
  }
  auto msg = detail::format("string \"{}\" does not match regex \"{}\"", str,
                            rx);
  reporter::instance().fail(msg, location);
  return false;
}

void runnable::require(bool value, const std::source_location& location) {
  if (!check(value, location))
    requirement_failed::raise(location);
}

runnable& runnable::current() {
  auto ptr = current_runnable;
  if (!ptr)
    CAF_RAISE_ERROR(std::logic_error, "no current runnable");
  return *ptr;
}

block& runnable::current_block() {
  if (test_state_->ctx_->call_stack.empty())
    CAF_RAISE_ERROR(std::logic_error, "no current block");
  return *test_state_->ctx_->call_stack.back();
}

namespace {

template <class Range1, class Range2>
bool labels_match(const Range1& want, const Range2& found) {
  if (want.size() != found.size()) {
    return false;
  }
  for (const auto& lbl : want) {
    auto is_match = [&lbl](const auto& other) {
      return lbl.name() == other.name() && lbl.value() == other.value();
    };
    if (std::ranges::none_of(found, is_match)) {
      return false;
    }
  }
  return true;
}

} // namespace

bool runnable::do_check_metric(std::string_view prefix, std::string_view name,
                               std::span<const telemetry::label_view> labels,
                               callback<bool(int64_t)>& pred,
                               const std::source_location& location) {
  using namespace std::literals;
  if (!test_state_->current_registry_) {
    CAF_RAISE_ERROR(std::logic_error, "no metric registry set");
  }
  auto result = test_state_->current_registry_->wait_for(
    prefix, name, labels, test_state_->registry_poll_timeout_,
    test_state_->registry_poll_interval_, pred);
  if (result) {
    reporter::instance().pass(location);
    return true;
  }
  auto label_name = render_label_name(prefix, name, labels);
  std::string msg;
  auto collector = [prefix, name, labels, &label_name,
                    &msg]<class Wrapped>(const telemetry::metric_family* family,
                                         const telemetry::metric* instance,
                                         const Wrapped* wrapped) {
    if (family->prefix() == prefix && family->name() == name
        && labels_match(labels, instance->labels())) {
      if constexpr (detail::one_of<Wrapped, telemetry::int_counter,
                                   telemetry::int_gauge>) {
        msg = detail::format("metric {} has value {}", label_name,
                             wrapped->value());
      } else {
        msg = detail::format("metric {} has an unexpected type: "
                             "expected an integer gauge or counter",
                             label_name);
      }
    }
  };
  test_state_->current_registry_->collect(collector);
  if (msg.empty()) {
    msg = detail::format("metric {} does not exist", label_name);
  }
  reporter::instance().fail(msg, location);
  return false;
}

bool runnable::do_check_metric(std::string_view prefix, std::string_view name,
                               std::span<const telemetry::label_view> labels,
                               callback<bool(double)>& pred,
                               const std::source_location& location) {
  using namespace std::literals;
  if (!test_state_->current_registry_) {
    CAF_RAISE_ERROR(std::logic_error, "no metric registry set");
  }
  auto result = test_state_->current_registry_->wait_for(
    prefix, name, labels, test_state_->registry_poll_timeout_,
    test_state_->registry_poll_interval_, pred);
  if (result) {
    reporter::instance().pass(location);
    return true;
  }
  auto label_name = render_label_name(prefix, name, labels);
  std::string msg;
  auto collector = [prefix, name, labels, &label_name,
                    &msg]<class Wrapped>(const telemetry::metric_family* family,
                                         const telemetry::metric* instance,
                                         const Wrapped* wrapped) {
    if (family->prefix() == prefix && family->name() == name
        && labels_match(labels, instance->labels())) {
      if constexpr (detail::one_of<Wrapped, telemetry::dbl_counter,
                                   telemetry::dbl_gauge>) {
        msg = detail::format("metric {} has value {}", label_name,
                             wrapped->value());
      } else {
        msg = detail::format("metric {} has an unexpected type: "
                             "expected a double gauge or counter",
                             label_name);
      }
    }
  };
  test_state_->current_registry_->collect(collector);
  if (msg.empty()) {
    msg = detail::format("metric {} does not exist", label_name);
  }
  reporter::instance().fail(msg, location);
  return false;
}

} // namespace caf::test
