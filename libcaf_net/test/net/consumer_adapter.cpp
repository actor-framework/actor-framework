// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.consumer_adapter

#include "caf/net/consumer_adapter.hpp"

#include "net-test.hpp"

#include "caf/async/spsc_buffer.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"
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
  byte_buffer buf_;
  net::socket_guard<net::stream_socket> sg_;
};

class app_t : public net::stream_oriented::upper_layer {
public:
  using resource_type = async::consumer_resource<int32_t>;

  using buffer_type = resource_type::buffer_type;

  using adapter_ptr = net::consumer_adapter_ptr<buffer_type>;

  using adapter_type = adapter_ptr::element_type;

  explicit app_t(resource_type input) : input(std::move(input)) {
    // nop
  }

  static auto make(resource_type input) {
    return std::make_unique<app_t>(std::move(input));
  }

  error init(net::socket_manager* mgr,
             net::stream_oriented::lower_layer* down_ptr,
             const settings&) override {
    down = down_ptr;
    if (auto buf = input.try_open()) {
      auto do_wakeup = make_action([this] { prepare_send(); });
      adapter = adapter_type::make(std::move(buf), mgr, std::move(do_wakeup));
      return none;
    } else {
      FAIL("unable to open the resource");
    }
  }

  struct send_helper {
    app_t* thisptr;

    void on_next(int32_t item) {
      thisptr->written_values.emplace_back(item);
      auto offset = thisptr->written_bytes.size();
      binary_serializer sink{nullptr, thisptr->written_bytes};
      if (!sink.apply(item))
        FAIL("sink.apply failed: " << sink.get_error());
      auto bytes = make_span(thisptr->written_bytes).subspan(offset);
      thisptr->down->begin_output();
      auto& buf = thisptr->down->output_buffer();
      buf.insert(buf.end(), bytes.begin(), bytes.end());
      thisptr->down->end_output();
    }

    void on_complete() {
      // nop
    }

    void on_error(const error&) {
      // nop
    }
  };

  bool prepare_send() override {
    if (done || !adapter)
      return true;
    auto helper = send_helper{this};
    while (down->can_send_more()) {
      auto [again, consumed] = adapter->pull(async::delay_errors, 1, helper);
      if (!again) {
        MESSAGE("adapter signaled end-of-buffer");
        adapter = nullptr;
        done = true;
        break;
      } else if (consumed == 0) {
        break;
      }
    }
    MESSAGE(written_bytes.size() << " bytes written");
    return true;
  }

  bool done_sending() override {
    return done || !adapter->has_data();
  }

  void abort(const error& reason) override {
    MESSAGE("app::abort: " << reason);
  }

  ptrdiff_t consume(byte_span, byte_span) override {
    FAIL("app::consume called: unexpected data");
  }

  net::stream_oriented::lower_layer* down = nullptr;
  bool done = false;
  std::vector<int32_t> written_values;
  byte_buffer written_bytes;
  adapter_ptr adapter;
  resource_type input;
};

struct fixture : test_coordinator_fixture<> {
  fixture() : mm(sys) {
    mm.mpx().set_thread_id();
    if (auto err = mm.mpx().init())
      CAF_FAIL("mpx.init() failed: " << err);
  }

  bool handle_io_event() override {
    return mm.mpx().poll_once(false);
  }

  auto mpx() {
    return mm.mpx_ptr();
  }

  net::middleman mm;
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("subscriber adapters wake up idle socket managers") {
  GIVEN("an actor pushing into a buffer resource") {
    static constexpr size_t num_items = 79;
    auto [rd, wr] = async::make_spsc_buffer_resource<int32_t>(8, 2);
    sys.spawn([wr{wr}](event_based_actor* self) {
      self->make_observable().repeat(42).take(num_items).subscribe(wr);
    });
    WHEN("draining the buffer resource and sending its items over a socket") {
      auto [fd1, fd2] = unbox(net::make_stream_socket_pair());
      if (auto err = nonblocking(fd1, true))
        FAIL("nonblocking(fd1) returned an error: " << err);
      if (auto err = nonblocking(fd2, true))
        FAIL("nonblocking(fd2) returned an error: " << err);
      auto app = app_t::make(std::move(rd));
      auto& state = *app;
      auto transport = net::stream_transport::make(fd1, std::move(app));
      auto mgr = net::socket_manager::make(mpx(), fd1, std::move(transport));
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
        CHECK_EQ(state.written_values, std::vector<int32_t>(num_items, 42));
        CHECK_EQ(state.written_bytes.size(), num_items * sizeof(int32_t));
        CHECK_EQ(rd.buf().size(), num_items * sizeof(int32_t));
        CHECK_EQ(state.written_bytes, rd.buf());
      }
    }
  }
}

END_FIXTURE_SCOPE()
