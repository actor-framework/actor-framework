// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.typed_actor_shell

#include "caf/net/typed_actor_shell.hpp"

#include "caf/net/make_actor_shell.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/anon_mail.hpp"
#include "caf/byte_span.hpp"
#include "caf/callback.hpp"
#include "caf/typed_actor.hpp"

#include "net-test.hpp"

using namespace caf;

using namespace std::string_literals;

namespace {

using svec = std::vector<std::string>;

using string_consumer = typed_actor<result<void>(std::string)>;

class app_t : public net::octet_stream::upper_layer {
public:
  explicit app_t(actor_system& sys, async::execution_context_ptr loop,
                 actor hdl = {})
    : worker(std::move(hdl)) {
    self = net::make_actor_shell<string_consumer>(sys, loop);
  }

  static auto make(actor_system& sys, async::execution_context_ptr loop,
                   actor hdl = {}) {
    return std::make_unique<app_t>(sys, std::move(loop), std::move(hdl));
  }

  error start(net::octet_stream::lower_layer* down) override {
    this->down = down;
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

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    return true;
  }

  void abort(const error&) override {
    // nop
  }

  ptrdiff_t consume(byte_span buf, byte_span) override {
    // Seek newline character.
    constexpr auto nl = std::byte{'\n'};
    if (auto i = std::find(buf.begin(), buf.end(), nl); i != buf.end()) {
      // Skip empty lines.
      if (i == buf.begin()) {
        consumed_bytes += 1;
        auto sub_res = consume(buf.subspan(1), {});
        return sub_res >= 0 ? sub_res + 1 : sub_res;
      }
      // Deserialize config value from received line.
      auto num_bytes = std::distance(buf.begin(), i) + 1;
      std::string_view line{reinterpret_cast<const char*>(buf.data()),
                            static_cast<size_t>(num_bytes) - 1};
      config_value val;
      if (auto parsed_res = config_value::parse(line)) {
        val = std::move(*parsed_res);
      } else {
        return -1;
      }
      // Deserialize message from received dictionary.
      config_value_reader reader{&val};
      caf::message msg;
      if (!reader.apply(msg)) {
        return -1;
      }
      // Dispatch message to worker.
      CAF_MESSAGE("app received a message from its socket: " << msg);
      self->request(worker, std::chrono::seconds{1}, std::move(msg))
        .then(
          [this](int32_t value) mutable {
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
          [this](error& err) mutable { self->quit(err); });
      // Try consuming more from the buffer.
      consumed_bytes += static_cast<size_t>(num_bytes);
      auto sub_buf = buf.subspan(num_bytes);
      auto sub_res = consume(sub_buf, {});
      return sub_res >= 0 ? num_bytes + sub_res : sub_res;
    }
    return 0;
  }

  // Pointer to the layer below.
  net::octet_stream::lower_layer* down;

  // Handle to the worker-under-test.
  actor worker;

  // Lines received as asynchronous messages.
  std::vector<std::string> lines;

  // Actor shell representing this app.
  net::actor_shell_ptr_t<string_consumer> self;

  // Counts how many bytes we've consumed in total.
  size_t consumed_bytes = 0;

  // Counts how many response messages we've received from the worker.
  size_t received_responses = 0;
};

struct fixture : test_coordinator_fixture<> {
  fixture() : mm(sys), mpx(net::multiplexer::make(&mm)) {
    mpx->set_thread_id();
    if (auto err = mpx->init())
      CAF_FAIL("mpx->init() failed: " << err);
    auto sockets = unbox(net::make_stream_socket_pair());
    self_socket_guard.reset(sockets.first);
    testee_socket_guard.reset(sockets.second);
    if (auto err = nonblocking(self_socket_guard.socket(), true))
      CAF_FAIL("nonblocking returned an error: " << err);
    if (auto err = nonblocking(testee_socket_guard.socket(), true))
      CAF_FAIL("nonblocking returned an error: " << err);
  }

  ~fixture() {
    while (mpx->poll_once(false)) {
      // repeat
    }
  }

  template <class Predicate>
  void run_while(Predicate predicate) {
    if (!predicate())
      return;
    for (size_t i = 0; i < 1000; ++i) {
      mpx->apply_updates();
      mpx->poll_once(false);
      std::byte tmp[1024];
      auto bytes = read(self_socket_guard.socket(), make_span(tmp, 1024));
      if (bytes > 0)
        recv_buf.insert(recv_buf.end(), tmp, tmp + bytes);
      if (!predicate())
        return;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CAF_FAIL("reached max repeat rate without meeting the predicate");
  }

  void send(std::string_view str) {
    auto res = write(self_socket_guard.socket(), as_bytes(make_span(str)));
    if (res != static_cast<ptrdiff_t>(str.size()))
      CAF_FAIL("expected write() to return " << str.size() << ", got: " << res);
  }

  net::middleman mm;
  net::multiplexer_ptr mpx;
  net::socket_guard<net::stream_socket> self_socket_guard;
  net::socket_guard<net::stream_socket> testee_socket_guard;
  byte_buffer recv_buf;
};

constexpr std::string_view input = R"__(
[ { "@type" : "int32_t", value: 123 } ]
)__";

} // namespace

CAF_TEST_FIXTURE_SCOPE(actor_shell_tests, fixture)

CAF_TEST(actor shells expose their mailbox to their owners) {
  auto fd = testee_socket_guard.release();
  auto app_uptr = app_t::make(sys, mpx);
  auto app = app_uptr.get();
  auto transport = net::octet_stream::transport::make(fd, std::move(app_uptr));
  auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
  if (auto err = mgr->start())
    CAF_FAIL("mgr->start() failed: " << err);
  auto hdl = app->self.as_actor();
  anon_mail("line 1").send(hdl);
  anon_mail("line 2").send(hdl);
  anon_mail("line 3").send(hdl);
  run_while([&] { return app->lines.size() != 3; });
  CAF_CHECK_EQUAL(app->lines, svec({"line 1", "line 2", "line 3"}));
  self_socket_guard.reset();
}

CAF_TEST(actor shells can send requests and receive responses) {
  auto worker = sys.spawn([] {
    return behavior{
      [](int32_t value) { return value * 2; },
    };
  });
  auto fd = testee_socket_guard.release();
  auto app_uptr = app_t::make(sys, mpx, worker);
  auto app = app_uptr.get();
  auto transport = net::octet_stream::transport::make(fd, std::move(app_uptr));
  auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
  if (auto err = mgr->start())
    CAF_FAIL("mgr->start() failed: " << err);
  send(input);
  run_while([&] { return app->consumed_bytes != input.size(); });
  expect((int32_t), to(worker).with(123));
  std::string_view expected_response = "246\n";
  run_while([&] { return recv_buf.size() < expected_response.size(); });
  std::string_view received_response{reinterpret_cast<char*>(recv_buf.data()),
                                     recv_buf.size()};
  CAF_CHECK_EQUAL(received_response, expected_response);
  self_socket_guard.reset();
}

CAF_TEST_FIXTURE_SCOPE_END()
