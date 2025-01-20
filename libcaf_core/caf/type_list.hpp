// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf {

template <class...>
struct type_list;

} // namespace caf

namespace caf::detail {

template <class...>
struct tl_concat;

template <class... Ts>
struct tl_concat<type_list<Ts...>> {
  using type = type_list<Ts...>;
};

template <class... Ts, class... Us, class... Lists>
struct tl_concat<type_list<Ts...>, type_list<Us...>, Lists...>
  : tl_concat<type_list<Ts..., Us...>, Lists...> {
  // nop
};

template <class... Ts>
using tl_concat_t = typename tl_concat<Ts...>::type;

} // namespace caf::detail

namespace caf {

/// A list of types.
template <class... Ts>
struct type_list {
  constexpr type_list() {
    // nop
  }

  template <class... Us>
  using append = type_list<Ts..., Us...>;

  template <class... Lists>
  using append_from = detail::tl_concat_t<type_list<Ts...>, Lists...>;
};

template <class... Ts>
constexpr auto type_list_v = type_list<Ts...>{};

} // namespace caf
