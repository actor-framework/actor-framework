// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/callback.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/error.hpp"
#include "caf/net/fwd.hpp"

#include <utility>

namespace caf::net::web_socket {

/// Accepts or rejects incoming connection requests.
/// @tparam Ts Denotes the types for the worker handshake.
template <class... Ts>
class acceptor {
public:
  template <class Trait>
  using server_factory_type = server_factory<Trait, Ts...>;

  virtual ~acceptor() = default;

  virtual void accept(Ts... xs) = 0;

  void reject(error reason) {
    reject_reason_ = reason;
    if (accepted_)
      accepted_ = false;
  }

  bool accepted() const noexcept {
    return accepted_;
  }

  error&& reject_reason() && noexcept {
    return std::move(reject_reason_);
  }

  const error& reject_reason() const& noexcept {
    return reject_reason_;
  }

protected:
  bool accepted_ = false;
  error reject_reason_;
};

template <class Trait, class... Ts>
class acceptor_impl : public acceptor<Ts...> {
public:
  using super = acceptor<Ts...>;

  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  /// The event type for informing the application of an accepted connection.
  using app_event_type
    = cow_tuple<async::consumer_resource<input_type>,
                async::producer_resource<output_type>, Ts...>;

  /// The pair of resources for the WebSocket worker.
  using ws_res_type = async::resource_pair<output_type, input_type>;

  void accept(Ts... xs) override {
    if (super::accepted_)
      return;
    auto [app_pull, ws_push] = async::make_spsc_buffer_resource<input_type>();
    auto [ws_pull, app_push] = async::make_spsc_buffer_resource<output_type>();
    ws_resources = ws_res_type{ws_pull, ws_push};
    app_event = make_cow_tuple(app_pull, app_push, std::move(xs)...);
    super::accepted_ = true;
  }

  ws_res_type ws_resources;

  app_event_type app_event;
};

/// Type trait that determines if a type is an `acceptor`.
template <class T>
struct is_acceptor : std::false_type {};

/// Specialization of `is_acceptor` for `acceptor` types.
template <class... Ts>
struct is_acceptor<acceptor<Ts...>> : std::true_type {};

/// A `constexpr bool` variable that evaluates to `true` if the given type is an
/// `acceptor`, `false` otherwise.
template <class T>
inline constexpr bool is_acceptor_v = is_acceptor<T>::value;

} // namespace caf::net::web_socket
