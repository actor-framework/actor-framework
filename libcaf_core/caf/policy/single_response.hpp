// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/behavior.hpp"
#include "caf/config.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/error.hpp"
#include "caf/message_id.hpp"

namespace caf::policy {

/// Trivial policy for handling a single result in a `response_handler`.
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

} // namespace caf::policy
