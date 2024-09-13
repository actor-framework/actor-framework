// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/typed_actor_shell.hpp"

#include "caf/test/test.hpp"

#include "caf/net/make_actor_shell.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/socket_id.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/anon_mail.hpp"
#include "caf/byte_span.hpp"
#include "caf/callback.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/single.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/type_id.hpp"
#include "caf/typed_actor.hpp"

using namespace caf;
using namespace std::literals;

namespace {

struct testee_trait {
  using signatures = type_list<result<void>(std::string)>;
};

using testee_actor = typed_actor<testee_trait>;

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(typed_actor_shell_test, caf::first_custom_type_id + 10)
  CAF_ADD_TYPE_ID(typed_actor_shell_test, (testee_actor))
CAF_END_TYPE_ID_BLOCK(typed_actor_shell_test)

namespace {

using svec = std::vector<std::string>;

class app_t : public net::octet_stream::upper_layer {
public:
  using on_start_fn = std::function<void(app_t*)>;

  app_t(actor hdl, on_start_fn fn)
    : receiver(std::move(hdl)), on_start(std::move(fn)) {
    // nop
  }

  static auto make(actor hdl, on_start_fn fn = {}) {
    return std::make_unique<app_t>(std::move(hdl), std::move(fn));
  }

  error start(net::octet_stream::lower_layer* down) override {
    this->down = down;
    self = net::make_actor_shell<testee_actor>(down->manager());
    anon_mail(actor_cast<testee_actor>(self->ctrl())).send(receiver);
    self->set_behavior([down](const std::string& line) {
      down->begin_output();
      auto& buf = down->output_buffer();
      auto bytes = as_bytes(make_span(line));
      buf.insert(buf.end(), bytes.begin(), bytes.end());
      down->end_output();
    });
    down->configure_read(net::receive_policy::up_to(2048));
    if (on_start) {
      on_start(this);
      on_start = nullptr;
    }
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
      auto pos = std::distance(buf.begin(), i);
      std::string line;
      line.reserve(pos);
      std::transform(buf.begin(), i, std::back_inserter(line),
                     [](auto x) { return static_cast<char>(x); });
      anon_mail(std::move(line)).send(receiver);
      return pos + 1;
    }
    return 0;
  }

  // Pointer to the layer below.
  net::octet_stream::lower_layer* down;

  // Handle to the actor that receives the lines that we read from the socket.
  actor receiver;

  std::function<void(app_t*)> on_start;

  // Actor shell representing this app.
  net::typed_actor_shell_ptr<testee_trait> self;
};

actor_system_config& init(actor_system_config& cfg) {
  cfg.set("caf.scheduler.max-threads", 2);
  cfg.load<net::middleman>();
  return cfg;
}

auto to_str(caf::byte_span buffer) {
  return std::string_view{reinterpret_cast<const char*>(buffer.data()),
                          buffer.size()};
}

struct fixture {
  fixture() : sys(init(cfg)) {
    auto fd_pair = net::make_stream_socket_pair();
    if (!fd_pair) {
      CAF_RAISE_ERROR("make_stream_socket_pair failed");
    }
    std::tie(fd1, fd2) = *fd_pair;
  }

  ~fixture() {
    if (fd1 != net::invalid_socket)
      net::close(fd1);
    if (fd2 != net::invalid_socket)
      net::close(fd2);
  }

  actor_system_config cfg;
  actor_system sys;
  net::stream_socket fd1;
  net::stream_socket fd2;
};

} // namespace

WITH_FIXTURE(fixture) {

TEST("actor shells can receive messages") {
  scoped_actor self{sys};
  auto& mpx = sys.network_manager().mpx();
  auto app = app_t::make(actor{self});
  auto transport = net::octet_stream::transport::make(fd1, std::move(app));
  auto mgr = net::socket_manager::make(&mpx, std::move(transport));
  mpx.start(mgr);
  fd1.id = net::invalid_socket_id;
  testee_actor hdl;
  self->receive([&hdl](testee_actor& x) { hdl = x; },
                after(1s) >> [this] { fail("shell did not send its handle"); });
  auto msg = "hello actor shell\n"s;
  anon_mail(msg).send(hdl);
  byte_buffer buf;
  buf.resize(msg.size());
  auto n = net::read(fd2, buf);
  check_eq(n, static_cast<ptrdiff_t>(msg.size()));
  check_eq(to_str(buf), msg);
}

TEST("actor shells can send asynchronous messages") {
  scoped_actor self{sys};
  auto& mpx = sys.network_manager().mpx();
  auto app = app_t::make(actor{self});
  auto transport = net::octet_stream::transport::make(fd1, std::move(app));
  auto mgr = net::socket_manager::make(&mpx, std::move(transport));
  mpx.start(mgr);
  fd1.id = net::invalid_socket_id;
  auto msg = "hello actor shell\n"s;
  auto n = net::write(fd2, as_bytes(make_span(msg)));
  check_eq(n, static_cast<ptrdiff_t>(msg.size()));
  self->receive(
    [this](const std::string& str) { check_eq(str, "hello actor shell"); },
    after(1s) >> [this] { fail("shell did not send its handle"); });
}

TEST("actor shells can send request messages") {
  scoped_actor self{sys};
  auto self_hdl = actor_cast<actor>(self);
  auto cb = [self_hdl](app_t* app) {
    app->self->mail(get_atom_v)
      .request(self_hdl, 1s)
      .then([app](const std::string& line) {
        auto* down = app->down;
        down->begin_output();
        auto& buf = down->output_buffer();
        auto bytes = as_bytes(make_span(line));
        buf.insert(buf.end(), bytes.begin(), bytes.end());
        down->end_output();
      });
  };
  auto& mpx = sys.network_manager().mpx();
  auto app = app_t::make(actor{self}, cb);
  auto transport = net::octet_stream::transport::make(fd1, std::move(app));
  auto mgr = net::socket_manager::make(&mpx, std::move(transport));
  mpx.start(mgr);
  fd1.id = net::invalid_socket_id;
  auto msg = "request"s;
  self->receive([msg](get_atom) { return msg; },
                after(1s) >> [this] { fail("shell did not send a request"); });
  byte_buffer buf;
  buf.resize(msg.size());
  auto n = net::read(fd2, buf);
  check_eq(n, static_cast<ptrdiff_t>(msg.size()));
  check_eq(to_str(buf), msg);
}

TEST("actor shells can use flows") {
  scoped_actor self{sys};
  auto self_hdl = actor_cast<actor>(self);
  auto cb = [self_hdl](app_t* app) {
    app->self->mail(get_atom_v)
      .request(self_hdl, 1s)
      .as_observable<std::string>()
      .for_each([app](const std::string& line) {
        auto* down = app->down;
        down->begin_output();
        auto& buf = down->output_buffer();
        auto bytes = as_bytes(make_span(line));
        buf.insert(buf.end(), bytes.begin(), bytes.end());
        down->end_output();
      });
  };
  auto& mpx = sys.network_manager().mpx();
  auto app = app_t::make(actor{self}, cb);
  auto transport = net::octet_stream::transport::make(fd1, std::move(app));
  auto mgr = net::socket_manager::make(&mpx, std::move(transport));
  mpx.start(mgr);
  fd1.id = net::invalid_socket_id;
  auto msg = "request"s;
  self->receive([msg](get_atom) { return msg; },
                after(1s) >> [this] { fail("shell did not send a request"); });
  byte_buffer buf;
  buf.resize(msg.size());
  auto n = net::read(fd2, buf);
  check_eq(n, static_cast<ptrdiff_t>(msg.size()));
  check_eq(to_str(buf), msg);
}

} // WITH_FIXTURE(fixture)

TEST_INIT() {
  init_global_meta_objects<caf::id_block::typed_actor_shell_test>();
}
