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

#include "caf/detail/type_list.hpp"
#include "caf/message.hpp"

namespace caf {

template <class... Ts>
class const_typed_message_view {
public:
  explicit const_typed_message_view(const message& msg) noexcept
    : ptr_(msg.cvals().get()) {
    // nop
  }

  const_typed_message_view() = delete;

  const_typed_message_view(const const_typed_message_view&) noexcept = default;

  const_typed_message_view& operator=(const const_typed_message_view&) noexcept
    = default;

  const detail::message_data* operator->() const noexcept {
    return ptr_;
  }

private:
  const detail::message_data* ptr_;
};

template <size_t Position, class... Ts>
const auto& get(const const_typed_message_view<Ts...>& x) {
  static_assert(Position < sizeof...(Ts));
  using types = detail::type_list<Ts...>;
  using type = detail::tl_at_t<types, Position>;
  return *reinterpret_cast<const type*>(x->get(Position));
}

} // namespace caf
