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

#define CAF_SUITE protocol_policy

#include "caf/config.hpp"

#include <cstdint>
#include <cstring>
#include <tuple>

#include "caf/test/dsl.hpp"

#include "caf/io/network/newb.hpp"
#include "caf/policy/newb_basp.hpp"
#include "caf/policy/newb_ordering.hpp"
#include "caf/policy/newb_raw.hpp"

using namespace caf;
using namespace caf::io;
using namespace caf::policy;

using io::network::byte_buffer;
using io::network::header_writer;
using network::default_multiplexer;
using network::invalid_native_socket;
using network::last_socket_error_as_string;
using network::native_socket;

namespace {

// -- atoms --------------------------------------------------------------------

using expect_atom = atom_constant<atom("expect")>;
using ordering_atom = atom_constant<atom("ordering")>;
using send_atom = atom_constant<atom("send")>;
using shutdown_atom = atom_constant<atom("shutdown")>;
using quit_atom = atom_constant<atom("quit")>;

using set_atom = atom_constant<atom("set")>;
using get_atom = atom_constant<atom("get")>;

struct test_state {
  int value;
  std::vector<std::pair<atom_value, uint32_t>> timeout_messages;
  std::vector<std::pair<new_basp_msg, std::vector<char>>> messages;
  std::deque<std::pair<basp_header, uint32_t>> expected;
};

behavior dummy_broker(network::stateful_newb<new_basp_msg, test_state>* self) {
  return {
    [=](new_basp_msg& msg) {
      auto& s = self->state;
      CAF_MESSAGE("handling new basp message = " << to_string(msg));
      CAF_ASSERT(!s.expected.empty());
      auto& e = s.expected.front();
      CAF_CHECK_EQUAL(msg.header.from, e.first.from);
      CAF_CHECK_EQUAL(msg.header.to, e.first.to);
      int pl;
      binary_deserializer bd(&self->backend(), msg.payload, msg.payload_len);
      bd(pl);
      CAF_CHECK_EQUAL(pl, e.second);
      std::vector<char> payload{msg.payload, msg.payload + msg.payload_len};
      s.messages.emplace_back(msg, payload);
      s.messages.back().first.payload = s.messages.back().second.data();
      self->transport->receive_buffer.clear();
      s.expected.pop_front();
    },
    [=](send_atom, actor_id sender, actor_id receiver, uint32_t payload) {
      CAF_MESSAGE("send: from = " << sender << " to = " << receiver
                  << " payload = " << payload);
      {
        auto hw = caf::make_callback([&](byte_buffer& buf) -> error {
          binary_serializer bs(&self->backend(), buf);
          bs(basp_header{0, sender, receiver});
          return none;
        });
        auto whdl = self->wr_buf(&hw);
        CAF_CHECK(whdl.buf != nullptr);
        CAF_CHECK(whdl.protocol != nullptr);
        binary_serializer bs(&self->backend(), *whdl.buf);
        bs(payload);
      }
      std::swap(self->transport->receive_buffer, self->transport->offline_buffer);
      self->transport->send_buffer.clear();
      self->transport->received_bytes = self->transport->receive_buffer.size();
    },
    [=](send_atom, ordering_header& ohdr, basp_header& bhdr, uint32_t payload) {
      CAF_MESSAGE("send: ohdr = " << to_string(ohdr) << " bhdr = "
                  << to_string(bhdr) << " payload = " << payload);
      auto& buf = self->transport->receive_buffer;
      binary_serializer bs(&self->backend(), buf);
      bs(ohdr);
      auto bhdr_start = buf.size();
      bs(bhdr);
      auto payload_start = buf.size();
      bs(payload);
      auto packet_len = buf.size();
      self->transport->received_bytes = packet_len;
      stream_serializer<charbuf> out{&self->backend(),
                                     buf.data() + bhdr_start,
                                     sizeof(uint32_t)};
      auto payload_len = static_cast<uint32_t>(packet_len - payload_start);
      out(payload_len);
    },
    [=](expect_atom, basp_header& bhdr, uint32_t payload) {
      self->state.expected.push_back(std::make_pair(bhdr, payload));
    }
  };
}

struct dummy_transport : public network::transport_policy {
  io::network::rw_state read_some(network::newb_base*) {
    return receive_buffer.size() > 0 ? network::rw_state::success : network::rw_state::indeterminate;
    //return io::network::rw_state::success;
  }
};

struct accept_policy_impl : public network::accept_policy {
  expected<native_socket> create_socket(uint16_t, const char*, bool) override {
    return sec::bad_function_call;
  }

  std::pair<native_socket, network::transport_policy_ptr>
  accept(network::newb_base*) override {
    auto esock = network::new_local_udp_endpoint_impl(0, nullptr);
    CAF_REQUIRE(esock);
    network::transport_policy_ptr ptr{new dummy_transport};
    return {esock->first, std::move(ptr)};
  }

  void init(network::newb_base& n) override {
    n.handle_event(network::operation::read);
  }
};

template <class Protocol>
struct dummy_basp_newb_acceptor
    : public network::newb_acceptor<typename Protocol::message_type> {
  using super = network::newb_acceptor<typename Protocol::message_type>;
  using message_tuple_t = std::tuple<ordering_header, basp_header, int>;

  dummy_basp_newb_acceptor(default_multiplexer& dm, native_socket sockfd)
      : super(dm, sockfd) {
    // nop
  }

  expected<actor> create_newb(native_socket sockfd,
                              network::transport_policy_ptr pol) override {

    spawned.emplace_back(
      network::spawn_newb<Protocol>(this->backend().system(), dummy_broker,
                                    std::move(pol), sockfd)
    );
    auto ptr = caf::actor_cast<caf::abstract_actor*>(spawned.back());
    if (ptr == nullptr)
      return sec::runtime_error;
//    auto& ref
//        = dynamic_cast<network::stateful_newb<new_basp_msg, test_state>&>(*ptr);
//    // TODO: Call read_some using the buffer of the ref as a destination.
//    binary_serializer bs(&this->backend(), ref.transport->receive_buffer);
//    bs(get<0>(msg));
//    bs(get<1>(msg));
//    bs(get<2>(msg));
//    ref.transport->received_bytes = ref.transport->receive_buffer.size();
//    ref.state.expected.emplace_back(get<1>(msg), get<2>(msg));
    return spawned.back();
  }

  message_tuple_t msg;
  std::vector<actor> spawned;
};

class config : public actor_system_config {
public:
  config() {
    set("scheduler.policy", atom("testing"));
    set("logger.inline-output", true);
    set("middleman.manual-multiplexing", true);
    set("middleman.attach-utility-actors", true);
    load<middleman>();
  }
};

struct fixture {
  using newb_t = network::stateful_newb<new_basp_msg, test_state>;
  using protocol_t = network::generic_protocol<ordering<datagram_basp>>;
  using acceptor_t = dummy_basp_newb_acceptor<protocol_t>;
  config cfg;
  actor_system sys;
  default_multiplexer& mpx;
  scheduler::test_coordinator& sched;
  actor self;
  std::unique_ptr<acceptor_t> na;

  fixture()
      : sys(cfg.parse(test::engine::argc(), test::engine::argv())),
        mpx(dynamic_cast<default_multiplexer&>(sys.middleman().backend())),
        sched(dynamic_cast<caf::scheduler::test_coordinator&>(sys.scheduler())) {
    // Create a socket;
    auto esock = network::new_local_udp_endpoint_impl(0, nullptr);
    CAF_REQUIRE(esock);
    // Create newb.
    network::transport_policy_ptr pol{new dummy_transport};
    self = network::spawn_newb<protocol_t>(sys, dummy_broker,
                                           std::move(pol), esock->first);
    // And another socket.
    esock = network::new_local_udp_endpoint_impl(0, nullptr);
    CAF_REQUIRE(esock);
    // Create acceptor.
    auto& mpx = dynamic_cast<default_multiplexer&>(sys.middleman().backend());
    std::unique_ptr<acceptor_t> ptr{new acceptor_t(mpx, esock->first)};
    ptr->acceptor.reset(new accept_policy_impl);
    na = std::move(ptr);
  }

  ~fixture() {
    anon_send_exit(self, exit_reason::user_shutdown);
    exec_all();
    na->stop();
  }

  // -- supporting -------------------------------------------------------------

  void exec_all() {
    while (mpx.try_run_once()) {
      // rince and repeat
    }
  }

  template <class T = caf::scheduled_actor, class Handle = caf::actor>
  T& deref(const Handle& hdl) {
    auto ptr = caf::actor_cast<caf::abstract_actor*>(hdl);
    CAF_REQUIRE(ptr != nullptr);
    return dynamic_cast<T&>(*ptr);
  }

  void write_packet(byte_buffer& buf, const ordering_header& ohdr,
                    const basp_header& bhdr, uint32_t payload) {
    // Write header and payload.
    binary_serializer bs(sys, buf);
    bs(ohdr);
    auto bhdr_start = buf.size();
    bs(bhdr);
    auto payload_start = buf.size();
    bs(payload);
    auto packet_len = buf.size();
    // Write payload size into basp header.
    stream_serializer<charbuf> out{sys, buf.data() + bhdr_start,
                                   sizeof(uint32_t)};
    auto payload_len = static_cast<uint32_t>(packet_len - payload_start);
    out(payload_len);
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(newb_basics, fixture)

CAF_TEST(spawn newb) {
  scoped_actor self{sys};
  auto rcvd = false;
  auto my_newb = [&rcvd] (newb_t*) -> behavior {
    return {
      [&rcvd](int) {
        rcvd = true;
      },
    };
  };
  CAF_MESSAGE("create newb");
  auto esock = network::new_local_udp_endpoint_impl(0, nullptr);
  CAF_REQUIRE(esock);
  network::transport_policy_ptr transport{new dummy_transport};
  auto n = io::network::spawn_newb<protocol_t>(sys, my_newb, std::move(transport),
                                               esock->first);
  exec_all();
  CAF_MESSAGE("send test message");
  self->send(n, 3);
  exec_all();
  CAF_CHECK(rcvd);
  CAF_MESSAGE("shutdown newb");
  self->send_exit(n, exit_reason::user_shutdown);
  exec_all();
  CAF_MESSAGE("done");
}

CAF_TEST(spawn stateful newb) {
  scoped_actor self{sys};
  auto my_newb = [] (newb_t* self) -> behavior {
    self->state.value = 0;
    return {
      [=](set_atom, int i) {
        self->state.value = i;
      },
      [=](get_atom) {
        return self->state.value;
      },
    };
  };
  CAF_MESSAGE("create newb");
  auto esock = network::new_local_udp_endpoint_impl(0, nullptr);
  CAF_REQUIRE(esock);
  network::transport_policy_ptr transport{new dummy_transport};
  auto n = io::network::spawn_newb<protocol_t>(sys, my_newb, std::move(transport),
                                               esock->first);
  exec_all();
  CAF_MESSAGE("set value in state");
  self->send(n, set_atom::value, 3);
  exec_all();
  CAF_MESSAGE("get value back");
  self->send(n, get_atom::value);
  exec_all();
  self->receive(
    [&](int r) {
      CAF_CHECK_EQUAL(r, 3);
      CAF_MESSAGE("matches expected value");
    },
    [&](const error& err) {
      CAF_FAIL(sys.render(err));
    }
  );
  CAF_MESSAGE("shutdown newb");
  anon_send_exit(n, exit_reason::user_shutdown);
  exec_all();
  CAF_MESSAGE("done");
}

CAF_TEST(read event) {
  exec_all();
  CAF_MESSAGE("create some values for our buffer");
  const ordering_header ohdr{0};
  const basp_header bhdr{0, 13, 42};
  const uint32_t payload = 1337;
  CAF_MESSAGE("set the expected message");
  anon_send(self, expect_atom::value, bhdr, payload);
  exec_all();
  CAF_MESSAGE("copy them into the buffer");
  auto& dummy = deref<newb_t>(self);
  auto& buf = dummy.transport->receive_buffer;
  write_packet(buf, ohdr, bhdr, payload);
  dummy.transport->received_bytes = buf.size();
  CAF_MESSAGE("trigger a read event");
  dummy.read_event();
  CAF_MESSAGE("check the basp header and payload");
  CAF_REQUIRE(!dummy.state.messages.empty());
  auto& msg = dummy.state.messages.front().first;
  CAF_CHECK_EQUAL(msg.header.from, bhdr.from);
  CAF_CHECK_EQUAL(msg.header.to, bhdr.to);
  uint32_t return_payload = 0;
  binary_deserializer bd(sys, msg.payload, msg.payload_len);
  bd(return_payload);
  CAF_CHECK_EQUAL(return_payload, payload);
}

CAF_TEST(message passing) {
  exec_all();
  const ordering_header ohdr{0};
  const basp_header bhdr{0, 13, 42};
  const uint32_t payload = 1337;
  CAF_MESSAGE("setup read event");
  anon_send(self, expect_atom::value, bhdr, payload);
  anon_send(self, send_atom::value, ohdr, bhdr, payload);
  exec_all();
  auto& dummy = deref<newb_t>(self);
  dummy.handle_event(network::operation::read);
  CAF_MESSAGE("check the basp header and payload");
  auto& msg = dummy.state.messages.front().first;
  CAF_CHECK_EQUAL(msg.header.from, bhdr.from);
  CAF_CHECK_EQUAL(msg.header.to, bhdr.to);
  int return_payload = 0;
  binary_deserializer bd(sys, msg.payload, msg.payload_len);
  bd(return_payload);
  CAF_CHECK_EQUAL(return_payload, payload);
}

CAF_TEST(timeouts) {
  // Should be an unexpected sequence number and lead to an error. Since
  // we start with 0, the 1 below should be out of order.
  const ordering_header ohdr{1};
  const basp_header bhdr{0, 13, 42};
  const uint32_t payload = 1337;
  CAF_MESSAGE("setup read event");
  anon_send(self, expect_atom::value, bhdr, payload);
  anon_send(self, send_atom::value, ohdr, bhdr, payload);
  exec_all();
  CAF_MESSAGE("trigger read event");
  auto& dummy = deref<newb_t>(self);
  dummy.read_event();
  CAF_CHECK(!dummy.state.expected.empty());
  CAF_MESSAGE("trigger waiting timeouts");
  // Trigger timeout.
  sched.trigger_timeout();
  // Handle received message.
  exec_all();
  // Message handler will check if the expected message was received.
  CAF_CHECK(dummy.state.expected.empty());
}

CAF_TEST(message ordering) {
  CAF_MESSAGE("create data for two messges");
  // Message one.
  const ordering_header ohdr_first{0};
  const basp_header bhdr_first{0, 10, 11};
  const uint32_t payload_first = 100;
  // Message two.
  const ordering_header ohdr_second{1};
  const basp_header bhdr_second{0, 12, 13};
  const uint32_t payload_second = 101;
  CAF_MESSAGE("setup read events");
  anon_send(self, expect_atom::value, bhdr_first, payload_first);
  anon_send(self, expect_atom::value, bhdr_second, payload_second);
  exec_all();
  auto& dummy = deref<newb_t>(self);
  auto& buf = dummy.transport->receive_buffer;
  CAF_MESSAGE("read second message first");
  write_packet(buf, ohdr_second, bhdr_second, payload_second);
  dummy.transport->received_bytes = buf.size();
  dummy.read_event();
  CAF_MESSAGE("followed by first message");
  buf.clear();
  write_packet(buf, ohdr_first, bhdr_first, payload_first);
  dummy.transport->received_bytes = buf.size();
  dummy.read_event();
}

CAF_TEST(write buf) {
  exec_all();
  const basp_header bhdr{0, 13, 42};
  const uint32_t payload = 1337;
  CAF_MESSAGE("setup read event");
  anon_send(self, expect_atom::value, bhdr, payload);
  anon_send(self, send_atom::value, bhdr.from, bhdr.to, payload);
  exec_all();
  auto& dummy = deref<newb_t>(self);
  dummy.handle_event(network::operation::read);
  // Message handler will check if the expected message was received.
}

CAF_TEST(newb acceptor) {
  CAF_MESSAGE("trigger read event on acceptor");
  na->handle_event(network::operation::read);
  auto& dummy = dynamic_cast<acceptor_t&>(*na.get());
  CAF_CHECK(!dummy.spawned.empty());
  CAF_MESSAGE("shutdown the create newb");
  for (actor& d : dummy.spawned)
    anon_send_exit(d, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()
