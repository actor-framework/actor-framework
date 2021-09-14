// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.consumer_adapter

#include "caf/net/consumer_adapter.hpp"

#include "net-test.hpp"

#include "caf/async/bounded_buffer.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/scheduled_actor/flow.hpp"

using namespace caf;

namespace {

class reader {
public:
  reader(net::stream_socket fd, size_t n) : sg_(fd) {
    buf_.resize(n);
  }

  auto fd() {
    return sg_.socket();
  }

  void read_some() {
    if (rd_pos_ < buf_.size()) {
      auto res = read(fd(), make_span(buf_).subspan(rd_pos_));
      if (res > 0) {
        rd_pos_ += static_cast<size_t>(res);
        MESSAGE(rd_pos_ << " bytes received");
      } else if (res < 0 && !net::last_socket_error_is_temporary()) {
        FAIL("failed to read: " << net::last_socket_error_as_string());
      }
    }
  }

  size_t remaining() const noexcept {
    return buf_.size() - rd_pos_;
  }

  bool done() const noexcept {
    return remaining() == 0;
  }

  auto& buf() const {
    return buf_;
  }

private:
  size_t rd_pos_ = 0;
  std::vector<byte> buf_;
  net::socket_guard<net::stream_socket> sg_;
};

class app_t {
public:
  using input_tag = tag::stream_oriented;

  using resource_type = async::consumer_resource<int32_t>;

  using buffer_type = resource_type::buffer_type;

  using adapter_ptr = net::consumer_adapter_ptr<buffer_type>;

  using adapter_type = adapter_ptr::element_type;

  explicit app_t(resource_type input) : input_(std::move(input)) {
    // nop
  }

  template <class LowerLayerPtr>
  error init(net::socket_manager* mgr, LowerLayerPtr, const settings&) {
    if (auto ptr = adapter_type::try_open(mgr, std::move(input_))) {
      adapter_ = std::move(ptr);
      return none;
    } else {
      FAIL("unable to open the resource");
    }
  }

  template <class LowerLayerPtr>
  bool prepare_send(LowerLayerPtr down) {
    bool run = !done_;
    while (run && down->can_send_more()) {
      bool on_next_called = false;
      auto fin = adapter_->consume(
        async::ignore_errors, 1,
        [this, down, &on_next_called](span<const int32_t> items) {
          REQUIRE_EQ(items.size(), 1u);
          auto val = items[0];
          written_values_.emplace_back(val);
          auto offset = written_bytes_.size();
          binary_serializer sink{nullptr, written_bytes_};
          if (!sink.apply(val))
            FAIL("sink.apply failed: " << sink.get_error());
          auto bytes = make_span(written_bytes_).subspan(offset);
          down->begin_output();
          auto& buf = down->output_buffer();
          buf.insert(buf.end(), bytes.begin(), bytes.end());
          down->end_output();
          on_next_called = true;
        });
      if (fin) {
        MESSAGE("adapter signaled end-of-buffer");
        done_ = true;
      }
      run = !done_ && on_next_called;
    }
    MESSAGE(written_bytes_.size() << " bytes written");
    return true;
  }

  template <class LowerLayerPtr>
  bool done_sending(LowerLayerPtr) {
    return done_ || !adapter_->has_data();
  }

  template <class LowerLayerPtr>
  void continue_reading(LowerLayerPtr) {
    CAF_FAIL("continue_reading called");
  }

  template <class LowerLayerPtr>
  void abort(LowerLayerPtr, const error& reason) {
    FAIL("app::abort called: " << reason);
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume(LowerLayerPtr, byte_span, byte_span) {
    FAIL("app::consume called: unexpected data");
  }

  auto& written_values() {
    return written_values_;
  }

  auto& written_bytes() {
    return written_bytes_;
  }

private:
  bool done_ = false;
  std::vector<int32_t> written_values_;
  std::vector<byte> written_bytes_;
  adapter_ptr adapter_;
  resource_type input_;
};

struct fixture : test_coordinator_fixture<>, host_fixture {
  fixture() : mm(sys) {
    mm.mpx().set_thread_id();
    if (auto err = mm.mpx().init())
      CAF_FAIL("mpx.init() failed: " << err);
  }

  bool handle_io_event() override {
    return mm.mpx().poll_once(false);
  }

  net::middleman mm;
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("subscriber adapters wake up idle socket managers") {
  GIVEN("an actor pushing into a buffer resource") {
    static constexpr size_t num_items = 79;
    auto [rd, wr] = async::make_bounded_buffer_resource<int32_t>(8, 2);
    sys.spawn([wr{wr}](event_based_actor* self) {
      self->make_observable().repeat(42).take(num_items).subscribe(wr);
    });
    WHEN("draining the buffer resource and sending its items over a socket") {
      auto [fd1, fd2] = unbox(net::make_stream_socket_pair());
      if (auto err = nonblocking(fd1, true))
        FAIL("nonblocking(fd1) returned an error: " << err);
      if (auto err = nonblocking(fd2, true))
        FAIL("nonblocking(fd2) returned an error: " << err);
      auto mgr = net::make_socket_manager<app_t, net::stream_transport>(
        fd1, mm.mpx_ptr(), std::move(rd));
      auto& app = mgr->top_layer();
      if (auto err = mgr->init(content(cfg)))
        FAIL("mgr->init() failed: " << err);
      THEN("the reader receives all items before the connection closes") {
        auto remaining = num_items * sizeof(int32_t);
        reader rd{fd2, remaining};
        while (!rd.done()) {
          if (auto new_val = rd.remaining(); remaining != new_val) {
            remaining = new_val;
            MESSAGE("want " << remaining << " more bytes");
          }
          run();
          rd.read_some();
        }
        CHECK_EQ(app.written_values(), std::vector<int32_t>(num_items, 42));
        CHECK_EQ(app.written_bytes().size(), num_items * sizeof(int32_t));
        CHECK_EQ(rd.buf().size(), num_items * sizeof(int32_t));
        CHECK_EQ(app.written_bytes(), rd.buf());
      }
    }
  }
}

END_FIXTURE_SCOPE()
