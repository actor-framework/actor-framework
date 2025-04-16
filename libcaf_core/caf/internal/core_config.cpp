// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/internal/core_config.hpp"

#include "caf/config_option_adder.hpp"
#include "caf/detail/format.hpp"

namespace caf::internal {

void core_logger_file_config::init(config_option_set& options) {
  config_option_adder{options, group_name}
    .add(path, "path", "filesystem path for the log file")
    .add(format, "format", "format for individual log file entries")
    .add(verbosity, "verbosity", "minimum severity level for file output")
    .add(excluded_components, "excluded-components",
         "excluded components in files");
}

caf::error
core_logger_file_config::validate(const detail::log_level_map& levels) const {
  if (!levels.contains(verbosity)) {
    auto msg = detail::format("unknown verbosity level in {}.verbosity: {}",
                              group_name, verbosity);
    return make_error(sec::invalid_argument, std::move(msg));
  }
  return {};
}

void core_logger_file_config::dump(settings& out) const {
  out.insert_or_assign("path", path);
  out.insert_or_assign("format", format);
  out.insert_or_assign("verbosity", verbosity);
  out.insert_or_assign("excluded-components", excluded_components);
}

void core_logger_console_config::init(config_option_set& options) {
  config_option_adder{options, group_name}
    .add(colored, "colored", "forces colored or uncolored output")
    .add(format, "format", "format for printed console lines")
    .add(verbosity, "verbosity", "minimum severity level for console output")
    .add(excluded_components, "excluded-components",
         "excluded components on console");
}

caf::error core_logger_console_config::validate(
  const detail::log_level_map& levels) const {
  if (!levels.contains(verbosity)) {
    auto msg = detail::format("unknown verbosity level in {}.verbosity: {}",
                              group_name, verbosity);
    return make_error(sec::invalid_argument, std::move(msg));
  }
  return {};
}

void core_logger_console_config::dump(settings& out) const {
  out.insert_or_assign("colored", colored);
  out.insert_or_assign("format", format);
  out.insert_or_assign("verbosity", verbosity);
  out.insert_or_assign("excluded-components", excluded_components);
}

void core_logger_config::init(config_option_set& options) {
  file.init(options);
  console.init(options);
}

caf::error core_logger_config::validate() const {
  if (auto err = file.validate(log_levels)) {
    return err;
  }
  return console.validate(log_levels);
}

void core_logger_config::dump(settings& out) const {
  file.dump(out["file"].as_dictionary());
  console.dump(out["console"].as_dictionary());
}

void core_scheduler_config::init(config_option_set& options) {
  config_option_adder{options, group_name}
    .add(policy, "policy", "'stealing' (default) or 'sharing'")
    .add(max_threads, "max-threads", "maximum number of worker threads")
    .add(max_throughput, "max-throughput",
         "nr. of messages actors can consume per run");
}

caf::error core_scheduler_config::validate() const {
  if (policy != "stealing" && policy != "sharing") {
    auto msg = detail::format("unknown scheduler policy in {}.policy: {}",
                              group_name, policy);
    return make_error(sec::invalid_argument, std::move(msg));
  }
  if (max_threads == 0) {
    auto msg = detail::format("invalid max-threads in {}.max-threads: {}",
                              group_name, max_threads);
    return make_error(sec::invalid_argument, std::move(msg));
  }
  if (max_throughput == 0) {
    auto msg = detail::format("invalid max-throughput in {}.max-throughput: {}",
                              group_name, max_throughput);
    return make_error(sec::invalid_argument, std::move(msg));
  }
  return {};
}

void core_scheduler_config::dump(settings& out) const {
  out.insert_or_assign("policy", policy);
  out.insert_or_assign("max-threads", max_threads);
  out.insert_or_assign("max-throughput", max_throughput);
}

void core_work_stealing_config::init(config_option_set& options) {
  config_option_adder{options, group_name}
    .add(aggressive_poll_attempts, "aggressive-poll-attempts",
         "nr. of aggressive steal attempts")
    .add(aggressive_steal_interval, "aggressive-steal-interval",
         "frequency of aggressive steal attempts")
    .add(moderate_poll_attempts, "moderate-poll-attempts",
         "nr. of moderate steal attempts")
    .add(moderate_steal_interval, "moderate-steal-interval",
         "frequency of moderate steal attempts")
    .add(moderate_sleep_duration, "moderate-sleep-duration",
         "sleep duration between moderate steal attempts")
    .add(relaxed_steal_interval, "relaxed-steal-interval",
         "frequency of relaxed steal attempts")
    .add(relaxed_sleep_duration, "relaxed-sleep-duration",
         "sleep duration between relaxed steal attempts");
}

caf::error core_work_stealing_config::validate() const {
  if (aggressive_poll_attempts == 0) {
    auto msg = detail::format(
      "invalid aggressive-poll-attempts in {}.aggressive-poll-attempts: {}",
      group_name, aggressive_poll_attempts);
    return make_error(sec::invalid_argument, std::move(msg));
  }
  if (aggressive_steal_interval == 0) {
    auto msg = detail::format(
      "invalid aggressive-steal-interval in {}.aggressive-steal-interval: {}",
      group_name, aggressive_steal_interval);
    return make_error(sec::invalid_argument, std::move(msg));
  }
  if (moderate_poll_attempts == 0) {
    auto msg = detail::format(
      "invalid moderate-poll-attempts in {}.moderate-poll-attempts: {}",
      group_name, moderate_poll_attempts);
    return make_error(sec::invalid_argument, std::move(msg));
  }
  if (moderate_steal_interval == 0) {
    auto msg = detail::format(
      "invalid moderate-steal-interval in {}.moderate-steal-interval: {}",
      group_name, moderate_steal_interval);
    return make_error(sec::invalid_argument, std::move(msg));
  }
  if (relaxed_steal_interval == 0) {
    auto msg = detail::format(
      "invalid relaxed-steal-interval in {}.relaxed-steal-interval: {}",
      group_name, relaxed_steal_interval);
    return make_error(sec::invalid_argument, std::move(msg));
  }
  return {};
}

void core_work_stealing_config::dump(settings& out) const {
  out.insert_or_assign("aggressive-poll-attempts", aggressive_poll_attempts);
  out.insert_or_assign("aggressive-steal-interval", aggressive_steal_interval);
  out.insert_or_assign("moderate-poll-attempts", moderate_poll_attempts);
  out.insert_or_assign("moderate-steal-interval", moderate_steal_interval);
  out.insert_or_assign("moderate-sleep-duration", moderate_sleep_duration);
  out.insert_or_assign("relaxed-steal-interval", relaxed_steal_interval);
  out.insert_or_assign("relaxed-sleep-duration", relaxed_sleep_duration);
}

void core_metrics_config::init(config_option_set& options) {
  config_option_adder{options, group_name}.add(
    disable_running_actors, "disable-running-actors",
    "sets whether to collect metrics for running actors per type");
}

caf::error core_metrics_config::validate() const {
  return {};
}

void core_metrics_config::dump(settings& out) const {
  out.insert_or_assign("disable-running-actors", disable_running_actors);
}

void core_metrics_filter_config::init(config_option_set& options) {
  config_option_adder{options, group_name}
    .add(includes, "includes", "selects actors for run-time metrics")
    .add(excludes, "excludes", "excludes actors from run-time metrics");
}

caf::error core_metrics_filter_config::validate() const {
  return {};
}

void core_metrics_filter_config::dump(settings& out) const {
  out.insert_or_assign("includes", includes);
  out.insert_or_assign("excludes", excludes);
}

void core_console_config::init(config_option_set& options) {
  config_option_adder{options, group_name}
    .add(colored, "colored", "forces colored or uncolored output")
    .add(stream, "stream", "'stdout' (default), 'stderr' or 'none'");
}

caf::error core_console_config::validate() const {
  if (stream != "stdout" && stream != "stderr" && stream != "none") {
    auto msg = detail::format("unknown stream in {}.stream: {}", group_name,
                              stream);
    return make_error(sec::invalid_argument, std::move(msg));
  }
  return {};
}

void core_console_config::dump(settings& out) const {
  out.insert_or_assign("colored", colored);
  out.insert_or_assign("stream", stream);
}

void core_config::init(config_option_set& options) {
  logger.init(options);
  scheduler.init(options);
  work_stealing.init(options);
  metrics.init(options);
  metrics_filter.init(options);
  console.init(options);
}

caf::error core_config::validate() const {
  return logger.validate();
}

void core_config::dump(settings& out) const {
  logger.dump(out["logger"].as_dictionary());
}

} // namespace caf::internal
