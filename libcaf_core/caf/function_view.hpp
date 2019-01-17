/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <new>
#include <functional>
#include <utility>

#include "caf/expected.hpp"
#include "caf/typed_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/response_type.hpp"

namespace caf {

template <class T>
class function_view_storage {
public:
  using type = function_view_storage;

  function_view_storage(T& storage) : storage_(&storage) {
    // nop
  }

  void operator()(T& x) {
    *storage_ = std::move(x);
  }

private:
  T* storage_;
};

template <class... Ts>
class function_view_storage<std::tuple<Ts...>> {
public:
  using type = function_view_storage;

  function_view_storage(std::tuple<Ts...>& storage) : storage_(&storage) {
    // nop
  }

  void operator()(Ts&... xs) {
    *storage_ = std::forward_as_tuple(std::move(xs)...);
  }

private:
  std::tuple<Ts...>* storage_;
};

template <>
class function_view_storage<unit_t> {
public:
  using type = function_view_storage;

  function_view_storage(unit_t&) {
    // nop
  }

  void operator()() {
    // nop
  }
};

struct function_view_storage_catch_all {
  message* storage_;

  function_view_storage_catch_all(message& ptr) : storage_(&ptr) {
    // nop
  }

  result<message> operator()(message_view& xs) {
    *storage_ = xs.move_content_to_message();
    return message{};
  }
};

template <>
class function_view_storage<message> {
public:
  using type = catch_all<function_view_storage_catch_all>;
};

template <class T>
struct function_view_flattened_result {
  using type = T;
};

template <class T>
struct function_view_flattened_result<std::tuple<T>> {
  using type = T;
};

template <>
struct function_view_flattened_result<std::tuple<void>> {
  using type = unit_t;
};

template <class T>
using function_view_flattened_result_t =
  typename function_view_flattened_result<T>::type;

template <class T>
struct function_view_result {
  T value;
};

template <class... Ts>
struct function_view_result<typed_actor<Ts...>> {
  typed_actor<Ts...> value{nullptr};
};

/// A function view for an actor hides any messaging from the caller.
/// Internally, a function view uses a `scoped_actor` and uses
/// blocking send and receive operations.
/// @experimental
template <class Actor>
class function_view {
public:
  using type = Actor;

  function_view(duration rel_timeout = infinite) : timeout(rel_timeout) {
    // nop
  }

  function_view(type  impl, duration rel_timeout = infinite)
      : timeout(rel_timeout),
        impl_(std::move(impl)) {
    new_self(impl_);
  }

  ~function_view() {
    if (impl_)
      self_.~scoped_actor();
  }

  function_view(function_view&& x)
      : timeout(x.timeout),
        impl_(std::move(x.impl_)) {
    if (impl_) {
      new (&self_) scoped_actor(impl_.home_system()); //(std::move(x.self_));
      x.self_.~scoped_actor();
    }
  }

  function_view& operator=(function_view&& x) {
    timeout = x.timeout;
    assign(x.impl_);
    x.reset();
    return *this;
  }

  /// Sends a request message to the assigned actor and returns the result.
  template <class... Ts,
            class R =
              function_view_flattened_result_t<
                typename response_type<
                  typename type::signatures,
                  detail::implicit_conversions_t<
                    typename std::decay<Ts>::type
                  >...
                >::tuple_type>>
  expected<R> operator()(Ts&&... xs) {
    if (!impl_)
      return sec::bad_function_call;
    error err;
    function_view_result<R> result;
    self_->request(impl_, timeout, std::forward<Ts>(xs)...).receive(
      [&](error& x) {
        err = std::move(x);
      },
      typename function_view_storage<R>::type{result.value}
    );
    if (err)
      return err;
    return flatten(result.value);
  }

  void assign(type x) {
    if (!impl_ && x)
      new_self(x);
    if (impl_ && !x)
      self_.~scoped_actor();
    impl_.swap(x);
  }

  void reset() {
    self_.~scoped_actor();
    impl_ = type();
  }

  /// Checks whether this function view has an actor assigned to it.
  explicit operator bool() const {
    return static_cast<bool>(impl_);
  }

  /// Returns the associated actor handle.
  type handle() const {
    return impl_;
  }

  duration timeout;

private:
  template <class T>
  T&& flatten(T& x) {
    return std::move(x);
  }

  template <class T>
  T&& flatten(std::tuple<T>& x) {
    return std::move(get<0>(x));
  }

  void new_self(const Actor& x) {
    if (x)
      new (&self_) scoped_actor(x->home_system());
  }

  union { scoped_actor self_; };
  type impl_;
};

/// @relates function_view
template <class T>
bool operator==(const function_view<T>& x, std::nullptr_t) {
  return !x;
}

/// @relates function_view
template <class T>
bool operator==(std::nullptr_t x, const function_view<T>& y) {
  return y == x;
}

/// @relates function_view
template <class T>
bool operator!=(const function_view<T>& x, std::nullptr_t y) {
  return !(x == y);
}

/// @relates function_view
template <class T>
bool operator!=(std::nullptr_t x, const function_view<T>& y) {
  return !(y == x);
}

/// Creates a new function view for `x`.
/// @relates function_view
/// @experimental
template <class T>
function_view<T> make_function_view(const T& x, duration t = infinite) {
  return {x, t};
}

} // namespace caf

