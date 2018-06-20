/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <string>

#include "caf/fwd.hpp"

namespace caf {

/// Defines a configuration option for the application.
class config_option {
public:
  // -- member types -----------------------------------------------------------

  /// Custom vtable-like struct for delegating to type-specific functions and
  /// storing type-specific information shared by several config options.
  struct meta_state {
    /// Checks whether a value matches the option's type.
    error (*check)(const config_value&);

    /// Stores a value in the given location.
    void (*store)(void*, const config_value&);

    /// Tries to extrac a value from the given location. Exists for backward
    /// compatibility only and will get removed with CAF 0.17.
    config_value (*get)(const void*);

    /// Human-readable name of the option's type.
    std::string type_name;
  };

  // -- constructors, destructors, and assignment operators --------------------

  ///  Constructs a config option.
  config_option(std::string category, const char* name, std::string description,
                const meta_state* meta, void* storage = nullptr);

  config_option(config_option&&) = default;

  config_option& operator=(config_option&&) = default;

  // -- properties -------------------------------------------------------------

  inline const std::string& category() const noexcept {
    return category_;
  }

  /// Returns the
  inline const std::string& long_name() const noexcept {
    return long_name_;
  }

  inline const std::string& short_names() const noexcept {
    return short_names_;
  }

  inline const std::string& description() const noexcept {
    return description_;
  }

  /// Returns the full name for this config option as "<category>.<long name>".
  std::string full_name() const;

  /// Checks whether `x` holds a legal value for this option.
  error check(const config_value& x) const;

  /// Stores `x` in this option unless it is stateless.
  /// @pre `check(x) == none`.
  void store(const config_value& x) const;

  /// Returns a human-readable representation of this option's expected type.
  const std::string& type_name() const noexcept;

  /// Returns whether this config option stores a boolean flag.
  bool is_flag() const noexcept;

  /// @private
  // TODO: remove with CAF 0.17
  optional<config_value> get() const;

private:
  std::string category_;
  std::string long_name_;
  std::string short_names_;
  std::string description_;
  const meta_state* meta_;
  mutable void* value_;
};

} // namespace caf

