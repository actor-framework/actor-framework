/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include "caf/optional.hpp"
#include "caf/string_view.hpp"

namespace caf {

/// Describes a field of type T of an adaptor.
template <class T>
struct config_value_adaptor_field {
  /// Type of the field.
  using value_type = T;

  /// Predicate function for verifying user input.
  using predicate_function = bool (*)(const value_type&);

  /// Name of the field in configuration files and on the CLI.
  string_view name;

  /// If set, makes the field optional in configuration files and on the CLI by
  /// assigning the default whenever the user provides no value.
  optional<value_type> default_value;

  /// If set, makes the field only accept values that pass this predicate.
  predicate_function predicate;
};

/// Convenience function for creating a `config_value_adaptor_field`.
/// @param name name of the field in configuration files and on the CLI.
/// @param default_value if set, provides a fallback value if the user does not
///                      provide a value.
/// @param predicate if set, restricts what values the field accepts.
/// @returns a `config_value_adaptor_field` object, constructed from given
///          arguments.
/// @relates config_value_adaptor_field
template <class T>
config_value_adaptor_field<T>
make_config_value_adaptor_field(string_view name,
                                optional<T> default_value = none,
                                bool (*predicate)(const T&) = nullptr) {
  return {name, std::move(default_value), predicate};
}

} // namespace caf
