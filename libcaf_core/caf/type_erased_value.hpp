/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_TYPE_ERASED_VALUE_HPP
#define CAF_TYPE_ERASED_VALUE_HPP

#include <cstdint>
#include <typeinfo>
#include <functional>

#include "caf/fwd.hpp"
#include "caf/type_nr.hpp"
#include "caf/deep_to_string.hpp"

#include "caf/detail/safe_equal.hpp"
#include "caf/detail/try_serialize.hpp"

namespace caf {

/// Represents a single type-erased value.
class type_erased_value {
public:
  // -- member types -----------------------------------------------------------

  using rtti_pair = std::pair<uint16_t, const std::type_info*>;

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~type_erased_value();

  // -- pure virtual modifiers -------------------------------------------------

  /// Returns a mutable pointer to the stored value.
  virtual void* get_mutable() = 0;

  /// Load the content for the stored value from `source`.
  virtual void load(deserializer& source) = 0;

  // -- pure virtual observers -------------------------------------------------

  /// Returns the type number and type information object for the stored value.
  virtual rtti_pair type() const = 0;

  /// Returns a pointer to the stored value.
  virtual const void* get() const = 0;

  /// Saves the content of the stored value to `sink`.
  virtual void save(serializer& sink) const = 0;

  /// Converts the stored value to a string.
  virtual std::string stringify() const = 0;

  /// Returns a copy of the stored value.
  virtual type_erased_value_ptr copy() const = 0;

  // -- observers --------------------------------------------------------------

  /// Checks whether the type of the stored value matches
  /// the type nr and type info object.
  bool matches(uint16_t tnr, const std::type_info* tinf) const;

  // -- inline observers -------------------------------------------------------

  /// Returns the type number for the stored value.
  inline uint16_t type_nr() const {
    return type().first;
  }

  /// Checks whether the type of the stored value matches `rtti`.
  inline bool matches(const rtti_pair& rtti) const {
    return matches(rtti.first, rtti.second);
  }
};

/// @relates type_erased_value_impl
template <class Processor>
typename std::enable_if<Processor::is_saving::value>::type
serialize(Processor& proc, type_erased_value& x) {
  x.save(proc);
}

/// @relates type_erased_value_impl
template <class Processor>
typename std::enable_if<Processor::is_loading::value>::type
serialize(Processor& proc, type_erased_value& x) {
  x.load(proc);
}

/// @relates type_erased_value_impl
inline std::string to_string(const type_erased_value& x) {
  return x.stringify();
}

/// @relates type_erased_value
/// Default implementation for single type-erased values.
template <class T>
class type_erased_value_impl : public type_erased_value {
public:
  // -- member types -----------------------------------------------------------

  using value_type = typename detail::strip_reference_wrapper<T>::type;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  type_erased_value_impl(Ts&&... xs) : x_(std::forward<Ts>(xs)...) {
    // nop
  }

  template <class U, size_t N,
            class = typename std::enable_if<std::is_same<T, U[N]>::value>::type>
  type_erased_value_impl(const U (&ys)[N]) {
    array_copy(x_, ys);
  }

  template <class U, size_t N,
            class = typename std::enable_if<std::is_same<T, U[N]>::value>::type>
  type_erased_value_impl(const U (&&ys)[N]) {
    array_copy(x_, ys);
  }

  type_erased_value_impl(type_erased_value_impl&& other)
      : type_erased_value_impl(std::move(other.x_)) {
    // nop
  }

  type_erased_value_impl(const type_erased_value_impl& other)
      : type_erased_value_impl(other.x_) {
    // nop
  }

  // -- overridden modifiers ---------------------------------------------------

  void* get_mutable() override {
    return addr_of(x_);
  }

  void load(deserializer& source) override {
    detail::try_serialize(source, addr_of(x_));
  }

  // -- overridden observers ---------------------------------------------------

  static rtti_pair type(std::integral_constant<uint16_t, 0>) {
    return {0, &typeid(value_type)};
  }

  template <uint16_t V>
  static rtti_pair type(std::integral_constant<uint16_t, V>) {
    return {V, nullptr};
  }

  rtti_pair type() const override {
    std::integral_constant<uint16_t, caf::type_nr<value_type>::value> token;
    return type(token);
  }

  const void* get() const override {
    // const is restored when returning from the function
    return addr_of(const_cast<T&>(x_));
  }

  void save(serializer& sink) const override {
    detail::try_serialize(sink, addr_of(x_));
  }

  std::string stringify() const override {
    return deep_to_string(x_);
  }

  type_erased_value_ptr copy() const override {
    return type_erased_value_ptr{new type_erased_value_impl(x_)};
  }

private:
  // -- address-of-member utility ----------------------------------------------

  template <class U>
  static inline U* addr_of(const U& x) {
    return const_cast<U*>(&x);
  }

  template <class U, size_t S>
  static inline U* addr_of(const U (&x)[S]) {
    return const_cast<U*>(static_cast<const U*>(x));
  }

  template <class U>
  static inline U* addr_of(std::reference_wrapper<U>& x) {
    return &x.get();
  }

  template <class U>
  static inline U* addr_of(const std::reference_wrapper<U>& x) {
    return &x.get();
  }

  // -- array copying utility --------------------------------------------------

  template <class U, size_t Len>
  static void array_copy_impl(U (&x)[Len], const U (&y)[Len], std::true_type) {
    for (size_t i = 0; i < Len; ++i)
      array_copy(x[i], y[i]);
  }

  template <class U, size_t Len>
  static void array_copy_impl(U (&x)[Len], const U (&y)[Len], std::false_type) {
    std::copy(y, y + Len, x);
  }

  template <class U, size_t Len>
  static void array_copy(U (&x)[Len], const U (&y)[Len]) {
    std::integral_constant<bool, std::is_array<U>::value> token;
    array_copy_impl(x, y, token);
  }

  // -- data members -----------------------------------------------------------

  T x_;
};

/// @relates type_erased_value
/// Creates a type-erased value of type `T` from `xs`.
template <class T, class... Ts>
type_erased_value_ptr make_type_erased_value(Ts&&... xs) {
  type_erased_value_ptr result;
  result.reset(new type_erased_value_impl<T>(std::forward<Ts>(xs)...));
  return result;
}

/// @relates type_erased_value
/// Creates a type-erased view for `x`.
template <class T>
type_erased_value_impl<std::reference_wrapper<T>> make_type_erased_view(T& x) {
  return {std::ref(x)};
}

} // namespace caf

#endif // CAF_TYPE_ERASED_VALUE_HPP
