/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include <type_traits>

#include "caf/allowed_unsafe_message_type.hpp"
#include "caf/detail/inspect.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/meta/annotation.hpp"
#include "caf/meta/save_callback.hpp"
#include "caf/sec.hpp"
#include "caf/unifyn.hpp"

#define CAF_READ_INSPECTOR_TRY(statement)                                      \
  if constexpr (std::is_same<decltype(statement), void>::value) {              \
    statement;                                                                 \
  } else {                                                                     \
    if (auto err = statement) {                                                \
      result = err;                                                            \
      return false;                                                            \
    }                                                                          \
  }

namespace caf {

/// Injects an `operator()` that dispatches to `Subtype::apply`.
template <class Subtype>
class read_inspector {
public:
  static constexpr bool reads_state = true;

  static constexpr bool writes_state = false;

  template <class Sub = Subtype, class... Ts>
  std::enable_if_t<std::is_same<typename Sub::result_type, void>::value>
  operator()(Ts&&... xs) {
    auto dummy = unit;
    static_cast<void>((try_apply(dummy, xs) && ...));
  }

  template <class Sub = Subtype, class... Ts>
  std::enable_if_t<!std::is_same<typename Sub::result_type, void>::value,
                   typename Sub::result_type>
  operator()(Ts&&... xs) {
    auto result = typename Sub::result_type{};
    static_cast<void>((try_apply(result, xs) && ...));
    return result;
  }

private:
  template <class Tuple, size_t... Is>
  static auto
  apply_tuple(Subtype& dref, const Tuple& xs, std::index_sequence<Is...>) {
    return dref(std::get<Is>(xs)...);
  }

  template <class T, size_t... Is>
  static auto
  apply_array(Subtype& dref, const T* xs, std::index_sequence<Is...>) {
    return dref(xs[Is]...);
  }

  template <class R, class T>
  std::enable_if_t<meta::is_annotation_v<T>, bool> try_apply(R& result, T& x) {
    if constexpr (meta::is_save_callback_v<T>) {
      CAF_READ_INSPECTOR_TRY(x.fun())
    }
    return true;
  }

  template <class R, class T>
  std::enable_if_t<!meta::is_annotation_v<T>, bool>
  try_apply(R& result, const T& x) {
    Subtype& dref = *static_cast<Subtype*>(this);
    if constexpr (std::is_empty<T>::value
                  || is_allowed_unsafe_message_type_v<T>) {
      // skip element
    } else if constexpr (std::is_integral<T>::value) {
      using squashed_type = detail::squashed_int_t<T>;
      auto squashed_x = static_cast<squashed_type>(x);
      CAF_READ_INSPECTOR_TRY(dref.apply(squashed_x))
    } else if constexpr (detail::can_apply_v<Subtype, decltype(x)>) {
      CAF_READ_INSPECTOR_TRY(dref.apply(x))
    } else if constexpr (std::is_array<T>::value) {
      std::make_index_sequence<std::extent<T>::value> seq;
      CAF_READ_INSPECTOR_TRY(apply_array(dref, x, seq))
    } else if constexpr (detail::is_stl_tuple_type<T>::value) {
      std::make_index_sequence<std::tuple_size<T>::value> seq;
      CAF_READ_INSPECTOR_TRY(apply_tuple(dref, x, seq))
    } else if constexpr (detail::is_map_like<T>::value) {
      CAF_READ_INSPECTOR_TRY(dref.begin_sequence(x.size()))
      for (const auto& kvp : x) {
        CAF_READ_INSPECTOR_TRY(dref(kvp.first, kvp.second))
      }
      CAF_READ_INSPECTOR_TRY(dref.end_sequence())
    } else if constexpr (detail::is_list_like<T>::value) {
      CAF_READ_INSPECTOR_TRY(dref.begin_sequence(x.size()))
      for (const auto& value : x) {
        CAF_READ_INSPECTOR_TRY(dref(value))
      }
      CAF_READ_INSPECTOR_TRY(dref.end_sequence())
    } else {
      static_assert(detail::is_inspectable<Subtype, T>::value);
      using caf::detail::inspect;
      // We require that the implementation for `inspect` does not modify its
      // arguments when passing a reading inspector.
      auto& mutable_x = const_cast<T&>(x);
      CAF_READ_INSPECTOR_TRY(inspect(dref, mutable_x));
    }
    return true;
  }
};

} // namespace caf

#undef CAF_READ_INSPECTOR_TRY
