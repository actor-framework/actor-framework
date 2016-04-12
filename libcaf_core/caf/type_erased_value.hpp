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

#include "caf/deep_to_string.hpp"

#include "caf/detail/type_nr.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/try_serialize.hpp"

namespace caf {

class type_erased_value;

using type_erased_value_ptr = std::unique_ptr<type_erased_value>;

/// Represents a single type-erased value.
class type_erased_value {
public:
  using rtti_pair = std::pair<uint16_t, const std::type_info*>;

  virtual ~type_erased_value();

  // pure virtual modifiers

  /// Returns a mutable pointer to the stored value.
  virtual void* get_mutable() = 0;

  /// Load the content for the stored value from `source`.
  virtual void load(deserializer& source) = 0;

  // pure virtual observers

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

  // observers

  /// Checks whether the type of the stored value matches
  /// the type nr and type info object.
  bool matches(uint16_t tnr, const std::type_info* tinf) const;

  // inline observers

  /// Returns the type number for the stored value.
  inline uint16_t type_nr() const {
    return type().first;
  }

  /// Checks whether the type of the stored value matches `rtti`.
  inline bool matches(const rtti_pair& rtti) const {
    return matches(rtti.first, rtti.second);
  }
};

template <class T>
struct type_erased_value_impl : public type_erased_value {
  T x;

  template <class... Ts>
  type_erased_value_impl(Ts&&... xs) : x(std::forward<Ts>(xs)...) {
    // nop
  }

  type_erased_value_impl(const T& value) : x(value) {
    // nop
  }

  void* get_mutable() override {
    return &x;
  }

  const void* get() const override {
    return &x;
  }

  void save(serializer& sink) const override {
    detail::try_serialize(sink, const_cast<T*>(&x));
  }

  void load(deserializer& source) override {
    detail::try_serialize(source, &x);
  }

  std::string stringify() const override {
    return deep_to_string(x);
  }

  type_erased_value_ptr copy() const override {
    return type_erased_value_ptr{new type_erased_value_impl(x)};
  }

  rtti_pair type() const override {
    auto nr = detail::type_nr<T>::value;
    return {nr, &typeid(T)};
  }
};

template <class T, size_t N>
struct type_erased_value_impl<T[N]> : public type_erased_value {
  using array_type = T[N];
  array_type xs;

  type_erased_value_impl(const T (&ys)[N]) {
    array_copy(xs, ys);
  }

  type_erased_value_impl() {
    // nop
  }

  void* get_mutable() override {
    T* tmp = xs;
    return reinterpret_cast<void*>(tmp);
  }

  const void* get() const override {
    const T* tmp = xs;
    return reinterpret_cast<const void*>(tmp);
  }

  void save(serializer& sink) const override {
    array_serialize(sink, const_cast<array_type&>(xs));
  }

  void load(deserializer& source) override {
    array_serialize(source, xs);
  }

  std::string stringify() const override {
    return deep_to_string(xs);
  }

  type_erased_value_ptr copy() const override {
    return new type_erased_value_impl(xs);
  }

  rtti_pair type() const override {
    auto nr = detail::type_nr<T>::value;
    return {nr, &typeid(T)};
  }

  template <class U, size_t Len>
  static bool array_cmp_impl(U (&lhs)[Len], const U (&rhs)[Len],
                             std::true_type) {
    for (size_t i = 0; i < Len; ++i)
      if (! array_cmp(lhs[i], rhs[i]))
        return false;
    return true;
  }

  template <class U, size_t Len>
  static bool array_cmp_impl(U (&lhs)[Len], const U (&rhs)[Len],
                             std::false_type) {
    for (size_t i = 0; i < Len; ++i)
      if (! detail::safe_equal(lhs[i], rhs[i]))
        return false;
    return true;
  }

  template <class U, size_t Len>
  static bool array_cmp(U (&lhs)[Len], const U (&rhs)[Len]) {
    std::integral_constant<bool, std::is_array<U>::value> token;
    return array_cmp_impl(lhs, rhs, token);
  }

  template <class U, size_t Len>
  static void array_copy_impl(U (&lhs)[Len], const U (&rhs)[Len],
                              std::true_type) {
    for (size_t i = 0; i < Len; ++i)
      array_copy(lhs[i], rhs[i]);
  }

  template <class U, size_t Len>
  static void array_copy_impl(U (&lhs)[Len], const U (&rhs)[Len],
                              std::false_type) {
    std::copy(rhs, rhs + Len, lhs);
  }

  template <class U, size_t Len>
  static void array_copy(U (&lhs)[Len], const U (&rhs)[Len]) {
    std::integral_constant<bool,std::is_array<U>::value> token;
    array_copy_impl(lhs, rhs, token);
  }

  template <class P, class U, size_t Len>
  static void array_serialize_impl(P& proc, U (&ys)[Len], std::true_type) {
    for (auto& y : ys)
      array_serialize(proc, y);
  }

  template <class P, class U, size_t Len>
  static void array_serialize_impl(P& proc, U (&ys)[Len], std::false_type) {
    for (auto& y : ys)
      proc & y;
  }

  template <class P, class U, size_t Len>
  static void array_serialize(P& proc, U (&ys)[Len]) {
    std::integral_constant<bool,std::is_array<U>::value> token;
    array_serialize_impl(proc, ys, token);
  }
};

template <class T, class... Ts>
type_erased_value_ptr make_type_erased(Ts&&... xs) {
  type_erased_value_ptr result;
  result.reset(new type_erased_value_impl<T>(std::forward<Ts>(xs)...));
  return result;
}

} // namespace caf

#endif // CAF_TYPE_ERASED_VALUE_HPP
