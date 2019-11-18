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
  using CAF_UNIFYN(statement_result) = decltype(statement);                    \
  if constexpr (std::is_same<CAF_UNIFYN(statement_result), void>::value) {     \
    statement;                                                                 \
  } else if constexpr (std::is_same<CAF_UNIFYN(statement_result),              \
                                    bool>::value) {                            \
    if (!statement)                                                            \
      return false;                                                            \
  } else if (auto err = statement) {                                           \
    result = err;                                                              \
    return false;                                                              \
  }

namespace caf {

/// Injects an `operator()` that dispatches to `Subtype::apply`.
template <class Subtype>
class read_inspector {
public:
  static constexpr bool reads_state = true;

  static constexpr bool writes_state = false;

  template <class... Ts>
  [[nodiscard]] auto operator()(Ts&&... xs) {
    auto& dref = *static_cast<Subtype*>(this);
    typename Subtype::result_type result;
    auto f = [&result, &dref](auto&& x) {
      using type = std::remove_const_t<std::remove_reference_t<decltype(x)>>;
      if constexpr (std::is_empty<type>::value) {
        // nop
      } else if constexpr (meta::is_save_callback_v<type>) {
        CAF_READ_INSPECTOR_TRY(x.fun())
      } else if constexpr (meta::is_annotation_v<type> //
                           || is_allowed_unsafe_message_type_v<type>) {
        // skip element
      } else if constexpr (detail::can_apply_v<Subtype, decltype(x)>) {
        using apply_type = decltype(dref.apply(x));
        if constexpr (std::is_same<apply_type, void>::value) {
          dref.apply(x);
        } else if constexpr (std::is_same<apply_type, bool>::value) {
          if (!dref.apply(x)) {
            result = sec::end_of_stream;
            return false;
          }
        } else {
          CAF_READ_INSPECTOR_TRY(dref.apply(x))
        }
      } else if constexpr (std::is_integral<type>::value) {
        using squashed_type = detail::squashed_int_t<type>;
        CAF_READ_INSPECTOR_TRY(dref.apply(static_cast<squashed_type>(x)))
      } else if constexpr (std::is_array<type>::value) {
        CAF_READ_INSPECTOR_TRY(apply_array(dref, x))
      } else if constexpr (detail::is_stl_tuple_type<type>::value) {
        std::make_index_sequence<std::tuple_size<type>::value> seq;
        CAF_READ_INSPECTOR_TRY(apply_tuple(dref, x, seq))
      } else if constexpr (detail::is_map_like<type>::value) {
        CAF_READ_INSPECTOR_TRY(dref.begin_sequence(x.size()))
        for (const auto& kvp : x) {
          CAF_READ_INSPECTOR_TRY(dref(kvp.first, kvp.second))
        }
        CAF_READ_INSPECTOR_TRY(dref.end_sequence())
      } else if constexpr (detail::is_list_like<type>::value) {
        CAF_READ_INSPECTOR_TRY(dref.begin_sequence(x.size()))
        for (const auto& value : x) {
          CAF_READ_INSPECTOR_TRY(dref(value))
        }
        CAF_READ_INSPECTOR_TRY(dref.end_sequence())
      } else {
        static_assert(detail::is_inspectable<Subtype, type>::value);
        using caf::detail::inspect;
        // We require that the implementation for `inspect` does not modify its
        // arguments when passing a reading inspector.
        CAF_READ_INSPECTOR_TRY(inspect(dref, const_cast<type&>(x)));
      }
      return true;
    };
    static_cast<void>((f(std::forward<Ts>(xs)) && ...));
    return result;
  }

private:
  template <class Tuple, size_t... Is>
  static auto apply_tuple(Subtype& dref, const Tuple& xs,
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

#undef CAF_READ_INSPECTOR_TRY
