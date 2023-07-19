// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/implicit_conversions.hpp"
#include "caf/span.hpp"
#include "caf/type_id.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

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
  constexpr size_t size() const noexcept {
    return data_[0];
  }

  /// Returns `size() == 0`.
  constexpr bool empty() const noexcept {
    return size() == 0;
  }

  /// Returns the type ID at `index`.
  constexpr type_id_t operator[](size_t index) const noexcept {
    return data_[index + 1];
  }

  /// Compares this list to `other`.
  int compare(type_id_list other) const noexcept {
    // These conversions are safe, because the size is stored in 16 bits.
    int s1 = data_[0];
    int s2 = other.data_[0];
    int diff = s1 - s2;
    if (diff == 0)
      return memcmp(begin(), other.begin(),
                    static_cast<unsigned>(s1) * sizeof(type_id_t));
    return diff;
  }

  /// Returns an iterator to the first type ID.
  pointer begin() const noexcept {
    return data_ + 1;
  }

  /// Returns the past-the-end iterator.
  pointer end() const noexcept {
    return begin() + size();
  }

  /// Returns the number of bytes that a buffer needs to allocate for storing a
  /// type-erased tuple for the element types stored in this list.
  size_t data_size() const noexcept;

  /// Concatenates all `lists` into a single type ID list.
  static type_id_list concat(span<type_id_list> lists);

  /// Concatenates all `lists` into a single type ID list.
  template <class... Ts>
  static type_id_list
  concat(type_id_list list1, type_id_list list2, Ts... lists) {
    type_id_list arr[] = {list1, list2, lists...};
    return concat(arr);
  }

private:
  pointer data_;
};

/// @private
template <class... Ts>
struct make_type_id_list_helper {
  static inline type_id_t data[] = {static_cast<type_id_t>(sizeof...(Ts)),
                                    type_id_v<Ts>...};
};

/// Constructs a ::type_id_list from the template parameter pack `Ts`.
/// @relates type_id_list
template <class... Ts>
constexpr type_id_list make_type_id_list() {
  return type_id_list{make_type_id_list_helper<Ts...>::data};
}

/// @relates type_id_list
CAF_CORE_EXPORT std::string to_string(type_id_list xs);

/// @relates type_id_list
CAF_CORE_EXPORT type_id_list types_of(const message& msg);

template <class... Ts>
type_id_list types_of(const std::tuple<Ts...>&) {
  return make_type_id_list<detail::strip_and_convert_t<Ts>...>();
}

} // namespace caf

namespace caf::detail {

template <class F>
struct argument_type_id_list_factory;

template <class R, class... Ts>
struct argument_type_id_list_factory<R(Ts...)> {
  static type_id_list make() {
    return make_type_id_list<Ts...>();
  }
};

template <class F>
type_id_list make_argument_type_id_list() {
  return argument_type_id_list_factory<F>::make();
}

template <class List>
struct to_type_id_list_helper;

template <class... Ts>
struct to_type_id_list_helper<type_list<Ts...>> {
  static constexpr type_id_list get() {
    return make_type_id_list<typename strip_param<Ts>::type...>();
  }
};

template <class List>
constexpr type_id_list to_type_id_list() {
  return to_type_id_list_helper<List>::get();
}

} // namespace caf::detail
