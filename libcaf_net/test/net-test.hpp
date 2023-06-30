#pragma once

#include "caf/error.hpp"
#include "caf/net/octet_stream/lower_layer.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/web_socket/server.hpp"
#include "caf/settings.hpp"
#include "caf/span.hpp"
#include "caf/string_view.hpp"
#include "caf/test/bdd_dsl.hpp"

/// Implements a trivial transport layer that stores the contents of all
/// received frames in a respective output buffer, it can propagate the content
/// of the input buffer to the upper layer, and switch protocols if configured
/// so.
class mock_stream_transport : public caf::net::octet_stream::lower_layer {
public:
  // -- member types -----------------------------------------------------------

  using upper_layer_ptr = std::unique_ptr<caf::net::octet_stream::upper_layer>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit mock_stream_transport(upper_layer_ptr ptr) : up(std::move(ptr)) {
    // nop
  }

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<mock_stream_transport> make(upper_layer_ptr ptr) {
    return std::make_unique<mock_stream_transport>(std::move(ptr));
  }

  // -- implementation of octet_stream::lower_layer ----------------------------

  caf::net::multiplexer& mpx() noexcept override;

  bool can_send_more() const noexcept override;

  bool is_reading() const noexcept override;

  void write_later() override;

  void shutdown() override;

  void configure_read(caf::net::receive_policy policy) override;

  void begin_output() override;

  caf::byte_buffer& output_buffer() override;

  bool end_output() override;

  void switch_protocol(upper_layer_ptr) override;

  bool switching_protocol() const noexcept override;

  // -- initialization ---------------------------------------------------------

  caf::error start(caf::net::multiplexer* ptr) {
    mpx_ = ptr;
    return up->start(this);
  }

  // -- buffer management ------------------------------------------------------

  void push(caf::span<const std::byte> bytes) {
    input.insert(input.end(), bytes.begin(), bytes.end());
  }

  void push(std::string_view str) {
    push(caf::as_bytes(caf::make_span(str)));
  }

  size_t unconsumed() const noexcept {
    return read_buf_.size();
  }

  std::string_view output_as_str() const noexcept {
    return {reinterpret_cast<const char*>(output.data()), output.size()};
  }

  // -- event callbacks --------------------------------------------------------

  ptrdiff_t handle_input();

  // -- member variables -------------------------------------------------------

  upper_layer_ptr up;

  upper_layer_ptr next;

  caf::byte_buffer output;

  caf::byte_buffer input;

  uint32_t min_read_size = 0;

  uint32_t max_read_size = 0;

  size_t delta_offset = 0;

private:
  caf::byte_buffer read_buf_;

  caf::error abort_reason_;

  caf::net::multiplexer* mpx_;
};

/// Tag used to configure mock_web_socket_app to request messages on start
struct request_messages_on_start_t {};
constexpr auto request_messages_on_start = request_messages_on_start_t{};

/// Implements a trivial WebSocket application that stores the contents of all
/// received messages in respective text/binary buffers. It can take both
/// roles, server and client, request messages and track whether the
/// lower layer was aborted.
class mock_web_socket_app : public caf::net::web_socket::upper_layer::server {
public:
  // -- constructor ------------------------------------------------------------

  explicit mock_web_socket_app(bool request_messages_on_start);

  // -- factories --------------------------------------------------------------

  static auto make(request_messages_on_start_t) {
    return std::make_unique<mock_web_socket_app>(true);
  }

  static auto make() {
    return std::make_unique<mock_web_socket_app>(false);
  }

  // -- initialization ---------------------------------------------------------

  caf::error start(caf::net::web_socket::lower_layer* ll) override;

  // -- implementation ---------------------------------------------------------

  caf::error accept(const caf::net::http::request_header& hdr) override;

  void prepare_send() override;

  bool done_sending() override;

  void abort(const caf::error& reason) override;

  ptrdiff_t consume_text(std::string_view text) override;

  ptrdiff_t consume_binary(caf::byte_span bytes) override;

  bool has_aborted() const noexcept {
    return !abort_reason.empty();
  }

  // -- member variables -------------------------------------------------------

  std::string text_input;

  caf::byte_buffer binary_input;

  caf::net::web_socket::lower_layer* down = nullptr;

  caf::settings cfg;

  bool request_messages_on_start = false;

  caf::error abort_reason;
};

// Drop-in replacement for std::barrier (based on the TS API as of 2020).
class barrier {
public:
  explicit barrier(ptrdiff_t num_threads)
    : num_threads_(num_threads), count_(0) {
    // nop
  }

  void arrive_and_wait();

private:
  ptrdiff_t num_threads_;
  std::mutex mx_;
  std::atomic<ptrdiff_t> count_;
  std::condition_variable cv_;
};
