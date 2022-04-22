// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte_span.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/net/message_oriented.hpp"
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
    public message_oriented::lower_layer {
public:
  // -- member types -----------------------------------------------------------

  using upper_layer_ptr = std::unique_ptr<message_oriented::upper_layer>;

  // -- constants --------------------------------------------------------------

  static constexpr size_t hdr_size = sizeof(uint32_t);

  static constexpr size_t max_message_length = INT32_MAX - sizeof(uint32_t);

  // -- constructors, destructors, and assignment operators --------------------

  explicit length_prefix_framing(upper_layer_ptr up);

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<length_prefix_framing> make(upper_layer_ptr up);

  // -- high-level factory functions -------------------------------------------

  //   /// Runs a WebSocket server on the connected socket `fd`.
  //   /// @param mpx The multiplexer that takes ownership of the socket.
  //   /// @param fd A connected stream socket.
  //   /// @param cfg Additional configuration parameters for the protocol
  //   stack.
  //   /// @param in Inputs for writing to the socket.
  //   /// @param out Outputs from the socket.
  //   /// @param trait Converts between the native and the wire format.
  //   /// @relates length_prefix_framing
  //   template <template <class> class Transport = stream_transport, class
  //   Socket,
  //             class T, class Trait, class... TransportArgs>
  //   error run(actor_system&sys,Socket fd,
  //                                        const settings& cfg,
  //                                        async::consumer_resource<T> in,
  //                                        async::producer_resource<T> out,
  //                                        Trait trait, TransportArgs&&...
  //                                        args) {
  //     using app_t
  //       = Transport<length_prefix_framing<message_flow_bridge<T, Trait>>>;
  //     auto mgr = make_socket_manager<app_t>(fd, &mpx,
  //                                           std::forward<TransportArgs>(args)...,
  //                                           std::move(in), std::move(out),
  //                                           std::move(trait));
  //     return mgr->init(cfg);
  // }

  // -- implementation of stream_oriented::upper_layer -------------------------

  error init(socket_manager* owner, stream_oriented::lower_layer* down,
             const settings& config) override;

  void abort(const error& reason) override;

  ptrdiff_t consume(byte_span buffer, byte_span delta) override;

  void continue_reading() override;

  bool prepare_send() override;

  bool done_sending() override;

  // -- implementation of message_oriented::lower_layer ------------------------

  bool can_send_more() const noexcept override;

  void request_messages() override;

  void suspend_reading() override;

  void begin_message() override;

  byte_buffer& message_buffer() override;

  bool end_message() override;

  void send_close_message() override;

  void send_close_message(const error& reason) override;

  // -- utility functions ------------------------------------------------------

  static std::pair<size_t, byte_span> split(byte_span buffer) noexcept;

private:
  // -- member variables -------------------------------------------------------

  stream_oriented::lower_layer* down_;
  upper_layer_ptr up_;
  size_t message_offset_ = 0;
};

} // namespace caf::net
