// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/client_factory_base.hpp"
#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/octet_stream/flow_bridge.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/defaults.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/observable_builder.hpp"

#include <functional>
#include <type_traits>

namespace caf::detail {

struct octet_stream_no_outputs {
  auto operator()(flow::coordinator* self) {
    return self->make_observable().never<std::byte>();
  }
};

} // namespace caf::detail

namespace caf::net::octet_stream {

/// Configuration for the `with(...).connect(...).start(...)` DSL.
class client_factory_config : public dsl::client_config_value {
public:
  using super = dsl::client_config_value;

  using super::super;

  /// Sets the default buffer size for reading from the network.
  uint32_t read_buffer_size = defaults::net::octet_stream_buffer_size;

  /// Sets the default buffer size for writing to the network.
  uint32_t write_buffer_size = defaults::net::octet_stream_buffer_size;

  template <class T, class... Args>
  static auto make(dsl::client_config_tag<T>,
                   const dsl::generic_config_value& from, Args&&... args) {
    return super::make_impl(std::in_place_type<client_factory_config>, from,
                            std::in_place_type<T>, std::forward<Args>(args)...);
  }
};

/// Factory for the `with(...).connect(...).start(...)` DSL.
template <class MakeOutputs>
class client_factory_with_outputs
  : public dsl::client_factory_base<client_factory_config,
                                    client_factory_with_outputs<MakeOutputs>> {
public:
  using super
    = dsl::client_factory_base<client_factory_config,
                               client_factory_with_outputs<MakeOutputs>>;

  using config_pointer = intrusive_ptr<client_factory_config>;

  template <class MakeInputs>
  using publisher_type = flow_bridge_publisher_t<MakeInputs, MakeOutputs>;

  client_factory_with_outputs(config_pointer cfg, MakeOutputs make_outputs)
    : super(std::move(cfg)), make_outputs_(std::move(make_outputs)) {
    // nop
  }

  /// Starts the client and returns a publisher for receiving its inputs and a
  /// disposable for shutting down the client.
  /// @param make_inputs A function that takes an `observable<std::byte>` and
  ///                    optionally returns a new `observable` for modifying the
  ///                    inputs. The function may return `void`, but it must
  ///                    subscribe to the input observable in this case. To
  ///                    discard any data from socket, pass `std::ignore` to
  ///                    `subscribe`.
  template <class MakeInputs>
  [[nodiscard]] auto start(MakeInputs make_inputs) && {
    return super::config().visit([this, &make_inputs](auto& data) { //
      return this->do_start(data, make_inputs);
    });
  }

private:
  template <class Conn, class MakeInputs>
  auto do_start_impl(Conn conn, MakeInputs& make_inputs) {
    auto& cfg = super::config();
    auto bridge = flow_bridge::make(cfg.read_buffer_size, cfg.write_buffer_size,
                                    std::move(make_inputs),
                                    std::move(make_outputs_));
    auto bridge_ptr = bridge.get();
    auto transport = std::unique_ptr<octet_stream::transport>{};
    if constexpr (std::is_same_v<Conn, ssl::connection>)
      transport = ssl::transport::make(std::move(conn), std::move(bridge));
    else
      transport = octet_stream::transport::make(std::move(conn),
                                                std::move(bridge));
    transport->active_policy().connect();
    auto ptr = net::socket_manager::make(cfg.mpx, std::move(transport));
    bridge_ptr->init(ptr.get());
    auto pub = bridge_ptr->publisher();
    cfg.mpx->start(ptr);
    return expected{std::pair{pub, disposable{std::move(ptr)}}};
  }

  template <class MakeInputs>
  auto do_start(dsl::client_config::lazy& data, dsl::server_address& addr,
                MakeInputs& make_inputs) {
    return detail::tcp_try_connect(std::move(addr.host), addr.port,
                                   data.connection_timeout,
                                   data.max_retry_count, data.retry_delay)
      .and_then(
        this->with_ssl_connection_or_socket([this, &make_inputs](auto&& conn) {
          using conn_t = decltype(conn);
          return this->do_start_impl(std::forward<conn_t>(conn), make_inputs);
        }));
  }

  template <class MakeInputs>
  auto
  do_start(dsl::client_config::lazy&, const uri&, MakeInputs& make_inputs) {
    auto err = make_error(sec::invalid_argument,
                          "connecting via URI is not supported");
    return do_start(err, make_inputs);
  }

  template <class MakeInputs>
  auto do_start(dsl::client_config::lazy& data, MakeInputs& make_inputs) {
    auto fn = [this, &data, &make_inputs](auto& addr) {
      return this->do_start(data, addr, make_inputs);
    };
    return std::visit(fn, data.server);
  }

  template <class MakeInputs>
  auto do_start(dsl::client_config::socket& data, MakeInputs& make_inputs) {
    return expected{data.take_fd()}
      .and_then(check_socket)
      .and_then(
        this->with_ssl_connection_or_socket([this, &make_inputs](auto&& conn) {
          using conn_t = decltype(conn);
          return this->do_start_impl(std::forward<conn_t>(conn), make_inputs);
        })

      );
  }

  template <class MakeInputs>
  auto do_start(dsl::client_config::conn& data, MakeInputs& make_inputs) {
    return do_start_impl(std::move(data.state), make_inputs);
  }

  template <class MakeInputs>
  auto do_start(error& err, MakeInputs&) {
    using pub_t = publisher_type<MakeInputs>;
    super::config().call_on_error(err);
    return expected<std::pair<pub_t, disposable>>{std::move(err)};
  }

  MakeOutputs make_outputs_;
};

/// Factory for the `with(...).connect(...).start(...)` DSL.
class client_factory
  : public dsl::client_factory_base<client_factory_config, client_factory> {
public:
  using super = dsl::client_factory_base<client_factory_config, client_factory>;

  using super::super;

  using config_type = typename super::config_type;

  /// Overrides the default buffer size for reading from the network.
  client_factory&& read_buffer_size(uint32_t new_value) && {
    config().read_buffer_size = new_value;
    return std::move(*this);
  }

  /// Overrides the default buffer size for writing to the network.
  client_factory&& write_buffer_size(uint32_t new_value) && {
    config().write_buffer_size = new_value;
    return std::move(*this);
  }

  /// Configures the client to send the outputs from the observable created by
  /// `make_outputs`.
  /// @param make_outputs A function that takes a `flow::coordinator*` and
  ///                     returns an `observable<std::byte>`.
  template <class MakeOutputs>
  client_factory_with_outputs<MakeOutputs>
  send_from(MakeOutputs make_outputs) && {
    return {std::move(super::cfg_), std::move(make_outputs)};
  }

  /// @copydoc client_factory_with_outputs::start
  template <class MakeInputs>
  [[nodiscard]] auto start(MakeInputs make_inputs) && {
    return std::move(*this)
      .send_from(detail::octet_stream_no_outputs{})
      .start(std::move(make_inputs));
  }
};

} // namespace caf::net::octet_stream
