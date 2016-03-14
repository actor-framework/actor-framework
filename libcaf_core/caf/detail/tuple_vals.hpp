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

#ifndef CAF_DETAIL_TUPLE_VALS_HPP
#define CAF_DETAIL_TUPLE_VALS_HPP

#include <tuple>
#include <stdexcept>

#include "caf/serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/deep_to_string.hpp"

#include "caf/detail/type_nr.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/message_data.hpp"
#include "caf/detail/try_serialize.hpp"

namespace caf {
namespace detail {

template <size_t Pos, size_t Max, bool InRange = (Pos < Max)>
struct tup_ptr_access {
  template <class T>
  static typename std::conditional<
    std::is_const<T>::value,
    const void*,
    void*
  >::type
  get(size_t pos, T& tup) {
    if (pos == Pos) return &std::get<Pos>(tup);
    return tup_ptr_access<Pos + 1, Max>::get(pos, tup);
  }

  template <class T>
  static bool cmp(size_t pos, T& tup, const void* x) {
    using type = typename std::tuple_element<Pos, T>::type;
    if (pos == Pos)
      return safe_equal(std::get<Pos>(tup), *reinterpret_cast<const type*>(x));
    return tup_ptr_access<Pos + 1, Max>::cmp(pos, tup, x);
  }

  template <class T, class Processor>
  static void serialize(size_t pos, T& tup, Processor& proc) {
    if (pos == Pos)
      try_serialize(proc, &std::get<Pos>(tup));
    else
      tup_ptr_access<Pos + 1, Max>::serialize(pos, tup, proc);
  }

  template <class T>
  static std::string stringify(size_t pos, const T& tup) {
    if (pos == Pos)
      return deep_to_string(std::get<Pos>(tup));
    return tup_ptr_access<Pos + 1, Max>::stringify(pos, tup);
  }
};

template <size_t Pos, size_t Max>
struct tup_ptr_access<Pos, Max, false> {
  template <class T>
  static typename std::conditional<
    std::is_const<T>::value,
    const void*,
    void*
  >::type
  get(size_t, T&) {
    // end of recursion
    return nullptr;
  }

  template <class T>
  static bool cmp(size_t, T&, const void*) {
    return false;
  }

  template <class T, class Processor>
  static void serialize(size_t, T&, Processor&) {
    // end of recursion
  }

  template <class T>
  static std::string stringify(size_t, const T&) {
    return "<unprintable>";
  }
};

template <class T, uint16_t N = detail::type_nr<T>::value>
struct tuple_vals_type_helper {
  static typename message_data::element_rtti get() {
    return {N, nullptr};
  }
};

template <class T>
struct tuple_vals_type_helper<T, 0> {
  static typename message_data::element_rtti get() {
    return {0, &typeid(T)};
  }
};

template <class... Ts>
class tuple_vals : public message_data {
public:
  static_assert(sizeof...(Ts) > 0, "tuple_vals is not allowed to be empty");

  using super = message_data;

  using element_rtti = typename message_data::element_rtti;

  using data_type = std::tuple<Ts...>;

  tuple_vals(const tuple_vals&) = default;

  template <class... Us>
  tuple_vals(Us&&... xs)
      : data_(std::forward<Us>(xs)...),
        types_{{tuple_vals_type_helper<Ts>::get()...}} {
    // nop
  }

  data_type& data() {
    return data_;
  }

  const data_type& data() const {
    return data_;
  }

  size_t size() const override {
    return sizeof...(Ts);
  }

  message_data::cow_ptr copy() const override {
    return message_data::cow_ptr(new tuple_vals(*this), false);
  }

  const void* at(size_t pos) const override {
    CAF_ASSERT(pos < size());
    return tup_ptr_access<0, sizeof...(Ts)>::get(pos, data_);
  }

  bool compare_at(size_t pos, const element_rtti& rtti, const void* x) const override {
    CAF_ASSERT(pos < size());
    auto& rtti_at_pos = types_[pos];
    if (rtti.first != rtti_at_pos.first)
      return false;
    if (rtti_at_pos.first == 0 && *rtti_at_pos.second != *rtti.second)
      return false;
    return tup_ptr_access<0, sizeof...(Ts)>::cmp(pos, data_, x);
  }

  void* mutable_at(size_t pos) override {
    CAF_ASSERT(pos < size());
    return const_cast<void*>(at(pos));
  }

  std::string stringify_at(size_t pos) const override {
    return tup_ptr_access<0, sizeof...(Ts)>::stringify(pos, data_);
  }

  void serialize_at(deserializer& source, size_t pos) override {
    return tup_ptr_access<0, sizeof...(Ts)>::serialize(pos, data_, source);
  }

  bool match_element(size_t pos, uint16_t typenr,
                     const std::type_info* rtti) const override {
    CAF_ASSERT(pos < size());
    auto& et = types_[pos];
    if (et.first != typenr)
      return false;
    return et.first != 0 || et.second == rtti || *et.second == *rtti;
  }

  uint32_t type_token() const override {
    return make_type_token<Ts...>();
  }

  element_rtti type_at(size_t pos) const override {
    return types_[pos];
  }

  void serialize_at(serializer& sink, size_t pos) const override {
    // the serialization framework uses non-const arguments for deserialization,
    // but this cast is safe since the values are not actually changed
    auto& nc_data = const_cast<data_type&>(data_);
    return tup_ptr_access<0, sizeof...(Ts)>::serialize(pos, nc_data, sink);
  }

private:
  data_type data_;
  std::array<element_rtti, sizeof...(Ts)> types_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TUPLE_VALS_HPP
