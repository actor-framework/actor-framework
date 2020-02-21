/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/type_id.hpp"

namespace caf {

/// A list of type IDs, stored in a size-prefix, contiguous memory block.
class type_id_list : detail::comparable<type_id_list> {
public:
  using pointer = const type_id_t*;

  constexpr explicit type_id_list(pointer data) noexcept : data_(data) {
    // nop
  }

  constexpr type_id_list(const type_id_list&) noexcept = default;

  type_id_list& operator=(const type_id_list&) noexcept = default;

  /// Queries whether this type list contains data, i.e, `data() != nullptr`.
  constexpr operator bool() const noexcept {
    return data_ != nullptr;
  }

  /// Returns the raw pointer to the size-prefixed list.
  constexpr pointer data() const noexcept {
    return data_;
  }

  /// Returns the number of elements in the list.
  /// @pre `data() != nullptr`
  constexpr size_t size() const noexcept {
    return data_[0];
  }

  /// Returns the type ID at `index`.
  /// @pre `data() != nullptr`
  constexpr type_id_t operator[](size_t index) const noexcept {
    return data_[index + 1];
  }

  /// Compares this list to `other`.
  int compare(type_id_list other) const noexcept {
    return memcmp(data_, other.data_, (size() + 1) * sizeof(type_id_t));
  }

  /// Returns an iterator to the first type ID.
  pointer begin() const noexcept {
    return data_ + 1;
  }

  /// Returns the past-the-end iterator.
  pointer end() const noexcept {
    return begin() + size();
  }

private:
  pointer data_;
};

/// @private
template <class... Ts>
struct make_type_id_list_helper {
  static constexpr inline type_id_t data[]
    = {static_cast<type_id_t>(sizeof...(Ts)), type_id_v<Ts>...};
};

/// Constructs a ::type_id_list from the template parameter pack `Ts`.
/// @relates type_id_list
template <class... Ts>
constexpr type_id_list make_type_id_list() {
  return type_id_list{make_type_id_list_helper<Ts...>::data};
}

/// @relates type_id_list
CAF_CORE_EXPORT std::string to_string(type_id_list xs);

} // namespace caf
