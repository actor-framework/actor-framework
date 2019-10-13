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
  using value_type = T;
  using predicate_function = bool (*)(const T&);
  string_view name;
  optional<T> default_value;
  predicate_function predicate;
};

template <class T>
config_value_adaptor_field<T>
make_config_value_adaptor_field(string_view name,
                                optional<T> default_value = none,
                                bool (*predicate)(const T&) = nullptr) {
  return {name, std::move(default_value), predicate};
}

} // namespace caf
