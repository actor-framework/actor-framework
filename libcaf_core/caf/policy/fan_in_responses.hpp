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

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

#include "caf/behavior.hpp"
#include "caf/config.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/error.hpp"
#include "caf/message_id.hpp"

namespace caf {
namespace detail {

template <class F, class T>
struct fan_in_responses_helper {
  std::vector<T> results;
  std::shared_ptr<size_t> pending;
  F f;

  void operator()(T& x) {
    if (*pending > 0) {
      results.emplace_back(std::move(x));
      if (--*pending == 0)
        f(std::move(results));
    }
  }

  template <class Fun>
  fan_in_responses_helper(size_t pending, Fun&& f)
    : pending(std::make_shared<size_t>(pending)), f(std::forward<Fun>(f)) {
    results.reserve(pending);
  }

  // TODO: return 'auto' from this function when switching to C++17
  std::function<void(T&)> wrap() {
    return [this](T& x) { (*this)(x); };
  }
};

template <class F, class... Ts>
struct fan_in_responses_tuple_helper {
  using value_type = std::tuple<Ts...>;
  std::vector<value_type> results;
  std::shared_ptr<size_t> pending;
  F f;

  void operator()(Ts&... xs) {
    if (*pending > 0) {
      results.emplace_back(std::move(xs)...);
      if (--*pending == 0)
        f(std::move(results));
    }
  }

  template <class Fun>
  fan_in_responses_tuple_helper(size_t pending, Fun&& f)
    : pending(std::make_shared<size_t>(pending)), f(std::forward<Fun>(f)) {
    results.reserve(pending);
  }

  // TODO: return 'auto' from this function when switching to C++17
  std::function<void(Ts&...)> wrap() {
    return [this](Ts&... xs) { (*this)(xs...); };
  }
};

template <class F, class = typename detail::get_callable_trait<F>::arg_types>
struct select_fan_in_responses_helper;

template <class F, class... Ts>
struct select_fan_in_responses_helper<
  F, detail::type_list<std::vector<std::tuple<Ts...>>>> {
  using type = fan_in_responses_tuple_helper<F, Ts...>;
};

template <class F, class T>
struct select_fan_in_responses_helper<F, detail::type_list<std::vector<T>>> {
  using type = fan_in_responses_helper<F, T>;
};

template <class F>
using fan_in_responses_helper_t =
  typename select_fan_in_responses_helper<F>::type;

// TODO: Replace with a lambda when switching to C++17 (move g into lambda).
template <class G>
class fan_in_responses_error_handler {
public:
  template <class Fun>
  fan_in_responses_error_handler(Fun&& fun, std::shared_ptr<size_t> pending)
    : handler(std::forward<Fun>(fun)), pending(std::move(pending)) {
    // nop
  }

  void operator()(error& err) {
    if (*pending > 0) {
      *pending = 0;
      handler(err);
    }
  }

private:
  G handler;
  std::shared_ptr<size_t> pending;
};

} // namespace detail
} // namespace caf

namespace caf {
namespace policy {

/// Enables a `response_handle` to fan-in multiple responses into a single
/// result (a `vector` of individual values) for the client.
/// @relates mixin::requester
/// @relates response_handle
template <class ResponseType>
class fan_in_responses {
public:
  static constexpr bool is_trivial = false;

  using response_type = ResponseType;

  using message_id_list = std::vector<message_id>;

  explicit fan_in_responses(message_id_list ids) : ids_(std::move(ids)) {
    CAF_ASSERT(ids_.size()
               <= static_cast<size_t>(std::numeric_limits<int>::max()));
  }

  fan_in_responses(fan_in_responses&&) noexcept = default;

  fan_in_responses& operator=(fan_in_responses&&) noexcept = default;

  template <class Self, class F, class OnError>
  void await(Self* self, F&& f, OnError&& g) const {
    auto bhvr = make_behavior(std::forward<F>(f), std::forward<OnError>(g));
    for (auto id : ids_)
      self->add_awaited_response_handler(id, bhvr);
  }

  template <class Self, class F, class OnError>
  void then(Self* self, F&& f, OnError&& g) const {
    auto bhvr = make_behavior(std::forward<F>(f), std::forward<OnError>(g));
    for (auto id : ids_)
      self->add_multiplexed_response_handler(id, bhvr);
  }

  template <class Self, class F, class G>
  void receive(Self* self, F&& f, G&& g) const {
    using helper_type = detail::fan_in_responses_helper_t<detail::decay_t<F>>;
    helper_type helper{ids_.size(), std::forward<F>(f)};
    detail::type_checker<ResponseType, helper_type>::check();
    auto error_handler = [&](error& err) {
      if (*helper.pending > 0) {
        *helper.pending = 0;
        helper.results.clear();
        g(err);
      }
    };
    auto wrapped_helper = helper.wrap();
    for (auto id : ids_) {
      typename Self::accept_one_cond rc;
      self->varargs_receive(rc, id, wrapped_helper, error_handler);
    }
  }

  const message_id_list& ids() const noexcept {
    return ids_;
  }

private:
  template <class F, class OnError>
  behavior make_behavior(F&& f, OnError&& g) const {
    using namespace detail;
    using helper_type = fan_in_responses_helper_t<decay_t<F>>;
    using error_handler_type = fan_in_responses_error_handler<decay_t<OnError>>;
    helper_type helper{ids_.size(), std::move(f)};
    type_checker<ResponseType, helper_type>::check();
    error_handler_type err_helper{std::forward<OnError>(g), helper.pending};
    return {
      std::move(helper),
      std::move(err_helper),
    };
  }

  message_id_list ids_;
};

} // namespace policy
} // namespace caf
