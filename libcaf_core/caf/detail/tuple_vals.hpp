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

#include "caf/detail/type_list.hpp"

#include "caf/detail/message_data.hpp"

namespace caf {
namespace detail {

template <size_t Pos, size_t Max, bool InRange = (Pos < Max)>
struct tup_ptr_access {
  template <class T>
  static inline typename std::conditional<std::is_const<T>::value,
                      const void*, void*>::type
  get(size_t pos, T& tup) {
    if (pos == Pos) return &std::get<Pos>(tup);
    return tup_ptr_access<Pos + 1, Max>::get(pos, tup);
  }
};

template <size_t Pos, size_t Max>
struct tup_ptr_access<Pos, Max, false> {
  template <class T>
  static inline typename std::conditional<std::is_const<T>::value,
                      const void*, void*>::type
  get(size_t, T&) {
    // end of recursion
    return nullptr;
  }
};

using tuple_vals_rtti = std::pair<uint16_t, const std::type_info*>;

template <class T, uint16_t N = type_nr<T>::value>
struct tuple_vals_type_helper {
  static tuple_vals_rtti get() {
    return {N, nullptr};
  }
};

template <class T>
struct tuple_vals_type_helper<T, 0> {
  static tuple_vals_rtti get() {
    return {0, &typeid(T)};
  }
};

template <class... Ts>
class tuple_vals : public message_data {
public:
  static_assert(sizeof...(Ts) > 0, "tuple_vals is not allowed to be empty");

  using super = message_data;

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

  void* mutable_at(size_t pos) override {
    CAF_ASSERT(pos < size());
    return const_cast<void*>(at(pos));
  }

  bool match_element(size_t pos, uint16_t typenr,
                     const std::type_info* rtti) const override {
    CAF_ASSERT(pos < size());
    auto& et = types_[pos];
    if (et.first != typenr) {
      return false;
    }
    return et.first != 0 || et.second == rtti || *et.second == *rtti;
  }

  uint32_t type_token() const override {
    return make_type_token<Ts...>();
  }

  const char* uniform_name_at(size_t pos) const override {
    auto& et = types_[pos];
    if (et.first != 0) {
      return numbered_type_names[et.first - 1];
    }
    auto uti = uniform_typeid(*et.second, true);
    return uti ? uti->name() : "-invalid-";
  }

  uint16_t type_nr_at(size_t pos) const override {
    return types_[pos].first;
  }

private:
  data_type data_;
  std::array<tuple_vals_rtti, sizeof...(Ts)> types_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TUPLE_VALS_HPP
