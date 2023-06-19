// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/string_algorithms.hpp"

namespace caf {

/// Defines a configuration option for the application.
class CAF_CORE_EXPORT config_option {
public:
  // -- member types -----------------------------------------------------------

  /// Custom vtable-like struct for delegating to type-specific functions and
  /// storing type-specific information shared by several config options.
  struct meta_state {
    /// Tries to perform this sequence of steps:
    /// - Convert the config value to the type of the config option.
    /// - Assign the converted value back to the config value to synchronize
    ///   conversions back to the caller.
    /// - Store the converted value in the pointer unless it is `nullptr`.
    error (*sync)(void*, config_value&);

    /// Tries to extract a value from the given location. Exists for backward
    /// compatibility only and will get removed with CAF 0.17.
    config_value (*get)(const void*);

    /// Human-readable name of the option's type.
    std::string_view type_name;
  };

  // -- constructors, destructors, and assignment operators --------------------

  ///  Constructs a config option.
  config_option(std::string_view category, std::string_view name,
                std::string_view description, const meta_state* meta,
                void* value = nullptr);

  config_option(const config_option&);

  config_option(config_option&&) = default;

  config_option& operator=(const config_option&);

  config_option& operator=(config_option&&) = default;

  // -- swap function ----------------------------------------------------------

  friend void swap(config_option& first, config_option& second) noexcept;

  // -- properties -------------------------------------------------------------

  /// Returns the category of the option.
  std::string_view category() const noexcept;

  /// Returns the name of the option.
  std::string_view long_name() const noexcept;

  /// Returns (optional) one-letter short names of the option.
  std::string_view short_names() const noexcept;

  /// Returns a human-readable description of the option.
  std::string_view description() const noexcept;

  /// Returns the full name for this config option as "<category>.<long name>".
  std::string_view full_name() const noexcept;

  /// Synchronizes the value of this config option with `x` and vice versa.
  ///
  /// Tries to perform this sequence of steps:
  /// - Convert the config value to the type of the config option.
  /// - Assign the converted value back to the config value to synchronize
  ///   conversions back to the caller.
  /// - Store the converted value unless this option is stateless.
  error sync(config_value& x) const;

  /// Returns a human-readable representation of this option's expected type.
  std::string_view type_name() const noexcept;

  /// Returns whether this config option stores a boolean flag.
  bool is_flag() const noexcept;

  /// Returns whether the category is optional for CLI options.
  bool has_flat_cli_name() const noexcept;

private:
  std::string_view buf_slice(size_t from, size_t to) const noexcept;

  std::unique_ptr<char[]> buf_;
  uint16_t category_separator_;
  uint16_t long_name_separator_;
  uint16_t short_names_separator_;
  uint16_t buf_size_;
  const meta_state* meta_;
  mutable void* value_;
};

/// Finds `config_option` string with a matching long name in the string
/// argument list [`first`, `last`). Returns an `argument_iterator` to the
/// matching location and a `string_view` of the option value if the entry is
/// found, or a `arugment_iterator` to `last` with an empty `string_view`
/// otherwise.
std::pair<std::vector<std::string>::const_iterator, std::string_view>
find_by_long_name(const config_option& x,
                  std::vector<std::string>::const_iterator first,
                  std::vector<std::string>::const_iterator last);

} // namespace caf
