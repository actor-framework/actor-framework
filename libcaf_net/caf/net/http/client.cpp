#include "caf/net/http/client.hpp"

#include "caf/net/octet_stream/lower_layer.hpp"

#include "caf/detail/format.hpp"
#include "caf/log/net.hpp"

namespace caf::net::http {

// -- factories ----------------------------------------------------------------

std::unique_ptr<client> client::make(upper_layer_ptr up) {
  return std::make_unique<client>(std::move(up));
}

// -- http::lower_layer implementation -----------------------------------------

multiplexer& client::mpx() noexcept {
  return down_->mpx();
}

bool client::can_send_more() const noexcept {
  return down_->can_send_more();
}

bool client::is_reading() const noexcept {
  return down_->is_reading();
}

void client::write_later() {
  down_->write_later();
}

void client::shutdown() {
  down_->shutdown();
}

void client::request_messages() {
  if (!down_->is_reading())
    down_->configure_read(receive_policy::up_to(max_response_size_));
}

void client::suspend_reading() {
  down_->configure_read(receive_policy::stop());
}

void client::begin_header(http::method method, std::string_view path) {
  down_->begin_output();
  v1::begin_request_header(method, path, down_->output_buffer());
}

void client::add_header_field(std::string_view key, std::string_view val) {
  v1::add_header_field(key, val, down_->output_buffer());
}

bool client::end_header() {
  return v1::end_header(down_->output_buffer()) && down_->end_output();
}

bool client::send_payload(const_byte_span bytes) {
  down_->begin_output();
  auto& buf = down_->output_buffer();
  buf.insert(buf.end(), bytes.begin(), bytes.end());
  down_->end_output();
  return true;
}

bool client::send_chunk(const_byte_span bytes) {
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

bool client::send_end_of_chunks() {
  std::string_view str = "0\r\n\r\n";
  auto bytes = as_bytes(make_span(str));
  down_->begin_output();
  auto& buf = down_->output_buffer();
  buf.insert(buf.end(), bytes.begin(), bytes.end());
  return down_->end_output();
}

void client::switch_protocol(std::unique_ptr<octet_stream::upper_layer> next) {
  down_->switch_protocol(std::move(next));
}

// -- octet_stream::upper_layer implementation ---------------------------------

error client::start(octet_stream::lower_layer* down) {
  down_ = down;
  return up_->start(this);
}

void client::abort(const error& reason) {
  up_->abort(reason);
}

void client::prepare_send() {
  up_->prepare_send();
}

bool client::done_sending() {
  return up_->done_sending();
}

ptrdiff_t client::consume(byte_span input, byte_span) {
  CAF_LOG_TRACE(CAF_ARG2("bytes", input.size()));
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

// -- utility functions ------------------------------------------------------

bool client::invoke_upper_layer(const_byte_span payload) {
  return up_->consume(hdr_, payload) >= 0;
}

bool client::handle_header(std::string_view http) {
  // Parse the header and reject invalid inputs.
  auto [code, msg] = hdr_.parse(http);
  if (code != status::ok) {
    log::net::debug("received malformed header");
    abort(msg);
    return false;
  }
  return true;
}

} // namespace caf::net::http
