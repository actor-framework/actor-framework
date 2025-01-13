#include "caf/net/lp/framing.hpp"

#include "caf/net/lp/upper_layer.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/lower_layer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/async/spsc_buffer.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/error.hpp"
#include "caf/internal/lp_flow_bridge.hpp"
#include "caf/internal/make_transport.hpp"
#include "caf/log/net.hpp"
#include "caf/sec.hpp"

#include <cstdint>
#include <cstring>
#include <memory>

namespace caf::net::lp {

namespace {

template <class T>
std::pair<size_t, byte_span> split(byte_span buffer) noexcept {
  CAF_ASSERT(buffer.size() >= sizeof(T));
  auto t_size = T{0};
  memcpy(&t_size, buffer.data(), sizeof(T));
  auto msg_size = size_t{0};
  if constexpr (std::is_same_v<T, uint8_t>)
    msg_size = static_cast<size_t>(t_size);
  else
    msg_size = static_cast<size_t>(detail::from_network_order(t_size));
  return std::make_pair(msg_size, buffer.subspan(sizeof(T)));
}

class framing_impl : public framing {
public:
  // -- member types -----------------------------------------------------------

  using upper_layer_ptr = std::unique_ptr<lp::upper_layer>;

  // -- constants --------------------------------------------------------------

  static constexpr size_t max_message_length = INT64_MAX - sizeof(uint64_t);

  // -- constructors, destructors, and assignment operators --------------------

  framing_impl(upper_layer_ptr up, dsl::size_field_type lp_size)
    : up_(std::move(up)), lp_size_(lp_size) {
    switch (lp_size_) {
      case dsl::size_field_type::u1:
        hdr_size_ = sizeof(uint8_t);
        break;
      case dsl::size_field_type::u2:
        hdr_size_ = sizeof(uint16_t);
        break;
      case dsl::size_field_type::u4:
        hdr_size_ = sizeof(uint32_t);
        break;
      case dsl::size_field_type::u8:
        hdr_size_ = sizeof(uint64_t);
        break;
    }
  }

  // -- implementation of octet_stream::upper_layer ----------------------------

  error start(octet_stream::lower_layer* down) override {
    down_ = down;
    return up_->start(this);
  }

  void abort(const error& reason) override {
    up_->abort(reason);
  }

  ptrdiff_t consume(byte_span input, byte_span) override {
    switch (lp_size_) {
      case dsl::size_field_type::u1:
        return consume_impl<uint8_t>(input, {});
      case dsl::size_field_type::u2:
        return consume_impl<uint16_t>(input, {});
      case dsl::size_field_type::u4:
        return consume_impl<uint32_t>(input, {});
      case dsl::size_field_type::u8:
        return consume_impl<uint64_t>(input, {});
    }
    log::net::error("invalid size field type");
    up_->abort(make_error(sec::logic_error, "invalid size field type"));
    return -1;
  }

  void prepare_send() override {
    up_->prepare_send();
  }

  bool done_sending() override {
    return up_->done_sending();
  }

  // -- implementation of lp::lower_layer --------------------------------------

  socket_manager* manager() noexcept override {
    return down_->manager();
  }

  bool can_send_more() const noexcept override {
    return down_->can_send_more();
  }

  void suspend_reading() override {
    down_->configure_read(receive_policy::stop());
  }

  bool is_reading() const noexcept override {
    return down_->is_reading();
  }

  void write_later() override {
    down_->write_later();
  }

  void request_messages() override {
    if (!down_->is_reading())
      down_->configure_read(receive_policy::exactly(hdr_size_));
  }

  void begin_message() override {
    down_->begin_output();
    auto& buf = down_->output_buffer();
    message_offset_ = buf.size();
    buf.insert(buf.end(), hdr_size_, std::byte{0});
  }

  byte_buffer& message_buffer() override {
    return down_->output_buffer();
  }

  bool end_message() {
    switch (lp_size_) {
      case dsl::size_field_type::u1:
        return end_message_impl<uint8_t>();
      case dsl::size_field_type::u2:
        return end_message_impl<uint16_t>();
      case dsl::size_field_type::u4:
        return end_message_impl<uint32_t>();
      case dsl::size_field_type::u8:
        return end_message_impl<uint64_t>();
    }
    log::net::error("invalid size field type");
    up_->abort(make_error(sec::logic_error, "invalid size field type"));
    return false;
  }

  void shutdown() override {
    down_->shutdown();
  }

private:
  template <class T>
  ptrdiff_t consume_impl(byte_span input, byte_span) {
    auto lg = log::net::trace("got {} bytes\n", input.size());
    if (input.size() < sizeof(T)) {
      log::net::error("received too few bytes from underlying transport");
      up_->abort(make_error(
        sec::logic_error, "received too few bytes from underlying transport"));
      return -1;
    } else if (input.size() == hdr_size_) {
      auto t_size = T{0};
      memcpy(&t_size, input.data(), sizeof(T));
      auto msg_size = size_t{0};
      if constexpr (std::is_same_v<T, uint8_t>)
        msg_size = static_cast<size_t>(t_size);
      else
        msg_size = static_cast<size_t>(detail::from_network_order(t_size));
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
        down_->configure_read(receive_policy::exactly(hdr_size_ + msg_size));
        return 0;
      }
    } else {
      auto [msg_size, msg] = split<T>(input);
      if (msg_size == msg.size() && msg_size + hdr_size_ == input.size()) {
        log::net::debug("got message of size {}", msg_size);
        if (up_->consume(msg) >= 0) {
          if (down_->is_reading())
            down_->configure_read(receive_policy::exactly(hdr_size_));
          return static_cast<ptrdiff_t>(input.size());
        } else {
          return -1;
        }
      } else {
        log::net::debug("received malformed message");
        up_->abort(
          make_error(sec::protocol_error, "received malformed message"));
        return -1;
      }
    }
  }

  template <class T>
  bool end_message_impl() {
    using detail::to_network_order;
    auto& buf = down_->output_buffer();
    CAF_ASSERT(message_offset_ < buf.size());
    auto msg_begin = buf.begin() + static_cast<ptrdiff_t>(message_offset_);
    auto msg_size = std::distance(msg_begin + hdr_size_, buf.end());
    if (msg_size > 0 && static_cast<size_t>(msg_size) < max_message_length) {
      auto t_size = T{0};
      if constexpr (std::is_same_v<T, uint8_t>)
        t_size = static_cast<uint8_t>(msg_size);
      else
        t_size = to_network_order(static_cast<T>(msg_size));
      memcpy(std::addressof(*msg_begin), &t_size, hdr_size_);
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

  // -- member variables -------------------------------------------------------

  octet_stream::lower_layer* down_;

  upper_layer_ptr up_;

  dsl::size_field_type lp_size_;

  size_t message_offset_ = 0;

  size_t hdr_size_ = 0;
};

} // namespace

// -- factories ----------------------------------------------------------------

std::unique_ptr<framing> framing::make(upper_layer_ptr up,
                                       dsl::size_field_type lp_size) {
  return std::make_unique<framing_impl>(std::move(up), lp_size);
}

namespace {

template <class Conn>
disposable run_impl(multiplexer& mpx, Conn& conn,
                    async::consumer_resource<chunk>& pull,
                    async::producer_resource<chunk>& push) {
  auto bridge = internal::make_lp_flow_bridge(std::move(pull), std::move(push));
  auto transport = internal::make_transport(
    std::move(conn),
    framing::make(std::move(bridge), dsl::size_field_type::u2));
  auto manager = net::socket_manager::make(&mpx, std::move(transport));
  if (mpx.start(manager))
    return manager->as_disposable();
  return disposable{};
}

} // namespace

disposable framing::run(multiplexer& mpx, stream_socket fd,
                        async::consumer_resource<chunk> pull,
                        async::producer_resource<chunk> push) {
  return run_impl(mpx, fd, pull, push);
}

disposable framing::run(multiplexer& mpx, ssl::connection conn,
                        async::consumer_resource<chunk> pull,
                        async::producer_resource<chunk> push) {
  return run_impl(mpx, conn, pull, push);
}

} // namespace caf::net::lp
