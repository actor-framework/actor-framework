#include "caf/net/http/server.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/http/request_header.hpp"
#include "caf/net/http/status.hpp"
#include "caf/net/http/upper_layer.hpp"
#include "caf/net/http/v1.hpp"
#include "caf/net/octet_stream/lower_layer.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/byte_span.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/format.hpp"
#include "caf/error.hpp"
#include "caf/log/net.hpp"

#include <algorithm>

namespace caf::net::http {

namespace {

/// Implements the server part for the HTTP Protocol as defined in RFC 7231.
class server_impl : public http::server {
public:
  // -- member types -----------------------------------------------------------

  enum class mode {
    read_header,
    read_payload,
    read_chunks,
  };

  // -- constructors, destructors, and assignment operators --------------------

  explicit server_impl(upper_layer_ptr up) : up_(std::move(up)) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  size_t max_request_size() const noexcept override {
    return max_request_size_;
  }

  void max_request_size(size_t value) noexcept override {
    if (value > 0)
      max_request_size_ = value;
  }

  // -- http::lower_layer implementation ---------------------------------------

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
      down_->configure_read(receive_policy::up_to(max_request_size_));
  }

  void suspend_reading() override {
    down_->configure_read(receive_policy::stop());
  }

  void begin_header(status code) override {
    down_->begin_output();
    v1::begin_response_header(code, down_->output_buffer());
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
    std::ranges::transform(size_str, std::back_inserter(buf),
                           [](auto c) { return static_cast<std::byte>(c); });
    buf.insert(buf.end(), bytes.begin(), bytes.end());
    buf.emplace_back(std::byte{'\r'});
    buf.emplace_back(std::byte{'\n'});
    return down_->end_output();
  }

  bool send_end_of_chunks() override {
    std::string_view str = "0\r\n\r\n";
    auto bytes = as_bytes(std::span{str});
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
    // Only pass on the first error in case `abort` is called multiple times.
    if (!aborted_) {
      aborted_ = true;
      up_->abort(reason);
    }
  }

  void abort(sec code, std::string_view reason) {
    // Only pass on the first error in case `abort` is called multiple times.
    if (!aborted_) {
      aborted_ = true;
      up_->abort(make_error(code, std::string{reason}));
    }
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
    for (;;) {
      switch (mode_) {
        case mode::read_header: {
          auto [hdr, remainder] = v1::split_header(input);
          if (hdr.empty()) {
            if (input.size() >= max_request_size_) {
              write_response(status::request_header_fields_too_large,
                             "Header exceeds maximum size.");
              abort(sec::protocol_error, "header exceeds maximum size");
              return -1;
            } else {
              return consumed;
            }
          } else if (!handle_header(hdr)) {
            // Note: handle_header already calls abort().
            return -1;
          } else {
            // Prepare for next loop iteration.
            consumed += static_cast<ptrdiff_t>(hdr.size());
            input = remainder;
            // Transition to the next mode.
            if (hdr_.chunked_transfer_encoding()) {
              mode_ = mode::read_chunks;
              if (auto err = up_->begin_chunked_message(hdr_); err.valid()) {
                write_response(status::internal_server_error,
                               "Failed to initiate chunked message.");
                abort(err);
                return -1;
              }
            } else if (auto len = hdr_.content_length()) {
              // Protect against payloads that exceed the maximum size.
              if (*len >= max_request_size_) {
                write_response(status::payload_too_large,
                               "Payload exceeds maximum size.");
                abort(sec::protocol_error, "payload exceeds maximum size");
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
            }
          }
          break;
        }
        case mode::read_payload: {
          if (input.size() >= payload_len_) {
            if (!invoke_upper_layer(input.subspan(0, payload_len_)))
              return -1;
            consumed += static_cast<ptrdiff_t>(payload_len_);
            input = input.subspan(payload_len_);
            mode_ = mode::read_header;
          } else {
            // Wait for more data.
            return consumed;
          }
          break;
        }
        case mode::read_chunks: {
          auto res = v1::parse_chunk(input);
          if (!res) {
            // No error code signals we didn't receive enough data.
            if (res.error().empty())
              return consumed;
            write_response(status::bad_request, "Invalid chunk encoding.");
            abort(res.error());
            return -1;
          }
          auto [chunk_size, remainder] = *res;
          // Protect early against payloads that exceed the maximum size.
          if (chunk_size > max_request_size_ - received_chunks_size_) {
            write_response(status::payload_too_large,
                           "Payload exceeds maximum size.");
            abort(sec::protocol_error, "payload exceeds maximum size");
            return -1;
          }
          if (remainder.size() < chunk_size + 2) {
            // Configure the policy for the next call to consume.
            // Await at least chunk line length + chunk_size bytes + crlf.
            const auto least = input.size() - remainder.size() + chunk_size + 2;
            const auto most = max_request_size_ - received_chunks_size_;
            down_->configure_read(receive_policy::between(least, most));
            return consumed;
          }
          // Reset the policy from the previous call to consume.
          down_->configure_read(
            receive_policy::up_to(max_request_size_ - received_chunks_size_));
          consumed += static_cast<ptrdiff_t>(input.size() - remainder.size()
                                             + chunk_size + 2);
          // Check crlf at the end of chunk.
          if (remainder[chunk_size] != std::byte{'\r'}
              || remainder[chunk_size + 1] != std::byte{'\n'}) {
            write_response(status::bad_request,
                           "Missing CRLF sequence at the end of the chunk.");
            abort(sec::protocol_error,
                  "missing CRLF sequence at the end of the chunk");
            return -1;
          }
          // End of chunk encoded request comes with a zero length chunk.
          if (chunk_size == 0) {
            if (auto err = up_->end_chunked_message(); err.valid()) {
              write_response(
                status::internal_server_error,
                "Failed to process the end of the chunked request.");
              abort(err);
              return -1;
            }
            mode_ = mode::read_header;
            return consumed;
          }
          up_->consume_chunk(remainder.subspan(0, chunk_size));
          received_chunks_size_ += chunk_size;
          input = remainder.subspan(chunk_size + 2);
          break;
        }
      }
    }
  }

private:
  // -- utility functions ------------------------------------------------------

  void write_response(status code, std::string_view content) {
    down_->begin_output();
    v1::write_response(code, "text/plain", content, down_->output_buffer());
    down_->end_output();
  }

  bool invoke_upper_layer(const_byte_span payload) {
    return up_->consume(hdr_, payload) >= 0;
  }

  bool handle_header(std::string_view http) {
    // Parse the header and reject invalid inputs.
    auto [code, msg] = hdr_.parse(http);
    if (code != status::ok) {
      log::net::debug("received malformed header");
      write_response(code, msg);
      abort(sec::protocol_error, "received malformed header");
      return false;
    } else {
      return true;
    }
  }

  octet_stream::lower_layer* down_;

  upper_layer_ptr up_;

  /// Buffer for re-using memory.
  request_header hdr_;

  /// Stores whether we are currently waiting for the payload.
  mode mode_ = mode::read_header;

  /// Stores the expected payload size when in read_payload mode.
  size_t payload_len_ = 0;

  /// Maximum size for incoming HTTP requests.
  size_t max_request_size_ = caf::defaults::net::http_max_request_size;

  /// Specific to chunked requests - aggregates the size of all received chunks.
  size_t received_chunks_size_ = 0;

  bool aborted_ = false;
};

} // namespace

// -- factories ----------------------------------------------------------------

std::unique_ptr<server> server::make(upper_layer_ptr up) {
  return std::make_unique<server_impl>(std::move(up));
}

// -- constructors, destructors, and assignment operators --------------------

server::~server() {
  // nop
}

} // namespace caf::net::http
