#include "caf/net/http/client.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/http/lower_layer.hpp"
#include "caf/net/http/response_header.hpp"
#include "caf/net/http/status.hpp"
#include "caf/net/http/upper_layer.hpp"
#include "caf/net/http/v1.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/lower_layer.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/byte_span.hpp"
#include "caf/detail/append_hex.hpp"
#include "caf/detail/format.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/log/net.hpp"
#include "caf/logger.hpp"
#include "caf/pec.hpp"
#include "caf/settings.hpp"
#include "caf/unordered_flat_map.hpp"

#include <algorithm>

namespace caf::net::http {

namespace {

class client_impl : public client {
public:
  // -- member types -----------------------------------------------------------

  enum class mode {
    read_header,
    read_payload,
    read_chunks,
  };

  // -- constants --------------------------------------------------------------

  /// Default maximum size for incoming HTTP responses: 512KiB.
  static constexpr uint32_t default_max_response_size = 512 * 1024;

  // -- constructors, destructors, and assignment operators --------------------

  explicit client_impl(upper_layer_ptr up) : up_(std::move(up)) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  size_t max_response_size() const override {
    return max_response_size_;
  }

  void max_response_size(size_t value) override {
    max_response_size_ = value;
  }

  // -- http::lower_layer::client implementation -------------------------------

  socket_manager* manager() noexcept override {
    return down_->manager();
  }

  bool can_send_more() const noexcept override {
    return down_->can_send_more();
  }

  bool is_reading() const noexcept override {
    return down_->is_reading();
  }

  void write_later() override {
    down_->write_later();
  }

  void shutdown() override {
    down_->shutdown();
  }

  void request_messages() override {
    if (!down_->is_reading())
      down_->configure_read(receive_policy::up_to(max_response_size_));
  }

  void suspend_reading() override {
    down_->configure_read(receive_policy::stop());
  }

  void begin_header(http::method method, std::string_view path) override {
    down_->begin_output();
    v1::begin_request_header(method, path, down_->output_buffer());
  }

  void add_header_field(std::string_view key, std::string_view val) override {
    v1::add_header_field(key, val, down_->output_buffer());
  }

  bool end_header() override {
    return v1::end_header(down_->output_buffer()) && down_->end_output();
  }

  bool send_payload(const_byte_span bytes) override {
    down_->begin_output();
    auto& buf = down_->output_buffer();
    buf.insert(buf.end(), bytes.begin(), bytes.end());
    down_->end_output();
    return true;
  }

  bool send_chunk(const_byte_span bytes) override {
    down_->begin_output();
    auto& buf = down_->output_buffer();
    auto size_str = detail::format("{:X}\r\n", bytes.size());
    std::transform(size_str.begin(), size_str.end(), std::back_inserter(buf),
                   [](auto c) { return static_cast<std::byte>(c); });
    buf.insert(buf.end(), bytes.begin(), bytes.end());
    buf.emplace_back(std::byte{'\r'});
    buf.emplace_back(std::byte{'\n'});
    return down_->end_output();
  }

  bool send_end_of_chunks() override {
    std::string_view str = "0\r\n\r\n";
    auto bytes = as_bytes(make_span(str));
    down_->begin_output();
    auto& buf = down_->output_buffer();
    buf.insert(buf.end(), bytes.begin(), bytes.end());
    return down_->end_output();
  }

  void
  switch_protocol(std::unique_ptr<octet_stream::upper_layer> next) override {
    down_->switch_protocol(std::move(next));
  }

  // -- octet_stream::upper_layer implementation -------------------------------

  error start(octet_stream::lower_layer* down) override {
    down_ = down;
    return up_->start(this);
  }

  void abort(const error& reason) override {
    up_->abort(reason);
  }

  void prepare_send() override {
    up_->prepare_send();
  }

  bool done_sending() override {
    return up_->done_sending();
  }

  ptrdiff_t consume(byte_span input, byte_span) override {
    auto lg = log::net::trace("bytes = {}", input.size());
    ptrdiff_t consumed = 0;
    if (mode_ == mode::read_header) {
      if (input.size() >= max_response_size_) {
        abort("Header exceeds maximum size.");
        return -1;
      }
      auto [hdr, remainder] = v1::split_header(input);
      // Wait for more data.
      if (hdr.empty())
        return 0;
      // Note: handle_header already calls up_->abort().
      if (!handle_header(hdr))
        return -1;
      // Prepare for next call to consume.
      consumed = static_cast<ptrdiff_t>(hdr.size());
      input = remainder;
      // Transition to the next mode.
      if (hdr_.chunked_transfer_encoding()) {
        mode_ = mode::read_chunks;
      } else if (auto len = hdr_.content_length()) {
        // Protect against payloads that exceed the maximum size.
        if (*len >= max_response_size_) {
          abort("Payload exceeds maximum size.");
          return -1;
        }
        // Transition to read_payload mode and continue.
        payload_len_ = *len;
        mode_ = mode::read_payload;
      } else {
        // TODO: we may *still* have a payload since HTTP can omit the
        //       Content-Length field and simply close the connection
        //       after the payload.
        if (!invoke_upper_layer(const_byte_span{}))
          return -1;
        return consumed;
      }
    }
    if (mode_ == mode::read_payload) {
      if (input.size() < payload_len_)
        // Wait for more data.
        return consumed;
      if (!invoke_upper_layer(input.subspan(0, payload_len_)))
        return -1;
      consumed += static_cast<ptrdiff_t>(payload_len_);
      input.subspan(payload_len_);
      mode_ = mode::read_header;
      return consumed;
    }
    // else  if (mode_ == mode::read_chunks) {
    // TODO: implement me
    abort("Chunked transfer not implemented yet.");
    return -1;
  }

private:
  // -- utility functions ------------------------------------------------------

  void abort(std::string_view message) {
    up_->abort(make_error(sec::protocol_error, message));
  }

  bool invoke_upper_layer(const_byte_span payload) {
    return up_->consume(hdr_, payload) >= 0;
  }

  bool handle_header(std::string_view http) {
    // Parse the header and reject invalid inputs.
    auto [code, msg] = hdr_.parse(http);
    if (code != status::ok) {
      log::net::debug("received malformed header");
      abort(msg);
      return false;
    }
    return true;
  }

  /// Points to the transport layer below.
  octet_stream::lower_layer* down_;

  /// Next layer in the processing chain.
  upper_layer_ptr up_;

  /// Buffer for re-using memory.
  response_header hdr_;

  /// Stores whether we are currently waiting for the payload.
  mode mode_ = mode::read_header;

  /// Stores the expected payload size when in read_payload mode.
  size_t payload_len_ = 0;

  /// Maximum size for incoming HTTP requests.
  size_t max_response_size_ = default_max_response_size;
};

} // namespace

client::~client() {
  // nop
}

// -- factories ----------------------------------------------------------------

std::unique_ptr<client> client::make(upper_layer_ptr up) {
  return std::make_unique<client_impl>(std::move(up));
}

} // namespace caf::net::http
