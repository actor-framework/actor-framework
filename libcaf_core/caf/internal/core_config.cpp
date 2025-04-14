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

void core_config::init(config_option_set& options) {
  logger.init(options);
}

caf::error core_config::validate() const {
  return logger.validate();
}

void core_config::dump(settings& out) const {
  logger.dump(out["logger"].as_dictionary());
}

} // namespace caf::internal
