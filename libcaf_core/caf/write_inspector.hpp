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

#include <array>
#include <tuple>
#include <type_traits>
#include <utility>

#include "caf/allowed_unsafe_message_type.hpp"
#include "caf/detail/inspect.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/meta/annotation.hpp"
#include "caf/meta/load_callback.hpp"
#include "caf/unifyn.hpp"

#define CAF_WRITE_INSPECTOR_TRY(statement)                                     \
  if constexpr (std::is_same<decltype(statement), void>::value) {              \
    statement;                                                                 \
  } else if (auto err = statement) {                                           \
    result = err;                                                              \
    return false;                                                              \
  }

namespace caf {

/// Injects an `operator()` that dispatches to `Subtype::apply`. The `Subtype`
/// shall overload `apply` for:
/// - all fixed-size integer types from `<cstdint>`
/// - floating point numbers
/// - enum types
/// - `std::string`, `std::u16string`, and `std::u32string`
template <class Subtype>
class write_inspector {
public:
  static constexpr bool reads_state = false;

  static constexpr bool writes_state = true;

  template <class... Ts>
  [[nodiscard]] auto operator()(Ts&&... xs) {
    auto& dref = *static_cast<Subtype*>(this);
    typename Subtype::result_type result;
    auto f = [&result, &dref](auto&& x) {
      using type = std::remove_reference_t<decltype(x)>;
      if constexpr (std::is_empty<type>::value) {
        // nop
      } else if constexpr (meta::is_load_callback_v<type>) {
        CAF_WRITE_INSPECTOR_TRY(x.fun())
      } else if constexpr (meta::is_annotation_v<type> //
                           || is_allowed_unsafe_message_type_v<type>) {
        // skip element
      } else if constexpr (detail::can_apply_v<Subtype, decltype(x)>) {
        CAF_WRITE_INSPECTOR_TRY(dref.apply(x))
      } else if constexpr (std::is_integral_v<type>) {
        using squashed_type = detail::squashed_int_t<type>;
        CAF_WRITE_INSPECTOR_TRY(dref.apply(reinterpret_cast<squashed_type&>(x)))
      } else if constexpr (std::is_array<type>::value) {
        CAF_WRITE_INSPECTOR_TRY(apply_array(dref, x))
      } else if constexpr (detail::is_stl_tuple_type_v<type>) {
        std::make_index_sequence<std::tuple_size_v<type>> seq;
        CAF_WRITE_INSPECTOR_TRY(apply_tuple(dref, x, seq))
      } else if constexpr (detail::is_map_like_v<type>) {
        x.clear();
        size_t size = 0;
        CAF_WRITE_INSPECTOR_TRY(dref.begin_sequence(size))
        for (size_t i = 0; i < size; ++i) {
          auto key = typename type::key_type{};
          auto val = typename type::mapped_type{};
          CAF_WRITE_INSPECTOR_TRY(dref(key, val))
          x.emplace(std::move(key), std::move(val));
        }
        CAF_WRITE_INSPECTOR_TRY(dref.end_sequence())
      } else if constexpr (detail::is_list_like_v<type>) {
        x.clear();
        size_t size = 0;
        CAF_WRITE_INSPECTOR_TRY(dref.begin_sequence(size))
        for (size_t i = 0; i < size; ++i) {
          auto tmp = typename type::value_type{};
          CAF_WRITE_INSPECTOR_TRY(dref(tmp))
          x.insert(x.end(), std::move(tmp));
        }
        CAF_WRITE_INSPECTOR_TRY(dref.end_sequence())
      } else {
        static_assert(std::is_lvalue_reference_v<decltype(x)> //
                      && !std::is_const_v<decltype(x)>);
        static_assert(detail::is_inspectable<Subtype, type>::value);
        using caf::detail::inspect;
        CAF_WRITE_INSPECTOR_TRY(inspect(dref, x));
      }
      return true;
    };
    static_cast<void>((f(std::forward<Ts>(xs)) && ...));
    return result;
  }

private:
  template <class Tuple, size_t... Is>
  static auto apply_tuple(Subtype& dref, Tuple& xs,
                          std::index_sequence<Is...>) {
    return dref(std::get<Is>(xs)...);
  }

  template <class T, size_t... Is>
  static auto apply_array(Subtype& dref, T* xs, std::index_sequence<Is...>) {
    return dref(xs[Is]...);
  }

  template <class T, size_t N>
  static auto apply_array(Subtype& dref, T (&xs)[N]) {
    std::make_index_sequence<N> seq;
    return apply_array(dref, xs, seq);
  }
};

} // namespace caf

#undef CAF_WRITE_INSPECTOR_TRY
