// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/behavior.hpp"
#include "caf/config.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/disposable.hpp"
#include "caf/log/core.hpp"
#include "caf/sec.hpp"
#include "caf/type_list.hpp"

#include <cstddef>
#include <memory>

namespace caf::detail {

template <class F, class = typename get_callable_trait<F>::arg_types>
struct select_any_factory;

template <class F, class... Ts>
struct select_any_factory<F, type_list<Ts...>> {
  template <class Fun>
  static auto
  make(std::shared_ptr<size_t> pending, disposable timeouts, Fun f) {
    return [pending{std::move(pending)}, timeouts{std::move(timeouts)},
            f{std::move(f)}](Ts... xs) mutable {
      auto lg = log::core::trace("pending = {}", *pending);
      if (*pending > 0) {
        timeouts.dispose();
        f(xs...);
        *pending = 0;
      }
    };
  }
};

} // namespace caf::detail

namespace caf::policy {

/// Enables a `response_handle` to pick the first arriving response, ignoring
/// all other results.
/// @relates response_handle
template <class ResponseType>
class select_any {
public:
  static constexpr bool is_trivial = false;

  using response_type = ResponseType;

  using message_id_list = std::vector<message_id>;

  template <class Fun>
  using type_checker = detail::type_checker<response_type, std::decay_t<Fun>>;

  explicit select_any(message_id_list ids, disposable pending_timeouts)
    : ids_(std::move(ids)), pending_timeouts_(std::move(pending_timeouts)) {
    CAF_ASSERT(ids_.size()
               <= static_cast<size_t>(std::numeric_limits<int>::max()));
  }

  template <class Self, class F, class OnError>
  void await(Self* self, F&& f, OnError&& g) {
    auto lg = log::core::trace("ids_ = {}", ids_);
    auto bhvr = make_behavior(std::forward<F>(f), std::forward<OnError>(g));
    for (auto id : ids_)
      self->add_awaited_response_handler(id, bhvr);
  }

  template <class Self, class F, class OnError>
  void then(Self* self, F&& f, OnError&& g) {
    auto lg = log::core::trace("ids_ = {}", ids_);
    auto bhvr = make_behavior(std::forward<F>(f), std::forward<OnError>(g));
    for (auto id : ids_)
      self->add_multiplexed_response_handler(id, bhvr);
  }

  template <class Self, class F, class G>
  void receive(Self* self, F&& f, G&& g) {
    auto lg = log::core::trace("ids_ = {}", ids_);
    using factory = detail::select_any_factory<std::decay_t<F>>;
    auto pending = std::make_shared<size_t>(ids_.size());
    auto fw = factory::make(pending, pending_timeouts_, std::forward<F>(f));
    auto gw = make_error_handler(std::move(pending), std::forward<G>(g));
    for (auto id : ids_) {
      typename Self::accept_one_cond rc;
      auto fcopy = fw;
      auto gcopy = gw;
      self->varargs_receive(rc, id, fcopy, gcopy);
    }
  }

  const message_id_list& ids() const noexcept {
    return ids_;
  }

  disposable pending_timeouts() {
    return pending_timeouts_;
  }

private:
  template <class OnError>
  auto make_error_handler(std::shared_ptr<size_t> p, OnError&& g) {
    return [p{std::move(p)}, timeouts{pending_timeouts_},
            g{std::forward<OnError>(g)}](error&) mutable {
      if (*p == 0) {
        // nop
      } else if (*p == 1) {
        timeouts.dispose();
        auto err = make_error(sec::all_requests_failed);
        g(err);
      } else {
        --*p;
      }
    };
  }

  template <class F, class OnError>
  behavior make_behavior(F&& f, OnError&& g) {
    using factory = detail::select_any_factory<std::decay_t<F>>;
    auto pending = std::make_shared<size_t>(ids_.size());
    auto result_handler = factory::make(pending, pending_timeouts_,
                                        std::forward<F>(f));
    return {std::move(result_handler),
            make_error_handler(std::move(pending), std::forward<OnError>(g))};
  }

  message_id_list ids_;
  disposable pending_timeouts_;
};

} // namespace caf::policy
