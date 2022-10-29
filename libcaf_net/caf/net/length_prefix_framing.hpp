// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_system.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/byte_span.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/detail/accept_handler.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/disposable.hpp"
#include "caf/net/binary/default_trait.hpp"
#include "caf/net/binary/flow_bridge.hpp"
#include "caf/net/binary/frame.hpp"
#include "caf/net/binary/lower_layer.hpp"
#include "caf/net/binary/upper_layer.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/stream_oriented.hpp"

#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

namespace caf::net {

/// Length-prefixed message framing for discretizing a Byte stream into messages
/// of varying size. The framing uses 4 Bytes for the length prefix, but
/// messages (including the 4 Bytes for the length prefix) are limited to a
/// maximum size of INT32_MAX. This limitation comes from the POSIX API (recv)
/// on 32-bit platforms.
class CAF_NET_EXPORT length_prefix_framing
  : public stream_oriented::upper_layer,
    public binary::lower_layer {
public:
  // -- member types -----------------------------------------------------------

  using upper_layer_ptr = std::unique_ptr<binary::upper_layer>;

  // -- constants --------------------------------------------------------------

  static constexpr size_t hdr_size = sizeof(uint32_t);

  static constexpr size_t max_message_length = INT32_MAX - sizeof(uint32_t);

  // -- constructors, destructors, and assignment operators --------------------

  explicit length_prefix_framing(upper_layer_ptr up);

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<length_prefix_framing> make(upper_layer_ptr up);

  // -- high-level factory functions -------------------------------------------

  /// Binds a trait class to the framing protocol to enable a high-level API for
  /// operating on flows.
  template <class Trait>
  struct bind {
    /// Describes the one-time connection event.
    using connect_event_t
      = cow_tuple<async::consumer_resource<typename Trait::input_type>,
                  async::producer_resource<typename Trait::output_type>>;

    /// Runs length-prefix framing on given connection.
    /// @param sys The host system.
    /// @param conn A connected stream socket or SSL connection, depending on
    /// the
    ///             `Transport`.
    /// @param pull Source for pulling data to send.
    /// @param push Source for pushing received data.
    template <class Connection>
    static disposable
    run(actor_system& sys, Connection conn,
        async::consumer_resource<typename Trait::output_type> pull,
        async::producer_resource<typename Trait::input_type> push) {
      using transport_t = typename Connection::transport_type;
      auto mpx = sys.network_manager().mpx_ptr();
      auto fc = flow_connector<Trait>::make_trivial(std::move(pull),
                                                    std::move(push));
      auto bridge = binary::flow_bridge<Trait>::make(mpx, std::move(fc));
      auto bridge_ptr = bridge.get();
      auto impl = length_prefix_framing::make(std::move(bridge));
      auto transport = transport_t::make(std::move(conn), std::move(impl));
      auto ptr = socket_manager::make(mpx, std::move(transport));
      bridge_ptr->self_ref(ptr->as_disposable());
      mpx->start(ptr);
      return disposable{std::move(ptr)};
    }

    /// Runs length-prefix framing on given connection.
    /// @param sys The host system.
    /// @param conn A connected stream socket or SSL connection, depending on
    /// the
    ///             `Transport`.
    /// @param init Function object for setting up the created flows.
    template <class Connection, class Init>
    static disposable run(actor_system& sys, Connection conn, Init init) {
      using app_in_t = typename Trait::input_type;
      using app_out_t = typename Trait::output_type;
      static_assert(std::is_invocable_v<Init, connect_event_t&&>,
                    "invalid signature found for init");
      auto [app_pull, fd_push] = async::make_spsc_buffer_resource<app_in_t>();
      auto [fd_pull, app_push] = async::make_spsc_buffer_resource<app_out_t>();
      auto result = run(sys, std::move(conn), std::move(fd_pull),
                        std::move(fd_push));
      init(connect_event_t{std::move(app_pull), std::move(app_push)});
      return result;
    }

    /// The default number of concurrently open connections when using `accept`.
    static constexpr size_t default_max_connections = 128;

    /// A producer resource for the acceptor. Any accepted connection is
    /// represented by two buffers: one for input and one for output.
    using acceptor_resource_t = async::producer_resource<connect_event_t>;

    /// Describes the per-connection event.
    using accept_event_t
      = cow_tuple<async::consumer_resource<typename Trait::input_type>,
                  async::producer_resource<typename Trait::output_type>>;

    /// Convenience function for creating an event listener resource and an
    /// @ref acceptor_resource_t via @ref async::make_spsc_buffer_resource.
    static auto make_accept_event_resources() {
      return async::make_spsc_buffer_resource<accept_event_t>();
    }

    /// Listens for incoming connection on @p fd.
    /// @param sys The host system.
    /// @param acc A connection acceptor such as @ref tcp_accept_socket or
    ///            @ref ssl::acceptor.
    /// @param cfg Configures the acceptor. Currently, the only supported
    ///            configuration parameter is `max-connections`.
    template <class Acceptor>
    static disposable accept(actor_system& sys, Acceptor acc,
                             acceptor_resource_t out,
                             const settings& cfg = {}) {
      using transport_t = typename Acceptor::transport_type;
      using trait_t = binary::default_trait;
      using factory_t = cf_impl<transport_t>;
      using conn_t = typename transport_t::connection_handle;
      using impl_t = detail::accept_handler<Acceptor, conn_t>;
      auto max_connections = get_or(cfg, defaults::net::max_connections);
      if (auto buf = out.try_open()) {
        auto& mpx = sys.network_manager().mpx();
        auto conn = flow_connector<trait_t>::make_basic_server(std::move(buf));
        auto factory = std::make_unique<factory_t>(std::move(conn));
        auto impl = impl_t::make(std::move(acc), std::move(factory),
                                 max_connections);
        auto impl_ptr = impl.get();
        auto ptr = net::socket_manager::make(&mpx, std::move(impl));
        impl_ptr->self_ref(ptr->as_disposable());
        mpx.start(ptr);
        return disposable{std::move(ptr)};
      } else {
        return {};
      }
    }
  };

  // -- implementation of stream_oriented::upper_layer -------------------------

  error start(stream_oriented::lower_layer* down,
              const settings& config) override;

  void abort(const error& reason) override;

  ptrdiff_t consume(byte_span buffer, byte_span delta) override;

  void prepare_send() override;

  bool done_sending() override;

  // -- implementation of binary::lower_layer ----------------------------------

  bool can_send_more() const noexcept override;

  void request_messages() override;

  void suspend_reading() override;

  bool is_reading() const noexcept override;

  void write_later() override;

  void begin_message() override;

  byte_buffer& message_buffer() override;

  bool end_message() override;

  void shutdown() override;

  // -- utility functions ------------------------------------------------------

  static std::pair<size_t, byte_span> split(byte_span buffer) noexcept;

private:
  // -- helper classes ---------------------------------------------------------

  template <class Transport>
  class cf_impl
    : public detail::connection_factory<typename Transport::connection_handle> {
  public:
    using connection_handle = typename Transport::connection_handle;

    using connector_ptr = flow_connector_ptr<binary::default_trait>;

    explicit cf_impl(connector_ptr connector) : conn_(std::move(connector)) {
      // nop
    }

    net::socket_manager_ptr make(net::multiplexer* mpx,
                                 connection_handle conn) override {
      using trait_t = binary::default_trait;
      auto bridge = binary::flow_bridge<trait_t>::make(mpx, conn_);
      auto bridge_ptr = bridge.get();
      auto impl = length_prefix_framing::make(std::move(bridge));
      auto fd = conn.fd();
      auto transport = Transport::make(std::move(conn), std::move(impl));
      transport->active_policy().accept(fd);
      auto mgr = socket_manager::make(mpx, std::move(transport));
      bridge_ptr->self_ref(mgr->as_disposable());
      return mgr;
    }

  private:
    connector_ptr conn_;
  };

  // -- member variables -------------------------------------------------------

  stream_oriented::lower_layer* down_;
  upper_layer_ptr up_;
  size_t message_offset_ = 0;
};

} // namespace caf::net
