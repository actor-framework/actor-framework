#pragma once

#include "caf/error.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/test/host_fixture.hpp"
#include "caf/settings.hpp"
#include "caf/span.hpp"
#include "caf/string_view.hpp"
#include "caf/tag/stream_oriented.hpp"
#include "caf/test/dsl.hpp"

template <class UpperLayer>
class mock_stream_transport {
public:
  // -- member types -----------------------------------------------------------

  using output_tag = caf::tag::stream_oriented;

  // -- interface for the upper layer ------------------------------------------

  class access {
  public:
    explicit access(mock_stream_transport* transport) : transport_(transport) {
      // nop
    }

    void begin_output() {
      // nop
    }

    auto& output_buffer() {
      return transport_->output;
    }

    constexpr void end_output() {
      // nop
    }

    bool can_send_more() const noexcept {
      return true;
    }

    void abort_reason(caf::error reason) {
      transport_->abort_reason = std::move(reason);
    }

    void configure_read(caf::net::receive_policy policy) {
      transport_->min_read_size = policy.min_size;
      transport_->max_read_size = policy.max_size;
    }

  private:
    mock_stream_transport* transport_;
  };

  friend class access;

  // -- initialization ---------------------------------------------------------

  caf::error init(const caf::settings& config) {
    access this_layer{this};
    return upper_layer.init(nullptr, this_layer, config);
  }

  caf::error init() {
    caf::settings config;
    return init(config);
  }

  // -- buffer management ------------------------------------------------------

  void push(caf::span<const caf::byte> bytes) {
    input.insert(input.begin(), bytes.begin(), bytes.end());
  }

  void push(caf::string_view str) {
    push(caf::as_bytes(caf::make_span(str)));
  }

  size_t unconsumed() const noexcept {
    return read_buf_.size();
  }

  caf::string_view output_as_str() const noexcept {
    return {reinterpret_cast<const char*>(output.data()), output.size()};
  }

  // -- event callbacks --------------------------------------------------------

  ptrdiff_t handle_input() {
    ptrdiff_t result = 0;
    access this_layer{this};
    while (max_read_size > 0) {
      CAF_ASSERT(max_read_size > static_cast<size_t>(read_size_));
      size_t len = max_read_size - static_cast<size_t>(read_size_);
      CAF_LOG_DEBUG(CAF_ARG2("available capacity:", len));
      auto num_bytes = std::min(input.size(), len);
      if (num_bytes == 0)
        return result;
      auto delta_offset = static_cast<ptrdiff_t>(read_buf_.size());
      read_buf_.insert(read_buf_.end(), input.begin(),
                       input.begin() + num_bytes);
      input.erase(input.begin(), input.begin() + num_bytes);
      read_size_ += static_cast<ptrdiff_t>(num_bytes);
      if (static_cast<size_t>(read_size_) < min_read_size)
        return result;
      auto delta = make_span(read_buf_.data() + delta_offset,
                             read_size_ - delta_offset);
      auto consumed = upper_layer.consume(this_layer, caf::make_span(read_buf_),
                                          delta);
      if (consumed > 0) {
        result += static_cast<ptrdiff_t>(consumed);
        read_buf_.erase(read_buf_.begin(), read_buf_.begin() + consumed);
        read_size_ -= consumed;
      } else if (consumed < 0) {
        if (!abort_reason)
          abort_reason = caf::sec::runtime_error;
        upper_layer.abort(this_layer, abort_reason);
        return -1;
      }
    }
    return result;
  }

  // -- member variables -------------------------------------------------------

  caf::error abort_reason;

  UpperLayer upper_layer;

  std::vector<caf::byte> output;

  std::vector<caf::byte> input;

  uint32_t min_read_size = 0;

  uint32_t max_read_size = 0;

private:
  std::vector<caf::byte> read_buf_;

  ptrdiff_t read_size_ = 0;
};
