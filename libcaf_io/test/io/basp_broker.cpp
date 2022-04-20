// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE io.basp_broker

#include "caf/io/basp_broker.hpp"

#include "io-test.hpp"

#include <array>
#include <condition_variable>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

#include "caf/all.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/io/all.hpp"
#include "caf/io/network/interfaces.hpp"
#include "caf/io/network/test_multiplexer.hpp"

using namespace caf;
using namespace caf::io;

namespace {

using caf::make_message_id;

struct anything {};

anything any_vals;

template <class T>
struct maybe {
  maybe(T x) : val(std::move(x)) {
    // nop
  }

  maybe(anything) {
    // nop
  }

  std::optional<T> val;
};

template <class T>
std::string to_string(const maybe<T>& x) {
  return deep_to_string(x.val);
}

template <class T>
bool operator==(const maybe<T>& x, const T& y) {
  return x.val ? x.val == y : true;
}

constexpr uint8_t no_flags = 0;
constexpr uint64_t no_operation_data = 0;
constexpr uint64_t default_operation_data = make_message_id().integer_value();

constexpr uint32_t num_remote_nodes = 2;

constexpr uint64_t spawn_serv_id = basp::header::spawn_server_id;

constexpr uint64_t config_serv_id = basp::header::config_server_id;

std::string hexstr(const byte_buffer& buf) {
  // TODO: re-implement hex-formatted option
  return deep_to_string(buf);
}

struct node {
  std::string name;
  node_id id;
  connection_handle connection;
  union {
    scoped_actor dummy_actor;
  };

  node() {
    // nop
  }

  ~node() {
    // nop
  }
};

class fixture {
public:
  fixture(bool autoconn = false)
    : sys(cfg.load<io::middleman, network::test_multiplexer>()
            .set("caf.middleman.enable-automatic-connections", autoconn)
            .set("caf.middleman.heartbeat-interval", timespan{0})
            .set("caf.middleman.connection-timeout", timespan{0})
            .set("caf.middleman.workers", size_t{0})
            .set("caf.scheduler.policy", autoconn ? "testing" : "stealing")
            .set("caf.logger.inline-output", true)
            .set("caf.logger.console.verbosity", "debug")
            .set("caf.middleman.attach-utility-actors", autoconn)) {
    app_ids.emplace_back(std::string{defaults::middleman::app_identifier});
    auto& mm = sys.middleman();
    mpx_ = dynamic_cast<network::test_multiplexer*>(&mm.backend());
    CAF_REQUIRE(mpx_ != nullptr);
    CAF_REQUIRE(&sys == &mpx_->system());
    auto hdl = mm.named_broker<basp_broker>("BASP");
    aut_ = static_cast<basp_broker*>(actor_cast<abstract_actor*>(hdl));
    this_node_ = sys.node();
    self_.reset(new scoped_actor{sys});
    ahdl_ = accept_handle::from_int(1);
    aut_->add_doorman(mpx_->new_doorman(ahdl_, 1u));
    registry_ = &sys.registry();
    registry_->put((*self_)->id(), actor_cast<strong_actor_ptr>(*self_));
    // first remote node is everything of this_node + 1, then +2, etc.
    auto pid = get<hashed_node_id>(this_node_->content).process_id;
    auto hid = get<hashed_node_id>(this_node_->content).host;
    for (uint32_t i = 0; i < num_remote_nodes; ++i) {
      auto& n = nodes_[i];
      for (auto& c : hid)
        ++c;
      n.id = make_node_id(++pid, hid);
      n.connection = connection_handle::from_int(i + 1);
      new (&n.dummy_actor) scoped_actor(sys);
      // register all pseudo remote actors in the registry
      registry_->put(n.dummy_actor->id(),
                     actor_cast<strong_actor_ptr>(n.dummy_actor));
    }
    // make sure all init messages are handled properly
    mpx_->flush_runnables();
    nodes_[0].name = "Jupiter";
    nodes_[1].name = "Mars";
    CAF_REQUIRE_NOT_EQUAL(jupiter().connection, mars().connection);
    MESSAGE("Earth:   " << to_string(this_node_));
    MESSAGE("Jupiter: " << to_string(jupiter().id));
    MESSAGE("Mars:    " << to_string(mars().id));
    CAF_REQUIRE_NOT_EQUAL(this_node_, jupiter().id);
    CAF_REQUIRE_NOT_EQUAL(jupiter().id, mars().id);
  }

  ~fixture() {
    this_node_ = none;
    self_.reset();
    for (auto& n : nodes_) {
      n.id = none;
      n.dummy_actor.~scoped_actor();
    }
  }

  uint32_t serialized_size(const message& msg) {
    byte_buffer buf;
    binary_serializer sink{mpx_, buf};
    if (!sink.apply(msg))
      CAF_FAIL("failed to serialize message: " << sink.get_error());
    return static_cast<uint32_t>(buf.size());
  }

  node& jupiter() {
    return nodes_[0];
  }

  node& mars() {
    return nodes_[1];
  }

  // our "virtual communication backend"
  network::test_multiplexer* mpx() {
    return mpx_;
  }

  // actor-under-test
  basp_broker* aut() {
    return aut_;
  }

  // our node ID
  node_id& this_node() {
    return this_node_;
  }

  // an actor reference representing a local actor
  scoped_actor& self() {
    return *self_;
  }

  // implementation of the Binary Actor System Protocol
  basp::instance& instance() {
    return aut()->instance;
  }

  // our routing table (filled by BASP)
  basp::routing_table& tbl() {
    return aut()->instance.tbl();
  }

  // access to proxy instances
  proxy_registry& proxies() {
    return aut()->proxies();
  }

  // stores the singleton pointer for convenience
  actor_registry* registry() {
    return registry_;
  }

  using payload_writer = basp::instance::payload_writer;

  template <class... Ts>
  void to_payload(byte_buffer& buf, const Ts&... xs) {
    binary_serializer sink{mpx_, buf};
    if (!(sink.apply(xs) && ...))
      CAF_FAIL("failed to serialize payload: " << sink.get_error());
  }

  void to_buf(byte_buffer& buf, basp::header& hdr, payload_writer* writer) {
    instance().write(mpx_, buf, hdr, writer);
  }

  template <class... Ts>
  void to_buf(byte_buffer& buf, basp::header& hdr, payload_writer* writer,
              const Ts&... xs) {
    auto pw = make_callback([&](binary_serializer& sink) {
      if (writer != nullptr && !(*writer)(sink))
        return false;
      return (sink.apply(xs) && ...);
    });
    to_buf(buf, hdr, &pw);
  }

  std::pair<basp::header, byte_buffer> from_buf(const byte_buffer& buf) {
    basp::header hdr;
    binary_deserializer source{mpx_, buf};
    if (!source.apply(hdr))
      CAF_FAIL("failed to deserialize header: " << source.get_error());
    byte_buffer payload;
    if (hdr.payload_len > 0) {
      std::copy(buf.begin() + basp::header_size, buf.end(),
                std::back_inserter(payload));
    }
    return {hdr, std::move(payload)};
  }

  void connect_node(node& n, std::optional<accept_handle> ax = std::nullopt,
                    actor_id published_actor_id = invalid_actor_id,
                    const std::set<std::string>& published_actor_ifs = {}) {
    auto src = ax ? *ax : ahdl_;
    MESSAGE("connect remote node " << n.name
                                   << ", connection ID = " << n.connection.id()
                                   << ", acceptor ID = " << src.id());
    auto hdl = n.connection;
    mpx_->add_pending_connect(src, hdl);
    mpx_->accept_connection(src);
    // technically, the server handshake arrives
    // before we send the client handshake
    mock(hdl,
         {basp::message_type::client_handshake, 0, 0, 0, invalid_actor_id,
          invalid_actor_id},
         n.id)
      .receive(hdl, basp::message_type::server_handshake, no_flags, any_vals,
               basp::version, invalid_actor_id, invalid_actor_id, this_node(),
               app_ids, published_actor_id, published_actor_ifs)
      // upon receiving our client handshake, BASP will check
      // whether there is a SpawnServ actor on this node
      .receive(hdl, basp::message_type::direct_message,
               basp::header::named_receiver_flag, any_vals,
               default_operation_data, any_vals, spawn_serv_id,
               std::vector<strong_actor_ptr>{},
               make_message(sys_atom_v, get_atom_v, "info"));
    // test whether basp instance correctly updates the
    // routing table upon receiving client handshakes
    auto path = tbl().lookup(n.id);
    CAF_REQUIRE(path);
    CHECK_EQ(path->hdl, n.connection);
    CHECK_EQ(path->next_hop, n.id);
  }

  auto read_from_out_buf(connection_handle hdl) {
    MESSAGE("read from output buffer for connection " << hdl.id());
    auto& buf = mpx_->output_buffer(hdl);
    while (buf.size() < basp::header_size)
      mpx()->exec_runnable();
    auto result = from_buf(buf);
    buf.erase(buf.begin(),
              buf.begin() + basp::header_size + result.first.payload_len);
    return result;
  }

  void dispatch_out_buf(connection_handle hdl) {
    basp::header hdr;
    byte_buffer buf;
    std::tie(hdr, buf) = read_from_out_buf(hdl);
    MESSAGE("dispatch output buffer for connection " << hdl.id());
    CAF_REQUIRE_EQUAL(hdr.operation, basp::message_type::direct_message);
    binary_deserializer source{mpx_, buf};
    std::vector<strong_actor_ptr> stages;
    message msg;
    if (!source.apply(stages) || !source.apply(msg))
      CAF_FAIL("deserialization failed: " << source.get_error());
    auto src = actor_cast<strong_actor_ptr>(registry_->get(hdr.source_actor));
    auto dest = registry_->get(hdr.dest_actor);
    CAF_REQUIRE(dest);
    dest->enqueue(make_mailbox_element(src, make_message_id(),
                                       std::move(stages), std::move(msg)),
                  nullptr);
  }

  class mock_t {
  public:
    mock_t(fixture* thisptr) : this_(thisptr) {
      // nop
    }

    mock_t(mock_t&&) = default;

    ~mock_t() {
      if (num > 1)
        MESSAGE("implementation under test responded with "
                << (num - 1) << " BASP message" << (num > 2 ? "s" : ""));
    }

    template <class... Ts>
    mock_t& receive(connection_handle hdl, maybe<basp::message_type> operation,
                    maybe<uint8_t> flags, maybe<uint32_t> payload_len,
                    maybe<uint64_t> operation_data,
                    maybe<actor_id> source_actor, maybe<actor_id> dest_actor,
                    const Ts&... xs) {
      MESSAGE("expect #" << num);
      byte_buffer buf;
      this_->to_payload(buf, xs...);
      auto& ob = this_->mpx()->output_buffer(hdl);
      while (this_->mpx()->try_exec_runnable()) {
        // repeat
      }
      MESSAGE("output buffer has " << ob.size() << " bytes");
      basp::header hdr;
      { // lifetime scope of source
        binary_deserializer source{this_->mpx(), ob};
        if (!source.apply(hdr))
          CAF_FAIL("failed to deserialize header: " << source.get_error());
      }
      byte_buffer payload;
      if (hdr.payload_len > 0) {
        CAF_REQUIRE(ob.size() >= (basp::header_size + hdr.payload_len));
        auto first = ob.begin() + basp::header_size;
        auto end = first + hdr.payload_len;
        payload.assign(first, end);
        MESSAGE("erase " << std::distance(ob.begin(), end)
                         << " bytes from output buffer");
        ob.erase(ob.begin(), end);
      } else {
        ob.erase(ob.begin(), ob.begin() + basp::header_size);
      }
      CHECK_EQ(operation, hdr.operation);
      CHECK_EQ(flags, static_cast<uint8_t>(hdr.flags));
      CHECK_EQ(payload_len, hdr.payload_len);
      CHECK_EQ(operation_data, hdr.operation_data);
      CHECK_EQ(source_actor, hdr.source_actor);
      CHECK_EQ(dest_actor, hdr.dest_actor);
      CAF_REQUIRE_EQUAL(buf.size(), payload.size());
      CAF_REQUIRE_EQUAL(hexstr(buf), hexstr(payload));
      ++num;
      return *this;
    }

  private:
    fixture* this_;
    size_t num = 1;
  };

  template <class... Ts>
  mock_t mock(connection_handle hdl, basp::header hdr, const Ts&... xs) {
    byte_buffer buf;
    to_buf(buf, hdr, nullptr, xs...);
    MESSAGE("virtually send " << to_string(hdr.operation) << " with "
                              << (buf.size() - basp::header_size)
                              << " bytes payload");
    mpx()->virtual_send(hdl, buf);
    return {this};
  }

  mock_t mock() {
    return {this};
  }

  actor_system_config cfg;
  actor_system sys;
  std::vector<std::string> app_ids;

private:
  basp_broker* aut_;
  accept_handle ahdl_;
  network::test_multiplexer* mpx_;
  node_id this_node_;
  std::unique_ptr<scoped_actor> self_;
  std::array<node, num_remote_nodes> nodes_;
  /*
  array<node_id, num_remote_nodes> remote_node_;
  array<connection_handle, num_remote_nodes> remote_hdl_;
  array<unique_ptr<scoped_actor>, num_remote_nodes> pseudo_remote_;
  */
  actor_registry* registry_;
};

class autoconn_enabled_fixture : public fixture {
public:
  using scheduler_type = caf::scheduler::test_coordinator;

  scheduler_type& sched;
  middleman_actor mma;

  autoconn_enabled_fixture()
    : fixture(true),
      sched(dynamic_cast<scheduler_type&>(sys.scheduler())),
      mma(sys.middleman().actor_handle()) {
    // nop
  }

  void publish(const actor& whom, uint16_t port) {
    using sig_t = std::set<std::string>;
    scoped_actor tmp{sys};
    sig_t sigs;
    tmp->send(mma, publish_atom_v, port, actor_cast<strong_actor_ptr>(whom),
              std::move(sigs), "", false);
    expect((publish_atom, uint16_t, strong_actor_ptr, sig_t, std::string, bool),
           from(tmp).to(mma));
    expect((uint16_t), from(mma).to(tmp).with(port));
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(empty_server_handshake) {
  // test whether basp instance correctly sends a
  // server handshake when there's no actor published
  byte_buffer buf;
  instance().write_server_handshake(mpx(), buf, std::nullopt);
  basp::header hdr;
  byte_buffer payload;
  std::tie(hdr, payload) = from_buf(buf);
  basp::header expected{basp::message_type::server_handshake,
                        0,
                        static_cast<uint32_t>(payload.size()),
                        basp::version,
                        invalid_actor_id,
                        invalid_actor_id};
  CHECK(basp::valid(hdr));
  CHECK(basp::is_handshake(hdr));
  CHECK_EQ(deep_to_string(hdr), deep_to_string(expected));
}

CAF_TEST(non_empty_server_handshake) {
  // test whether basp instance correctly sends a
  // server handshake with published actors
  byte_buffer buf;
  instance().add_published_actor(4242, actor_cast<strong_actor_ptr>(self()),
                                 {"caf::replies_to<@u16>::with<@u16>"});
  instance().write_server_handshake(mpx(), buf, uint16_t{4242});
  basp::header hdr;
  byte_buffer payload;
  std::tie(hdr, payload) = from_buf(buf);
  basp::header expected{basp::message_type::server_handshake,
                        0,
                        static_cast<uint32_t>(payload.size()),
                        basp::version,
                        invalid_actor_id,
                        invalid_actor_id};
  CHECK(basp::valid(hdr));
  CHECK(basp::is_handshake(hdr));
  CHECK_EQ(deep_to_string(hdr), deep_to_string(expected));
  byte_buffer expected_payload;
  std::set<std::string> ifs{"caf::replies_to<@u16>::with<@u16>"};
  binary_serializer sink{nullptr, expected_payload};
  auto id = self()->id();
  if (!sink.apply(instance().this_node()) //
      || !sink.apply(app_ids)             //
      || !sink.apply(id)                  //
      || !sink.apply(ifs))
    CAF_FAIL("serializing handshake failed: " << sink.get_error());
  CHECK_EQ(hexstr(payload), hexstr(expected_payload));
}

CAF_TEST(remote_address_and_port) {
  MESSAGE("connect to Mars");
  connect_node(mars());
  auto mm = sys.middleman().actor_handle();
  MESSAGE("ask MM about node ID of Mars");
  self()->send(mm, get_atom_v, mars().id);
  do {
    mpx()->exec_runnable();
  } while (self()->mailbox().empty());
  MESSAGE("receive result of MM");
  self()->receive(
    [&](const node_id& nid, const std::string& addr, uint16_t port) {
      CHECK_EQ(nid, mars().id);
      // all test nodes have address "test" and connection handle ID as port
      CHECK_EQ(addr, "test");
      CHECK_EQ(port, mars().connection.id());
    });
}

CAF_TEST(client_handshake_and_dispatch) {
  MESSAGE("connect to Jupiter");
  connect_node(jupiter());
  // send a message via `dispatch` from node 0
  mock(jupiter().connection,
       {basp::message_type::direct_message, 0, 0, 0,
        jupiter().dummy_actor->id(), self()->id()},
       std::vector<strong_actor_ptr>{}, make_message(1, 2, 3))
    .receive(jupiter().connection, basp::message_type::monitor_message,
             no_flags, any_vals, no_operation_data, invalid_actor_id,
             jupiter().dummy_actor->id(), this_node(), jupiter().id);
  // must've created a proxy for our remote actor
  CAF_REQUIRE(proxies().count_proxies(jupiter().id) == 1);
  // must've send remote node a message that this proxy is monitored now
  // receive the message
  self()->receive([](int a, int b, int c) {
    CHECK_EQ(a, 1);
    CHECK_EQ(b, 2);
    CHECK_EQ(c, 3);
    return a + b + c;
  });
  MESSAGE("exec message of forwarding proxy");
  mpx()->exec_runnable();
  // deserialize and send message from out buf
  dispatch_out_buf(jupiter().connection);
  jupiter().dummy_actor->receive([](int i) { CHECK_EQ(i, 6); });
}

CAF_TEST(message_forwarding) {
  // connect two remote nodes
  connect_node(jupiter());
  connect_node(mars());
  auto msg = make_message(1, 2, 3);
  // send a message from node 0 to node 1, forwarded by this node
  mock(jupiter().connection,
       {basp::message_type::routed_message, 0, 0, default_operation_data,
        invalid_actor_id, mars().dummy_actor->id()},
       jupiter().id, mars().id, std::vector<strong_actor_ptr>{}, msg)
    .receive(mars().connection, basp::message_type::routed_message, no_flags,
             any_vals, default_operation_data, invalid_actor_id,
             mars().dummy_actor->id(), jupiter().id, mars().id,
             std::vector<strong_actor_ptr>{}, msg);
}

CAF_TEST(publish_and_connect) {
  auto ax = accept_handle::from_int(4242);
  mpx()->provide_acceptor(4242, ax);
  auto res = sys.middleman().publish(self(), 4242);
  CAF_REQUIRE(res == 4242);
  mpx()->flush_runnables(); // process publish message in basp_broker
  connect_node(jupiter(), ax, self()->id());
}

CAF_TEST(remote_actor_and_send) {
  constexpr const char* lo = "localhost";
  MESSAGE("self: " << to_string(self()->address()));
  mpx()->provide_scribe(lo, 4242, jupiter().connection);
  CAF_REQUIRE(mpx()->has_pending_scribe(lo, 4242));
  auto mm1 = sys.middleman().actor_handle();
  actor result;
  auto f = self()->request(mm1, infinite, connect_atom_v, lo, uint16_t{4242});
  // wait until BASP broker has received and processed the connect message
  while (!aut()->valid(jupiter().connection))
    mpx()->exec_runnable();
  CAF_REQUIRE(!mpx()->has_pending_scribe(lo, 4242));
  // build a fake server handshake containing the id of our first pseudo actor
  MESSAGE("server handshake => client handshake + proxy announcement");
  auto na = registry()->named_actors();
  mock(jupiter().connection,
       {basp::message_type::server_handshake, 0, 0, basp::version,
        invalid_actor_id, invalid_actor_id},
       jupiter().id, app_ids, jupiter().dummy_actor->id(),
       std::set<std::string>{})
    .receive(jupiter().connection, basp::message_type::client_handshake,
             no_flags, any_vals, no_operation_data, invalid_actor_id,
             invalid_actor_id, this_node())
    .receive(jupiter().connection, basp::message_type::direct_message,
             basp::header::named_receiver_flag, any_vals,
             default_operation_data, any_vals, spawn_serv_id,
             std::vector<strong_actor_ptr>{},
             make_message(sys_atom_v, get_atom_v, "info"))
    .receive(jupiter().connection, basp::message_type::monitor_message,
             no_flags, any_vals, no_operation_data, invalid_actor_id,
             jupiter().dummy_actor->id(), this_node(), jupiter().id);
  MESSAGE("BASP broker should've send the proxy");
  f.receive(
    [&](node_id nid, strong_actor_ptr res, std::set<std::string> ifs) {
      CAF_REQUIRE(res);
      auto aptr = actor_cast<abstract_actor*>(res);
      CAF_REQUIRE(dynamic_cast<forwarding_actor_proxy*>(aptr) != nullptr);
      CHECK_EQ(proxies().count_proxies(jupiter().id), 1u);
      CHECK_EQ(nid, jupiter().id);
      CHECK_EQ(res->node(), jupiter().id);
      CHECK_EQ(res->id(), jupiter().dummy_actor->id());
      CHECK(ifs.empty());
      auto proxy = proxies().get(jupiter().id, jupiter().dummy_actor->id());
      CAF_REQUIRE(proxy != nullptr);
      CAF_REQUIRE(proxy == res);
      result = actor_cast<actor>(res);
    },
    [&](error& err) { CAF_FAIL("error: " << err); });
  MESSAGE("send message to proxy");
  anon_send(actor_cast<actor>(result), 42);
  mpx()->flush_runnables();
  //  mpx()->exec_runnable(); // process forwarded message in basp_broker
  mock().receive(jupiter().connection, basp::message_type::direct_message,
                 no_flags, any_vals, default_operation_data, invalid_actor_id,
                 jupiter().dummy_actor->id(), std::vector<strong_actor_ptr>{},
                 make_message(42));
  auto msg = make_message("hi there!");
  MESSAGE("send message via BASP (from proxy)");
  mock(jupiter().connection,
       {basp::message_type::direct_message, 0, 0, 0,
        jupiter().dummy_actor->id(), self()->id()},
       std::vector<strong_actor_ptr>{}, make_message("hi there!"));
  self()->receive([&](const std::string& str) {
    CHECK_EQ(to_string(self()->current_sender()), to_string(result));
    CHECK_EQ(self()->current_sender(), result.address());
    CHECK_EQ(str, "hi there!");
  });
}

CAF_TEST(actor_serialize_and_deserialize) {
  auto testee_impl = [](event_based_actor* testee_self) -> behavior {
    testee_self->set_default_handler(reflect_and_quit);
    return {[] {
      // nop
    }};
  };
  connect_node(jupiter());
  auto prx = proxies().get_or_put(jupiter().id, jupiter().dummy_actor->id());
  mock().receive(jupiter().connection, basp::message_type::monitor_message,
                 no_flags, any_vals, no_operation_data, invalid_actor_id,
                 prx->id(), this_node(), prx->node());
  CHECK_EQ(prx->node(), jupiter().id);
  CHECK_EQ(prx->id(), jupiter().dummy_actor->id());
  auto testee = sys.spawn(testee_impl);
  registry()->put(testee->id(), actor_cast<strong_actor_ptr>(testee));
  MESSAGE("send message via BASP (from proxy)");
  auto msg = make_message(actor_cast<actor_addr>(prx));
  mock(jupiter().connection,
       {basp::message_type::direct_message, 0, 0, 0, prx->id(), testee->id()},
       std::vector<strong_actor_ptr>{}, msg);
  // testee must've responded (process forwarded message in BASP broker)
  MESSAGE("wait until BASP broker writes to its output buffer");
  while (mpx()->output_buffer(jupiter().connection).empty())
    mpx()->exec_runnable(); // process forwarded message in basp_broker
  // output buffer must contain the reflected message
  mock().receive(jupiter().connection, basp::message_type::direct_message,
                 no_flags, any_vals, default_operation_data, testee->id(),
                 prx->id(), std::vector<strong_actor_ptr>{}, msg);
}

CAF_TEST(indirect_connections) {
  // this node receives a message from jupiter via mars and responds via mars
  // and any ad-hoc automatic connection requests are ignored
  MESSAGE("self: " << to_string(self()->address()));
  MESSAGE("publish self at port 4242");
  auto ax = accept_handle::from_int(4242);
  mpx()->provide_acceptor(4242, ax);
  sys.middleman().publish(self(), 4242);
  mpx()->flush_runnables(); // process publish message in basp_broker
  MESSAGE("connect to Mars");
  connect_node(mars(), ax, self()->id());
  MESSAGE("actor from Jupiter sends a message to us via Mars");
  auto mx = mock(mars().connection,
                 {basp::message_type::routed_message, 0, 0, 0,
                  jupiter().dummy_actor->id(), self()->id()},
                 jupiter().id, this_node(), std::vector<strong_actor_ptr>{},
                 make_message("hello from jupiter!"));
  MESSAGE("expect ('sys', 'get', \"info\") from Earth to Jupiter at Mars");
  // this asks Jupiter if it has a 'SpawnServ'
  mx.receive(mars().connection, basp::message_type::routed_message,
             basp::header::named_receiver_flag, any_vals,
             default_operation_data, any_vals, spawn_serv_id, this_node(),
             jupiter().id, std::vector<strong_actor_ptr>{},
             make_message(sys_atom_v, get_atom_v, "info"));
  MESSAGE("expect announce_proxy message at Mars from Earth to Jupiter");
  mx.receive(mars().connection, basp::message_type::monitor_message, no_flags,
             any_vals, no_operation_data, invalid_actor_id,
             jupiter().dummy_actor->id(), this_node(), jupiter().id);
  MESSAGE("receive message from jupiter");
  self()->receive([](const std::string& str) -> std::string {
    CHECK_EQ(str, "hello from jupiter!");
    return "hello from earth!";
  });
  mpx()->exec_runnable(); // process forwarded message in basp_broker
  mock().receive(mars().connection, basp::message_type::routed_message,
                 no_flags, any_vals, default_operation_data, self()->id(),
                 jupiter().dummy_actor->id(), this_node(), jupiter().id,
                 std::vector<strong_actor_ptr>{},
                 make_message("hello from earth!"));
}

END_FIXTURE_SCOPE()

BEGIN_FIXTURE_SCOPE(autoconn_enabled_fixture)

CAF_TEST(automatic_connection) {
  // Utility helper for verifying routing tables.
  auto check_node_in_tbl = [&](node& n) {
    auto hdl = tbl().lookup_direct(n.id);
    CAF_REQUIRE(hdl);
  };
  // Setup.
  mpx()->provide_scribe("jupiter", 8080, jupiter().connection);
  CHECK(mpx()->has_pending_scribe("jupiter", 8080));
  MESSAGE("self: " << to_string(self()->address()));
  auto ax = accept_handle::from_int(4242);
  mpx()->provide_acceptor(4242, ax);
  publish(self(), 4242);
  // Process publish message in basp_broker.
  mpx()->flush_runnables();
  MESSAGE("connect to mars");
  connect_node(mars(), ax, self()->id());
  check_node_in_tbl(mars());
  MESSAGE("simulate that a message from jupiter travels over mars");
  mock(mars().connection,
       {basp::message_type::routed_message, 0, 0,
        make_message_id().integer_value(), jupiter().dummy_actor->id(),
        self()->id()},
       jupiter().id, this_node(), std::vector<strong_actor_ptr>{},
       make_message("hello from jupiter!"))
    .receive(mars().connection, basp::message_type::routed_message,
             basp::header::named_receiver_flag, any_vals,
             default_operation_data, any_vals, spawn_serv_id, this_node(),
             jupiter().id, std::vector<strong_actor_ptr>{},
             make_message(sys_atom_v, get_atom_v, "info"))
    .receive(mars().connection, basp::message_type::routed_message,
             basp::header::named_receiver_flag, any_vals,
             default_operation_data,
             any_vals, // actor ID of an actor spawned by the BASP broker
             config_serv_id, this_node(), jupiter().id,
             std::vector<strong_actor_ptr>{},
             make_message(get_atom_v, "basp.default-connectivity-tcp"))
    .receive(mars().connection, basp::message_type::monitor_message, no_flags,
             any_vals, no_operation_data, invalid_actor_id,
             jupiter().dummy_actor->id(), this_node(), jupiter().id);
  CHECK_EQ(mpx()->output_buffer(mars().connection).size(), 0u);
  CHECK_EQ(tbl().lookup_indirect(jupiter().id), mars().id);
  CHECK_EQ(tbl().lookup_indirect(mars().id), none);
  auto connection_helper_actor = sys.latest_actor_id();
  CHECK_EQ(mpx()->output_buffer(mars().connection).size(), 0u);
  // Create a dummy config server and respond to the name lookup.
  MESSAGE("receive ConfigServ of jupiter");
  network::address_listing res;
  res[network::protocol::ipv4].emplace_back("jupiter");
  mock(mars().connection,
       {basp::message_type::routed_message, 0, 0,
        make_message_id().integer_value(), invalid_actor_id,
        connection_helper_actor},
       this_node(), this_node(), std::vector<strong_actor_ptr>{},
       make_message("basp.default-connectivity-tcp",
                    make_message(uint16_t{8080}, std::move(res))));
  // Our connection helper should now connect to jupiter and send the scribe
  // handle over to the BASP broker.
  while (mpx()->has_pending_scribe("jupiter", 8080)) {
    sched.run();
    mpx()->flush_runnables();
  }
  CAF_REQUIRE(mpx()->output_buffer(mars().connection).empty());
  // Send handshake from jupiter.
  mock(jupiter().connection,
       {basp::message_type::server_handshake, no_flags, 0, basp::version,
        invalid_actor_id, invalid_actor_id},
       jupiter().id, app_ids, jupiter().dummy_actor->id(),
       std::set<std::string>{})
    .receive(jupiter().connection, basp::message_type::client_handshake,
             no_flags, any_vals, no_operation_data, invalid_actor_id,
             invalid_actor_id, this_node());
  CHECK_EQ(tbl().lookup_indirect(jupiter().id), none);
  CHECK_EQ(tbl().lookup_indirect(mars().id), none);
  check_node_in_tbl(jupiter());
  check_node_in_tbl(mars());
  MESSAGE("receive message from jupiter");
  self()->receive([](const std::string& str) -> std::string {
    CHECK_EQ(str, "hello from jupiter!");
    return "hello from earth!";
  });
  mpx()->exec_runnable(); // process forwarded message in basp_broker
  MESSAGE("response message must take direct route now");
  mock().receive(jupiter().connection, basp::message_type::direct_message,
                 no_flags, any_vals, make_message_id().integer_value(),
                 self()->id(), jupiter().dummy_actor->id(),
                 std::vector<strong_actor_ptr>{},
                 make_message("hello from earth!"));
  CHECK_EQ(mpx()->output_buffer(mars().connection).size(), 0u);
}

END_FIXTURE_SCOPE()
