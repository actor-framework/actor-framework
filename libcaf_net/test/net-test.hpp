#pragma once

#include "caf/error.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/stream_oriented.hpp"
#include "caf/settings.hpp"
#include "caf/span.hpp"
#include "caf/string_view.hpp"
#include "caf/test/bdd_dsl.hpp"

class mock_stream_transport : public caf::net::stream_oriented::lower_layer {
public:
  // -- member types -----------------------------------------------------------

  using upper_layer_ptr
    = std::unique_ptr<caf::net::stream_oriented::upper_layer>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit mock_stream_transport(upper_layer_ptr ptr) : up(std::move(ptr)) {
    // nop
  }

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<mock_stream_transport> make(upper_layer_ptr ptr) {
    return std::make_unique<mock_stream_transport>(std::move(ptr));
  }

  // -- implementation of stream_oriented::lower_layer -------------------------

  bool can_send_more() const noexcept override;

  bool is_reading() const noexcept override;

  void write_later() override;

  void shutdown() override;

  void configure_read(caf::net::receive_policy policy) override;

  void begin_output() override;

  caf::byte_buffer& output_buffer() override;

  bool end_output() override;

  // -- initialization ---------------------------------------------------------

  caf::error start(const caf::settings& cfg) {
    return up->start(this, cfg);
  }

  caf::error start() {
    caf::settings cfg;
    return start(cfg);
  }

  // -- buffer management ------------------------------------------------------

  void push(caf::span<const std::byte> bytes) {
    input.insert(input.begin(), bytes.begin(), bytes.end());
  }

  void push(std::string_view str) {
    push(caf::as_bytes(caf::make_span(str)));
  }

  size_t unconsumed() const noexcept {
    return read_buf_.size();
  }

  std::string_view output_as_str() const noexcept {
    return {reinterpret_cast<const char*>(output.data()), output.size()};
  }

  // -- event callbacks --------------------------------------------------------

  ptrdiff_t handle_input();

  // -- member variables -------------------------------------------------------

  upper_layer_ptr up;

  caf::byte_buffer output;

  caf::byte_buffer input;

  uint32_t min_read_size = 0;

  uint32_t max_read_size = 0;

private:
  caf::byte_buffer read_buf_;

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

  void arrive_and_wait();

private:
  ptrdiff_t num_threads_;
  std::mutex mx_;
  std::atomic<ptrdiff_t> count_;
  std::condition_variable cv_;
};
