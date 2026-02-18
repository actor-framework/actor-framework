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

} // namespace

runnable::~runnable() {
  // nop
}

void runnable::run() {
  current_runnable = this;
  auto guard
    = detail::scope_guard{[]() noexcept { current_runnable = nullptr; }};
  switch (root_type_) {
    case block_type::outline:
      if (auto guard = ctx_->get<outline>(0, description_, loc_)->commit()) {
        do_run();
        return;
      }
      CAF_RAISE_ERROR(std::logic_error,
                      "failed to select  the root block for the outline");
      break;
    case block_type::scenario:
      if (auto guard = ctx_->get<scenario>(0, description_, loc_)->commit()) {
        do_run();
        return;
      }
      CAF_RAISE_ERROR(std::logic_error,
                      "failed to select  the root block for the scenario");
      break;
    case block_type::test:
      if (auto guard = ctx_->get<test>(0, description_, loc_)->commit()) {
        do_run();
        return;
      }
      CAF_RAISE_ERROR(std::logic_error,
                      "failed to select the root block for the test");
      break;
    default:
      CAF_RAISE_ERROR(std::logic_error, "invalid root type");
  }
}

void runnable::call_do_run() {
  current_runnable = this;
  auto guard
    = detail::scope_guard{[]() noexcept { current_runnable = nullptr; }};
  do_run();
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
  if (ctx_->call_stack.empty())
    CAF_RAISE_ERROR(std::logic_error, "no current block");
  return *ctx_->call_stack.back();
}

namespace {

bool labels_match(std::span<const telemetry::label_view> want,
                  std::span<const telemetry::label> found) {
  if (want.size() != found.size())
    return false;
  for (const auto& lbl : want) {
    auto is_match = [&lbl](const telemetry::label& other) {
      return lbl.name() == other.name() && lbl.value() == other.value();
    };
    if (std::ranges::none_of(found, is_match))
      return false;
  }
  return true;
}

} // namespace

bool runnable::do_check_metric(const telemetry::metric_registry& metrics,
                               std::string_view prefix, std::string_view name,
                               std::span<const telemetry::label_view> labels,
                               callback<bool(int64_t)>& pred,
                               const std::source_location& location) {
  using namespace std::literals;
  auto result = metrics.wait_for(prefix, name, labels, 1s, 10ms, pred);
  if (result) {
    reporter::instance().pass(location);
    return true;
  }
  std::string msg;
  auto collector = [prefix, name, labels,
                    &msg]<class Wrapped>(const telemetry::metric_family* family,
                                         const telemetry::metric* instance,
                                         const Wrapped* wrapped) {
    if constexpr (detail::one_of<Wrapped, telemetry::int_counter,
                                 telemetry::dbl_counter, telemetry::int_gauge,
                                 telemetry::dbl_gauge>) {
      if (family->prefix() == prefix && family->name() == name
          && labels_match(labels, instance->labels())) {
        msg = detail::format("metric \"{}.{}\" has value {}", prefix, name,
                             wrapped->value());
      }
    }
  };
  metrics.collect(collector);
  if (msg.empty()) {
    msg = detail::format("metric \"{}.{}\" does not exist", prefix, name);
  }
  reporter::instance().fail(msg, location);
  return false;
}
} // namespace caf::test
