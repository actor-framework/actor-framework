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
#include <typeinfo>
#include <functional>

#include "caf/error.hpp"
#include "caf/type_erased_value.hpp"

#include "caf/detail/safe_equal.hpp"
#include "caf/detail/try_serialize.hpp"

namespace caf {
namespace detail {

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

  error load(deserializer& source) override {
    return source(*addr_of(x_));
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

  error save(serializer& sink) const override {
    return sink(*addr_of(const_cast<T&>(x_)));
  }

  std::string stringify() const override {
    return deep_to_string(x_);
  }

  type_erased_value_ptr copy() const override {
    return type_erased_value_ptr{new type_erased_value_impl(x_)};
  }

  // -- conversion operators ---------------------------------------------------

  operator value_type&() {
    return x_;
  }

  operator const value_type&() const {
    return x_;
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

} // namespace detail
} // namespace caf

