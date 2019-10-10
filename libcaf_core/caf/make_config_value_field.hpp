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

#include "caf/config_value_field.hpp"
#include "caf/detail/config_value_field_impl.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

/// Creates a field with direct access to a member in `T` via member-to-object
/// pointer.
template <class T, class U, class... Args>
detail::config_value_field_impl<U T::*>
make_config_value_field(string_view name, U T::*ptr, Args&&... xs) {
  return {name, ptr, std::forward<Args>(xs)...};
}

/// Creates a field with access to a member in `T` via `getter` and `setter`.
template <class Getter, class Setter,
          class E = detail::enable_if_t<!std::is_member_pointer<Getter>::value>,
          class... Args>
detail::config_value_field_impl<std::pair<Getter, Setter>>
make_config_value_field(string_view name, Getter getter, Setter setter,
                        Args&&... xs) {
  return {name, std::move(getter), std::move(setter),
          std::forward<Args>(xs)...};
}

template <class T, class... Ts>
class config_value_field_storage {
public:
  using tuple_type = std::tuple<T, Ts...>;

  using object_type = typename T::object_type;

  using indices = typename detail::il_indices<tuple_type>::type;

  using array_type = std::array<config_value_field<object_type>*,
                                sizeof...(Ts) + 1>;

  template <class... Us>
  config_value_field_storage(T x, Us&&... xs)
    : fields_(std::move(x), std::forward<Us>(xs)...) {
    init(detail::get_indices(fields_));
  }

  config_value_field_storage(config_value_field_storage&&) = default;

  span<config_value_field<object_type>*> fields() {
    return make_span(ptr_fields_);
  }

private:
  template <long... Pos>
  void init(detail::int_list<Pos...>) {
    ptr_fields_ = array_type{{&std::get<Pos>(fields_)...}};
  }

  std::tuple<T, Ts...> fields_;

  array_type ptr_fields_;
};

template <class... Ts>
config_value_field_storage<Ts...>
make_config_value_field_storage(Ts... fields) {
  return {std::move(fields)...};
}

} // namespace caf
