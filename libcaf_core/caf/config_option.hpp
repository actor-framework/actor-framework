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

  /// Custom vtable-like struct for delegating to type-specific functions.
  struct vtbl_type {
    error (*check)(const config_value&);
    void (*store)(void*, const config_value&);
    std::string (*type_name)();
  };

  // -- constructors, destructors, and assignment operators --------------------

  ///  Constructs a config option.
  config_option(std::string category, const char* name, std::string description,
                bool is_flag, const vtbl_type& vtbl, void* storage = nullptr);

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

  inline bool is_flag() const noexcept {
    return is_flag_;
  }

  /// Returns the full name for this config option as "<category>.<long name>".
  std::string full_name() const;

  /// Checks whether `x` holds a legal value for this option.
  error check(const config_value& x) const;

  /// Stores `x` in this option unless it is stateless.
  /// @pre `check(x) == none`.
  void store(const config_value& x) const;

  /// Returns a human-readable representation of this option's expected type.
  std::string type_name() const;

private:
  std::string category_;
  std::string long_name_;
  std::string short_names_;
  std::string description_;
  bool is_flag_;
  vtbl_type vtbl_;
  mutable void* value_;
};

} // namespace caf

