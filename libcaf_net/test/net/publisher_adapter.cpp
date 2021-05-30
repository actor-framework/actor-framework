// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.publisher_adapter

#include "caf/net/publisher_adapter.hpp"

#include "net-test.hpp"

#include "caf/async/publisher.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/net/length_prefix_framing.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/tag/message_oriented.hpp"

using namespace caf;

namespace {

class writer {
public:
  explicit writer(net::stream_socket fd) : sg_(fd) {
    // nop
  }

  auto fd() {
    return sg_.socket();
  }

  byte_buffer encode(string_view msg) {
    using detail::to_network_order;
    auto prefix = to_network_order(static_cast<uint32_t>(msg.size()));
    auto prefix_bytes = as_bytes(make_span(&prefix, 1));
    byte_buffer buf;
    buf.insert(buf.end(), prefix_bytes.begin(), prefix_bytes.end());
    auto bytes = as_bytes(make_span(msg));
    buf.insert(buf.end(), bytes.begin(), bytes.end());
    return buf;
  }

  void write(string_view msg) {
    auto buf = encode(msg);
    if (net::write(fd(), buf) < 0)
      FAIL("failed to write: " << net::last_socket_error_as_string());
  }

private:
  net::socket_guard<net::stream_socket> sg_;
};

class app {
public:
  using input_tag = tag::message_oriented;

  template <class LowerLayerPtr>
  error init(net::socket_manager* owner, LowerLayerPtr, const settings&) {
    adapter = make_counted<net::publisher_adapter<int32_t>>(owner, 3, 2);
    return none;
  }

  template <class LowerLayerPtr>
  bool prepare_send(LowerLayerPtr) {
    return true;
  }

  template <class LowerLayerPtr>
  bool done_sending(LowerLayerPtr) {
    return true;
  }

  template <class LowerLayerPtr>
  void abort(LowerLayerPtr, const error& reason) {
    adapter->flush();
    if (reason == caf::sec::socket_disconnected)
      adapter->on_complete();
    else
      adapter->on_error(reason);
  }

  template <class LowerLayerPtr>
  void after_reading(LowerLayerPtr) {
    adapter->flush();
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume(LowerLayerPtr down, byte_span buf) {
    auto val = int32_t{0};
    auto str = string_view{reinterpret_cast<char*>(buf.data()), buf.size()};
    if (auto err = detail::parse(str, val))
      FAIL("unable to parse input: " << err);
    ++received_messages;
    if (auto n = adapter->push(val); n == 0)
      down->suspend_reading();
    return static_cast<ptrdiff_t>(buf.size());
  }

  size_t received_messages = 0;
  net::publisher_adapter_ptr<int32_t> adapter;
};

struct mock_observer : flow::observer<int32_t>::impl {
  void dispose() {
    if (sub) {
      sub.cancel();
      sub = nullptr;
    }
    done = true;
  }

  bool disposed() const noexcept {
    return done;
  }

  void on_complete() {
    sub = nullptr;
    done = true;
  }

  void on_error(const error& what) {
    FAIL("observer received an error: " << what);
  }

  void on_attach(flow::subscription new_sub) {
    REQUIRE(!sub);
    sub = std::move(new_sub);
  }

  void on_next(span<const int32_t> items) {
    buf.insert(buf.end(), items.begin(), items.end());
  }

  bool done = false;
  flow::subscription sub;
  std::vector<int32_t> buf;
};

struct fixture {

};

} // namespace

CAF_TEST_FIXTURE_SCOPE(publisher_adapter_tests, fixture)

SCENARIO("publisher adapters suspend reads if the buffer becomes full") {
  auto ls = [](auto... xs) { return std::vector<int32_t>{xs...}; };
  GIVEN("a writer and a message-based application") {
    auto [fd1, fd2] = unbox(net::make_stream_socket_pair());
    auto writer_thread = std::thread{[fd1{fd1}] {
      writer out{fd1};
      for (int i = 0; i < 12; ++i)
        out.write(std::to_string(i));
    }};
    net::multiplexer mpx{nullptr};
    if (auto err = mpx.init())
      FAIL("mpx.init failed: " << err);
    mpx.set_thread_id();
    REQUIRE_EQ(mpx.num_socket_managers(), 1u);
    if (auto err = net::nonblocking(fd2, true))
      CAF_FAIL("nonblocking returned an error: " << err);
    auto mgr = net::make_socket_manager<app, net::length_prefix_framing,
                                        net::stream_transport>(fd2, &mpx);
    auto& st = mgr->top_layer();
    CHECK_EQ(mgr->init(settings{}), none);
    REQUIRE_EQ(mpx.num_socket_managers(), 2u);
    CHECK_EQ(mgr->mask(), net::operation::read);
    WHEN("the publisher adapter runs out of capacity") {
      while (mpx.num_socket_managers() > 1u)
        mpx.poll_once(true);
      CHECK_EQ(mgr->mask(), net::operation::none);
      CHECK_EQ(st.received_messages, 3u);
      THEN("reading from the adapter registers the manager for reading again") {
        auto obs = make_counted<mock_observer>();
        st.adapter->subscribe(flow::observer<int32_t>{obs});
        REQUIRE(obs->sub.valid());
        obs->sub.request(1);
        while (st.received_messages != 4u)
          mpx.poll_once(true);
        CHECK_EQ(obs->buf, ls(0));
        obs->sub.request(20);
        while (st.received_messages != 12u)
          mpx.poll_once(true);
        CHECK_EQ(obs->buf, ls(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11));
      }
    }
    writer_thread.join();
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
