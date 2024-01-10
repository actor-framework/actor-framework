// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/datagram_transport.hpp"

namespace {

// Name of the component for logging.
static constexpr std::string_view component = "datagram_transport";

} // namespace

using namespace std::literals;

namespace caf::net {

error datagram_transport::start(socket_manager* owner) {
  auto tg = logger::trace(component, "Starting");
  parent_ = owner;
  parent_->register_reading();
  self->set_behavior(
    [this](ip_endpoint dest, std::string& line) {
      log::net::debug("queueing message of length {} to {}:{}", line,
                      dest.address(), dest.port());
      dest_ = dest;
      parent_->register_writing();
      std::transform(line.begin(), line.end(), std::back_inserter(write_buf_),
                     [](char c) { return static_cast<std::byte>(c); });
    },
    [this](const exit_msg&) { //
      parent_->shutdown();
    });
  self->set_fallback(
    [](net::abstract_actor_shell*, message& msg) -> result<message> {
      log::net::error("received unexpected message {}", msg);
      return make_error(sec::unexpected_message);
    });
  return none;
}

socket datagram_transport::handle() const {
  return handle_;
}

void datagram_transport::handle_read_event() {
  auto tg = logger::trace(component, "Read for socket {}", handle_.id);
  if (read_buf_.size() < max_read_size_)
    read_buf_.resize(max_read_size_);
  // Fill up our buffer.
  ip_endpoint source;
  auto rd = read(handle_, read_buf_, &source);
  auto result = check_datagram_socket_io_res(rd);
  if (std::holds_alternative<sec>(result)) {
    auto errc = std::get<sec>(result);
    if (errc == sec::unavailable_or_would_block)
      log::net::error("Read operation failed with {}", errc);
    else
      abort(make_error(errc, "Socket read operation failed"));
    return;
  }
  auto received = make_span(read_buf_).subspan(0, std::get<size_t>(result));
  log::net::info("Received {} bytes on socket {}", received.size(), handle_.id);
  if (is_valid_utf8(received)) {
    auto str = std::string{reinterpret_cast<const char*>(received.data()),
                           received.size()};
    self->send(worker_, source, str);
  } else {
    std::string hex;
    detail::append_hex(hex, received.data(), received.size());
    self->send(worker_, source, hex);
  }
  read_buf_.clear();
}

void datagram_transport::handle_write_event() {
  auto tg = logger::trace(component, "Write for socket {}", handle_.id);
  auto rd = write(handle_, write_buf_, dest_);
  auto result = check_datagram_socket_io_res(rd);
  if (std::holds_alternative<sec>(result)) {
    auto errc = std::get<sec>(result);
    if (errc == sec::unavailable_or_would_block)
      log::net::error("Write operation failed with {}", errc);
    else
      abort(make_error(errc, "Socket write operation failed"));
    return;
  }
  write_buf_.clear();
  parent_->deregister_writing();
}

void datagram_transport::abort(const error& error) {
  log::net::debug("Aboring with error: {}", to_string(error));
  parent_->shutdown();
}

} // namespace caf::net
