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

#include <sstream>
#include <tuple>
#include <type_traits>

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/implicit_conversions.hpp"
#include "caf/detail/message_data.hpp"
#include "caf/detail/padded_size.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_cow_ptr.hpp"

namespace caf {

/// Describes a fixed-length, copy-on-write, type-erased
/// tuple with elements of any type.
class CAF_CORE_EXPORT message {
public:
  // -- member types -----------------------------------------------------------

  using data_ptr = intrusive_cow_ptr<detail::message_data>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit message(data_ptr data) noexcept : data_(std::move(data)) {
    // nop
  }

  message() noexcept = default;

  message(message&) noexcept = default;

  message(const message&) noexcept = default;

  message& operator=(message&) noexcept = default;

  message& operator=(const message&) noexcept = default;

  // -- properties -------------------------------------------------------------

  auto types() const noexcept {
    return data_ ? data_->types() : make_type_id_list();
  }

  size_t size() const noexcept {
    return types().size();
  }

  size_t empty() const noexcept {
    return size() == 0;
  }

  template <class... Ts>
  bool match_elements() const noexcept {
    return types() == make_type_id_list<Ts...>();
  }

  /// @private
  detail::message_data& data() {
    return data_.unshared();
  }

  /// @private
  const detail::message_data& data() const noexcept {
    return *data_;
  }

  /// @private
  const detail::message_data& cdata() const noexcept {
    return *data_;
  }

  /// @private
  detail::message_data* ptr() noexcept {
    return data_.unshared_ptr();
  }

  /// @private
  const detail::message_data* ptr() const noexcept {
    return data_.get();
  }

  /// @private
  const detail::message_data* cptr() const noexcept {
    return data_.get();
  }

  constexpr operator bool() const {
    return static_cast<bool>(data_);
  }

  constexpr bool operator!() const {
    return !data_;
  }

  // -- serialization ----------------------------------------------------------

  error save(serializer& sink) const;

  error_code<sec> save(binary_serializer& sink) const;

  error load(deserializer& source);

  error_code<sec> load(binary_deserializer& source);

  // -- element access ---------------------------------------------------------

  /// Returns the type ID of the element at `index`.
  /// @pre `index < size()`
  type_id_t type_at(size_t index) const noexcept {
    auto xs = types();
    return xs[index];
  }

  /// Returns whether the element at `index` is of type `T`.
  /// @pre `index < size()`
  template <class T>
  bool match_element(size_t index) const noexcept {
    return type_at(index) == type_id_v<T>;
  }

  /// @pre `index < size()`
  /// @pre `match_element<T>(index)`
  template <class T>
  const T& get_as(size_t index) const noexcept {
    CAF_ASSERT(type_at(index) == type_id_v<T>);
    return *reinterpret_cast<const T*>(data_->at(index));
  }

  /// @pre `index < size()`
  /// @pre `match_element<T>(index)`
  template <class T>
  T& get_mutable_as(size_t index) noexcept {
    CAF_ASSERT(type_at(index) == type_id_v<T>);
    return *reinterpret_cast<T*>(data_.unshared().at(index));
  }

  // -- modifiers --------------------------------------------------------------

  void swap(message& other) noexcept {
    data_.swap(other.data_);
  }

  void reset(detail::message_data* new_ptr = nullptr,
             bool add_ref = true) noexcept {
    data_.reset(new_ptr, add_ref);
  }

private:
  data_ptr data_;
};

// -- related non-members ------------------------------------------------------

/// @relates message
inline message make_message() {
  return {};
}

/// @relates message
template <class... Ts>
message make_message(Ts&&... xs) {
  using namespace detail;
  static_assert((!std::is_pointer<strip_and_convert_t<Ts>>::value && ...));
  static constexpr size_t data_size = sizeof(message_data)
                                      + (padded_size_v<std::decay_t<Ts>> + ...);
  auto types = make_type_id_list<strip_and_convert_t<Ts>...>();
  auto vptr = malloc(data_size);
  if (vptr == nullptr)
    throw std::bad_alloc();
  auto raw_ptr = new (vptr) message_data(types);
  intrusive_cow_ptr<message_data> ptr{};
  message_data_init(raw_ptr->storage(), std::forward<Ts>(xs)...);
  return message{std::move(ptr)};
}

template <class Tuple, size_t... Is>
message make_message_from_tuple(Tuple&& xs, std::index_sequence<Is...>) {
  return make_message(std::get<Is>(std::forward<Tuple>(xs))...);
}

template <class Tuple>
message make_message_from_tuple(Tuple&& xs) {
  using tuple_type = std::decay_t<Tuple>;
  std::make_index_sequence<std::tuple_size<tuple_type>::value> seq;
  return make_message_from_tuple(std::forward<Tuple>(xs), seq);
}

/// @relates message
CAF_CORE_EXPORT error inspect(serializer& sink, const message& msg);

/// @relates message
CAF_CORE_EXPORT error_code<sec> inspect(binary_serializer& sink,
                                        const message& msg);

/// @relates message
CAF_CORE_EXPORT error inspect(deserializer& source, message& msg);

/// @relates message
CAF_CORE_EXPORT error_code<sec>
inspect(binary_deserializer& source, message& msg);

/// @relates message
CAF_CORE_EXPORT std::string to_string(const message& msg);

} // namespace caf
