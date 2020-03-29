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

#include <cstddef>
#include <memory>

#include "caf/behavior.hpp"
#include "caf/config.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/logger.hpp"
#include "caf/sec.hpp"

namespace caf::detail {

template <class F, class = typename get_callable_trait<F>::arg_types>
struct select_any_factory;

template <class F, class... Ts>
struct select_any_factory<F, type_list<Ts...>> {
  template <class Fun>
  static auto make(std::shared_ptr<size_t> pending, Fun&& fun) {
    return [pending, f{std::forward<Fun>(fun)}](Ts... xs) mutable {
      CAF_LOG_TRACE(CAF_ARG2("pending", *pending));
      if (*pending > 0) {
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
  using type_checker
    = detail::type_checker<response_type, detail::decay_t<Fun>>;

  explicit select_any(message_id_list ids) : ids_(std::move(ids)) {
    CAF_ASSERT(ids_.size()
               <= static_cast<size_t>(std::numeric_limits<int>::max()));
  }

  template <class Self, class F, class OnError>
  void await(Self* self, F&& f, OnError&& g) const {
    CAF_LOG_TRACE(CAF_ARG(ids_));
    auto bhvr = make_behavior(std::forward<F>(f), std::forward<OnError>(g));
    for (auto id : ids_)
      self->add_awaited_response_handler(id, bhvr);
  }

  template <class Self, class F, class OnError>
  void then(Self* self, F&& f, OnError&& g) const {
    CAF_LOG_TRACE(CAF_ARG(ids_));
    auto bhvr = make_behavior(std::forward<F>(f), std::forward<OnError>(g));
    for (auto id : ids_)
      self->add_multiplexed_response_handler(id, bhvr);
  }

  template <class Self, class F, class G>
  void receive(Self* self, F&& f, G&& g) const {
    CAF_LOG_TRACE(CAF_ARG(ids_));
    using factory = detail::select_any_factory<std::decay_t<F>>;
    auto pending = std::make_shared<size_t>(ids_.size());
    auto fw = factory::make(pending, std::forward<F>(f));
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

private:
  template <class OnError>
  auto make_error_handler(std::shared_ptr<size_t> p, OnError&& g) const {
    return [p{std::move(p)}, g{std::forward<OnError>(g)}](error&) mutable {
      if (*p == 0) {
        // nop
      } else if (*p == 1) {
        auto err = make_error(sec::all_requests_failed);
        g(err);
      } else {
        --*p;
      }
    };
  }

  template <class F, class OnError>
  behavior make_behavior(F&& f, OnError&& g) const {
    using factory = detail::select_any_factory<std::decay_t<F>>;
    auto pending = std::make_shared<size_t>(ids_.size());
    auto result_handler = factory::make(pending, std::forward<F>(f));
    return {
      std::move(result_handler),
      make_error_handler(std::move(pending), std::forward<OnError>(g)),
    };
  }

  message_id_list ids_;
};

} // namespace caf::policy
