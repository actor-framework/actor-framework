// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/expected.hpp"
#include "caf/response_type.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/timespan.hpp"
#include "caf/typed_actor.hpp"

#include <functional>
#include <new>
#include <utility>

namespace caf {

template <class T>
class function_view_storage {
public:
  using type = function_view_storage;

  explicit function_view_storage(T& storage) : storage_(&storage) {
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

  explicit function_view_storage(std::tuple<Ts...>& storage)
    : storage_(&storage) {
    // nop
  }

  void operator()(Ts&... xs) {
    *storage_ = std::forward_as_tuple(std::move(xs)...);
  }

private:
  std::tuple<Ts...>* storage_;
};

/// Convenience alias for `function_view_storage<T>::type`.
template <class T>
using function_view_storage_t = typename function_view_storage<T>::type;

struct function_view_storage_catch_all {
  message* storage_;

  explicit function_view_storage_catch_all(message& ptr) : storage_(&ptr) {
    // nop
  }

  skippable_result operator()(message& msg) {
    *storage_ = std::move(msg);
    return message{};
  }
};

template <>
class function_view_storage<message> {
public:
  using type = function_view_storage;

  explicit function_view_storage(message& ptr) : storage_(&ptr) {
    // nop
  }

  void operator()(message& msg) {
    *storage_ = std::move(msg);
  }

private:
  message* storage_;
};

template <class T>
struct function_view_flattened_result {
  using type = T;
};

template <class T>
struct function_view_flattened_result<std::tuple<T>> {
  using type = T;
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

  function_view() : timeout(infinite) {
    // nop
  }

  explicit function_view(timespan rel_timeout) : timeout(rel_timeout) {
    // nop
  }

  explicit function_view(type impl)
    : timeout(infinite), impl_(std::move(impl)) {
    new_self(impl_);
  }

  function_view(type impl, timespan rel_timeout)
    : timeout(rel_timeout), impl_(std::move(impl)) {
    new_self(impl_);
  }

  ~function_view() {
    if (impl_)
      self_.~scoped_actor();
  }

  function_view(function_view&& x)
    : timeout(x.timeout), impl_(std::move(x.impl_)) {
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
  template <class... Ts>
  auto operator()(Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    using trait = response_type<typename type::signatures,
                                detail::strip_and_convert_t<Ts>...>;
    static_assert(trait::valid, "receiver does not accept given message");
    using tuple_type = typename trait::tuple_type;
    using value_type = function_view_flattened_result_t<tuple_type>;
    using result_type = expected<value_type>;
    if (!impl_)
      return result_type{sec::bad_function_call};
    error err;
    if constexpr (std::is_void_v<value_type>) {
      self_->mail(std::forward<Ts>(xs)...)
        .request(impl_, timeout)
        .receive([] {}, [&err](error& x) { err = std::move(x); });
      if (err)
        return result_type{err};
      else
        return result_type{};
    } else {
      function_view_result<value_type> result;
      self_->mail(std::forward<Ts>(xs)...)
        .request(impl_, timeout)
        .receive(function_view_storage_t<value_type>{result.value},
                 [&err](error& x) {
                   if (!x) {
                     err = caf::make_error(sec::bad_function_call);
                     return;
                   }
                   err = std::move(x);
                 });
      if (err)
        return result_type{err};
      else
        return result_type{flatten(result.value)};
    }
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

  timespan timeout;

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

  union {
    scoped_actor self_;
  };
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
auto make_function_view(const T& x, timespan t = infinite) {
  return function_view<T>{x, t};
}

} // namespace caf
