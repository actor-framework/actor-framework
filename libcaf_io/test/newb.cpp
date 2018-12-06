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

#define CAF_SUITE io_newbs

#include "caf/config.hpp"

#include <cstdint>
#include <cstring>
#include <tuple>

#include "caf/test/dsl.hpp"

#include "caf/io/newb.hpp"
#include "caf/policy/newb_basp.hpp"
#include "caf/policy/newb_ordering.hpp"
#include "caf/policy/newb_raw.hpp"

using namespace caf;
using namespace caf::io;
using namespace caf::policy;

using io::byte_buffer;
using io::header_writer;
using io::network::default_multiplexer;
using io::network::invalid_native_socket;
using io::network::last_socket_error_as_string;
using io::network::native_socket;

namespace {

// -- atoms --------------------------------------------------------------------

using expect_atom = atom_constant<atom("expect")>;
using ordering_atom = atom_constant<atom("ordering")>;
using send_atom = atom_constant<atom("send")>;
using shutdown_atom = atom_constant<atom("shutdown")>;
using quit_atom = atom_constant<atom("quit")>;

using set_atom = atom_constant<atom("set")>;
using get_atom = atom_constant<atom("get")>;

using ping_atom = atom_constant<atom("ping")>;
using pong_atom = atom_constant<atom("pong")>;

// -- test classes -------------------------------------------------------------

struct test_state {
  int value;
  std::vector<std::pair<atom_value, uint32_t>> timeout_messages;
  std::vector<std::pair<new_basp_msg, std::vector<char>>> messages;
  std::deque<std::pair<basp_header, uint32_t>> expected;
};

behavior dummy_broker(stateful_newb<new_basp_msg, test_state>* self) {
  return {
    [=](new_basp_msg& msg) {
      auto& s = self->state;
      CAF_MESSAGE("handling new basp message = " << to_string(msg));
      CAF_ASSERT(!s.expected.empty());
      auto& e = s.expected.front();
      CAF_CHECK_EQUAL(msg.header.from, e.first.from);
      CAF_CHECK_EQUAL(msg.header.to, e.first.to);
      uint32_t pl;
      binary_deserializer bd(&self->backend(), msg.payload, msg.payload_len);
      bd(pl);
      CAF_CHECK_EQUAL(pl, e.second);
      std::vector<char> payload{msg.payload, msg.payload + msg.payload_len};
      s.messages.emplace_back(msg, payload);
      s.messages.back().first.payload = s.messages.back().second.data();
      self->trans->receive_buffer.clear();
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
      std::swap(self->trans->receive_buffer, self->trans->offline_buffer);
      self->trans->send_buffer.clear();
      self->trans->received_bytes = self->trans->receive_buffer.size();
    },
    [=](send_atom, ordering_header& ohdr, basp_header& bhdr, uint32_t payload) {
      CAF_MESSAGE("send: ohdr = " << to_string(ohdr) << " bhdr = "
                  << to_string(bhdr) << " payload = " << payload);
      auto& buf = self->trans->receive_buffer;
      binary_serializer bs(&self->backend(), buf);
      bs(ohdr);
      auto bhdr_start = buf.size();
      bs(bhdr);
      auto payload_start = buf.size();
      bs(payload);
      auto packet_len = buf.size();
      self->trans->received_bytes = packet_len;
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

struct dummy_transport : public transport {
  io::network::rw_state read_some(network::newb_base*) {
    return receive_buffer.size() > 0 ? network::rw_state::success
                                     : network::rw_state::indeterminate;
    //return io::network::rw_state::success;
  }
};

template <class Message>
struct dummy_accept : public accept<Message> {
  expected<io::network::native_socket>
  create_socket(uint16_t, const char*, bool = false) {
    return network::invalid_native_socket;
  }
};

// -- config for controlled scheduling and multiplexing ------------------------

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
  using newb_t = stateful_newb<new_basp_msg, test_state>;
  using protocol_t = generic_protocol<ordering<datagram_basp>>;
  config cfg;
  actor_system sys;
  default_multiplexer& mpx;
  scheduler::test_coordinator& sched;
  actor test_newb;

  fixture()
      : sys(cfg.parse(test::engine::argc(), test::engine::argv())),
        mpx(dynamic_cast<default_multiplexer&>(sys.middleman().backend())),
        sched(dynamic_cast<caf::scheduler::test_coordinator&>(sys.scheduler())) {
    // Create a socket;
    auto esock = network::new_local_udp_endpoint_impl(0, nullptr);
    CAF_REQUIRE(esock);
    // Create newb.
    transport_ptr transport{new dummy_transport};
    test_newb = spawn_newb<protocol_t>(sys, dummy_broker, std::move(transport),
                                       esock->first, true);
  }

  ~fixture() {
    anon_send_exit(test_newb, exit_reason::user_shutdown);
    exec_all();
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

CAF_TEST(spawn acceptor) {
  auto newb_client= [] (newb_t* self) -> behavior {
    return {
      [=](quit_atom) {
        self->stop();
      },
    };
  };
  CAF_MESSAGE("create newb acceptor");
  auto esock = network::new_local_udp_endpoint_impl(0, nullptr);
  accept_ptr<new_basp_msg> accept{new dummy_accept<new_basp_msg>};
  CAF_REQUIRE(esock);
  auto n = spawn_acceptor<protocol_t>(sys, newb_client, std::move(accept),
                                      esock->first);
  exec_all();
  scoped_actor self{sys};
  self->send(n, quit_atom::value);
  exec_all();

}

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
  transport_ptr transport{new dummy_transport};
  auto n = spawn_newb<protocol_t>(sys, my_newb, std::move(transport),
                                  esock->first, true);
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
  transport_ptr transport{new dummy_transport};
  auto n = spawn_newb<protocol_t>(sys, my_newb, std::move(transport),
                                  esock->first, true);
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
  anon_send(test_newb, expect_atom::value, bhdr, payload);
  exec_all();
  CAF_MESSAGE("copy them into the buffer");
  auto& dummy = deref<newb_t>(test_newb);
  auto& buf = dummy.trans->receive_buffer;
  write_packet(buf, ohdr, bhdr, payload);
  dummy.trans->received_bytes = buf.size();
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
  anon_send(test_newb, expect_atom::value, bhdr, payload);
  anon_send(test_newb, send_atom::value, ohdr, bhdr, payload);
  exec_all();
  auto& dummy = deref<newb_t>(test_newb);
  dummy.handle_event(network::operation::read);
  CAF_MESSAGE("check the basp header and payload");
  auto& msg = dummy.state.messages.front().first;
  CAF_CHECK_EQUAL(msg.header.from, bhdr.from);
  CAF_CHECK_EQUAL(msg.header.to, bhdr.to);
  uint32_t return_payload = 0;
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
  anon_send(test_newb, expect_atom::value, bhdr, payload);
  anon_send(test_newb, send_atom::value, ohdr, bhdr, payload);
  exec_all();
  CAF_MESSAGE("trigger read event");
  auto& dummy = deref<newb_t>(test_newb);
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
  anon_send(test_newb, expect_atom::value, bhdr_first, payload_first);
  anon_send(test_newb, expect_atom::value, bhdr_second, payload_second);
  exec_all();
  auto& dummy = deref<newb_t>(test_newb);
  auto& buf = dummy.trans->receive_buffer;
  CAF_MESSAGE("read second message first");
  write_packet(buf, ohdr_second, bhdr_second, payload_second);
  dummy.trans->received_bytes = buf.size();
  dummy.read_event();
  CAF_MESSAGE("followed by first message");
  buf.clear();
  write_packet(buf, ohdr_first, bhdr_first, payload_first);
  dummy.trans->received_bytes = buf.size();
  dummy.read_event();
}

CAF_TEST(write buf) {
  exec_all();
  const basp_header bhdr{0, 13, 42};
  const uint32_t payload = 1337;
  CAF_MESSAGE("setup read event");
  anon_send(test_newb, expect_atom::value, bhdr, payload);
  anon_send(test_newb, send_atom::value, bhdr.from, bhdr.to, payload);
  exec_all();
  auto& dummy = deref<newb_t>(test_newb);
  dummy.handle_event(network::operation::read);
  // Message handler will check if the expected message was received.
}

CAF_TEST_FIXTURE_SCOPE_END()
