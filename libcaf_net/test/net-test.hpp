#pragma once

#include "caf/error.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/test/host_fixture.hpp"
#include "caf/settings.hpp"
#include "caf/span.hpp"
#include "caf/string_view.hpp"
#include "caf/tag/stream_oriented.hpp"
#include "caf/test/bdd_dsl.hpp"

template <class UpperLayer>
class mock_stream_transport {
public:
  // -- member types -----------------------------------------------------------

  using output_tag = caf::tag::stream_oriented;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit mock_stream_transport(Ts&&... xs)
    : upper_layer(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- interface for the upper layer ------------------------------------------

  void begin_output() {
    // nop
  }

  auto& output_buffer() {
    return output;
  }

  constexpr void end_output() {
    // nop
  }

  constexpr caf::net::socket handle() noexcept {
    return caf::net::invalid_socket;
  }

  bool can_send_more() const noexcept {
    return true;
  }

  const caf::error& abort_reason() {
    return abort_reason_;
  }

  void abort_reason(caf::error reason) {
    abort_reason_ = std::move(reason);
  }

  bool stopped() const noexcept {
    return max_read_size == 0;
  }

  void configure_read(caf::net::receive_policy policy) {
    min_read_size = policy.min_size;
    max_read_size = policy.max_size;
  }

  // -- initialization ---------------------------------------------------------

  caf::error init(const caf::settings& config) {
    return upper_layer.init(nullptr, this, config);
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
      auto consumed = upper_layer.consume(this, caf::make_span(read_buf_),
                                          delta);
      if (consumed > 0) {
        result += static_cast<ptrdiff_t>(consumed);
        read_buf_.erase(read_buf_.begin(), read_buf_.begin() + consumed);
        read_size_ -= consumed;
      } else if (consumed < 0) {
        if (!abort_reason_)
          abort_reason_ = caf::sec::runtime_error;
        upper_layer.abort(this, abort_reason_);
        return -1;
      }
    }
    return result;
  }

  // -- member variables -------------------------------------------------------

  UpperLayer upper_layer;

  std::vector<caf::byte> output;

  std::vector<caf::byte> input;

  uint32_t min_read_size = 0;

  uint32_t max_read_size = 0;

private:
  std::vector<caf::byte> read_buf_;

  ptrdiff_t read_size_ = 0;

  caf::error abort_reason_;
};

// Drop-in replacement for std::barrier (based on the TS API as of 2020).
class barrier {
public:
  explicit barrier(ptrdiff_t num_threads)
    : num_threads_(num_threads), count_(0) {
    // nop
  }

  void arrive_and_wait() {
    std::unique_lock<std::mutex> guard{mx_};
    auto new_count = ++count_;
    if (new_count == num_threads_) {
      cv_.notify_all();
    } else if (new_count > num_threads_) {
      count_ = 1;
      cv_.wait(guard, [this] { return count_.load() == num_threads_; });
    } else {
      cv_.wait(guard, [this] { return count_.load() == num_threads_; });
    }
  }

private:
  ptrdiff_t num_threads_;
  std::mutex mx_;
  std::atomic<ptrdiff_t> count_;
  std::condition_variable cv_;
};
