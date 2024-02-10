#include "caf/net/lp/framing.hpp"

#include "caf/net/octet_stream/lower_layer.hpp"
#include "caf/net/receive_policy.hpp"

#include "caf/detail/network_order.hpp"
#include "caf/error.hpp"
#include "caf/log/net.hpp"
#include "caf/sec.hpp"

namespace caf::net::lp {

// -- factories ----------------------------------------------------------------

std::unique_ptr<framing> framing::make(upper_layer_ptr up) {
  return std::make_unique<framing>(std::move(up));
}

// -- implementation of octet_stream::upper_layer ------------------------------

error framing::start(octet_stream::lower_layer* down) {
  down_ = down;
  return up_->start(this);
}

void framing::abort(const error& reason) {
  up_->abort(reason);
}

ptrdiff_t framing::consume(byte_span input, byte_span) {
  CAF_LOG_TRACE("got" << input.size() << "bytes\n");
  if (input.size() < sizeof(uint32_t)) {
    log::net::error("received too few bytes from underlying transport");
    up_->abort(make_error(sec::logic_error,
                          "received too few bytes from underlying transport"));
    return -1;
  } else if (input.size() == hdr_size) {
    auto u32_size = uint32_t{0};
    memcpy(&u32_size, input.data(), sizeof(uint32_t));
    auto msg_size = static_cast<size_t>(detail::from_network_order(u32_size));
    if (msg_size == 0) {
      // Ignore empty messages.
      log::net::error("received empty message");
      up_->abort(make_error(sec::logic_error,
                            "received empty buffer from stream layer"));
      return -1;
    } else if (msg_size > max_message_length) {
      log::net::debug("exceeded maximum message size");
      up_->abort(
        make_error(sec::protocol_error, "exceeded maximum message size"));
      return -1;
    } else {
      log::net::debug("wait for payload of size {}", msg_size);
      down_->configure_read(receive_policy::exactly(hdr_size + msg_size));
      return 0;
    }
  } else {
    auto [msg_size, msg] = split(input);
    if (msg_size == msg.size() && msg_size + hdr_size == input.size()) {
      log::net::debug("got message of size {}", msg_size);
      if (up_->consume(msg) >= 0) {
        if (down_->is_reading())
          down_->configure_read(receive_policy::exactly(hdr_size));
        return static_cast<ptrdiff_t>(input.size());
      } else {
        return -1;
      }
    } else {
      log::net::debug("received malformed message");
      up_->abort(make_error(sec::protocol_error, "received malformed message"));
      return -1;
    }
  }
}

void framing::prepare_send() {
  up_->prepare_send();
}

bool framing::done_sending() {
  return up_->done_sending();
}

// -- implementation of lp::lower_layer ----------------------------------------

multiplexer& framing::mpx() noexcept {
  return down_->mpx();
}

bool framing::can_send_more() const noexcept {
  return down_->can_send_more();
}

void framing::suspend_reading() {
  down_->configure_read(receive_policy::stop());
}

bool framing::is_reading() const noexcept {
  return down_->is_reading();
}

void framing::write_later() {
  down_->write_later();
}

void framing::request_messages() {
  if (!down_->is_reading())
    down_->configure_read(receive_policy::exactly(hdr_size));
}

void framing::begin_message() {
  down_->begin_output();
  auto& buf = down_->output_buffer();
  message_offset_ = buf.size();
  buf.insert(buf.end(), 4, std::byte{0});
}

byte_buffer& framing::message_buffer() {
  return down_->output_buffer();
}

bool framing::end_message() {
  using detail::to_network_order;
  auto& buf = down_->output_buffer();
  CAF_ASSERT(message_offset_ < buf.size());
  auto msg_begin = buf.begin() + static_cast<ptrdiff_t>(message_offset_);
  auto msg_size = std::distance(msg_begin + 4, buf.end());
  if (msg_size > 0 && static_cast<size_t>(msg_size) < max_message_length) {
    auto u32_size = to_network_order(static_cast<uint32_t>(msg_size));
    memcpy(std::addressof(*msg_begin), &u32_size, 4);
    down_->end_output();
    return true;
  } else {
    if (msg_size == 0)
      log::net::debug("logic error: message of size 0");
    if (msg_size != 0)
      log::net::debug("maximum message size exceeded");
    return false;
  }
}

void framing::shutdown() {
  down_->shutdown();
}

// -- utility functions ------------------------------------------------------

std::pair<size_t, byte_span> framing::split(byte_span buffer) noexcept {
  CAF_ASSERT(buffer.size() >= sizeof(uint32_t));
  auto u32_size = uint32_t{0};
  memcpy(&u32_size, buffer.data(), sizeof(uint32_t));
  auto msg_size = static_cast<size_t>(detail::from_network_order(u32_size));
  return std::make_pair(msg_size, buffer.subspan(sizeof(uint32_t)));
}

} // namespace caf::net::lp
