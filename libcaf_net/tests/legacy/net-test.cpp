#include "net-test.hpp"

#include "caf/net/middleman.hpp"
#include "caf/net/ssl/startup.hpp"
#include "caf/net/this_host.hpp"

#include "caf/error.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/raise_error.hpp"

#define CAF_TEST_NO_MAIN

#include "caf/test/unit_test_impl.hpp"

using namespace caf;

// -- mock_stream_transport ----------------------------------------------------

net::multiplexer& mock_stream_transport::mpx() noexcept {
  return *mpx_;
}

bool mock_stream_transport::can_send_more() const noexcept {
  return true;
}

bool mock_stream_transport::is_reading() const noexcept {
  return max_read_size > 0;
}

void mock_stream_transport::write_later() {
  // nop
}

void mock_stream_transport::shutdown() {
  // nop
}

void mock_stream_transport::switch_protocol(upper_layer_ptr new_up) {
  next.swap(new_up);
}

bool mock_stream_transport::switching_protocol() const noexcept {
  return next != nullptr;
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
  auto switch_to_next_protocol = [this] {
    assert(next);
    // Switch to the new protocol and initialize it.
    configure_read(net::receive_policy::stop());
    up.reset(next.release());
    if (auto err = up->start(this)) {
      up.reset();
      return false;
    }
    return true;
  };
  // Loop until we have drained the buffer as much as we can.
  while (max_read_size > 0 && input.size() >= min_read_size) {
    auto n = std::min(input.size(), size_t{max_read_size});
    auto bytes = make_span(input.data(), n);
    auto delta = bytes.subspan(delta_offset);
    auto consumed = up->consume(bytes, delta);
    if (consumed < 0) {
      // Negative values indicate that the application encountered an
      // unrecoverable error.
      return result;
    } else if (static_cast<size_t>(consumed) > n) {
      // Must not happen. An application cannot handle more data then we pass
      // to it.
      up->abort(make_error(sec::logic_error, "consumed > buffer.size"));
      return result;
    } else if (consumed == 0) {
      if (next) {
        // When switching protocol, the new layer has never seen the data, so we
        // might just re-invoke the same data again.
        if (!switch_to_next_protocol())
          return -1;
      } else {
        // See whether the next iteration would change what we pass to the
        // application (max_read_size_ may have changed). Otherwise, we'll try
        // again later.
        delta_offset = static_cast<ptrdiff_t>(n);
        if (n == std::min(input.size(), size_t{max_read_size})) {
          return result;
        }
        // else: "Fall through".
      }
    } else {
      if (next && !switch_to_next_protocol())
        return -1;
      // Shove the unread bytes to the beginning of the buffer and continue
      // to the next loop iteration.
      result += consumed;
      delta_offset = 0;
      input.erase(input.begin(), input.begin() + consumed);
    }
  }
  return result;
}

// -- mock_web_socket_app ------------------------------------------------------

mock_web_socket_app::mock_web_socket_app(bool request_messages_on_start)
  : request_messages_on_start(request_messages_on_start) {
  // nop
}

caf::error mock_web_socket_app::start(caf::net::web_socket::lower_layer* ll) {
  down = ll;
  if (request_messages_on_start)
    down->request_messages();
  return caf::none;
}

void mock_web_socket_app::prepare_send() {
  // nop
}

bool mock_web_socket_app::done_sending() {
  return true;
}

caf::error
mock_web_socket_app::accept(const caf::net::http::request_header& hdr) {
  // Store the request information in cfg to evaluate them later.
  auto& ws = cfg["web-socket"].as_dictionary();
  put(ws, "method", to_rfc_string(hdr.method()));
  put(ws, "path", std::string{hdr.path()});
  put(ws, "query", hdr.query());
  put(ws, "fragment", hdr.fragment());
  put(ws, "http-version", hdr.version());
  if (hdr.num_fields() > 0) {
    auto& fields = ws["fields"].as_dictionary();
    hdr.for_each_field([&fields](auto key, auto val) {
      put(fields, std::string{key}, std::string{val});
    });
  }
  return caf::none;
}

void mock_web_socket_app::abort(const caf::error& reason) {
  abort_reason = reason;
}

ptrdiff_t mock_web_socket_app::consume_text(std::string_view text) {
  text_input.insert(text_input.end(), text.begin(), text.end());
  return static_cast<ptrdiff_t>(text.size());
}

ptrdiff_t mock_web_socket_app::consume_binary(caf::byte_span bytes) {
  binary_input.insert(binary_input.end(), bytes.begin(), bytes.end());
  return static_cast<ptrdiff_t>(bytes.size());
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
