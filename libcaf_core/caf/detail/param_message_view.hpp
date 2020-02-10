/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
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

#include "caf/message.hpp"
#include "caf/param.hpp"

namespace caf::detail {

template <class... Ts>
class param_message_view {
public:
  explicit param_message_view(type_erased_tuple& msg) noexcept : ptr_(&msg) {
    // nop
  }

  param_message_view() = delete;

  param_message_view(const param_message_view&) noexcept = default;

  param_message_view& operator=(const param_message_view&) noexcept
    = default;

  const type_erased_tuple* operator->() const noexcept {
    return ptr_;
  }

private:
  const type_erased_tuple* ptr_;
  bool shared_;
};

template <size_t Position, class... Ts>
auto get(const param_message_view<Ts...>& x) {
  static_assert(Position < sizeof...(Ts));
  using types = detail::type_list<Ts...>;
  using type = detail::tl_at_t<types, Position>;
  return param<type>{x->get(Position), x->shared()};
}

} // namespace caf::detail
