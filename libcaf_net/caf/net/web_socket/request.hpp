// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/error.hpp"
#include "caf/net/fwd.hpp"

#include <utility>

namespace caf::net::web_socket {

/// Represents a WebSocket connection request.
/// @tparam Trait Converts between native and wire format.
template <class Trait, class... Ts>
class request {
public:
  template <class, class, class...>
  friend class flow_connector_request_impl;

  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  using app_res_type = cow_tuple<async::consumer_resource<input_type>,
                                 async::producer_resource<output_type>, Ts...>;

  using ws_res_type = async::resource_pair<output_type, input_type>;

  void accept(Ts... worker_args) {
    if (accepted())
      return;
    auto [app_pull, ws_push] = async::make_spsc_buffer_resource<input_type>();
    auto [ws_pull, app_push] = async::make_spsc_buffer_resource<output_type>();
    ws_resources_ = ws_res_type{ws_pull, ws_push};
    app_resources_ = make_cow_tuple(app_pull, app_push,
                                    std::move(worker_args)...);
    accepted_ = true;
  }

  void reject(error reason) {
    reject_reason_ = reason;
    if (accepted_)
      accepted_ = false;
  }

  bool accepted() const noexcept {
    return accepted_;
  }

  const error& reject_reason() const noexcept {
    return reject_reason_;
  }

private:
  bool accepted_ = false;
  ws_res_type ws_resources_;
  app_res_type app_resources_;
  error reject_reason_;
};

} // namespace caf::net::web_socket
