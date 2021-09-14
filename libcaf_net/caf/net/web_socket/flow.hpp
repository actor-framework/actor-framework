// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/publisher.hpp"
#include "caf/net/observer_adapter.hpp"
#include "caf/net/publisher_adapter.hpp"

namespace caf::net::web_socket::internal {

/// Implements a WebSocket application that uses two flows for bidirectional
/// communication: one input flow and one output flow.
template <class Reader, class Writer>
class bidir_app {
public:
  using input_tag = tag::message_oriented;

  using reader_output_type = typename Reader::value_type;

  using writer_input_type = typename Writer::value_type;

  bidir_app(Reader&& reader, Writer&& writer)
    : reader_(std::move(reader)), writer_(std::move(writer)) {
    // nop
  }

  async::publisher<reader_input_type>
  connect_flows(net::socket_manager* mgr,
                async::publisher<writer_input_type> in) {
    using make_counted;
    // Connect the writer adapter.
    using writer_input_t = net::observer_adapter<writer_input_type>;
    writer_input_ = make_counted<writer_input_t>(mgr);
    in.subscribe(writer_input_->as_observer());
    // Create the reader adapter.
    using reader_output_t = net::publisher_adapter<node_message>;
    reader_output_ = make_counted<reader_output_t>(mgr, reader_.buffer_size(),
                                                   reader_.batch_size());
    return reader_output_->as_publisher();
  }

  template <class LowerLayerPtr>
  error init(net::socket_manager* mgr, LowerLayerPtr&&, const settings&cfg) {
    if (auto err = reader_.init(cfg))
      return err;
    if (auto err = writer_.init(cfg))
      return err;
    return none;
  }

  template <class LowerLayerPtr>
  bool prepare_send(LowerLayerPtr down) {
    while (down->can_send_more()) {
      auto [val, done, err] = writer_input_->poll();
      if (val) {
        if (!write(down, *val)) {
          down->abort_reason(make_error(ec::invalid_message));
          return false;
        }
      } else if (done) {
        if (err) {
          down->abort_reason(*err);
          return false;
        }
      } else {
        break;
      }
    }
    return true;
  }

  template <class LowerLayerPtr>
  bool done_sending(LowerLayerPtr) {
    return !writer_input_->has_data();
  }

  template <class LowerLayerPtr>
  void abort(LowerLayerPtr, const error& reason) {
    reader_output_->flush();
    if (reason == sec::socket_disconnected || reason == sec::discarded)
      reader_output_->on_complete();
    else
      reader_output_->on_error(reason);
  }

  template <class LowerLayerPtr>
  void after_reading(LowerLayerPtr) {
    reader_output_->flush();
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume_text(LowerLayerPtr down, caf::string_view text) {
    reader_msgput_type msg;
    if (reader_.deserialize_text(text, msg)) {
      if (reader_output_->push(std::move(msg)) == 0)
        down->suspend_reading();
      return static_cast<ptrdiff_t>(text.size());
    } else {
      down->abort_reason(make_error(ec::invalid_message));
      return -1;
    }
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume_binary(LowerLayerPtr, caf::byte_span bytes) {
    reader_msgput_type msg;
    if (reader_.deserialize_binary(bytes, msg)) {
      if (reader_output_->push(std::move(msg)) == 0)
        down->suspend_reading();
      return static_cast<ptrdiff_t>(text.size());
    } else {
      down->abort_reason(make_error(ec::invalid_message));
      return -1;
    }
  }

private:
  template <class LowerLayerPtr>
  bool write(LowerLayerPtr down, const writer_input_type& msg) {
    if (writer_.is_text_message(msg)) {
      down->begin_text_message();
      if (writer_.serialize_text(msg, down->text_message_buffer())) {
        down->end_text_message();
        return true;
      } else {
        return false;
      }
    } else {
      down->begin_binary_message();
      if (writer_.serialize_binary(msg, down->binary_message_buffer())) {
        down->end_binary_message();
        return true;
      } else {
        return false;
      }
    }
  }

  /// Deserializes text or binary messages from the socket.
  Reader reader_;

  /// Serializes text or binary messages to the socket.
  Writer writer_;

  /// Forwards outgoing messages to the peer. We write whatever we receive from
  /// this channel to the socket.
  net::observer_adapter_ptr<node_message> writer_input_;

  /// After receiving messages from the socket, we publish to this adapter for
  /// downstream consumers.
  net::publisher_adapter_ptr<node_message> reader_output_;
};

} // namespace caf::net::web_socket::internal

namespace caf::net::web_socket {

/// Connects to a WebSocket server for bidirectional communication.
/// @param sys The enclosing actor system.
/// @param cfg Provides optional configuration parameters such as WebSocket
///            protocols and extensions for the handshake.
/// @param locator Identifies the WebSocket server.
/// @param writer_input Publisher of events that go out to the server.
/// @param reader Reads messages from the server and publishes them locally.
/// @param writer Writes messages from the @p writer_input to text or binary
///               messages for sending them to the server.
/// @returns a publisher that makes messages from the server accessible on
///          success, an error otherwise.
template <template <class> class Transport = stream_transport, class Reader,
          class Writer>
expected<async::publisher<typename Reader::value_type>>
flow_connect_bidir(actor_system& sys, const settings& cfg, uri locator,
                   async::publisher writer_input, Reader reader,
                   Writer writer) {
  using stack_t
    = Transport<web_socket::client<internal::bidir_app<Reader, Writer>>>;
  using impl = socket_manager_impl<stack_t>;
  if (locator.empty()) {
    return make_error(sec::invalid_argument, __func__,
                      "cannot connect to empty URI");
  } else if (locator.scheme() != "ws") {
    return make_error(sec::invalid_argument, __func__,
                      "malformed URI, expected format 'ws://<authority>'");
  } else if (!locator.fragment().empty()) {
    return make_error(sec::invalid_argument, __func__,
                      "query and fragment components are not supported");
  } else if (locator.authority().empty()) {
    return make_error(sec::invalid_argument, __func__,
                      "malformed URI, expected format 'ws://<authority>'");
  } else if (auto sock = impl::connect_to(locator.authority())) {
    web_socket::handshake hs;
    hs.host(to_string(locator.authority()));
    if (locator.path().empty())
      hs.endpoint("/");
    else
      hs.endpoint(to_string(locator.path()));
    if (auto protocols = get_as<std::string>(cfg, "protocols"))
      hs.protocols(std::move(*protocols));
    if (auto extensions = get_as<std::string>(cfg, "extensions"))
      hs.extensions(std::move(*extensions));
    auto mgr = make_counted<impl>(*sock, sys.network_manager().mpx_ptr(),
                                  std::move(hs), std::move(reader),
                                  std::move(writer));
    auto out = mgr->upper_layer().connect_flows(std::move(writer_input));
    if (auto err = mgr->init(cfg); !err) {
      return {std::move(out)};
    } else {
      return {std::move(err)};
    }
  } else {
    return {std::move(sock.error())};
  }
}

} // namespace caf::net::web_socket
