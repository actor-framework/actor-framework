// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/implicit_conversions.hpp"
#include "caf/detail/message_data.hpp"
#include "caf/detail/padded_size.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_cow_ptr.hpp"
#include "caf/raise_error.hpp"

#include <sstream>
#include <tuple>
#include <type_traits>

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

  message(message&&) noexcept = default;

  message(const message&) noexcept = default;

  message& operator=(message&&) noexcept = default;

  message& operator=(const message&) noexcept = default;

  // -- concatenation ----------------------------------------------------------

  template <class... Ts>
  static message concat(Ts&&... xs) {
    static_assert(sizeof...(Ts) >= 2);
    auto types = type_id_list::concat(types_of(xs)...);
    auto ptr = detail::message_data::make_uninitialized(types);
    ptr->init_from(std::forward<Ts>(xs)...);
    return message{data_ptr{ptr.release(), false}};
  }

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

  explicit operator bool() const noexcept {
    return static_cast<bool>(data_);
  }

  bool operator!() const noexcept {
    return !data_;
  }

  /// Checks whether this messages contains the types `Ts...` with values
  /// `values...`. Users may pass `std::ignore` as a wildcard for individual
  /// elements. Elements are compared using `operator==`.
  template <class... Ts>
  bool matches(const Ts&... values) const {
    return matches_impl(std::index_sequence_for<Ts...>{}, values...);
  }

  // -- serialization ----------------------------------------------------------

  bool save(serializer& sink) const;

  bool save(binary_serializer& sink) const;

  bool load(deserializer& source);

  bool load(binary_deserializer& source);

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

  /// Forces the message to copy its content if more than one reference to the
  /// content exists.
  void force_unshare() {
    data_.unshare();
  }

private:
  template <size_t Pos, class T>
  bool matches_at(const T& value) const {
    if constexpr (std::is_same_v<T, decltype(std::ignore)>)
      return true;
    else
      return match_element<T>(Pos) && get_as<T>(Pos) == value;
  }

  template <size_t... Is, class... Ts>
  bool matches_impl(std::index_sequence<Is...>, const Ts&... values) const {
    return (matches_at<Is>(values) && ...);
  }

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
  static_assert((!std::is_pointer_v<strip_and_convert_t<Ts>> && ...));
  static_assert((is_complete<type_id<strip_and_convert_t<Ts>>> && ...));
  static constexpr size_t data_size
    = sizeof(message_data) + (padded_size_v<strip_and_convert_t<Ts>> + ...);
  auto types = make_type_id_list<strip_and_convert_t<Ts>...>();
  auto vptr = malloc(data_size);
  if (vptr == nullptr)
    CAF_RAISE_ERROR(std::bad_alloc, "bad_alloc");
  auto raw_ptr = new (vptr) message_data(types);
  intrusive_cow_ptr<message_data> ptr{raw_ptr, false};
  raw_ptr->init(std::forward<Ts>(xs)...);
  return message{std::move(ptr)};
}

/// @relates message
template <class Tuple, size_t... Is>
message make_message_from_tuple(Tuple&& xs, std::index_sequence<Is...>) {
  return make_message(std::get<Is>(std::forward<Tuple>(xs))...);
}

/// @relates message
template <class Tuple>
message make_message_from_tuple(Tuple&& xs) {
  using tuple_type = std::decay_t<Tuple>;
  std::make_index_sequence<std::tuple_size_v<tuple_type>> seq;
  return make_message_from_tuple(std::forward<Tuple>(xs), seq);
}

/// @relates message
template <class Inspector>
auto inspect(Inspector& f, message& x)
  -> std::enable_if_t<Inspector::is_loading, decltype(x.load(f))> {
  return x.load(f);
}

/// @relates message
template <class Inspector>
auto inspect(Inspector& f, message& x)
  -> std::enable_if_t<!Inspector::is_loading, decltype(x.save(f))> {
  return x.save(f);
}

/// @relates message
CAF_CORE_EXPORT std::string to_string(const message& x);

} // namespace caf
