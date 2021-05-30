// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.observer_adapter

#include "caf/net/observer_adapter.hpp"

#include "net-test.hpp"

#include "caf/async/publisher.hpp"
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

  bool done() const noexcept {
    return rd_pos_ == buf_.size();
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

  explicit app_t(async::publisher<int32_t> input) : input_(std::move(input)) {
    // nop
  }

  template <class LowerLayerPtr>
  error init(net::socket_manager* owner, LowerLayerPtr, const settings&) {
    adapter_ = make_counted<net::observer_adapter<int32_t>>(owner);
    input_.subscribe(adapter_->as_observer());
    input_ = nullptr;
    return none;
  }

  template <class LowerLayerPtr>
  bool prepare_send(LowerLayerPtr down) {
    while (!done_ && down->can_send_more()) {
      auto [val, done, err] = adapter_->poll();
      if (val) {
        written_values_.emplace_back(*val);
        auto offset = written_bytes_.size();
        binary_serializer sink{nullptr, written_bytes_};
        if (!sink.apply(*val))
          FAIL("sink.apply failed: " << sink.get_error());
        auto bytes = make_span(written_bytes_).subspan(offset);
        down->begin_output();
        auto& buf = down->output_buffer();
        buf.insert(buf.end(), bytes.begin(), bytes.end());
        down->end_output();
      } else if (done) {
        done_ = true;
        if (err)
          FAIL("flow error: " << *err);
      } else {
        break;
      }
    }
    MESSAGE(written_bytes_.size() << " bytes written");
    return true;
  }

  template <class LowerLayerPtr>
  bool done_sending(LowerLayerPtr) {
    return !adapter_->has_data();
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
  net::observer_adapter_ptr<int32_t> adapter_;
  async::publisher<int32_t> input_;
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
  GIVEN("a publisher<T>") {
    static constexpr size_t num_items = 4211;
    auto src = async::publisher_from<event_based_actor>(sys, [](auto* self) {
      return self->make_observable().repeat(42).take(num_items);
    });
    WHEN("sending items of the stream over a socket") {
      auto [fd1, fd2] = unbox(net::make_stream_socket_pair());
      if (auto err = nonblocking(fd1, true))
        FAIL("nonblocking(fd1) returned an error: " << err);
      if (auto err = nonblocking(fd2, true))
        FAIL("nonblocking(fd2) returned an error: " << err);
      auto mgr = net::make_socket_manager<app_t, net::stream_transport>(
        fd1, mm.mpx_ptr(), src);
      auto& app = mgr->top_layer();
      if (auto err = mgr->init(content(cfg)))
        FAIL("mgr->init() failed: " << err);
      THEN("the reader receives all items before the connection closes") {
        reader rd{fd2, num_items * sizeof(int32_t)};
        while (!rd.done()) {
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
