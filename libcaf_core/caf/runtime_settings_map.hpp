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

#include <cstdint>
#include <unordered_map>

#include "caf/atom.hpp"
#include "caf/detail/shared_spinlock.hpp"
#include "caf/variant.hpp"

namespace caf {

/// Thread-safe container for mapping atoms to arbitrary settings.
class runtime_settings_map {
public:
  // -- member types -----------------------------------------------------------

  using mutex_type = detail::shared_spinlock;

  using generic_pointer = void*;

  using generic_function_pointer = void (*)();

  using mapped_type = variant<none_t, int64_t, uint64_t, atom_value,
                              generic_pointer, generic_function_pointer>;

  // -- thread-safe access -----------------------------------------------------

  /// Returns the value mapped to `key`.
  mapped_type get(atom_value key) const;

  /// Returns the value mapped to `key` or `fallback` if no value is mapped to
  /// this key.
  mapped_type get_or(atom_value key, mapped_type fallback) const;

  /// Maps `key` to `value` and returns the previous value.
  void set(atom_value key, mapped_type value);

  /// Removes `key` from the map.
  void erase(atom_value key);

  /// Returns the number of key-value entries.
  size_t size() const;

  /// Returns whether `size()` equals 0.
  bool empty() const {
    return size() == 0;
  }

private:
  // -- member variables -------------------------------------------------------

  mutable mutex_type mtx_;

  std::unordered_map<atom_value, mapped_type> map_;
};

} // namespace caf
