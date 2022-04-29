#include "caf/net/http/server.hpp"

namespace caf::net::http {

// -- factories ----------------------------------------------------------------

std::unique_ptr<server> server::make(upper_layer_ptr up) {
  return std::make_unique<server>(std::move(up));
}

// -- http::lower_layer implementation -----------------------------------------

bool server::can_send_more() const noexcept {
  return down_->can_send_more();
}

bool server::is_reading() const noexcept {
  return down_->is_reading();
}

void server::close() {
  down_->close();
}

void server::request_messages() {
  if (!down_->is_reading())
    down_->configure_read(receive_policy::up_to(max_request_size_));
}

void server::suspend_reading() {
  down_->configure_read(receive_policy::stop());
}

bool server::send_header(status code, const header_fields_map& fields) {
  down_->begin_output();
  v1::write_header(code, fields, down_->output_buffer());
  down_->end_output();
  return true;
}

bool server::send_payload(const_byte_span bytes) {
  down_->begin_output();
  auto& buf = down_->output_buffer();
  buf.insert(buf.end(), bytes.begin(), bytes.end());
  down_->end_output();
  return true;
}

bool server::send_chunk(const_byte_span bytes) {
  down_->begin_output();
  auto& buf = down_->output_buffer();
  auto size = bytes.size();
  detail::append_hex(buf, &size, sizeof(size));
  buf.emplace_back(std::byte{'\r'});
  buf.emplace_back(std::byte{'\n'});
  buf.insert(buf.end(), bytes.begin(), bytes.end());
  buf.emplace_back(std::byte{'\r'});
  buf.emplace_back(std::byte{'\n'});
  return down_->end_output();
}

bool server::send_end_of_chunks() {
  std::string_view str = "0\r\n\r\n";
  auto bytes = as_bytes(make_span(str));
  down_->begin_output();
  auto& buf = down_->output_buffer();
  buf.insert(buf.end(), bytes.begin(), bytes.end());
  return down_->end_output();
}

// -- stream_oriented::upper_layer implementation ------------------------------

error server::init(socket_manager* owner, stream_oriented::lower_layer* down,
                   const settings& cfg) {
  down_ = down;
  if (auto max_size = get_as<uint32_t>(cfg, "http.max-request-size"))
    max_request_size_ = *max_size;
  if (auto err = up_->init(owner, this, cfg))
    return err;
  return none;
}

void server::abort(const error& reason) {
  up_->abort(reason);
}

bool server::prepare_send() {
  return up_->prepare_send();
}

bool server::done_sending() {
  return up_->done_sending();
}

ptrdiff_t server::consume(byte_span input, byte_span) {
  using namespace literals;
  CAF_LOG_TRACE(CAF_ARG2("bytes", input.size()));
  ptrdiff_t consumed = 0;
  for (;;) {
    switch (mode_) {
      case mode::read_header: {
        auto [hdr, remainder] = v1::split_header(input);
        if (hdr.empty()) {
          if (input.size() >= max_request_size_) {
            up_->abort(
              make_error(sec::protocol_error, "header exceeds maximum size"));
            write_response(status::request_header_fields_too_large,
                           "Header exceeds maximum size.");
            return -1;
          } else {
            return consumed;
          }
        } else if (!handle_header(hdr)) {
          // Note: handle_header already calls up_->abort().
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
              up_->abort(make_error(sec::protocol_error,
                                    "payload exceeds maximum size"));
              write_response(status::payload_too_large,
                             "Payload exceeds maximum size.");
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
          mode_ = mode::read_header;
        } else {
          // Wait for more data.
          return consumed;
        }
        break;
      }
      case mode::read_chunks: {
        // TODO: implement me
        write_response(status::not_implemented,
                       "Chunked transfer not implemented yet.");
        return -1;
      }
    }
  }
}

// -- utility functions ------------------------------------------------------

void server::write_response(status code, std::string_view content) {
  down_->begin_output();
  v1::write_response(code, "text/plain", content, down_->output_buffer());
  down_->end_output();
}

bool server::invoke_upper_layer(const_byte_span payload) {
  return up_->consume(hdr_, payload) >= 0;
}

bool server::handle_header(std::string_view http) {
  // Parse the header and reject invalid inputs.
  auto [code, msg] = hdr_.parse(http);
  if (code != status::ok) {
    CAF_LOG_DEBUG("received malformed header");
    up_->abort(make_error(sec::protocol_error, "received malformed header"));
    write_response(code, msg);
    return false;
  } else {
    return true;
  }
}

} // namespace caf::net::http
