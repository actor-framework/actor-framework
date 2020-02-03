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
class typed_message_view {
public:
  explicit typed_message_view(message& msg) noexcept
    : ptr_(msg.vals().unshared_ptr()) {
    // nop
  }

  typed_message_view() = delete;

  typed_message_view(const typed_message_view&) noexcept = default;

  typed_message_view& operator=(const typed_message_view&) noexcept = default;

  detail::message_data* operator->() noexcept {
    return ptr_;
  }

private:
  detail::message_data* ptr_;
};

template <size_t Position, class... Ts>
auto& get(typed_message_view<Ts...>& x) {
  static_assert(Position < sizeof...(Ts));
  using types = detail::type_list<Ts...>;
  using type = detail::tl_at_t<types, Position>;
  return *reinterpret_cast<type*>(x->get_mutable(Position));
}

} // namespace caf
