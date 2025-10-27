// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/behavior.hpp"
#include "caf/config.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/log/core.hpp"
#include "caf/message_id.hpp"
#include "caf/policy/select_all_tag.hpp"
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
    auto lg = log::core::trace("pending = {}", *pending);
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
    auto lg = log::core::trace("pending = {}", *pending);
    if (*pending > 0 && --*pending == 0) {
      timeouts.dispose();
      f();
    }
  }

  auto wrap() {
    return [this] { (*this)(); };
  }
};

template <class F, class... Ts>
struct select_all_helper_cow_tuple;

template <class F, class T, class... Ts>
struct select_all_helper_cow_tuple<F, T, Ts...> {
  using value_type = cow_tuple<T, Ts...>;
  std::vector<value_type> results;
  std::shared_ptr<size_t> pending;
  disposable timeouts;
  F f;

  template <class Fun>
  select_all_helper_cow_tuple(size_t pending, disposable timeouts, Fun&& f)
    : pending(std::make_shared<size_t>(pending)),
      timeouts(std::move(timeouts)),
      f(std::forward<Fun>(f)) {
    results.reserve(pending);
  }

  void operator()(T& x, Ts&... xs) {
    auto lg = log::core::trace("pending = {}", *pending);
    if (*pending > 0) {
      results.emplace_back(make_cow_tuple(std::move(x), std::move(xs)...));
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

template <class F, class = typename detail::get_callable_trait<F>::arg_types>
struct select_select_all_helper;

template <class F, class... Ts>
struct select_select_all_helper<F, type_list<std::vector<std::tuple<Ts...>>>> {
  using type = select_all_helper<F, Ts...>;
};

template <class F, class... Ts>
struct select_select_all_helper<F, type_list<std::vector<cow_tuple<Ts...>>>> {
  using type = select_all_helper_cow_tuple<F, Ts...>;
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

class [[deprecated("obsolete, use the mail API instead")]] select_all {
public:
  static constexpr bool is_trivial = false;

  using tag_type = select_all_tag_t;
};

} // namespace caf::policy
