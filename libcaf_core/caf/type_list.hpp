// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf {

template <class... Ts>
struct type_list;

template <class... Ts>
struct tl_concat_helper;

template <class... Ts>
struct tl_concat_helper<type_list<Ts...>> {
  using type = type_list<Ts...>;
};

template <class... Ts, class... Us, class... Lists>
struct tl_concat_helper<type_list<Ts...>, type_list<Us...>, Lists...>
  : tl_concat_helper<type_list<Ts..., Us...>, Lists...> {
  // nop
};

template <class... Ts>
using tl_concat_t = typename tl_concat_helper<Ts...>::type;

/// A list of types.
template <class... Ts>
struct type_list {
  constexpr type_list() {
    // nop
  }

  template <class... Us>
  using append = type_list<Ts..., Us...>;

  template <class... Lists>
  using append_from = tl_concat_t<type_list<Ts...>, Lists...>;
};

template <class... Ts>
constexpr auto type_list_v = type_list<Ts...>{};

} // namespace caf
