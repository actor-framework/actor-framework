// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.length_prefix_framing

#include "caf/net/lp/with.hpp"

#include "net-test.hpp"

#include <cctype>
#include <numeric>
#include <vector>

#include "caf/binary_serializer.hpp"
#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/net/binary/frame.hpp"
#include "caf/net/binary/lower_layer.hpp"
#include "caf/net/binary/upper_layer.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/span.hpp"

using namespace caf;
using namespace std::literals;

namespace {

using string_list = std::vector<std::string>;

using shared_string_list = std::shared_ptr<string_list>;

template <bool EnableSuspend>
class app_t : public net::binary::upper_layer {
public:
  app_t(async::execution_context_ptr loop, shared_string_list ls_ptr)
    : loop(std::move(loop)), inputs(std::move(ls_ptr)) {
    // nop
  }

  static auto make(async::execution_context_ptr loop,
                   shared_string_list inputs) {
    return std::make_unique<app_t>(std::move(loop), std::move(inputs));
  }

  caf::error start(net::binary::lower_layer* down_ptr,
                   const settings&) override {
    // Start reading immediately.
    down = down_ptr;
    down->request_messages();
    return none;
  }

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    return true;
  }

  void abort(const error& err) override {
    MESSAGE("abort: " << err);
  }

  void continue_reading() {
    loop->schedule_fn([this] { down->request_messages(); });
  }

  ptrdiff_t consume(byte_span buf) override {
    auto printable = [](std::byte x) {
      return ::isprint(static_cast<uint8_t>(x));
    };
    if (CHECK(std::all_of(buf.begin(), buf.end(), printable))) {
      auto str_buf = reinterpret_cast<char*>(buf.data());
      inputs->emplace_back(std::string{str_buf, buf.size()});
      MESSAGE("app: consumed " << inputs->back());
      if constexpr (EnableSuspend)
        if (inputs->back() == "pause") {
          MESSAGE("app: suspend reading");
          down->suspend_reading();
        }
      std::string response = "ok ";
      response += std::to_string(inputs->size());
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

  async::execution_context_ptr loop;

  net::binary::lower_layer* down = nullptr;

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
    auto [msg_size, msg] = net::lp::framing::split(input);
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

void run_writer(net::stream_socket fd) {
  net::multiplexer::block_sigpipe();
  std::ignore = allow_sigpipe(fd, false);
  auto guard = make_socket_guard(fd);
  std::vector<std::string_view> inputs{"first", "second", "pause", "third",
                                       "fourth"};
  byte_buffer wr_buf;
  byte_buffer rd_buf;
  rd_buf.resize(512);
  for (auto input : inputs) {
    wr_buf.clear();
    encode(wr_buf, input);
    write(fd, wr_buf);
    read(fd, rd_buf);
  }
}

} // namespace

SCENARIO("length-prefix framing reads data with 32-bit size headers") {
  GIVEN("a framing object with an app that consumes strings") {
    WHEN("pushing data into the unit-under-test") {
      auto buf = std::make_shared<string_list>();
      auto app = app_t<false>::make(nullptr, buf);
      auto framing = net::lp::framing::make(std::move(app));
      auto uut = mock_stream_transport::make(std::move(framing));
      CHECK_EQ(uut->start(), error{});
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

SCENARIO("calling suspend_reading temporarily halts receiving of messages") {
  using namespace std::literals;
  GIVEN("a framing object with an app that consumes strings") {
    auto [fd1, fd2] = unbox(net::make_stream_socket_pair());
    auto writer = std::thread{run_writer, fd1};
    auto mpx = net::multiplexer::make(nullptr);
    mpx->set_thread_id();
    if (auto err = mpx->init())
      FAIL("mpx->init failed: " << err);
    mpx->apply_updates();
    REQUIRE_EQ(mpx->num_socket_managers(), 1u);
    if (auto err = net::nonblocking(fd2, true))
      CAF_FAIL("nonblocking returned an error: " << err);
    auto buf = std::make_shared<string_list>();
    auto app = app_t<true>::make(mpx, buf);
    auto app_ptr = app.get();
    auto framing = net::lp::framing::make(std::move(app));
    auto transport = net::stream_transport::make(fd2, std::move(framing));
    auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
    CHECK_EQ(mgr->start(settings{}), none);
    mpx->apply_updates();
    REQUIRE_EQ(mpx->num_socket_managers(), 2u);
    CHECK_EQ(mpx->mask_of(mgr), net::operation::read);
    WHEN("the app calls suspend_reading") {
      while (mpx->num_socket_managers() > 1u)
        mpx->poll_once(true);
      CHECK_EQ(mpx->mask_of(mgr), net::operation::none);
      if (CHECK_EQ(buf->size(), 3u)) {
        CHECK_EQ(buf->at(0), "first");
        CHECK_EQ(buf->at(1), "second");
        CHECK_EQ(buf->at(2), "pause");
      }
      THEN("users can resume it manually") {
        app_ptr->continue_reading();
        mpx->apply_updates();
        mpx->poll_once(true);
        while (mpx->num_socket_managers() > 1u)
          mpx->poll_once(true);
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
    while (mpx->poll_once(false)) {
      // repeat
    }
  }
}

SCENARIO("lp::with(...).connect(...) translates between flows and socket I/O") {
  using namespace std::literals;
  GIVEN("a connected socket with a writer at the other end") {
    auto [fd1, fd2] = unbox(net::make_stream_socket_pair());
    auto writer = std::thread{run_writer, fd1};
    WHEN("calling length_prefix_framing::run") {
      THEN("actors can consume the resulting flow") {
        caf::actor_system_config cfg;
        cfg.set("caf.scheduler.max-threads", 2);
        cfg.set("caf.scheduler.policy", "sharing");
        cfg.load<net::middleman>();
        caf::actor_system sys{cfg};
        auto buf = std::make_shared<std::vector<std::string>>();
        caf::actor hdl;
        net::lp::with(sys).connect(fd2).start([&](auto pull, auto push) {
          hdl = sys.spawn([buf, pull, push](event_based_actor* self) {
            pull.observe_on(self)
              .do_on_error([](const error& what) { //
                MESSAGE("flow aborted: " << what);
              })
              .do_on_complete([] { MESSAGE("flow completed"); })
              .do_on_next([buf](const net::binary::frame& x) {
                std::string str;
                for (auto val : x.bytes())
                  str.push_back(static_cast<char>(val));
                buf->push_back(std::move(str));
              })
              .map([](const net::binary::frame& x) {
                std::string response = "ok ";
                for (auto val : x.bytes())
                  response.push_back(static_cast<char>(val));
                return net::binary::frame{as_bytes(make_span(response))};
              })
              .subscribe(push);
          });
        });
        scoped_actor self{sys};
        self->wait_for(hdl);
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
