// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte_span.hpp"
#include "caf/detail/append_hex.hpp"
#include "caf/detail/unordered_flat_map.hpp"
#include "caf/error.hpp"
#include "caf/logger.hpp"
#include "caf/net/connection_acceptor.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/http/context.hpp"
#include "caf/net/http/header.hpp"
#include "caf/net/http/header_fields_map.hpp"
#include "caf/net/http/status.hpp"
#include "caf/net/http/v1.hpp"
#include "caf/net/hypertext_oriented_layer_ptr.hpp"
#include "caf/net/message_flow_bridge.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/pec.hpp"
#include "caf/settings.hpp"
#include "caf/tag/hypertext_oriented.hpp"
#include "caf/tag/stream_oriented.hpp"

#include <algorithm>

namespace caf::net::http {

/// Implements the server part for the HTTP Protocol as defined in RFC 7231.
template <class UpperLayer>
class server {
public:
  // -- member types -----------------------------------------------------------

  using input_tag = tag::stream_oriented;

  using output_tag = tag::hypertext_oriented;

  using header_fields_type = header_fields_map;

  using status_code_type = status;

  using context_type = context;

  using header_type = header;

  enum class mode {
    read_header,
    read_payload,
    read_chunks,
  };

  // -- constants --------------------------------------------------------------

  /// Default maximum size for incoming HTTP requests: 64KiB.
  static constexpr uint32_t default_max_request_size = 65'536;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit server(Ts&&... xs) : upper_layer_(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- initialization ---------------------------------------------------------

  template <class LowerLayerPtr>
  error init(socket_manager* owner, LowerLayerPtr down, const settings& cfg) {
    owner_ = owner;
    if (auto err = upper_layer_.init(owner, this_layer_ptr(down), cfg))
      return err;
    if (auto max_size = get_as<uint32_t>(cfg, "http.max-request-size"))
      max_request_size_ = *max_size;
    down->configure_read(receive_policy::up_to(max_request_size_));
    return none;
  }

  // -- properties -------------------------------------------------------------

  auto& upper_layer() noexcept {
    return upper_layer_;
  }

  const auto& upper_layer() const noexcept {
    return upper_layer_;
  }

  // -- interface for the upper layer ------------------------------------------

  template <class LowerLayerPtr>
  static bool can_send_more(LowerLayerPtr down) noexcept {
    return down->can_send_more();
  }

  template <class LowerLayerPtr>
  static void suspend_reading(LowerLayerPtr down) {
    down->configure_read(receive_policy::stop());
  }

  template <class LowerLayerPtr>
  static auto handle(LowerLayerPtr down) noexcept {
    return down->handle();
  }

  template <class LowerLayerPtr>
  bool send_header(LowerLayerPtr down, context, status code,
                   const header_fields_map& fields) {
    down->begin_output();
    v1::write_header(code, fields, down->output_buffer());
    down->end_output();
    return true;
  }

  template <class LowerLayerPtr>
  bool send_payload(LowerLayerPtr down, context, const_byte_span bytes) {
    down->begin_output();
    auto& buf = down->output_buffer();
    buf.insert(buf.end(), bytes.begin(), bytes.end());
    down->end_output();
    return true;
  }

  template <class LowerLayerPtr>
  bool send_chunk(LowerLayerPtr down, context, const_byte_span bytes) {
    down->begin_output();
    auto& buf = down->output_buffer();
    auto size = bytes.size();
    detail::append_hex(buf, &size, sizeof(size));
    buf.emplace_back(byte{'\r'});
    buf.emplace_back(byte{'\n'});
    buf.insert(buf.end(), bytes.begin(), bytes.end());
    buf.emplace_back(byte{'\r'});
    buf.emplace_back(byte{'\n'});
    return down->end_output();
  }

  template <class LowerLayerPtr>
  bool send_end_of_chunks(LowerLayerPtr down, context) {
    string_view str = "0\r\n\r\n";
    auto bytes = as_bytes(make_span(str));
    down->begin_output();
    auto& buf = down->output_buffer();
    buf.insert(buf.end(), bytes.begin(), bytes.end());
    return down->end_chunk();
  }

  template <class LowerLayerPtr>
  void fin(LowerLayerPtr, context) {
    // nop
  }

  template <class LowerLayerPtr>
  static void abort_reason(LowerLayerPtr down, error reason) {
    return down->abort_reason(std::move(reason));
  }

  template <class LowerLayerPtr>
  static const error& abort_reason(LowerLayerPtr down) {
    return down->abort_reason();
  }

  // -- interface for the lower layer ------------------------------------------

  template <class LowerLayerPtr>
  bool prepare_send(LowerLayerPtr down) {
    return upper_layer_.prepare_send(this_layer_ptr(down));
  }

  template <class LowerLayerPtr>
  bool done_sending(LowerLayerPtr down) {
    return upper_layer_.prepare_send(this_layer_ptr(down));
  }

  template <class LowerLayerPtr>
  void continue_reading(LowerLayerPtr down) {
    down->configure_read(receive_policy::up_to(max_request_size_));
  }

  template <class LowerLayerPtr>
  void abort(LowerLayerPtr down, const error& reason) {
    upper_layer_.abort(this_layer_ptr(down), reason);
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume(LowerLayerPtr down, byte_span input, byte_span) {
    using namespace literals;
    CAF_LOG_TRACE(CAF_ARG2("socket", down->handle().id)
                  << CAF_ARG2("bytes", input.size()));
    ptrdiff_t consumed = 0;
    for (;;) {
      switch (mode_) {
        case mode::read_header: {
          auto [hdr, remainder] = v1::split_header(input);
          if (hdr.empty()) {
            if (input.size() >= max_request_size_) {
              write_response(down, status::request_header_fields_too_large,
                             "Header exceeds maximum size.");
              auto err = make_error(pec::too_many_characters,
                                    "exceeded maximum request size");
              down->abort_reason(std::move(err));
              return -1;
            } else {
              return consumed;
            }
          } else if (!handle_header(down, hdr)) {
            return -1;
          } else {
            // Prepare for next loop iteration.
            consumed += static_cast<ptrdiff_t>(hdr.size());
            input = remainder;
            // Transition to the next mode.
            if (hdr_.chunked_transfer_encoding()) {
              mode_ = mode::read_chunks;
            } else if (auto len = hdr_.content_length()) {
              // Protect against payloads that exceed the maximum size.
              if (*len >= max_request_size_) {
                write_response(down, status::payload_too_large,
                               "Payload exceeds maximum size.");
                auto err = make_error(sec::invalid_argument,
                                      "exceeded maximum payload size");
                down->abort_reason(std::move(err));
                return -1;
              }
              // Transition to read_payload mode and continue.
              payload_len_ = *len;
              mode_ = mode::read_payload;
            } else {
              // TODO: we may *still* have a payload since HTTP can omit the
              //       Content-Length field and simply close the connection
              //       after the payload.
              if (!invoke_upper_layer(down, const_byte_span{}))
                return -1;
            }
          }
          break;
        }
        case mode::read_payload: {
          if (input.size() >= payload_len_) {
            if (!invoke_upper_layer(down, input.subspan(0, payload_len_)))
              return -1;
            consumed += static_cast<ptrdiff_t>(payload_len_);
            mode_ = mode::read_header;
          } else {
            // Wait for more data.
            return consumed;
          }
          break;
        }
        case mode::read_chunks: {
          // TODO: implement me
          write_response(down, status::not_implemented,
                         "Chunked transfer not implemented yet.");
          auto err = make_error(sec::invalid_argument,
                                "exceeded maximum payload size");
          down->abort_reason(std::move(err));
          return -1;
        }
      }
    }
  }

private:
  // -- implementation details -------------------------------------------------

  template <class LowerLayerPtr>
  auto this_layer_ptr(LowerLayerPtr down) {
    return make_hypertext_oriented_layer_ptr(this, down);
  }

  template <class LowerLayerPtr>
  void write_response(LowerLayerPtr down, status code, string_view content) {
    down->begin_output();
    v1::write_response(code, "text/plain", content, down->output_buffer());
    down->end_output();
  }

  template <class LowerLayerPtr>
  bool invoke_upper_layer(LowerLayerPtr down, const_byte_span payload) {
    if (!upper_layer_.consume(this_layer_ptr(down), context{}, hdr_, payload)) {
      if (!down->abort_reason()) {
        down->abort_reason(make_error(sec::unsupported_operation,
                                      "requested method not implemented yet"));
      }
      return false;
    }
    return true;
  }

  // -- HTTP request processing ------------------------------------------------

  template <class LowerLayerPtr>
  bool handle_header(LowerLayerPtr down, string_view http) {
    // Parse the header and reject invalid inputs.
    auto [code, msg] = hdr_.parse(http);
    if (code != status::ok) {
      write_response(down, code, msg);
      down->abort_reason(make_error(pec::invalid_argument, "malformed header"));
      return false;
    } else {
      return true;
    }
  }

  /// Stores the upper layer.
  UpperLayer upper_layer_;

  /// Stores a pointer to the owning manager for the delayed initialization.
  socket_manager* owner_ = nullptr;

  /// Buffer for re-using memory.
  header hdr_;

  /// Stores whether we are currently waiting for the payload.
  mode mode_ = mode::read_header;

  /// Stores the expected payload size when in read_payload mode.
  size_t payload_len_ = 0;

  /// Maximum size for incoming HTTP requests.
  uint32_t max_request_size_ = default_max_request_size;
};

/// Creates a WebSocket server on the connected socket `fd`.
/// @param mpx The multiplexer that takes ownership of the socket.
/// @param fd A connected stream socket.
/// @param in Inputs for writing to the socket.
/// @param out Outputs from the socket.
/// @param trait Converts between the native and the wire format.
template <template <class> class Transport = stream_transport, class Socket,
          class T, class Trait>
socket_manager_ptr make_server(multiplexer& mpx, Socket fd,
                               async::consumer_resource<T> in,
                               async::producer_resource<T> out, Trait trait) {
  using app_t = message_flow_bridge<T, Trait, tag::mixed_message_oriented>;
  using stack_t = Transport<server<app_t>>;
  auto mgr = make_socket_manager<stack_t>(fd, &mpx, std::move(in),
                                          std::move(out), std::move(trait));
  return mgr;
}

} // namespace caf::net::http
