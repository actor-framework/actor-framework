#include "net-test.hpp"

#include "caf/error.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/ssl/startup.hpp"
#include "caf/net/this_host.hpp"
#include "caf/raise_error.hpp"

#define CAF_TEST_NO_MAIN

#include "caf/test/unit_test_impl.hpp"

using namespace caf;

// -- mock_stream_transport ----------------------------------------------------

bool mock_stream_transport::can_send_more() const noexcept {
  return true;
}

bool mock_stream_transport::is_reading() const noexcept {
  return max_read_size > 0;
}

void mock_stream_transport::close() {
  // nop
}

void mock_stream_transport::configure_read(net::receive_policy policy) {
  min_read_size = policy.min_size;
  max_read_size = policy.max_size;
}

void mock_stream_transport::begin_output() {
  // nop
}

byte_buffer& mock_stream_transport::output_buffer() {
  return output;
}

bool mock_stream_transport::end_output() {
  return true;
}

ptrdiff_t mock_stream_transport::handle_input() {
  ptrdiff_t result = 0;
  while (max_read_size > 0) {
    CAF_ASSERT(max_read_size > static_cast<size_t>(read_size_));
    size_t len = max_read_size - static_cast<size_t>(read_size_);
    CAF_LOG_DEBUG(CAF_ARG2("available capacity:", len));
    auto num_bytes = std::min(input.size(), len);
    if (num_bytes == 0)
      return result;
    auto delta_offset = static_cast<ptrdiff_t>(read_buf_.size());
    read_buf_.insert(read_buf_.end(), input.begin(), input.begin() + num_bytes);
    input.erase(input.begin(), input.begin() + num_bytes);
    read_size_ += static_cast<ptrdiff_t>(num_bytes);
    if (static_cast<size_t>(read_size_) < min_read_size)
      return result;
    auto delta = make_span(read_buf_.data() + delta_offset,
                           read_size_ - delta_offset);
    auto consumed = up->consume(make_span(read_buf_), delta);
    if (consumed > 0) {
      result += static_cast<ptrdiff_t>(consumed);
      read_buf_.erase(read_buf_.begin(), read_buf_.begin() + consumed);
      read_size_ -= consumed;
    } else if (consumed < 0) {
      return -1;
    }
  }
  return result;
}

// -- barrier ------------------------------------------------------------------

void barrier::arrive_and_wait() {
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

// -- main --------------------------------------------------------------------

int main(int argc, char** argv) {
  net::this_host::startup();
  net::ssl::startup();
  using namespace caf;
  net::middleman::init_global_meta_objects();
  core::init_global_meta_objects();
  auto result = test::main(argc, argv);
  net::ssl::cleanup();
  net::this_host::cleanup();
  return result;
}
