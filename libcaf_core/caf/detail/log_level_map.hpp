// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace caf::detail {

/// Maps log levels to their names.
class CAF_CORE_EXPORT log_level_map {
public:
  log_level_map();

  log_level_map(const log_level_map&) = default;

  log_level_map(log_level_map&&) noexcept = default;

  log_level_map& operator=(const log_level_map&) = default;

  log_level_map& operator=(log_level_map&&) noexcept = default;

  /// Returns the name of the log level at `level`.
  [[nodiscard]] std::string_view operator[](unsigned level) const noexcept;

  /// Tries to find the key for `val` (case insensitive).
  /// @returns the key associated to `val` or 0.
  [[nodiscard]] unsigned by_name(std::string_view val) const noexcept;

  /// Inserts or overwrites a custom log level name.
  void set(std::string name, unsigned level);

  /// Inserts or overwrites all custom log level names from `input`.
  template <class Map>
  void set(const Map& input) {
    for (const auto& [name, level] : input)
      set(name, level);
  }

private:
  std::vector<std::pair<unsigned, std::string>> mapping_;
};

} // namespace caf::detail
