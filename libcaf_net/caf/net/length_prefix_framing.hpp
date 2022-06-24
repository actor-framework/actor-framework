// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_system.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/byte_span.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/disposable.hpp"
#include "caf/net/binary/default_trait.hpp"
#include "caf/net/binary/flow_bridge.hpp"
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

  /// Describes the one-time connection event.
  using connect_event_t
    = cow_tuple<async::consumer_resource<binary::frame>,  // Socket to App.
                async::producer_resource<binary::frame>>; // App to Socket.

  /// Runs length-prefix framing on given connection.
  /// @param sys The host system.
  /// @param conn A connected stream socket or SSL connection, depending on the
  ///             `Transport`.
  /// @param init Function object for setting up the created flows.
  template <class Transport = stream_transport, class Connection, class Init>
  static disposable run(actor_system& sys, Connection conn, Init init) {
    using trait_t = binary::default_trait;
    using frame_t = binary::frame;
    static_assert(std::is_invocable_v<Init, connect_event_t&&>,
                  "invalid signature found for init");
    auto [fd_pull, app_push] = async::make_spsc_buffer_resource<frame_t>();
    auto [app_pull, fd_push] = async::make_spsc_buffer_resource<frame_t>();
    auto mpx = sys.network_manager().mpx_ptr();
    auto fc = flow_connector<trait_t>::make_trivial(std::move(fd_pull),
                                                    std::move(fd_push));
    auto bridge = binary::flow_bridge<trait_t>::make(mpx, std::move(fc));
    auto bridge_ptr = bridge.get();
    auto impl = length_prefix_framing::make(std::move(bridge));
    auto transport = Transport::make(std::move(conn), std::move(impl));
    auto ptr = socket_manager::make(mpx, std::move(transport));
    bridge_ptr->self_ref(ptr->as_disposable());
    mpx->start(ptr);
    init(connect_event_t{app_pull, app_push});
    return disposable{std::move(ptr)};
  }

  /// The default number of concurrently open connections when using `accept`.
  static constexpr size_t default_max_connections = 128;

  /// A producer resource for the acceptor. Any accepted connection is
  /// represented by two buffers: one for input and one for output.
  using acceptor_resource_t = async::producer_resource<connect_event_t>;

  /// Listens for incoming connection on @p fd.
  /// @param sys The host system.
  /// @param fd An accept socket in listening mode. For a TCP socket, this
  ///           socket must already listen to a port.
  /// @param cfg Configures the acceptor. Currently, the only supported
  ///            configuration parameter is `max-connections`.
  template <class Transport = stream_transport, class Socket, class OnConnect>
  static disposable accept(actor_system& sys, Socket fd,
                           acceptor_resource_t out, const settings& cfg = {}) {
  }

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
  // -- member variables -------------------------------------------------------

  stream_oriented::lower_layer* down_;
  upper_layer_ptr up_;
  size_t message_offset_ = 0;
};

} // namespace caf::net
