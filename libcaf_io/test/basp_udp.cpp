/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE io_basp_udp
#include "caf/test/dsl.hpp"

#include <array>
#include <mutex>
#include <memory>
#include <limits>
#include <vector>
#include <iostream>
#include <condition_variable>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "caf/io/datagram_handle.hpp"
#include "caf/io/datagram_servant.hpp"

#include "caf/deep_to_string.hpp"

#include "caf/io/network/interfaces.hpp"
#include "caf/io/network/ip_endpoint.hpp"
#include "caf/io/network/test_multiplexer.hpp"

namespace {

struct anything { };

anything any_vals;

template <class T>
struct maybe {
  maybe(T x) : val(std::move(x)) {
    // nop
  }

  maybe(anything) {
    // nop
  }

  caf::optional<T> val;
};

template <class T>
std::string to_string(const maybe<T>& x) {
  return to_string(x.val);
}

template <class T>
bool operator==(const maybe<T>& x, const T& y) {
  return x.val ? x.val == y : true;
}

template <class T>
bool operator==(const T& x, const maybe<T>& y) {
  return y.val ? x == y.val : true;
}

template <class T>
bool operator!=(const maybe<T>& x, const T& y) {
  return !(x == y);
}

template <class T>
bool operator!=(const T& x, const maybe<T>& y) {
  return !(x == y);
}

constexpr uint8_t no_flags = 0;
constexpr uint32_t no_payload = 0;
constexpr uint64_t no_operation_data = 0;

constexpr auto basp_atom = caf::atom("BASP");
constexpr auto spawn_serv_atom = caf::atom("SpawnServ");
constexpr auto config_serv_atom = caf::atom("ConfigServ");

} // namespace <anonymous>

using namespace std;
using namespace caf;
using namespace caf::io;

namespace {

constexpr uint32_t num_remote_nodes = 2;

using buffer = std::vector<char>;

std::string hexstr(const buffer& buf) {
  return deep_to_string(meta::hex_formatted(), buf);
}

struct node {
  std::string name;
  node_id id;
  datagram_handle endpoint;
  union { scoped_actor dummy_actor; };

  node() {
    // nop
  }

  ~node() {
    // nop
  }
};

class fixture {
public:
  fixture(bool autoconn = false, bool use_test_coordinator = false)
      : sys(cfg.load<io::middleman, network::test_multiplexer>()
          .set("middleman.enable-automatic-connections", autoconn)
          .set("middleman.enable-udp", true)
          .set("middleman.disable-tcp", true)
          .set("scheduler.policy", autoconn || use_test_coordinator
                                    ? caf::atom("testing")
                                    : caf::atom("stealing"))
          .set("middleman.attach-utility-actors",
               autoconn || use_test_coordinator)) {
    auto& mm = sys.middleman();
    mpx_ = dynamic_cast<network::test_multiplexer*>(&mm.backend());
    CAF_REQUIRE(mpx_ != nullptr);
    CAF_REQUIRE(&sys == &mpx_->system());
    auto hdl = mm.named_broker<basp_broker>(basp_atom);
    aut_ = static_cast<basp_broker*>(actor_cast<abstract_actor*>(hdl));
    this_node_ = sys.node();
    self_.reset(new scoped_actor{sys});
    dhdl_ = datagram_handle::from_int(1);
    aut_->add_datagram_servant(mpx_->new_datagram_servant(dhdl_, 1u));
    registry_ = &sys.registry();
    registry_->put((*self_)->id(), actor_cast<strong_actor_ptr>(*self_));
    // first remote node is everything of this_node + 1, then +2, etc.
    for (uint32_t i = 0; i < num_remote_nodes; ++i) {
      auto& n = nodes_[i];
      node_id::host_id_type tmp = this_node_.host_id();
      for (auto& c : tmp)
        c = static_cast<uint8_t>(c + i + 1);
      n.id = node_id{this_node_.process_id() + i + 1, tmp};
      n.endpoint = datagram_handle::from_int(i + 2);
      new (&n.dummy_actor) scoped_actor(sys);
      // register all pseudo remote actors in the registry
      registry_->put(n.dummy_actor->id(),
                     actor_cast<strong_actor_ptr>(n.dummy_actor));
    }
    // make sure all init messages are handled properly
    mpx_->flush_runnables();
    nodes_[0].name = "Jupiter";
    nodes_[1].name = "Mars";
    CAF_REQUIRE_NOT_EQUAL(jupiter().endpoint, mars().endpoint);
    CAF_MESSAGE("Earth:   " << to_string(this_node_) << ", ID = "
                << endpoint_handle().id());
    CAF_MESSAGE("Jupiter: " << to_string(jupiter().id) << ", ID = "
                << jupiter().endpoint.id());
    CAF_MESSAGE("Mars:    " << to_string(mars().id) << ", ID = "
                << mars().endpoint.id());
    CAF_REQUIRE_NOT_EQUAL(this_node_, jupiter().id);
    CAF_REQUIRE_NOT_EQUAL(jupiter().id,  mars().id);
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
    buffer buf;
    binary_serializer bs{mpx_, buf};
    auto e = bs(const_cast<message&>(msg));
    CAF_REQUIRE(!e);
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

  datagram_handle endpoint_handle() {
    return dhdl_;
  }

  // implementation of the Binary Actor System Protocol
  basp::instance& instance() {
    return aut()->state.instance;
  }

  // our routing table (filled by BASP)
  basp::routing_table& tbl() {
    return aut()->state.instance.tbl();
  }

  // access to proxy instances
  proxy_registry& proxies() {
    return aut()->state.proxies();
  }

  // stores the singleton pointer for convenience
  actor_registry* registry() {
    return registry_;
  }

  using payload_writer = basp::instance::payload_writer;

  template <class... Ts>
  void to_payload(binary_serializer& bs, const Ts&... xs) {
    bs(const_cast<Ts&>(xs)...);
  }

  template <class... Ts>
  void to_payload(buffer& buf, const Ts&... xs) {
    binary_serializer bs{mpx_, buf};
    to_payload(bs, xs...);
  }

  void to_buf(buffer& buf, basp::header& hdr, payload_writer* writer) {
    instance().write(mpx_, buf, hdr, writer);
  }

  template <class T, class... Ts>
  void to_buf(buffer& buf, basp::header& hdr, payload_writer* writer,
              const T& x, const Ts&... xs) {
    auto pw = make_callback([&](serializer& sink) -> error {
      if (writer)
        return error::eval([&] { return (*writer)(sink); },
                           [&] { return sink(const_cast<T&>(x)); });
      return sink(const_cast<T&>(x));
    });
    to_buf(buf, hdr, &pw, xs...);
  }

  std::pair<basp::header, buffer> from_buf(const buffer& buf) {
    basp::header hdr;
    binary_deserializer bd{mpx_, buf};
    auto e = bd(hdr);
    CAF_REQUIRE(!e);
    buffer payload;
    if (hdr.payload_len > 0) {
      std::copy(buf.begin() + basp::header_size, buf.end(),
                std::back_inserter(payload));
    }
    return {hdr, std::move(payload)};
  }

  void establish_communication(node& n,
                               optional<datagram_handle> dx = none,
                               actor_id published_actor_id = invalid_actor_id,
                               const set<string>& published_actor_ifs
                                 = std::set<std::string>{}) {
    auto src = dx ? *dx : dhdl_;
    CAF_MESSAGE("establish communication on node " << n.name
                << ", delegated servant ID = " << n.endpoint.id()
                << ", initial servant ID = " << src.id());
    // send the client handshake and receive the server handshake
    // and a dispatch_message as answers
    auto hdl = n.endpoint;
    mpx_->add_pending_endpoint(src, hdl);
    CAF_MESSAGE("Send client handshake");
    mock(src, hdl,
         {basp::message_type::client_handshake, 0, 0, 0,
          n.id, this_node(),
          invalid_actor_id, invalid_actor_id}, std::string{})
    // upon receiving the client handshake, the server should answer
    // with the server handshake and send the dispatch_message blow
    .receive(hdl,
             basp::message_type::server_handshake, no_flags,
             any_vals, basp::version, this_node(), node_id{none},
             published_actor_id, invalid_actor_id, std::string{},
             published_actor_id, published_actor_ifs)
    // upon receiving our client handshake, BASP will check
    // whether there is a SpawnServ actor on this node
    .receive(hdl,
             basp::message_type::dispatch_message,
             basp::header::named_receiver_flag, any_vals,
             no_operation_data,
             this_node(), n.id,
             any_vals, invalid_actor_id,
             spawn_serv_atom,
             std::vector<actor_addr>{},
             make_message(sys_atom::value, get_atom::value, "info"));
    // test whether basp instance correctly updates the
    // routing table upon receiving client handshakes
    auto path = tbl().lookup(n.id);
    CAF_REQUIRE(path);
    CAF_CHECK_EQUAL(path->hdl, n.endpoint);
    CAF_CHECK_EQUAL(path->next_hop, n.id);
  }

  std::pair<basp::header, buffer> read_from_out_buf(datagram_handle hdl) {
    CAF_MESSAGE("read from output buffer for endpoint " << hdl.id());
    auto& que = mpx_->output_queue(hdl);
    while (que.empty())
      mpx()->exec_runnable();
    auto result = from_buf(que.front().second);
    que.pop_front();
    return result;
  }

  void dispatch_out_buf(datagram_handle hdl) {
    basp::header hdr;
    buffer buf;
    std::tie(hdr, buf) = read_from_out_buf(hdl);
    CAF_MESSAGE("dispatch output buffer for endpoint " << hdl.id());
    CAF_REQUIRE(hdr.operation == basp::message_type::dispatch_message);
    binary_deserializer source{mpx_, buf};
    std::vector<strong_actor_ptr> stages;
    message msg;
    auto e = source(stages, msg);
    CAF_REQUIRE(!e);
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
        CAF_MESSAGE("implementation under test responded with "
                    << (num - 1) << " BASP message" << (num > 2 ? "s" : ""));
    }

    template <class... Ts>
    mock_t& receive(datagram_handle hdl,
                    maybe<basp::message_type> operation,
                    maybe<uint8_t> flags,
                    maybe<uint32_t> payload_len,
                    maybe<uint64_t> operation_data,
                    maybe<node_id> source_node,
                    maybe<node_id> dest_node,
                    maybe<actor_id> source_actor,
                    maybe<actor_id> dest_actor,
                    const Ts&... xs) {
      CAF_MESSAGE("expect #" << num << " on endpoint ID = " << hdl.id());
      buffer buf;
      this_->to_payload(buf, xs...);
      auto& oq = this_->mpx()->output_queue(hdl);
      while (oq.empty())
        this_->mpx()->exec_runnable();
      CAF_MESSAGE("output queue has " << oq.size() << " messages");
      auto dh = oq.front().first;
      buffer& ob = oq.front().second;
      CAF_MESSAGE("next datagram has " << ob.size()
                  << " bytes, servant ID = " << dh.id());
      CAF_CHECK_EQUAL(dh.id(), hdl.id());
      basp::header hdr;
      { // lifetime scope of source
        binary_deserializer source{this_->mpx(), ob};
        auto e = source(hdr);
        CAF_REQUIRE_EQUAL(e, none);
      }
      buffer payload;
      if (hdr.payload_len > 0) {
        CAF_REQUIRE(ob.size() >= (basp::header_size + hdr.payload_len));
        auto first = ob.begin() + basp::header_size;
        auto end = first + hdr.payload_len;
        payload.assign(first, end);
      }
      CAF_MESSAGE("erase message from output queue");
      oq.pop_front();
      CAF_CHECK_EQUAL(operation, hdr.operation);
      CAF_CHECK_EQUAL(flags, static_cast<uint8_t>(hdr.flags));
      CAF_CHECK_EQUAL(payload_len, hdr.payload_len);
      CAF_CHECK_EQUAL(operation_data, hdr.operation_data);
      CAF_CHECK_EQUAL(source_node, hdr.source_node);
      CAF_CHECK_EQUAL(dest_node, hdr.dest_node);
      CAF_CHECK_EQUAL(source_actor, hdr.source_actor);
      CAF_CHECK_EQUAL(dest_actor, hdr.dest_actor);
      CAF_REQUIRE_EQUAL(buf.size(), payload.size());
      CAF_REQUIRE_EQUAL(hexstr(buf), hexstr(payload));
      ++num;
      return *this;
    }

    template <class... Ts>
    mock_t& enqueue_back(datagram_handle hdl, datagram_handle ep,
                         basp::header hdr, const Ts&... xs) {
      buffer buf;
      this_->to_buf(buf, hdr, nullptr, xs...);
      CAF_MESSAGE("adding msg " << to_string(hdr.operation)
                  << " with " << (buf.size() - basp::header_size)
                  << " bytes payload to back of queue");
      this_->mpx()->virtual_network_buffer(hdl).emplace_back(ep, buf);
      return *this;
    }

    template <class... Ts>
    mock_t& enqueue_back(datagram_handle hdl, basp::header hdr,
                         Ts&&... xs) {
      return enqueue_back(hdl, hdl, hdr, std::forward<Ts>(xs)...);
    }

    template <class... Ts>
    mock_t& enqueue_front(datagram_handle hdl, datagram_handle ep,
                          basp::header hdr, const Ts&... xs) {
      buffer buf;
      this_->to_buf(buf, hdr, nullptr, xs...);
      CAF_MESSAGE("adding msg " << to_string(hdr.operation)
                  << " with " << (buf.size() - basp::header_size)
                  << " bytes payload to front of queue");
      this_->mpx()->virtual_network_buffer(hdl).emplace_front(ep, buf);
      return *this;
    }

    template <class... Ts>
    mock_t& enqueue_front(datagram_handle hdl, basp::header hdr,
                          Ts&&... xs) {
      return enqueue_front(hdl, hdl, hdr, std::forward<Ts>(xs)...);
    }

    mock_t& deliver(datagram_handle hdl, size_t num_messages = 1) {
      for (size_t i = 0; i < num_messages; ++i)
        this_->mpx()->read_data(hdl);
      return *this;
    }

  private:
    fixture* this_;
    size_t num = 1;
  };

  template <class... Ts>
  mock_t mock(datagram_handle hdl, basp::header hdr,
              const Ts&... xs) {
    buffer buf;
    to_buf(buf, hdr, nullptr, xs...);
    CAF_MESSAGE("virtually send " << to_string(hdr.operation)
                << " with " << (buf.size() - basp::header_size)
                << " bytes payload");
    mpx()->virtual_send(hdl, hdl, buf);
    return {this};
  }

  template <class... Ts>
  mock_t mock(datagram_handle hdl, datagram_handle ep, basp::header hdr,
              const Ts&... xs) {
    buffer buf;
    to_buf(buf, hdr, nullptr, xs...);
    CAF_MESSAGE("virtually send " << to_string(hdr.operation)
                << " with " << (buf.size() - basp::header_size)
                << " bytes payload");
    mpx()->virtual_send(hdl, ep, buf);
    return {this};
  }

  mock_t mock() {
    return {this};
  }

  actor_system_config cfg;
  actor_system sys;

private:
  basp_broker* aut_;
  datagram_handle dhdl_;
  network::test_multiplexer* mpx_;
  node_id this_node_;
  unique_ptr<scoped_actor> self_;
  array<node, num_remote_nodes> nodes_;
  /*
  array<node_id, num_remote_nodes> remote_node_;
  array<connection_handle, num_remote_nodes> remote_hdl_;
  array<unique_ptr<scoped_actor>, num_remote_nodes> pseudo_remote_;
  */
  actor_registry* registry_;
};


class manual_timer_fixture : public fixture {
public:
  using scheduler_type = caf::scheduler::test_coordinator;

  scheduler_type& sched;

  manual_timer_fixture()
    : fixture(false, true),
      sched(dynamic_cast<scheduler_type&>(sys.scheduler())) {
    // nop
  }
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

  void publish(const actor& whom, uint16_t port, bool is_udp = false) {
    using sig_t = std::set<std::string>;
    scoped_actor tmp{sys};
    sig_t sigs;
    CAF_MESSAGE("publish whom on port " << port);
    if (is_udp)
      tmp->send(mma, publish_udp_atom::value, port,
                actor_cast<strong_actor_ptr>(whom), std::move(sigs), "", false);
    else
      tmp->send(mma, publish_atom::value, port,
                actor_cast<strong_actor_ptr>(whom), std::move(sigs), "", false);
    CAF_MESSAGE("publish from tmp to mma with port _");
    expect((atom_value, uint16_t, strong_actor_ptr, sig_t, std::string, bool),
           from(tmp).to(mma));
    CAF_MESSAGE("publish: from mma to tmp with port " << port);
    expect((uint16_t), from(mma).to(tmp).with(port));
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(basp_udp_tests, fixture)

CAF_TEST_DISABLED(empty_server_handshake_udp) {
  // test whether basp instance correctly sends a
  // client handshake when there's no actor published
  buffer buf;
  instance().write_server_handshake(mpx(), buf, none);
  basp::header hdr;
  buffer payload;
  std::tie(hdr, payload) = from_buf(buf);
  basp::header expected{basp::message_type::server_handshake, 0,
                        static_cast<uint32_t>(payload.size()),
                        basp::version,
                        this_node(), none,
                        invalid_actor_id, invalid_actor_id,
                        0};
  CAF_CHECK(basp::valid(hdr));
  CAF_CHECK(basp::is_handshake(hdr));
  CAF_CHECK_EQUAL(to_string(hdr), to_string(expected));
}

CAF_TEST_DISABLED(empty_client_handshake_udp) {
  // test whether basp instance correctly sends a
  // client handshake when there's no actor published
  buffer buf;
  instance().write_client_handshake(mpx(), buf, none);
  basp::header hdr;
  buffer payload;
  std::tie(hdr, payload) = from_buf(buf);
  basp::header expected{basp::message_type::client_handshake, 0,
                        static_cast<uint32_t>(payload.size()), 0,
                        this_node(), none,
                        invalid_actor_id, invalid_actor_id,
                        0};
  CAF_CHECK(basp::valid(hdr));
  CAF_CHECK(basp::is_handshake(hdr));
  CAF_MESSAGE("got      : " << to_string(hdr));
  CAF_MESSAGE("expecting: " << to_string(expected));
  CAF_CHECK_EQUAL(to_string(hdr), to_string(expected));
}

CAF_TEST_DISABLED(non_empty_server_handshake_udp) {
  // test whether basp instance correctly sends a
  // server handshake with published actors
  buffer buf;
  instance().add_published_actor(4242, actor_cast<strong_actor_ptr>(self()),
                                 {"caf::replies_to<@u16>::with<@u16>"});
  instance().write_server_handshake(mpx(), buf, uint16_t{4242});
  buffer expected_buf;
  basp::header expected{basp::message_type::server_handshake, 0, 0,
                        basp::version, this_node(), none,
                        self()->id(), invalid_actor_id, 0};
  to_buf(expected_buf, expected, nullptr, std::string{},
         self()->id(), set<string>{"caf::replies_to<@u16>::with<@u16>"});
  CAF_CHECK_EQUAL(hexstr(buf), hexstr(expected_buf));
}

CAF_TEST_DISABLED(remote_address_and_port_udp) {
  CAF_MESSAGE("connect to Mars");
  establish_communication(mars());
  auto mm = sys.middleman().actor_handle();
  CAF_MESSAGE("ask MM about node ID of Mars");
  self()->send(mm, get_atom::value, mars().id);
  do {
    mpx()->exec_runnable();
  } while (self()->mailbox().empty());
  CAF_MESSAGE("receive result of MM");
  self()->receive(
    [&](const node_id& nid, const std::string& addr, uint16_t port) {
      CAF_CHECK_EQUAL(nid, mars().id);
      // all test nodes have address "test" and connection handle ID as port
      CAF_CHECK_EQUAL(addr, "test");
      CAF_CHECK_EQUAL(port, mars().endpoint.id());
    }
  );
}

CAF_TEST_DISABLED(client_handshake_and_dispatch_udp) {
  CAF_MESSAGE("establish communication with Jupiter");
  establish_communication(jupiter());
  CAF_MESSAGE("send dispatch message");
  // send a message via `dispatch` from node 0 on endpoint 1
  mock(endpoint_handle(), jupiter().endpoint,
       {basp::message_type::dispatch_message, 0, 0, 0,
        jupiter().id, this_node(), jupiter().dummy_actor->id(), self()->id(),
        1}, // increment sequence number
       std::vector<actor_addr>{},
       make_message(1, 2, 3))
  .receive(jupiter().endpoint,
          basp::message_type::announce_proxy, no_flags, no_payload,
          no_operation_data, this_node(), jupiter().id,
          invalid_actor_id, jupiter().dummy_actor->id());
  // must've created a proxy for our remote actor
  CAF_REQUIRE(proxies().count_proxies(jupiter().id) == 1);
  // must've send remote node a message that this proxy is monitored now
  // receive the message
  self()->receive(
    [](int a, int b, int c) {
      CAF_CHECK_EQUAL(a, 1);
      CAF_CHECK_EQUAL(b, 2);
      CAF_CHECK_EQUAL(c, 3);
      return a + b + c;
    }
  );
  CAF_MESSAGE("exec message of forwarding proxy");
  mpx()->exec_runnable();
  // deserialize and send message from out buf
  dispatch_out_buf(jupiter().endpoint);
  jupiter().dummy_actor->receive(
    [](int i) {
      CAF_CHECK_EQUAL(i, 6);
    }
  );
}

CAF_TEST_DISABLED(message_forwarding_udp) {
  // connect two remote nodes
  CAF_MESSAGE("establish communication with Jupiter");
  establish_communication(jupiter());
  CAF_MESSAGE("establish communication with Mars");
  establish_communication(mars());
  CAF_MESSAGE("send dispatch message to ... ");
  auto msg = make_message(1, 2, 3);
  // send a message from node 0 to node 1, forwarded by this node
  mock(endpoint_handle(), jupiter().endpoint,
       {basp::message_type::dispatch_message, 0, 0, 0,
        jupiter().id, mars().id,
        invalid_actor_id, mars().dummy_actor->id(),
        1}, // increment sequence number
       msg)
  .receive(mars().endpoint,
           basp::message_type::dispatch_message, no_flags, any_vals,
           no_operation_data, jupiter().id, mars().id,
           invalid_actor_id, mars().dummy_actor->id(),
           msg);
}

CAF_TEST_DISABLED(publish_and_connect_udp) {
  auto dx = datagram_handle::from_int(4242);
  mpx()->provide_datagram_servant(4242, dx);
  auto res = sys.middleman().publish_udp(self(), 4242);
  CAF_REQUIRE(res == 4242);
  mpx()->flush_runnables(); // process publish message in basp_broker
  establish_communication(jupiter(), dx, self()->id());
}

CAF_TEST_DISABLED(remote_actor_and_send_udp) {
  constexpr const char* lo = "localhost";
  CAF_MESSAGE("self: " << to_string(self()->address()));
  mpx()->provide_datagram_servant(lo, 4242, jupiter().endpoint);
  CAF_REQUIRE(mpx()->has_pending_remote_endpoint(lo, 4242));
  auto mm1 = sys.middleman().actor_handle();
  actor result;
  auto f = self()->request(mm1, infinite, contact_atom::value,
                           lo, uint16_t{4242});
  // wait until BASP broker has received and processed the connect message
  while (!aut()->valid(jupiter().endpoint))
    mpx()->exec_runnable();
  CAF_REQUIRE(!mpx()->has_pending_scribe(lo, 4242));
  // build a fake server handshake containing the id of our first pseudo actor
  CAF_MESSAGE("client handshake => server handshake => proxy announcement");
  auto na = registry()->named_actors();
  mock()
  .receive(jupiter().endpoint,
           basp::message_type::client_handshake, no_flags, 1u,
           no_operation_data, this_node(), node_id(),
           invalid_actor_id, invalid_actor_id, std::string{});
  mock(jupiter().endpoint,
       {basp::message_type::server_handshake, 0, 0, basp::version,
        jupiter().id, none,
        jupiter().dummy_actor->id(), invalid_actor_id,
        0}, // sequence number, first message
       std::string{},
       jupiter().dummy_actor->id(),
       uint32_t{0})
  .receive(jupiter().endpoint,
           basp::message_type::dispatch_message,
           basp::header::named_receiver_flag, any_vals,
           no_operation_data, this_node(), jupiter().id,
           any_vals, invalid_actor_id,
           spawn_serv_atom,
           std::vector<actor_id>{},
           make_message(sys_atom::value, get_atom::value, "info"))
  .receive(jupiter().endpoint,
           basp::message_type::announce_proxy, no_flags, no_payload,
           no_operation_data, this_node(), jupiter().id,
           invalid_actor_id, jupiter().dummy_actor->id());
  CAF_MESSAGE("BASP broker should've send the proxy");
  f.receive(
    [&](node_id nid, strong_actor_ptr res, std::set<std::string> ifs) {
      CAF_REQUIRE(res);
      auto aptr = actor_cast<abstract_actor*>(res);
      CAF_REQUIRE(dynamic_cast<forwarding_actor_proxy*>(aptr) != nullptr);
      CAF_CHECK_EQUAL(proxies().count_proxies(jupiter().id), 1u);
      CAF_CHECK_EQUAL(nid, jupiter().id);
      CAF_CHECK_EQUAL(res->node(), jupiter().id);
      CAF_CHECK_EQUAL(res->id(), jupiter().dummy_actor->id());
      CAF_CHECK(ifs.empty());
      auto proxy = proxies().get(jupiter().id, jupiter().dummy_actor->id());
      CAF_REQUIRE(proxy != nullptr);
      CAF_REQUIRE(proxy == res);
      result = actor_cast<actor>(res);
    },
    [&](error& err) {
      CAF_FAIL("error: " << sys.render(err));
    }
  );
  CAF_MESSAGE("send message to proxy");
  anon_send(actor_cast<actor>(result), 42);
  mpx()->flush_runnables();
  mock()
  .receive(jupiter().endpoint,
           basp::message_type::dispatch_message, no_flags, any_vals,
           no_operation_data, this_node(), jupiter().id,
           invalid_actor_id, jupiter().dummy_actor->id(),
           std::vector<actor_id>{},
           make_message(42));
  auto msg = make_message("hi there!");
  CAF_MESSAGE("send message via BASP (from proxy)");
  mock(jupiter().endpoint,
       {basp::message_type::dispatch_message, 0, 0, 0,
        jupiter().id, this_node(),
        jupiter().dummy_actor->id(), self()->id(),
        1}, // sequence number, second message
       std::vector<actor_id>{},
       make_message("hi there!"));
  self()->receive(
    [&](const string& str) {
      CAF_CHECK_EQUAL(to_string(self()->current_sender()), to_string(result));
      CAF_CHECK_EQUAL(self()->current_sender(), result.address());
      CAF_CHECK_EQUAL(str, "hi there!");
    }
  );
}

CAF_TEST_DISABLED(actor_serialize_and_deserialize_udp) {
  auto testee_impl = [](event_based_actor* testee_self) -> behavior {
    testee_self->set_default_handler(reflect_and_quit);
    return {
      [] {
        // nop
      }
    };
  };
  establish_communication(jupiter());
  auto prx = proxies().get_or_put(jupiter().id, jupiter().dummy_actor->id());
  mock()
  .receive(jupiter().endpoint,
          basp::message_type::announce_proxy, no_flags, no_payload,
          no_operation_data, this_node(), prx->node(),
          invalid_actor_id, prx->id());
  CAF_CHECK_EQUAL(prx->node(), jupiter().id);
  CAF_CHECK_EQUAL(prx->id(), jupiter().dummy_actor->id());
  auto testee = sys.spawn(testee_impl);
  registry()->put(testee->id(), actor_cast<strong_actor_ptr>(testee));
  CAF_MESSAGE("send message via BASP (from proxy)");
  auto msg = make_message(actor_cast<actor_addr>(prx));
  mock(endpoint_handle(), jupiter().endpoint,
       {basp::message_type::dispatch_message, 0, 0, 0,
        prx->node(), this_node(),
        prx->id(), testee->id(),
        1}, // sequence number, first message after handshake
       std::vector<actor_id>{},
       msg);
  // testee must've responded (process forwarded message in BASP broker)
  CAF_MESSAGE("wait until BASP broker writes to its output buffer");
  while (mpx()->output_queue(jupiter().endpoint).empty())
    mpx()->exec_runnable(); // process forwarded message in basp_broker
  // output buffer must contain the reflected message
  mock()
  .receive(jupiter().endpoint,
          basp::message_type::dispatch_message, no_flags, any_vals,
          no_operation_data, this_node(), prx->node(), testee->id(), prx->id(),
          std::vector<actor_id>{}, msg);
}

CAF_TEST_DISABLED(indirect_connections_udp) {
  // this node receives a message from jupiter via mars and responds via mars
  // and any ad-hoc automatic connection requests are ignored
  CAF_MESSAGE("self: " << to_string(self()->address()));
  auto dx = datagram_handle::from_int(4242);
  mpx()->provide_datagram_servant(4242, dx);
  sys.middleman().publish_udp(self(), 4242);
  mpx()->flush_runnables(); // process publish message in basp_broker
  CAF_MESSAGE("connect to Mars");
  establish_communication(mars(), dx, self()->id());
  CAF_MESSAGE("actor from Jupiter sends a message to us via Mars");
  auto mx = mock(dx, mars().endpoint,
                 {basp::message_type::dispatch_message, 0, 0, 0,
                  jupiter().id, this_node(),
                  jupiter().dummy_actor->id(), self()->id(),
                  1}, // sequence number
                 std::vector<actor_id>{},
                 make_message("hello from jupiter!"));
  CAF_MESSAGE("expect ('sys', 'get', \"info\") from Earth to Jupiter at Mars");
  // this asks Jupiter if it has a 'SpawnServ'
  mx.receive(mars().endpoint,
             basp::message_type::dispatch_message,
             basp::header::named_receiver_flag, any_vals,
             no_operation_data, this_node(), jupiter().id,
             any_vals, invalid_actor_id,
             spawn_serv_atom,
             std::vector<actor_id>{},
             make_message(sys_atom::value, get_atom::value, "info"));
  CAF_MESSAGE("expect announce_proxy message at Mars from Earth to Jupiter");
  mx.receive(mars().endpoint,
             basp::message_type::announce_proxy, no_flags, no_payload,
             no_operation_data, this_node(), jupiter().id,
             invalid_actor_id, jupiter().dummy_actor->id());
  CAF_MESSAGE("receive message from jupiter");
  self()->receive(
    [](const std::string& str) -> std::string {
      CAF_CHECK_EQUAL(str, "hello from jupiter!");
      return "hello from earth!";
    }
  );
  mpx()->exec_runnable(); // process forwarded message in basp_broker
  mock()
  .receive(mars().endpoint,
           basp::message_type::dispatch_message, no_flags, any_vals,
           no_operation_data, this_node(), jupiter().id,
           self()->id(), jupiter().dummy_actor->id(),
           std::vector<actor_id>{},
           make_message("hello from earth!"));
}


CAF_TEST_FIXTURE_SCOPE_END()


CAF_TEST_FIXTURE_SCOPE(basp_udp_tests_with_manual_timer, manual_timer_fixture)

CAF_TEST_DISABLED(out_of_order_delivery_udp) {
  // This test uses the test_coordinator to get control over the
  // timeouts that deliver pending message.
  constexpr const char* lo = "localhost";
  CAF_MESSAGE("self: " << to_string(self()->address()));
  mpx()->provide_datagram_servant(lo, 4242, jupiter().endpoint);
  CAF_REQUIRE(mpx()->has_pending_remote_endpoint(lo, 4242));
  auto mm1 = sys.middleman().actor_handle();
  actor result;
  auto f = self()->request(mm1, infinite, contact_atom::value,
                           lo, uint16_t{4242});
  // wait until BASP broker has received and processed the connect message
  while (!aut()->valid(jupiter().endpoint)) {
    sched.run();
    mpx()->exec_runnable();
  }
  CAF_REQUIRE(!mpx()->has_pending_scribe(lo, 4242));
  // build a fake server handshake containing the id of our first pseudo actor
  CAF_MESSAGE("client handshake => server handshake => proxy announcement");
  auto na = registry()->named_actors();
  mock()
  .receive(jupiter().endpoint,
           basp::message_type::client_handshake, no_flags, 1u,
           no_operation_data, this_node(), node_id(),
           invalid_actor_id, invalid_actor_id, std::string{});
  mock(jupiter().endpoint, jupiter().endpoint,
       {basp::message_type::server_handshake, 0, 0, basp::version,
        jupiter().id, none,
        jupiter().dummy_actor->id(), invalid_actor_id,
        0}, // sequence number, first message
       std::string{},
       jupiter().dummy_actor->id(),
       uint32_t{0})
  .receive(jupiter().endpoint,
           basp::message_type::dispatch_message,
           basp::header::named_receiver_flag, any_vals,
           no_operation_data, this_node(), jupiter().id,
           any_vals, invalid_actor_id,
           spawn_serv_atom,
           std::vector<actor_id>{},
           make_message(sys_atom::value, get_atom::value, "info"))
  .receive(jupiter().endpoint,
           basp::message_type::announce_proxy, no_flags, no_payload,
           no_operation_data, this_node(), jupiter().id,
           invalid_actor_id, jupiter().dummy_actor->id());
  sched.run();
  CAF_MESSAGE("BASP broker should've send the proxy");
  f.receive(
    [&](node_id nid, strong_actor_ptr res, std::set<std::string> ifs) {
      CAF_REQUIRE(res);
      auto aptr = actor_cast<abstract_actor*>(res);
      CAF_REQUIRE(dynamic_cast<forwarding_actor_proxy*>(aptr) != nullptr);
      CAF_CHECK_EQUAL(proxies().count_proxies(jupiter().id), 1u);
      CAF_CHECK_EQUAL(nid, jupiter().id);
      CAF_CHECK_EQUAL(res->node(), jupiter().id);
      CAF_CHECK_EQUAL(res->id(), jupiter().dummy_actor->id());
      CAF_CHECK(ifs.empty());
      auto proxy = proxies().get(jupiter().id, jupiter().dummy_actor->id());
      CAF_REQUIRE(proxy != nullptr);
      CAF_REQUIRE(proxy == res);
      result = actor_cast<actor>(res);
    },
    [&](error& err) {
      CAF_FAIL("error: " << sys.render(err));
    }
  );
  CAF_MESSAGE("send message to proxy");
  anon_send(actor_cast<actor>(result), 42);
  mpx()->flush_runnables();
  mock()
  .receive(jupiter().endpoint,
           basp::message_type::dispatch_message, no_flags, any_vals,
           no_operation_data, this_node(), jupiter().id,
           invalid_actor_id, jupiter().dummy_actor->id(),
           std::vector<actor_id>{},
           make_message(42));
  auto header_with_seq = [&](uint16_t seq) -> basp::header {
    return {basp::message_type::dispatch_message, 0, 0, 0,
            jupiter().id, this_node(),
            jupiter().dummy_actor->id(), self()->id(),
            seq};
  };
  CAF_MESSAGE("send 10 messages out of order");
  mock()
  .enqueue_back(jupiter().endpoint, header_with_seq(1),
                std::vector<actor_id>{}, make_message(0))
  .enqueue_back(jupiter().endpoint, header_with_seq(2),
                std::vector<actor_id>{}, make_message(1))
  .enqueue_front(jupiter().endpoint, header_with_seq(3),
                 std::vector<actor_id>{}, make_message(2))
  .enqueue_back(jupiter().endpoint, header_with_seq(4),
                std::vector<actor_id>{}, make_message(3))
  .enqueue_back(jupiter().endpoint, header_with_seq(5),
                std::vector<actor_id>{}, make_message(4))
  .enqueue_back(jupiter().endpoint, header_with_seq(6),
                std::vector<actor_id>{}, make_message(5))
  .enqueue_front(jupiter().endpoint, header_with_seq(7),
                 std::vector<actor_id>{}, make_message(6))
  .enqueue_back(jupiter().endpoint, header_with_seq(8),
                std::vector<actor_id>{}, make_message(7))
  .enqueue_back(jupiter().endpoint, header_with_seq(9),
                std::vector<actor_id>{}, make_message(8))
  .enqueue_front(jupiter().endpoint, header_with_seq(10),
                 std::vector<actor_id>{}, make_message(9))
  .deliver(jupiter().endpoint, 10);
  int expected_next = 0;
  self()->receive_while([&] { return expected_next < 10; }) (
    [&](int val) {
      CAF_CHECK_EQUAL(to_string(self()->current_sender()), to_string(result));
      CAF_CHECK_EQUAL(self()->current_sender(), result.address());
      CAF_CHECK_EQUAL(expected_next, val);
      ++expected_next;
    }
  );
  sched.trigger_timeouts();
  mpx()->flush_runnables();
  CAF_MESSAGE("force delivery via timeout that skips messages");
  const basp::sequence_type seq_and_payload = 23;
  mock()
  .enqueue_back(jupiter().endpoint, header_with_seq(seq_and_payload),
                std::vector<actor_id>{}, make_message(seq_and_payload))
  .deliver(jupiter().endpoint, 1);
  sched.trigger_timeouts();
  mpx()->exec_runnable();
  self()->receive(
    [&](basp::sequence_type val) {
      CAF_CHECK_EQUAL(to_string(self()->current_sender()), to_string(result));
      CAF_CHECK_EQUAL(self()->current_sender(), result.address());
      CAF_CHECK_EQUAL(seq_and_payload, val);
    }
  );
}

CAF_TEST_FIXTURE_SCOPE_END()


CAF_TEST_FIXTURE_SCOPE(basp_udp_tests_with_autoconn, autoconn_enabled_fixture)

CAF_TEST_DISABLED(automatic_connection_udp) {
  // this tells our BASP broker to enable the automatic connection feature
  //anon_send(aut(), ok_atom::value,
  //          "middleman.enable-automatic-connections", make_message(true));
  //mpx()->exec_runnable(); // process publish message in basp_broker
  // jupiter [remote hdl 0] -> mars [remote hdl 1] -> earth [this_node]
  // (this node receives a message from jupiter via mars and responds via mars,
  //  but then also establishes a connection to jupiter directly)
  auto check_node_in_tbl = [&](node& n) {
    io::id_visitor id_vis;
    auto hdl = tbl().lookup_direct(n.id);
    CAF_REQUIRE(hdl);
    CAF_CHECK_EQUAL(visit(id_vis, *hdl), n.endpoint.id());
  };
  mpx()->provide_datagram_servant("jupiter", 8080, jupiter().endpoint);
  CAF_CHECK(mpx()->has_pending_remote_endpoint("jupiter", 8080));
  CAF_MESSAGE("self: " << to_string(self()->address()));
  auto dx = datagram_handle::from_int(4242);
  mpx()->provide_datagram_servant(4242, dx);
  publish(self(), 4242, true);
  mpx()->flush_runnables(); // process publish message in basp_broker
  CAF_MESSAGE("connect to mars");
  establish_communication(mars(), dx, self()->id());
  //CAF_CHECK_EQUAL(tbl().lookup_direct(mars().id).id(), mars().connection.id());
  check_node_in_tbl(mars());
  CAF_MESSAGE("simulate that an actor from jupiter "
              "sends a message to us via mars");
  mock(dx, mars().endpoint,
       {basp::message_type::dispatch_message, 0, 0, 0,
        jupiter().id, this_node(),
        jupiter().dummy_actor->id(), self()->id(),
        1}, // sequence number
       std::vector<actor_id>{},
       make_message("hello from jupiter!"))
  .receive(mars().endpoint,
           basp::message_type::dispatch_message,
           basp::header::named_receiver_flag, any_vals, no_operation_data,
           this_node(), jupiter().id, any_vals, invalid_actor_id,
           spawn_serv_atom,
           std::vector<actor_id>{},
           make_message(sys_atom::value, get_atom::value, "info"))
  // if we do not disable TCP, we would receive a second message here asking
  // for the tcp related addresses
  .receive(mars().endpoint,
           basp::message_type::dispatch_message,
           basp::header::named_receiver_flag, any_vals,
           no_operation_data, this_node(), jupiter().id,
           any_vals, // actor ID of an actor spawned by the BASP broker
           invalid_actor_id,
           config_serv_atom,
           std::vector<actor_id>{},
           make_message(get_atom::value, "basp.default-connectivity-udp"))
  .receive(mars().endpoint,
           basp::message_type::announce_proxy, no_flags, no_payload,
           no_operation_data, this_node(), jupiter().id,
           invalid_actor_id, jupiter().dummy_actor->id());
  CAF_CHECK_EQUAL(mpx()->output_queue(mars().endpoint).size(), 0u);
  CAF_CHECK_EQUAL(tbl().lookup_indirect(jupiter().id), mars().id);
  CAF_CHECK_EQUAL(tbl().lookup_indirect(mars().id), none);
  auto connection_helper_actor = sys.latest_actor_id();
  CAF_CHECK_EQUAL(mpx()->output_queue(mars().endpoint).size(), 0u);
  // create a dummy config server and respond to the name lookup
  CAF_MESSAGE("receive ConfigServ of jupiter");
  network::address_listing res;
  res[network::protocol::ipv4].emplace_back("jupiter");
  mock(dx, mars().endpoint,
       {basp::message_type::dispatch_message, 0, 0, 0,
        this_node(), this_node(),
        invalid_actor_id, connection_helper_actor,
        2}, // sequence number
       std::vector<actor_id>{},
       make_message("basp.default-connectivity-udp",
                    make_message(uint16_t{8080}, std::move(res))));
  // our connection helper should now connect to jupiter and
  // send the scribe handle over to the BASP broker
  while (mpx()->has_pending_remote_endpoint("jupiter", 8080)) {
    sched.run();
    mpx()->flush_runnables();
  }
  CAF_REQUIRE(mpx()->output_queue(mars().endpoint).empty());
  CAF_MESSAGE("Let's do the handshake.");
  mock()
  .receive(jupiter().endpoint,
           basp::message_type::client_handshake, no_flags, 1u,
           no_operation_data, this_node(), node_id(),
           invalid_actor_id, invalid_actor_id, std::string{});
  CAF_MESSAGE("Received client handshake.");
  // send handshake from jupiter
  mock(jupiter().endpoint, jupiter().endpoint,
       {basp::message_type::server_handshake, 0, 0, basp::version,
        jupiter().id, none,
        jupiter().dummy_actor->id(), invalid_actor_id},
       std::string{},
       jupiter().dummy_actor->id(),
       uint32_t{0});
  mpx()->exec_runnable(); // Receive message from connection helper
  mpx()->exec_runnable(); // Process BASP server handshake
  CAF_CHECK_EQUAL(tbl().lookup_indirect(jupiter().id), none);
  CAF_CHECK_EQUAL(tbl().lookup_indirect(mars().id), none);
  check_node_in_tbl(jupiter());
  check_node_in_tbl(mars());
  CAF_MESSAGE("receive message from jupiter");
  self()->receive(
    [](const std::string& str) -> std::string {
      CAF_CHECK_EQUAL(str, "hello from jupiter!");
      return "hello from earth!";
    }
  );
  mpx()->exec_runnable(); // process forwarded message in basp_broker
  CAF_MESSAGE("response message must take direct route now");
  mock()
  .receive(jupiter().endpoint,
          basp::message_type::dispatch_message, no_flags, any_vals,
          no_operation_data, this_node(), jupiter().id,
          self()->id(), jupiter().dummy_actor->id(),
          std::vector<actor_id>{},
          make_message("hello from earth!"));
  CAF_CHECK_EQUAL(mpx()->output_queue(mars().endpoint).size(), 0u);
}

CAF_TEST_FIXTURE_SCOPE_END()
