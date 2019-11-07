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

#include "caf/behavior.hpp"
#include "caf/config.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/error.hpp"
#include "caf/message_id.hpp"

namespace caf {
namespace policy {

/// Trivial policy for handling a single result in a `response_handler`.
/// @relates mixin::requester
/// @relates response_handle
template <class ResponseType>
class single_response {
public:
  static constexpr bool is_trivial = true;

  using response_type = ResponseType;

  template <class Fun>
  using type_checker = detail::type_checker<response_type, Fun>;

  explicit single_response(message_id mid) noexcept : mid_(mid) {
    // nop
  }

  single_response(single_response&&) noexcept = default;

  single_response& operator=(single_response&&) noexcept = default;

  template <class Self, class F, class OnError>
  void await(Self* self, F&& f, OnError&& g) const {
    behavior bhvr{std::forward<F>(f), std::forward<OnError>(g)};
    self->add_awaited_response_handler(mid_, std::move(bhvr));
  }

  template <class Self, class F, class OnError>
  void then(Self* self, F&& f, OnError&& g) const {
    behavior bhvr{std::forward<F>(f), std::forward<OnError>(g)};
    self->add_multiplexed_response_handler(mid_, std::move(bhvr));
  }

  template <class Self, class F, class OnError>
  void receive(Self* self, F&& f, OnError&& g) const {
    typename Self::accept_one_cond rc;
    self->varargs_receive(rc, mid_, std::forward<F>(f),
                          std::forward<OnError>(g));
  }

  message_id id() const noexcept {
    return mid_;
  }

private:
  message_id mid_;
};

} // namespace policy
} // namespace caf
