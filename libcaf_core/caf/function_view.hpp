/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_FUNCTION_VIEW_HPP
#define CAF_FUNCTION_VIEW_HPP

#include <new>
#include <functional>

#include "caf/expected.hpp"
#include "caf/typed_actor.hpp"
#include "caf/scoped_actor.hpp"

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
struct function_view_result {
  T value;
};

template <class... Ts>
struct function_view_result<typed_actor<Ts...>> {
  typed_actor<Ts...> value{unsafe_actor_handle_init};
};

template <>
struct function_view_result<actor> {
  actor value{unsafe_actor_handle_init};
};

/// A function view for an actor hides any messaging from the caller.
/// Internally, a function view uses a `scoped_actor` and uses
/// blocking send and receive operations.
/// @experimental
template <class Actor>
class function_view {
public:
  using type = Actor;

  function_view() : impl_(unsafe_actor_handle_init) {
    // nop
  }

  function_view(const type& impl) : impl_(impl) {
    new_self(impl_);
  }

  ~function_view() {
    if (! impl_.unsafe())
      self_.~scoped_actor();
  }

  function_view(function_view&& x) : impl_(std::move(x.impl_)) {
    if (! impl_.unsafe()) {
      new (&self_) scoped_actor(impl_.home_system()); //(std::move(x.self_));
      x.self_.~scoped_actor();
    }
  }

  function_view& operator=(function_view&& x) {
    assign(x.impl_);
    x.assign(unsafe_actor_handle_init);
    return *this;
  }

  /// Sends a request message to the assigned actor and returns the result.
  template <class... Ts,
            class R =
              typename function_view_flattened_result<
                typename detail::deduce_output_type<
                  type,
                  detail::type_list<
                    typename detail::implicit_conversions<
                      typename std::decay<Ts>::type
                    >::type...>
                >::tuple_type
              >::type>
  expected<R> operator()(Ts&&... xs) {
    if (impl_.unsafe())
      return sec::bad_function_call;
    error err;
    function_view_result<R> result;
    self_->request(impl_, infinite, std::forward<Ts>(xs)...).receive(
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
    if (impl_.unsafe() && ! x.unsafe())
      new_self(x);
    if (! impl_.unsafe() && x.unsafe())
      self_.~scoped_actor();
    impl_.swap(x);
  }

  /// Checks whether this function view has an actor assigned to it.
  explicit operator bool() const {
    return ! impl_.unsafe();
  }

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
    if (! x.unsafe())
      new (&self_) scoped_actor(x->home_system());
  }

  union { scoped_actor self_; };
  type impl_;
};

/// @relates function_view
template <class T>
bool operator==(const function_view<T>& x, std::nullptr_t) {
  return ! x;
}

/// @relates function_view
template <class T>
bool operator==(std::nullptr_t x, const function_view<T>& y) {
  return y == x;
}

/// @relates function_view
template <class T>
bool operator!=(const function_view<T>& x, std::nullptr_t y) {
  return ! (x == y);
}

/// @relates function_view
template <class T>
bool operator!=(std::nullptr_t x, const function_view<T>& y) {
  return ! (y == x);
}

/// Creates a new function view for `x`.
/// @relates function_view
/// @experimental
template <class T>
function_view<T> make_function_view(const T& x) {
  return {x};
}

} // namespace caf

#endif // CAF_FUNCTION_VIEW_HPP
