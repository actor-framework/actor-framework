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
  explicit param_message_view(const message& msg) noexcept : ptr_(msg.cptr()) {
    // nop
  }

  param_message_view() = delete;

  param_message_view(const param_message_view&) noexcept = default;

  param_message_view& operator=(const param_message_view&) noexcept
    = default;

  const detail::message_data* operator->() const noexcept {
    return ptr_;
  }

private:
  const detail::message_data* ptr_;
};

template <size_t Index, class... Ts>
auto get(const param_message_view<Ts...>& xs) {
  static_assert(Index < sizeof...(Ts));
  using type = caf::detail::tl_at_t<caf::detail::type_list<Ts...>, Index>;
  return param<type>{xs->storage() + detail::offset_at<Index, Ts...>,
                     !xs->unique()};
}

} // namespace caf::detail
