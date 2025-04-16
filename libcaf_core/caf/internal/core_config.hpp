// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/defaults.hpp"
#include "caf/detail/log_level_map.hpp"
#include "caf/fwd.hpp"
#include "caf/settings.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace caf::internal {

class core_logger_file_config {
public:
  static constexpr std::string_view group_name = "caf.logger.file";

  void init(config_option_set& options);

  [[nodiscard]] caf::error validate(const detail::log_level_map& levels) const;

  void dump(settings& out) const;

  std::string path = std::string{defaults::logger::file::path};
  std::string format = std::string{defaults::logger::file::format};
  std::string verbosity = std::string{defaults::logger::file::verbosity};
  std::vector<std::string> excluded_components;
};

class core_logger_console_config {
public:
  static constexpr std::string_view group_name = "caf.logger.console";

  void init(config_option_set& options);

  [[nodiscard]] caf::error validate(const detail::log_level_map& levels) const;

  void dump(settings& out) const;

  bool colored = true;
  std::string format = std::string{defaults::logger::console::format};
  std::string verbosity = std::string{defaults::logger::console::verbosity};
  std::vector<std::string> excluded_components;
};

class core_logger_config {
public:
  static constexpr std::string_view group_name = "caf.logger";

  void init(config_option_set& options);

  [[nodiscard]] caf::error validate() const;

  void dump(settings& out) const;

  core_logger_file_config file;
  core_logger_console_config console;
  detail::log_level_map log_levels;
};

class core_scheduler_config {
public:
  static constexpr std::string_view group_name = "caf.scheduler";

  void init(config_option_set& options);

  [[nodiscard]] caf::error validate() const;

  void dump(settings& out) const;

  std::string policy;
  size_t max_threads;
  size_t max_throughput;
};

class core_work_stealing_config {
public:
  static constexpr std::string_view group_name = "caf.work-stealing";

  void init(config_option_set& options);

  [[nodiscard]] caf::error validate() const;

  void dump(settings& out) const;

  size_t aggressive_poll_attempts;
  size_t aggressive_steal_interval;
  size_t moderate_poll_attempts;
  size_t moderate_steal_interval;
  timespan moderate_sleep_duration;
  size_t relaxed_steal_interval;
  timespan relaxed_sleep_duration;
};

class core_metrics_config {
public:
  static constexpr std::string_view group_name = "caf.metrics";

  void init(config_option_set& options);

  [[nodiscard]] caf::error validate() const;

  void dump(settings& out) const;

  bool disable_running_actors;
};

class core_metrics_filter_config {
public:
  static constexpr std::string_view group_name = "caf.metrics.filters.actors";

  void init(config_option_set& options);

  [[nodiscard]] caf::error validate() const;

  void dump(settings& out) const;

  std::vector<std::string> includes;
  std::vector<std::string> excludes;
};

class core_console_config {
public:
  static constexpr std::string_view group_name = "caf.console";

  void init(config_option_set& options);

  [[nodiscard]] caf::error validate() const;

  void dump(settings& out) const;

  bool colored;
  std::string stream;
};

class core_config {
public:
  static constexpr std::string_view group_name = "caf";

  void init(config_option_set& options);

  [[nodiscard]] caf::error validate() const;

  void dump(settings& out) const;

  core_logger_config logger;
  core_scheduler_config scheduler;
  core_work_stealing_config work_stealing;
  core_metrics_config metrics;
  core_metrics_filter_config metrics_filter;
  core_console_config console;
};

} // namespace caf::internal
