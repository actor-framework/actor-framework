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

#include <memory>
#include <string>

#include "caf/fwd.hpp"
#include "caf/string_view.hpp"
#include "caf/string_algorithms.hpp"

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

    /// Tries to extract a value from the given location. Exists for backward
    /// compatibility only and will get removed with CAF 0.17.
    config_value (*get)(const void*);

    /// Tries to parse an input string. Stores and returns the parsed value on
    /// success, returns an error otherwise.
    expected<config_value> (*parse)(void*, string_view);

    /// Human-readable name of the option's type.
    std::string type_name;
  };

  // -- constructors, destructors, and assignment operators --------------------

  ///  Constructs a config option.
  config_option(string_view category, string_view name, string_view description,
                const meta_state* meta, void* value = nullptr);

  config_option(const config_option&);

  config_option(config_option&&) = default;

  config_option& operator=(const config_option&);

  config_option& operator=(config_option&&) = default;

  // -- swap function ----------------------------------------------------------

  friend void swap(config_option& first, config_option& second) noexcept;

  // -- properties -------------------------------------------------------------

  /// Returns the category of the option.
  string_view category() const noexcept;

  /// Returns the name of the option.
  string_view long_name() const noexcept;

  /// Returns (optional) one-letter short names of the option.
  string_view short_names() const noexcept;

  /// Returns a human-readable description of the option.
  string_view  description() const noexcept;

  /// Returns the full name for this config option as "<category>.<long name>".
  string_view full_name() const noexcept;

  /// Checks whether `x` holds a legal value for this option.
  error check(const config_value& x) const;

  /// Stores `x` in this option unless it is stateless.
  /// @pre `check(x) == none`.
  void store(const config_value& x) const;

  /// Returns a human-readable representation of this option's expected type.
  string_view type_name() const noexcept;

  /// Returns whether this config option stores a boolean flag.
  bool is_flag() const noexcept;

  /// Returns whether the category is optional for CLI options.
  bool has_flat_cli_name() const noexcept;

  /// Tries to parse an input string. Stores and returns the parsed value on
  /// success, returns an error otherwise.
  expected<config_value> parse(string_view input) const;

  /// @private
  // TODO: remove with CAF 0.17
  optional<config_value> get() const;

private:
  string_view buf_slice(size_t from, size_t to) const noexcept;

  std::unique_ptr<char[]> buf_;
  uint16_t category_separator_;
  uint16_t long_name_separator_;
  uint16_t short_names_separator_;
  uint16_t buf_size_;
  const meta_state* meta_;
  mutable void* value_;
};

/// Finds `config_option` string with a matching long name in (`first`, `last`],
/// where each entry is a pointer to a string. Returns a `ForwardIterator` to
/// the match and a `caf::string_view` of the option value if the entry is found
/// and a `ForwardIterator` to `last` with an empty `string_view` otherwise.
template <class ForwardIterator, class Sentinel>
std::pair<ForwardIterator, string_view>
find_by_long_name(const config_option& x, ForwardIterator first, Sentinel last) {
  auto long_name = x.long_name();
  for (; first != last; ++first) {
    string_view str{*first};
    // Make sure this is a long option starting with "--".
    if (!starts_with(str, "--"))
      continue;
    str.remove_prefix(2);
    // Skip optional "caf#" prefix.
    if (starts_with(str, "caf#"))
      str.remove_prefix(4);
    // Make sure we are dealing with the right key.
    if (!starts_with(str, long_name))
      continue;
    // Make sure the key is followed by an assignment.
    str.remove_prefix(long_name.size());
    if (!starts_with(str, "="))
      continue;
    // Remove leading '=' and return the value.
    str.remove_prefix(1);
    return {first, str};
  }
  return {first, string_view{}};
}

} // namespace caf
