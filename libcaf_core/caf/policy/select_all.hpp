// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/behavior.hpp"
#include "caf/config.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/log/core.hpp"
#include "caf/message_id.hpp"
#include "caf/type_list.hpp"

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

namespace caf::detail {

template <class... Ts>
struct select_all_helper_value_oracle {
  using type = std::tuple<Ts...>;
};

template <class T>
struct select_all_helper_value_oracle<T> {
  using type = T;
};

template <class... Ts>
using select_all_helper_value_t =
  typename select_all_helper_value_oracle<Ts...>::type;

template <class F, class... Ts>
struct select_all_helper;

template <class F, class T, class... Ts>
struct select_all_helper<F, T, Ts...> {
  using value_type = select_all_helper_value_t<T, Ts...>;
  std::vector<value_type> results;
  std::shared_ptr<size_t> pending;
  disposable timeouts;
  F f;

  template <class Fun>
  select_all_helper(size_t pending, disposable timeouts, Fun&& f)
    : pending(std::make_shared<size_t>(pending)),
      timeouts(std::move(timeouts)),
      f(std::forward<Fun>(f)) {
    results.reserve(pending);
  }

  void operator()(T& x, Ts&... xs) {
    auto exit_guard = log::core::trace("pending = {}", *pending);
    if (*pending > 0) {
      results.emplace_back(std::move(x), std::move(xs)...);
      if (--*pending == 0) {
        timeouts.dispose();
        f(std::move(results));
      }
    }
  }

  auto wrap() {
    return [this](T& x, Ts&... xs) { (*this)(x, xs...); };
  }
};

template <class F>
struct select_all_helper<F> {
  std::shared_ptr<size_t> pending;
  disposable timeouts;
  F f;

  template <class Fun>
  select_all_helper(size_t pending, disposable timeouts, Fun&& f)
    : pending(std::make_shared<size_t>(pending)),
      timeouts(std::move(timeouts)),
      f(std::forward<Fun>(f)) {
    // nop
  }

  void operator()() {
    auto exit_guard = log::core::trace("pending = {}", *pending);
    if (*pending > 0 && --*pending == 0) {
      timeouts.dispose();
      f();
    }
  }

  auto wrap() {
    return [this] { (*this)(); };
  }
};

template <class F, class = typename detail::get_callable_trait<F>::arg_types>
struct select_select_all_helper;

template <class F, class... Ts>
struct select_select_all_helper<F, type_list<std::vector<std::tuple<Ts...>>>> {
  using type = select_all_helper<F, Ts...>;
};

template <class F, class T>
struct select_select_all_helper<F, type_list<std::vector<T>>> {
  using type = select_all_helper<F, T>;
};

template <class F>
struct select_select_all_helper<F, type_list<>> {
  using type = select_all_helper<F>;
};

template <class F>
using select_all_helper_t = typename select_select_all_helper<F>::type;

} // namespace caf::detail

namespace caf::policy {

/// Enables a `response_handle` to fan-in all responses messages into a single
/// result (a `vector` that stores all received results).
/// @relates mixin::requester
/// @relates response_handle
template <class ResponseType>
class select_all {
public:
  static constexpr bool is_trivial = false;

  using response_type = ResponseType;

  using message_id_list = std::vector<message_id>;

  template <class Fun>
  using type_checker
    = detail::type_checker<response_type,
                           detail::select_all_helper_t<std::decay_t<Fun>>>;

  explicit select_all(message_id_list ids, disposable pending_timeouts)
    : ids_(std::move(ids)), pending_timeouts_(std::move(pending_timeouts)) {
    CAF_ASSERT(ids_.size()
               <= static_cast<size_t>(std::numeric_limits<int>::max()));
  }

  select_all(select_all&&) noexcept = default;

  select_all& operator=(select_all&&) noexcept = default;

  template <class Self, class F, class OnError>
  void await(Self* self, F&& f, OnError&& g) {
    auto exit_guard = log::core::trace("ids_ = {}", ids_);
    auto bhvr = make_behavior(std::forward<F>(f), std::forward<OnError>(g));
    for (auto id : ids_)
      self->add_awaited_response_handler(id, bhvr);
  }

  template <class Self, class F, class OnError>
  void then(Self* self, F&& f, OnError&& g) {
    auto exit_guard = log::core::trace("ids_ = {}", ids_);
    auto bhvr = make_behavior(std::forward<F>(f), std::forward<OnError>(g));
    for (auto id : ids_)
      self->add_multiplexed_response_handler(id, bhvr);
  }

  template <class Self, class F, class G>
  void receive(Self* self, F&& f, G&& g) {
    auto exit_guard = log::core::trace("ids_ = {}", ids_);
    using helper_type = detail::select_all_helper_t<std::decay_t<F>>;
    helper_type helper{ids_.size(), pending_timeouts_, std::forward<F>(f)};
    auto error_handler = [&](error& err) mutable {
      if (*helper.pending > 0) {
        pending_timeouts_.dispose();
        *helper.pending = 0;
        helper.results.clear();
        g(err);
      }
    };
    for (auto id : ids_) {
      typename Self::accept_one_cond rc;
      auto error_handler_copy = error_handler;
      self->varargs_receive(rc, id, helper.wrap(), error_handler_copy);
    }
  }

  const message_id_list& ids() const noexcept {
    return ids_;
  }

  disposable pending_timeouts() {
    return pending_timeouts_;
  }

private:
  template <class F, class OnError>
  behavior make_behavior(F&& f, OnError&& g) {
    using namespace detail;
    using helper_type = select_all_helper_t<std::decay_t<F>>;
    helper_type helper{ids_.size(), pending_timeouts_, std::forward<F>(f)};
    auto pending = helper.pending;
    auto error_handler = [pending{std::move(pending)},
                          timeouts{pending_timeouts_},
                          g{std::forward<OnError>(g)}](error& err) mutable {
      auto exit_guard = log::core::trace("pending = {}", *pending);
      if (*pending > 0) {
        timeouts.dispose();
        *pending = 0;
        g(err);
      }
    };
    return {
      std::move(helper),
      std::move(error_handler),
    };
  }

  message_id_list ids_;
  disposable pending_timeouts_;
};

} // namespace caf::policy
