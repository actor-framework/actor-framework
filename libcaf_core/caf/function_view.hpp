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

#include "caf/typed_actor.hpp"
#include "caf/scoped_actor.hpp"

namespace caf {

template <class T>
class function_view_storage {
public:
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
  function_view_storage(unit_t&) {
    // nop
  }

  void operator()() {
    // nop
  }
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

/// A function view for an actor hides any messaging from the caller.
/// Internally, a function view uses a `scoped_actor` and uses
/// blocking send and receive operations.
template <class Actor>
class function_view {
public:
  using type = Actor;

  function_view() {
    // nop
  }

  function_view(const type& impl) : impl_(impl) {
    new_self(impl_);
  }

  ~function_view() {
    if (impl_)
      self_.~scoped_actor();
  }

  function_view(function_view&& x) : impl_(std::move(x.impl_)) {
    if (impl_) {
      new (&self_) scoped_actor(std::move(x.self_));
      x.self_.~scoped_actor();
    }
  }

  function_view& operator=(function_view&& x) {
    assign(x.impl_);
    x.assign(invalid_actor);
    return *this;
  }

  /// Sends a request message to the assigned actor and returns the result.
  /// @throws std::bad_function_call if no actor is assigned to view
  /// @throws actor_exited if the requests resulted in an error
  template <class... Ts,
            class R =
              typename function_view_flattened_result<
                typename detail::deduce_output_type<
                  typename type::signatures,
                  detail::type_list<
                    typename detail::implicit_conversions<
                      typename std::decay<Ts>::type
                    >::type...>
                >::tuple_type
              >::type>
  R operator()(Ts&&... xs) {
    if (! impl_)
      throw std::bad_function_call();
    R result;
    function_view_storage<R> h{result};
    try {
      self_->request(impl_, infinite, std::forward<Ts>(xs)...).receive(h);
    }
    catch (std::exception&) {
      assign(invalid_actor);
      throw;
    }
    return flatten(result);
  }

  void assign(type x) {
    if (! impl_ && x)
      new_self(x);
    if (impl_ && ! x)
      self_.~scoped_actor();
    impl_.swap(x);
  }

  /// Checks whether this function view has an actor assigned to it.
  explicit operator bool() const {
    return static_cast<bool>(impl_);
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
    if (x)
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
template <class T>
function_view<T> make_function_view(const T& x) {
  return {x};
}

} // namespace caf

#endif // CAF_FUNCTION_VIEW_HPP
