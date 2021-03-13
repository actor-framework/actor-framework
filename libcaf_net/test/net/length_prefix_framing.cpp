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
#include "caf/span.hpp"
#include "caf/tag/message_oriented.hpp"

using namespace caf;
using namespace std::literals;

namespace {

using string_list = std::vector<std::string>;

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
    auto [msg_size, msg] = net::length_prefix_framing<app>::split(input);
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
  GIVEN("a length_prefix_framing with an app that consumed strings") {
    mock_stream_transport<net::length_prefix_framing<app>> uut;
    CHECK_EQ(uut.init(), error{});
    WHEN("pushing data into the unit-under-test") {
      encode(uut.input, "hello");
      encode(uut.input, "world");
      auto input_size = static_cast<ptrdiff_t>(uut.input.size());
      CHECK_EQ(uut.handle_input(), input_size);
      THEN("the app receives all strings as individual messages") {
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
