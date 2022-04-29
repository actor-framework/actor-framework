#include "caf/net/length_prefix_framing.hpp"

#include "caf/detail/network_order.hpp"
#include "caf/error.hpp"
#include "caf/logger.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/sec.hpp"

namespace caf::net {

// -- constructors, destructors, and assignment operators ----------------------

length_prefix_framing::length_prefix_framing(upper_layer_ptr up)
  : up_(std::move(up)) {
  // nop
}

// -- factories ----------------------------------------------------------------

std::unique_ptr<length_prefix_framing>
length_prefix_framing::make(upper_layer_ptr up) {
  return std::make_unique<length_prefix_framing>(std::move(up));
}

// -- implementation of stream_oriented::upper_layer ---------------------------

error length_prefix_framing::init(socket_manager* owner,
                                  stream_oriented::lower_layer* down,
                                  const settings& cfg) {
  down_ = down;
  return up_->init(owner, this, cfg);
}

void length_prefix_framing::abort(const error& reason) {
  up_->abort(reason);
}

ptrdiff_t length_prefix_framing::consume(byte_span input, byte_span) {
  CAF_LOG_TRACE("got" << input.size() << "bytes\n");
  if (input.size() < sizeof(uint32_t)) {
    CAF_LOG_ERROR("received too few bytes from underlying transport");
    up_->abort(make_error(sec::logic_error,
                          "received too few bytes from underlying transport"));
    return -1;
  } else if (input.size() == hdr_size) {
    auto u32_size = uint32_t{0};
    memcpy(&u32_size, input.data(), sizeof(uint32_t));
    auto msg_size = static_cast<size_t>(detail::from_network_order(u32_size));
    if (msg_size == 0) {
      // Ignore empty messages.
      CAF_LOG_DEBUG("received empty message");
      return static_cast<ptrdiff_t>(input.size());
    } else if (msg_size > max_message_length) {
      CAF_LOG_DEBUG("exceeded maximum message size");
      up_->abort(
        make_error(sec::protocol_error, "exceeded maximum message size"));
      return -1;
    } else {
      CAF_LOG_DEBUG("wait for payload of size" << msg_size);
      down_->configure_read(receive_policy::exactly(hdr_size + msg_size));
      return 0;
    }
  } else {
    auto [msg_size, msg] = split(input);
    if (msg_size == msg.size() && msg_size + hdr_size == input.size()) {
      CAF_LOG_DEBUG("got message of size" << msg_size);
      if (up_->consume(msg) >= 0) {
        if (down_->is_reading())
          down_->configure_read(receive_policy::exactly(hdr_size));
        return static_cast<ptrdiff_t>(input.size());
      } else {
        return -1;
      }
    } else {
      CAF_LOG_DEBUG("received malformed message");
      up_->abort(make_error(sec::protocol_error, "received malformed message"));
      return -1;
    }
  }
}

bool length_prefix_framing::prepare_send() {
  return up_->prepare_send();
}

bool length_prefix_framing::done_sending() {
  return up_->done_sending();
}

// -- implementation of message_oriented::lower_layer --------------------------

bool length_prefix_framing::can_send_more() const noexcept {
  return down_->can_send_more();
}

void length_prefix_framing::suspend_reading() {
  down_->configure_read(receive_policy::stop());
}

bool length_prefix_framing::is_reading() const noexcept {
  return down_->is_reading();
}

void length_prefix_framing::request_messages() {
  if (!down_->is_reading())
    down_->configure_read(receive_policy::exactly(hdr_size));
}

void length_prefix_framing::begin_message() {
  down_->begin_output();
  auto& buf = down_->output_buffer();
  message_offset_ = buf.size();
  buf.insert(buf.end(), 4, std::byte{0});
}

byte_buffer& length_prefix_framing::message_buffer() {
  return down_->output_buffer();
}

bool length_prefix_framing::end_message() {
  using detail::to_network_order;
  auto& buf = down_->output_buffer();
  CAF_ASSERT(message_offset_ < buf.size());
  auto msg_begin = buf.begin() + message_offset_;
  auto msg_size = std::distance(msg_begin + 4, buf.end());
  if (msg_size > 0 && static_cast<size_t>(msg_size) < max_message_length) {
    auto u32_size = to_network_order(static_cast<uint32_t>(msg_size));
    memcpy(std::addressof(*msg_begin), &u32_size, 4);
    down_->end_output();
    return true;
  } else {
    CAF_LOG_DEBUG_IF(msg_size == 0, "logic error: message of size 0");
    CAF_LOG_DEBUG_IF(msg_size != 0, "maximum message size exceeded");
    return false;
  }
}

void length_prefix_framing::close() {
  down_->close();
}

// -- utility functions ------------------------------------------------------

std::pair<size_t, byte_span>
length_prefix_framing::split(byte_span buffer) noexcept {
  CAF_ASSERT(buffer.size() >= sizeof(uint32_t));
  auto u32_size = uint32_t{0};
  memcpy(&u32_size, buffer.data(), sizeof(uint32_t));
  auto msg_size = static_cast<size_t>(detail::from_network_order(u32_size));
  return std::make_pair(msg_size, buffer.subspan(sizeof(uint32_t)));
}

} // namespace caf::net
