// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.actor_shell

#include "caf/net/actor_shell.hpp"

#include "net-test.hpp"

#include "caf/byte.hpp"
#include "caf/byte_span.hpp"
#include "caf/callback.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_transport.hpp"

using namespace caf;

using namespace std::string_literals;

namespace {

using svec = std::vector<std::string>;

struct app_t {
  using input_tag = tag::stream_oriented;

  app_t() = default;

  explicit app_t(actor hdl) : worker(std::move(hdl)) {
    // nop
  }

  template <class LowerLayerPtr>
  error init(net::socket_manager* mgr, LowerLayerPtr down, const settings&) {
    self = mgr->make_actor_shell(down);
    self->set_behavior([this](std::string& line) {
      CAF_MESSAGE("received an asynchronous message: " << line);
      lines.emplace_back(std::move(line));
    });
    self->set_fallback([](message& msg) -> result<message> {
      CAF_FAIL("unexpected message: " << msg);
      return make_error(sec::unexpected_message);
    });
    down->configure_read(net::receive_policy::up_to(2048));
    return none;
  }

  template <class LowerLayerPtr>
  bool prepare_send(LowerLayerPtr down) {
    while (self->consume_message()) {
      // We set abort_reason in our response handlers in case of an error.
      if (down->abort_reason())
        return false;
      // else: repeat.
    }
    return true;
  }

  template <class LowerLayerPtr>
  bool done_sending(LowerLayerPtr) {
    return self->try_block_mailbox();
  }

  template <class LowerLayerPtr>
  void abort(LowerLayerPtr, const error& reason) {
    CAF_FAIL("app::abort called: " << reason);
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume(LowerLayerPtr down, byte_span buf, byte_span) {
    // Seek newline character.
    constexpr auto nl = byte{'\n'};
    if (auto i = std::find(buf.begin(), buf.end(), nl); i != buf.end()) {
      // Skip empty lines.
      if (i == buf.begin()) {
        consumed_bytes += 1;
        auto sub_res = consume(down, buf.subspan(1), {});
        return sub_res >= 0 ? sub_res + 1 : sub_res;
      }
      // Deserialize config value from received line.
      auto num_bytes = std::distance(buf.begin(), i) + 1;
      string_view line{reinterpret_cast<const char*>(buf.data()),
                       static_cast<size_t>(num_bytes) - 1};
      config_value val;
      if (auto parsed_res = config_value::parse(line)) {
        val = std::move(*parsed_res);
      } else {
        down->abort_reason(std::move(parsed_res.error()));
        return -1;
      }
      if (!holds_alternative<settings>(val)) {
        down->abort_reason(
          make_error(pec::type_mismatch,
                     "expected a dictionary, got a "s + val.type_name()));
        return -1;
      }
      // Deserialize message from received dictionary.
      config_value_reader reader{&val};
      caf::message msg;
      if (!reader.apply_object(msg)) {
        down->abort_reason(reader.get_error());
        return -1;
      }
      // Dispatch message to worker.
      CAF_MESSAGE("app received a message from its socket: " << msg);
      self->request(worker, std::chrono::seconds{1}, std::move(msg))
        .then(
          [this, down](int32_t value) mutable {
            ++received_responses;
            // Respond with the value as string.
            auto str_response = std::to_string(value);
            str_response += '\n';
            down->begin_output();
            auto& buf = down->output_buffer();
            auto bytes = as_bytes(make_span(str_response));
            buf.insert(buf.end(), bytes.begin(), bytes.end());
            down->end_output();
          },
          [down](error& err) mutable { down->abort_reason(std::move(err)); });
      // Try consuming more from the buffer.
      consumed_bytes += static_cast<size_t>(num_bytes);
      auto sub_buf = buf.subspan(num_bytes);
      auto sub_res = consume(down, sub_buf, {});
      return sub_res >= 0 ? num_bytes + sub_res : sub_res;
    }
    return 0;
  }

  // Handle to the worker-under-test.
  actor worker;

  // Lines received as asynchronous messages.
  std::vector<std::string> lines;

  // Actor shell representing this app.
  net::actor_shell_ptr self;

  // Counts how many bytes we've consumed in total.
  size_t consumed_bytes = 0;

  // Counts how many response messages we've received from the worker.
  size_t received_responses = 0;
};

struct fixture : host_fixture, test_coordinator_fixture<> {
  fixture() : mm(sys), mpx(&mm) {
    mpx.set_thread_id();
    if (auto err = mpx.init())
      CAF_FAIL("mpx.init() failed: " << err);
    auto sockets = unbox(net::make_stream_socket_pair());
    self_socket_guard.reset(sockets.first);
    testee_socket_guard.reset(sockets.second);
    if (auto err = nonblocking(self_socket_guard.socket(), true))
      CAF_FAIL("nonblocking returned an error: " << err);
    if (auto err = nonblocking(testee_socket_guard.socket(), true))
      CAF_FAIL("nonblocking returned an error: " << err);
  }

  template <class Predicate>
  void run_while(Predicate predicate) {
    if (!predicate())
      return;
    for (size_t i = 0; i < 1000; ++i) {
      mpx.poll_once(false);
      byte tmp[1024];
      auto bytes = read(self_socket_guard.socket(), make_span(tmp, 1024));
      if (bytes > 0)
        recv_buf.insert(recv_buf.end(), tmp, tmp + bytes);
      if (!predicate())
        return;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CAF_FAIL("reached max repeat rate without meeting the predicate");
  }

  void send(string_view str) {
    auto res = write(self_socket_guard.socket(), as_bytes(make_span(str)));
    if (res != static_cast<ptrdiff_t>(str.size()))
      CAF_FAIL("expected write() to return " << str.size() << ", got: " << res);
  }

  net::middleman mm;
  net::multiplexer mpx;
  net::socket_guard<net::stream_socket> self_socket_guard;
  net::socket_guard<net::stream_socket> testee_socket_guard;
  std::vector<byte> recv_buf;
};

constexpr std::string_view input = R"__(
{ values = [ { "@type" : "int32_t", value: 123 } ] }
)__";

} // namespace

CAF_TEST_FIXTURE_SCOPE(actor_shell_tests, fixture)

CAF_TEST(actor shells expose their mailbox to their owners) {
  auto sck = testee_socket_guard.release();
  auto mgr = net::make_socket_manager<app_t, net::stream_transport>(sck, &mpx);
  if (auto err = mgr->init(content(cfg)))
    CAF_FAIL("mgr->init() failed: " << err);
  auto& app = mgr->top_layer();
  auto hdl = app.self.as_actor();
  anon_send(hdl, "line 1");
  anon_send(hdl, "line 2");
  anon_send(hdl, "line 3");
  run_while([&] { return app.lines.size() != 3; });
  CAF_CHECK_EQUAL(app.lines, svec({"line 1", "line 2", "line 3"}));
}

CAF_TEST(actor shells can send requests and receive responses) {
  auto worker = sys.spawn([] {
    return behavior{
      [](int32_t value) { return value * 2; },
    };
  });
  auto sck = testee_socket_guard.release();
  auto mgr = net::make_socket_manager<app_t, net::stream_transport>(sck, &mpx,
                                                                    worker);
  if (auto err = mgr->init(content(cfg)))
    CAF_FAIL("mgr->init() failed: " << err);
  auto& app = mgr->top_layer();
  send(input);
  run_while([&] { return app.consumed_bytes != input.size(); });
  expect((int32_t), to(worker).with(123));
  string_view expected_response = "246\n";
  run_while([&] { return recv_buf.size() < expected_response.size(); });
  string_view received_response{reinterpret_cast<char*>(recv_buf.data()),
                                recv_buf.size()};
  CAF_CHECK_EQUAL(received_response, expected_response);
}

CAF_TEST_FIXTURE_SCOPE_END()
