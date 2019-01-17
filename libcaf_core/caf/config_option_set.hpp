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

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "caf/config_option.hpp"
#include "caf/fwd.hpp"
#include "caf/make_config_option.hpp"
#include "caf/pec.hpp"
#include "caf/string_view.hpp"

namespace caf {

/// A set of `config_option` objects that parses CLI arguments into a
/// `settings` object.
class config_option_set {
public:
  // -- member types -----------------------------------------------------------

  /// An iterator over CLI arguments.
  using argument_iterator = std::vector<std::string>::const_iterator;

  /// The result of `parse` member functions.
  using parse_result = std::pair<pec, argument_iterator>;

  /// List of config options.
  using option_vector = std::vector<config_option>;

  /// Pointer to a config option.
  using option_pointer = const config_option*;

  /// An iterator over ::config_option unique pointers.
  using iterator = option_vector::iterator;

  /// An iterator over ::config_option unique pointers.
  using const_iterator = option_vector::const_iterator;

  // -- properties -------------------------------------------------------------

  /// Returns the first `config_option` that matches the CLI name.
  /// @param name Config option name formatted as
  ///             `<prefix>#<category>.<long-name>`. Users can omit `<prefix>`
  ///             for options that have an empty prefix and `<category>`
  ///             if the category is "global".
  option_pointer cli_long_name_lookup(string_view name) const;

  /// Returns the first `config_option` that matches the CLI short option name.
  option_pointer cli_short_name_lookup(char short_name) const;

  /// Returns the first `config_option` that matches category and long name.
  option_pointer qualified_name_lookup(string_view category,
                                       string_view long_name) const;

  /// Returns the first `config_option` that matches the qualified name.
  /// @param name Config option name formatted as `<category>.<long-name>`.
  option_pointer qualified_name_lookup(string_view name) const;

  /// Returns the number of stored config options.
  size_t size() const noexcept {
    return opts_.size();
  }

  /// Returns the number of stored config options.
  bool empty() const noexcept {
    return opts_.empty();
  }

  /// Returns an iterator to the first ::config_option object.
  iterator begin() noexcept {
    return opts_.begin();
  }

  /// Returns an iterator to the first ::config_option object.
  const_iterator begin() const noexcept {
    return opts_.begin();
  }

  /// Returns the past-the-end iterator.
  iterator end() noexcept {
    return opts_.end();
  }

  /// Returns the past-the-end iterator.
  const_iterator end() const noexcept {
    return opts_.end();
  }

  /// Adds a config option to the set.
  template <class T>
  config_option_set& add(string_view category, string_view name,
                         string_view description) & {
    return add(make_config_option<T>(category, name, description));
  }

  /// Adds a config option to the set.
  template <class T>
  config_option_set& add(string_view name, string_view description) & {
    return add(make_config_option<T>("global", name, description));
  }

  /// Adds a config option to the set.
  template <class T>
  config_option_set&& add(string_view category, string_view name,
                          string_view description) && {
    return std::move(add<T>(category, name, description));
  }

  /// Adds a config option to the set.
  template <class T>
  config_option_set&& add(string_view name, string_view description) && {
    return std::move(add<T>("global", name, description));
  }

  /// Adds a config option to the set that synchronizes its value with `ref`.
  template <class T>
  config_option_set& add(T& ref, string_view category, string_view name,
                         string_view description) & {
    return add(make_config_option<T>(ref, category, name, description));
  }

  /// Adds a config option to the set that synchronizes its value with `ref`.
  template <class T>
  config_option_set& add(T& ref, string_view name, string_view description) & {
    return add(ref, "global", name, description);
  }

  /// Adds a config option to the set that synchronizes its value with `ref`.
  template <class T>
  config_option_set&& add(T& ref, string_view category, string_view name,
                          string_view description) && {
    return std::move(add(ref, category, name, description));
  }

  /// Adds a config option to the set that synchronizes its value with `ref`.
  template <class T>
  config_option_set&& add(T& ref, string_view name,
                          string_view description) && {
    return std::move(add(ref, "global", name, description));
  }

  /// Adds a config option to the set.
  config_option_set& add(config_option opt);

  /// Generates human-readable help text for all options.
  std::string help_text(bool global_only = true) const;

  // -- parsing ----------------------------------------------------------------

  /// Parses a given range as CLI arguments into `config`.
  parse_result parse(settings& config, argument_iterator begin,
                     argument_iterator end) const;

  /// Parses a given range as CLI arguments into `config`.
  parse_result parse(settings& config,
                     const std::vector<std::string>& args) const;

private:
  // -- member variables -------------------------------------------------------

  option_vector opts_;
};

} // namespace caf
