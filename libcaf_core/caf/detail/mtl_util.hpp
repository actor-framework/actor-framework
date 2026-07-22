// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor.hpp"
#include "caf/fwd.hpp"
#include "caf/type_list.hpp"

#include <type_traits>

namespace caf::detail {

template <class F>
struct mtl_util;

template <class... Rs, class... Ts>
struct mtl_util<result<Rs...>(Ts...)> {
  template <class Self, class Adapter, class Inspector>
  static bool
  send(Self& self, const actor& dst, Adapter& adapter, Inspector& f, Ts... xs) {
    f.revert();
    if (adapter.read(f, xs...)) {
      self->mail(std::move(xs)...).send(dst);
      return true;
    } else {
      return false;
    }
  }

  template <class Self, class Adapter, class Inspector>
  static bool
  send(Self& self, const actor& dst, Adapter& adapter, Inspector& f) {
    return send(self, dst, adapter, f, Ts{}...);
  }

  template <class Self, class Timeout, class Adapter, class Inspector,
            class OnResult, class OnError>
  static bool request(Self& self, const actor& dst, Timeout timeout,
                      Adapter& adapter, Inspector& f, OnResult& on_result,
                      OnError& on_error, Ts... xs) {
    f.revert();
    if (adapter.read(f, xs...)) {
      if constexpr (std::is_same_v<type_list<Rs...>, type_list<void>>)
        self->mail(std::move(xs)...)
          .request(dst, timeout)
          .then([f{std::move(on_result)}]() mutable { f(); },
                std::move(on_error));
      else
        self->mail(std::move(xs)...)
          .request(dst, timeout)
          .then([f{std::move(on_result)}](Rs&... res) mutable { f(res...); },
                std::move(on_error));
      return true;
    } else {
      return false;
    }
  }

  template <class Self, class Timeout, class Adapter, class Inspector,
            class OnResult, class OnError>
  static bool request(Self& self, const actor& dst, Timeout timeout,
                      Adapter& adapter, Inspector& f, OnResult& on_result,
                      OnError& on_error) {
    return request(self, dst, timeout, adapter, f, on_result, on_error,
                   Ts{}...);
  }
};

template <class>
struct mtl_try_send;

template <class... Sigs>
struct mtl_try_send<type_list<Sigs...>> {
  template <class Self, class Adapter, class Reader>
  static bool
  run(Self& self, const actor& dst, Adapter& adapter, Reader& reader) {
    return (mtl_util<Sigs>::send(self, dst, adapter, reader) || ...);
  }
};

template <class>
struct mtl_try_request;

template <class... Sigs>
struct mtl_try_request<type_list<Sigs...>> {
  template <class Self, class Timeout, class Adapter, class Reader,
            class OnResult, class OnError>
  static bool run(Self& self, const actor& dst, Timeout timeout,
                  Adapter& adapter, Reader& reader, OnResult& on_result,
                  OnError& on_error) {
    return (mtl_util<Sigs>::request(self, dst, timeout, adapter, reader,
                                    on_result, on_error)
            || ...);
  }
};

} // namespace caf::detail
