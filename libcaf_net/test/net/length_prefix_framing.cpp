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
#include "caf/net/message_oriented.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/span.hpp"

using namespace caf;
using namespace std::literals;

namespace {

using string_list = std::vector<std::string>;

using shared_string_list = std::shared_ptr<string_list>;

template <bool EnableSuspend>
class app_t : public net::message_oriented::upper_layer {
public:
  static auto make(shared_string_list inputs) {
    return std::make_unique<app_t>(std::move(inputs));
  }

  app_t(shared_string_list ls_ptr) : inputs(std::move(ls_ptr)) {
    // nop
  }

  caf::error init(net::socket_manager*,
                  net::message_oriented::lower_layer* down_ptr,
                  const settings&) override {
    // Start reading immediately.
    down = down_ptr;
    down->request_messages();
    return none;
  }

  bool prepare_send() override {
    return true;
  }

  bool done_sending() override {
    return true;
  }

  void abort(const error&) override {
    // nop
  }

  void continue_reading() override {
    down->request_messages();
  }

  ptrdiff_t consume(byte_span buf) override {
    printf("app_t::consume %d\n", __LINE__);
    auto printable = [](std::byte x) {
      return ::isprint(static_cast<uint8_t>(x));
    };
    if (CHECK(std::all_of(buf.begin(), buf.end(), printable))) {
      printf("app_t::consume %d\n", __LINE__);
      auto str_buf = reinterpret_cast<char*>(buf.data());
      inputs->emplace_back(std::string{str_buf, buf.size()});
      printf("app_t::consume %d added %s\n", __LINE__, inputs->back().c_str());
      if constexpr (EnableSuspend)
        if (inputs->back() == "pause")
          down->suspend_reading();
      std::string response = "ok ";
      response += std::to_string(inputs->size());
      auto response_bytes = as_bytes(make_span(response));
      down->begin_message();
      auto& buf = down->message_buffer();
      buf.insert(buf.end(), response_bytes.begin(), response_bytes.end());
      CHECK(down->end_message());
      return static_cast<ptrdiff_t>(buf.size());
    } else {
      printf("app_t::consume %d\n", __LINE__);
      return -1;
    }
  }

  net::message_oriented::lower_layer* down = nullptr;

  shared_string_list inputs;
};

void encode(byte_buffer& buf, std::string_view msg) {
  using detail::to_network_order;
  auto prefix = to_network_order(static_cast<uint32_t>(msg.size()));
  auto prefix_bytes = as_bytes(make_span(&prefix, 1));
  buf.insert(buf.end(), prefix_bytes.begin(), prefix_bytes.end());
  auto bytes = as_bytes(make_span(msg));
  buf.insert(buf.end(), bytes.begin(), bytes.end());
}

auto decode(byte_buffer& buf) {
  auto printable = [](std::byte x) {
    return ::isprint(static_cast<uint8_t>(x));
  };
  string_list result;
  auto input = make_span(buf);
  while (!input.empty()) {
    auto [msg_size, msg] = net::length_prefix_framing::split(input);
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
      auto buf = std::make_shared<string_list>();
      auto app = app_t<false>::make(buf);
      auto framing = net::length_prefix_framing::make(std::move(app));
      auto uut = mock_stream_transport::make(std::move(framing));
      CHECK_EQ(uut->init(), error{});
      THEN("the app receives all strings as individual messages") {
        encode(uut->input, "hello");
        encode(uut->input, "world");
        auto input_size = static_cast<ptrdiff_t>(uut->input.size());
        CHECK_EQ(uut->handle_input(), input_size);
        if (CHECK_EQ(buf->size(), 2u)) {
          CHECK_EQ(buf->at(0), "hello");
          CHECK_EQ(buf->at(1), "world");
        }
        CHECK_EQ(decode(uut->output), string_list({"ok 1", "ok 2"}));
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
    mpx.set_thread_id();
    if (auto err = mpx.init())
      FAIL("mpx.init failed: " << err);
    mpx.apply_updates();
    REQUIRE_EQ(mpx.num_socket_managers(), 1u);
    if (auto err = net::nonblocking(fd2, true))
      CAF_FAIL("nonblocking returned an error: " << err);
    auto buf = std::make_shared<string_list>();
    auto app = app_t<true>::make(buf);
    auto framing = net::length_prefix_framing::make(std::move(app));
    auto transport = net::stream_transport::make(fd2, std::move(framing));
    auto mgr = net::socket_manager::make(&mpx, fd2, std::move(transport));
    CHECK_EQ(mgr->init(settings{}), none);
    mpx.apply_updates();
    REQUIRE_EQ(mpx.num_socket_managers(), 2u);
    CHECK_EQ(mpx.mask_of(mgr), net::operation::read);
    WHEN("the app calls suspend_reading") {
      while (mpx.num_socket_managers() > 1u)
        mpx.poll_once(true);
      CHECK_EQ(mpx.mask_of(mgr), net::operation::none);
      if (CHECK_EQ(buf->size(), 3u)) {
        CHECK_EQ(buf->at(0), "first");
        CHECK_EQ(buf->at(1), "second");
        CHECK_EQ(buf->at(2), "pause");
      }
      THEN("users can resume it via continue_reading ") {
        mgr->continue_reading();
        mpx.apply_updates();
        mpx.poll_once(true);
        CHECK_EQ(mpx.mask_of(mgr), net::operation::read);
        while (mpx.num_socket_managers() > 1u)
          mpx.poll_once(true);
        if (CHECK_EQ(buf->size(), 5u)) {
          CHECK_EQ(buf->at(0), "first");
          CHECK_EQ(buf->at(1), "second");
          CHECK_EQ(buf->at(2), "pause");
          CHECK_EQ(buf->at(3), "third");
          CHECK_EQ(buf->at(4), "fourth");
        }
      }
    }
    writer.join();
  }
}
