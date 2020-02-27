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

#include <utility>

#include "caf/detail/message_data.hpp"
#include "caf/detail/offset_at.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/message.hpp"

namespace caf {

template <class... Ts>
class const_typed_message_view {
public:
  const_typed_message_view() noexcept : ptr_(nullptr) {
    // nop
  }

  explicit const_typed_message_view(const message& msg) noexcept
    : ptr_(msg.cptr()) {
    // nop
  }

  const_typed_message_view(const const_typed_message_view&) noexcept = default;

  const_typed_message_view& operator=(const const_typed_message_view&) noexcept
    = default;

  const detail::message_data* operator->() const noexcept {
    return ptr_;
  }

  explicit operator bool() const noexcept {
    return ptr_ != nullptr;
  }

private:
  const detail::message_data* ptr_;
};

template <size_t Index, class... Ts>
const auto& get(const_typed_message_view<Ts...>& xs) {
  static_assert(Index < sizeof...(Ts));
  using type = caf::detail::tl_at_t<caf::detail::type_list<Ts...>, Index>;
  return *reinterpret_cast<const type*>(xs->storage()
                                        + detail::offset_at<Index, Ts...>);
}

template <class... Ts, size_t... Is>
auto to_tuple(const_typed_message_view<Ts...> xs, std::index_sequence<Is...>) {
  return std::make_tuple(get<Is>(xs)...);
}

template <class... Ts>
auto to_tuple(const_typed_message_view<Ts...> xs) {
  std::make_index_sequence<sizeof...(Ts)> seq;
  return to_tuple(xs, seq);
}

template <class... Ts>
auto make_const_typed_message_view(const message& msg) {
  if (msg.types() == make_type_id_list<Ts...>())
    return const_typed_message_view<Ts...>{msg};
  return const_typed_message_view<Ts...>{};
}

} // namespace caf
