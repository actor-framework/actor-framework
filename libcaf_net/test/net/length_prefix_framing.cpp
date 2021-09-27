// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.length_prefix_framing

#include "caf/net/length_prefix_framing.hpp"

#include "net-test.hpp"

#include <cctype>
#include <numeric>
#include <vector>

#include "caf/binary_serializer.hpp"
#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/span.hpp"
#include "caf/tag/message_oriented.hpp"

using namespace caf;
using namespace std::literals;

namespace {

using string_list = std::vector<std::string>;

template <bool EnableSuspend>
struct app {
  using input_tag = tag::message_oriented;

  template <class LowerLayerPtr>
  caf::error init(net::socket_manager*, LowerLayerPtr, const settings&) {
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
  void abort(LowerLayerPtr, const error&) {
    // nop
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume(LowerLayerPtr down, byte_span buf) {
    auto printable = [](byte x) { return ::isprint(static_cast<uint8_t>(x)); };
    if (CHECK(std::all_of(buf.begin(), buf.end(), printable))) {
      auto str_buf = reinterpret_cast<char*>(buf.data());
      inputs.emplace_back(std::string{str_buf, buf.size()});
      if constexpr (EnableSuspend)
        if (inputs.back() == "pause")
          down->suspend_reading();
      std::string response = "ok ";
      response += std::to_string(inputs.size());
      auto response_bytes = as_bytes(make_span(response));
      down->begin_message();
      auto& buf = down->message_buffer();
      buf.insert(buf.end(), response_bytes.begin(), response_bytes.end());
      CHECK(down->end_message());
      return static_cast<ptrdiff_t>(buf.size());
    } else {
      return -1;
    }
  }

  std::vector<std::string> inputs;
};

void encode(byte_buffer& buf, string_view msg) {
  using detail::to_network_order;
  auto prefix = to_network_order(static_cast<uint32_t>(msg.size()));
  auto prefix_bytes = as_bytes(make_span(&prefix, 1));
  buf.insert(buf.end(), prefix_bytes.begin(), prefix_bytes.end());
  auto bytes = as_bytes(make_span(msg));
  buf.insert(buf.end(), bytes.begin(), bytes.end());
}

auto decode(byte_buffer& buf) {
  auto printable = [](byte x) { return ::isprint(static_cast<uint8_t>(x)); };
  string_list result;
  auto input = make_span(buf);
  while (!input.empty()) {
    auto [msg_size, msg] = net::length_prefix_framing<app<false>>::split(input);
    if (msg_size > msg.size()) {
      CAF_FAIL("cannot decode buffer: invalid message size");
    } else if (!std::all_of(msg.begin(), msg.begin() + msg_size, printable)) {
      CAF_FAIL("cannot decode buffer: unprintable characters found in message");
    } else {
      auto str = std::string{reinterpret_cast<char*>(msg.data()), msg_size};
      result.emplace_back(std::move(str));
      input = msg.subspan(msg_size);
    }
  }
  return result;
}

} // namespace

SCENARIO("length-prefix framing reads data with 32-bit size headers") {
  GIVEN("a length_prefix_framing with an app that consumes strings") {
    WHEN("pushing data into the unit-under-test") {
      mock_stream_transport<net::length_prefix_framing<app<false>>> uut;
      CHECK_EQ(uut.init(), error{});
      THEN("the app receives all strings as individual messages") {
        encode(uut.input, "hello");
        encode(uut.input, "world");
        auto input_size = static_cast<ptrdiff_t>(uut.input.size());
        CHECK_EQ(uut.handle_input(), input_size);
        auto& state = uut.upper_layer.upper_layer();
        if (CHECK_EQ(state.inputs.size(), 2u)) {
          CHECK_EQ(state.inputs[0], "hello");
          CHECK_EQ(state.inputs[1], "world");
        }
        auto response = string_view{reinterpret_cast<char*>(uut.output.data()),
                                    uut.output.size()};
        CHECK_EQ(decode(uut.output), string_list({"ok 1", "ok 2"}));
      }
    }
  }
}

SCENARIO("calling suspend_reading removes message apps temporarily") {
  using namespace std::literals;
  GIVEN("a length_prefix_framing with an app that consumes strings") {
    auto [fd1, fd2] = unbox(net::make_stream_socket_pair());
    auto writer = std::thread{[fd1{fd1}] {
      auto guard = make_socket_guard(fd1);
      std::vector<std::string_view> inputs{"first", "second", "pause", "third",
                                           "fourth"};
      byte_buffer wr_buf;
      byte_buffer rd_buf;
      rd_buf.resize(512);
      for (auto input : inputs) {
        wr_buf.clear();
        encode(wr_buf, input);
        write(fd1, wr_buf);
        read(fd1, rd_buf);
      }
    }};
    net::multiplexer mpx{nullptr};
    if (auto err = mpx.init())
      FAIL("mpx.init failed: " << err);
    mpx.set_thread_id();
    REQUIRE_EQ(mpx.num_socket_managers(), 1u);
    if (auto err = net::nonblocking(fd2, true))
      CAF_FAIL("nonblocking returned an error: " << err);
    auto mgr = net::make_socket_manager<app<true>, net::length_prefix_framing,
                                        net::stream_transport>(fd2, &mpx);
    CHECK_EQ(mgr->init(settings{}), none);
    REQUIRE_EQ(mpx.num_socket_managers(), 2u);
    CHECK_EQ(mgr->mask(), net::operation::read);
    auto& state = mgr->top_layer();
    WHEN("the app calls suspend_reading") {
      while (mpx.num_socket_managers() > 1u)
        mpx.poll_once(true);
      CHECK_EQ(mgr->mask(), net::operation::none);
      if (CHECK_EQ(state.inputs.size(), 3u)) {
        CHECK_EQ(state.inputs[0], "first");
        CHECK_EQ(state.inputs[1], "second");
        CHECK_EQ(state.inputs[2], "pause");
      }
      THEN("users can resume it via continue_reading ") {
        mgr->continue_reading();
        CHECK_EQ(mgr->mask(), net::operation::read);
        while (mpx.num_socket_managers() > 1u)
          mpx.poll_once(true);
        if (CHECK_EQ(state.inputs.size(), 5u)) {
          CHECK_EQ(state.inputs[0], "first");
          CHECK_EQ(state.inputs[1], "second");
          CHECK_EQ(state.inputs[2], "pause");
          CHECK_EQ(state.inputs[3], "third");
          CHECK_EQ(state.inputs[4], "fourth");
        }
      }
    }
    writer.join();
  }
}
