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

using namespace caf;
using namespace caf::io;

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

// -- dummy headers ------------------------------------------------------------

struct basp_header {
  actor_id from;
  actor_id to;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun, basp_header& hdr) {
  return fun(meta::type_name("basp_header"), hdr.from, hdr.to);
}

struct ordering_header {
  uint32_t seq_nr;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun, ordering_header& hdr) {
  return fun(meta::type_name("ordering_header"), hdr.seq_nr);
}

// -- message type -------------------------------------------------------------

struct new_basp_message {
  basp_header header;
  // TODO: should be const, but binary deserializer doesn't like that.
  char* payload;
  size_t payload_size;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, new_basp_message& x) {
  return f(meta::type_name("new_basp_message"), x.header);
}

} // namespace anonymous

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(new_basp_message);

namespace {

template <class T>
struct protocol_policy_impl
    : public network::protocol_policy<typename T::message_type> {
  T impl;

  protocol_policy_impl(network::newb<typename T::message_type>* parent)
      : impl(parent) {
    // nop
  }

  error read(char* bytes, size_t count) override {
    return impl.read(bytes, count);
  }

  error timeout(atom_value atm, uint32_t id) override {
    return impl.timeout(atm, id);
  }

  void write_header(byte_buffer& buf, header_writer* hw) override {
    return impl.write_header(buf, hw);
  }

  void prepare_for_sending(byte_buffer&, size_t, size_t, size_t) override {
    return;
  }
};

struct basp_policy {
  static constexpr size_t header_size = sizeof(basp_header);
  static constexpr size_t offset = header_size;
  using message_type = new_basp_message;
  using result_type = optional<new_basp_message>;
  network::newb<message_type>* parent;

  basp_policy(network::newb<message_type>* parent) : parent(parent) {
    // nop
  }

  error read(char* bytes, size_t count) {
    if (count < header_size) {
      CAF_MESSAGE("data left in packet to small to contain the basp header");
      return sec::unexpected_message;
    }
    new_basp_message msg;
    binary_deserializer bd(&parent->backend(), bytes, count);
    bd(msg.header);
    msg.payload = bytes + header_size;
    msg.payload_size = count - header_size;
    parent->handle(msg);
    return none;
  }

  error timeout(atom_value, uint32_t) {
    return none;
  }

  void write_header(byte_buffer& buf, header_writer* hw) {
    CAF_ASSERT(hw != nullptr);
    (*hw)(buf);
    return;
  }
};

template <class Next>
struct ordering {
  static constexpr size_t header_size = sizeof(ordering_header);
  static constexpr size_t offset = Next::offset + header_size;
  using message_type = typename Next::message_type;
  using result_type = typename Next::result_type;
  uint32_t seq_read = 0;
  uint32_t seq_write = 0;
  network::newb<message_type>* parent;
  Next next;
  std::unordered_map<uint32_t, std::vector<char>> pending;

  ordering(network::newb<message_type>* parent)
      : parent(parent), next(parent) {
    // nop
  }

  error deliver_pending() {
    if (pending.empty())
      return none;
    while (pending.count(seq_read) > 0) {
      auto& buf = pending[seq_read];
      auto res = next.read(buf.data(), buf.size());
      pending.erase(seq_read);
      if (res)
        return res;
    }
    return none;
  }

  error read(char* bytes, size_t count) {
    CAF_MESSAGE("ordering: read (" << count << ")");
    if (count < header_size)
      return sec::unexpected_message;
    ordering_header hdr;
    binary_deserializer bd(&parent->backend(), bytes, count);
    bd(hdr);
    // TODO: Use the comparison function from BASP instance.
    if (hdr.seq_nr == seq_read) {
      seq_read += 1;
      auto res = next.read(bytes + header_size, count - header_size);
      if (res)
        return res;
      return deliver_pending();
    } else if (hdr.seq_nr > seq_read) {
      pending[hdr.seq_nr] = std::vector<char>(bytes + header_size,
                                              bytes + count);
      parent->set_timeout(std::chrono::seconds(2), ordering_atom::value,
                          hdr.seq_nr);
      return none;
    }
    // Is late, drop it. TODO: Should this return an error?
    return none;
  }

  error timeout(atom_value atm, uint32_t id) {
    if (atm == ordering_atom::value) {
      error err;
      if (pending.count(id) > 0) {
        auto& buf = pending[id];
        err = next.read(buf.data(), buf.size());
        seq_read = id + 1;
        if (!err)
          err = deliver_pending();
      }
      return err;
    }
    return next.timeout(atm, id);
  }

  void write_header(byte_buffer& buf, header_writer* hw) {
    binary_serializer bs(&parent->backend(), buf);
    bs(ordering_header{seq_write});
    seq_write += 1;
    next.write_header(buf, hw);
    return;
  }
};

struct dummy_basp_newb : network::newb<new_basp_message> {
  std::vector<std::pair<atom_value, uint32_t>> timeout_messages;
  std::vector<std::pair<new_basp_message, std::vector<char>>> messages;
  std::deque<std::pair<basp_header, int>> expected;

  dummy_basp_newb(caf::actor_config& cfg, default_multiplexer& dm,
                  native_socket sockfd)
      : newb<new_basp_message>(cfg, dm, sockfd) {
    // nop
  }

  void handle(new_basp_message& msg) override {
    CAF_MESSAGE("handling new basp message = " << to_string(msg));
    CAF_ASSERT(!expected.empty());
    auto& e = expected.front();
    CAF_CHECK_EQUAL(msg.header.from, e.first.from);
    CAF_CHECK_EQUAL(msg.header.to, e.first.to);
    int pl;
    binary_deserializer bd(&backend(), msg.payload, msg.payload_size);
    bd(pl);
    CAF_CHECK_EQUAL(pl, e.second);
    std::vector<char> payload{msg.payload, msg.payload + msg.payload_size};
    messages.emplace_back(msg, payload);
    messages.back().first.payload = messages.back().second.data();
    transport->receive_buffer.clear();
    expected.pop_front();
  }

  behavior make_behavior() override {
    set_default_handler(print_and_drop);
    return {
      // Must be implemented at the moment, will be cought by the broker in a
      // later implementation.
      [=](atom_value atm, uint32_t id) {
        CAF_MESSAGE("timeout returned");
        timeout_messages.emplace_back(atm, id);
        protocol->timeout(atm, id);
      },
      [=](send_atom, actor_id sender, actor_id receiver, int payload) {
        CAF_MESSAGE("send: from = " << sender << " to = " << receiver
                    << " payload = " << payload);
        auto hw = caf::make_callback([&](byte_buffer& buf) -> error {
          binary_serializer bs(&backend(), buf);
          bs(basp_header{sender, receiver});
          return none;
        });
        auto whdl = wr_buf(&hw);
        CAF_CHECK(whdl.buf != nullptr);
        CAF_CHECK(whdl.protocol != nullptr);
        binary_serializer bs(&backend(), *whdl.buf);
        bs(payload);
        std::swap(transport->receive_buffer, transport->offline_buffer);
        transport->send_buffer.clear();
        transport->received_bytes = transport->receive_buffer.size();
      },
      [=](send_atom, ordering_header& ohdr, basp_header& bhdr, int payload) {
        CAF_MESSAGE("send: ohdr = " << to_string(ohdr) << " bhdr = "
                    << to_string(bhdr) << " payload = " << payload);
        binary_serializer bs(&backend_, transport->receive_buffer);
        bs(ohdr);
        bs(bhdr);
        bs(payload);
        transport->received_bytes = transport->receive_buffer.size();
      },
      [=](expect_atom, basp_header& bhdr, int payload) {
        expected.push_back(std::make_pair(bhdr, payload));
      }
    };
  }
};

struct accept_policy_impl : public network::accept_policy<new_basp_message> {
  expected<native_socket> create_socket(uint16_t, const char*, bool) override {
    return sec::bad_function_call;
  }

  std::pair<native_socket, network::transport_policy_ptr>
    accept(network::event_handler*) override {
    // TODO: For UDP read the message into a buffer. Create a new socket.
    //  Move the buffer into the transport policy as the new receive buffer.
    network::transport_policy_ptr ptr{new network::transport_policy};
    return {invalid_native_socket, std::move(ptr)};
  }

  void init(network::newb<new_basp_message>& n) override {
    n.handle_event(network::operation::read);
  }
};

template <class ProtocolPolicy>
struct dummy_basp_newb_acceptor
    : public network::newb_acceptor<typename ProtocolPolicy::message_type> {
  using super = network::newb_acceptor<typename ProtocolPolicy::message_type>;
  using message_tuple_t = std::tuple<ordering_header, basp_header, int>;

  dummy_basp_newb_acceptor(default_multiplexer& dm, native_socket sockfd)
      : super(dm, sockfd) {
    // nop
  }

  expected<actor> create_newb(native_socket sockfd,
                              network::transport_policy_ptr pol) override {
    spawned.emplace_back(
      network::make_newb<dummy_basp_newb>(this->backend().system(), sockfd)
    );
    auto ptr = caf::actor_cast<caf::abstract_actor*>(spawned.back());
    if (ptr == nullptr)
      return sec::runtime_error;
    auto& ref = dynamic_cast<dummy_basp_newb&>(*ptr);
    ref.transport.reset(pol.release());
    ref.protocol.reset(new ProtocolPolicy(&ref));
    ref.transport->max_consecutive_reads = 1;
    // TODO: Call read_some using the buffer of the ref as a destination.
    binary_serializer bs(&this->backend(), ref.transport->receive_buffer);
    bs(get<0>(msg));
    bs(get<1>(msg));
    bs(get<2>(msg));
    ref.transport->received_bytes = ref.transport->receive_buffer.size();
    ref.expected.emplace_back(get<1>(msg), get<2>(msg));
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
  using policy_t = protocol_policy_impl<ordering<basp_policy>>;
  using acceptor_t = dummy_basp_newb_acceptor<policy_t>;
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
    // Create newb.
    self = network::make_newb<dummy_basp_newb>(sys,
                                                   network::invalid_native_socket);
    auto& ref = deref<network::newb<new_basp_message>>(self);
    network::newb<new_basp_message>* self_ptr = &ref;
    ref.transport.reset(new network::transport_policy);
    ref.protocol.reset(new protocol_policy_impl<ordering<basp_policy>>(self_ptr));
    // Create acceptor.
    auto& mpx = dynamic_cast<default_multiplexer&>(sys.middleman().backend());
    std::unique_ptr<acceptor_t> ptr{new acceptor_t(mpx, invalid_native_socket)};
    ptr->acceptor.reset(new accept_policy_impl);
    na = std::move(ptr);
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
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(newb_basics, fixture)

CAF_TEST(read event) {
  exec_all();
  CAF_MESSAGE("create some values for our buffer");
  ordering_header ohdr{0};
  basp_header bhdr{13, 42};
  int payload = 1337;
  anon_send(self, expect_atom::value, bhdr, payload);
  exec_all();
  CAF_MESSAGE("copy them into the buffer");
  auto& dummy = deref<dummy_basp_newb>(self);
  auto& buf = dummy.transport->receive_buffer;
  dummy.transport->max_consecutive_reads = 1;
  // Write data to buffer.
  binary_serializer bs(sys, buf);
  bs(ohdr);
  bs(bhdr);
  bs(payload);
  dummy.transport->received_bytes = buf.size();
  CAF_MESSAGE("trigger a read event");
  auto err = dummy.read_event();
  CAF_REQUIRE(!err);
  CAF_MESSAGE("check the basp header and payload");
  CAF_REQUIRE(!dummy.messages.empty());
  auto& msg = dummy.messages.front().first;
  CAF_CHECK_EQUAL(msg.header.from, bhdr.from);
  CAF_CHECK_EQUAL(msg.header.to, bhdr.to);
  int return_payload = 0;
  binary_deserializer bd(sys, msg.payload, msg.payload_size);
  bd(return_payload);
  CAF_CHECK_EQUAL(return_payload, payload);
}

CAF_TEST(message passing) {
  exec_all();
  ordering_header ohdr{0};
  basp_header bhdr{13, 42};
  int payload = 1337;
  CAF_MESSAGE("setup read event");
  anon_send(self, expect_atom::value, bhdr, payload);
  anon_send(self, send_atom::value, ohdr, bhdr, payload);
  exec_all();
  auto& dummy = deref<dummy_basp_newb>(self);
  dummy.transport->max_consecutive_reads = 1;
  dummy.handle_event(network::operation::read);
  CAF_MESSAGE("check the basp header and payload");
  auto& msg = dummy.messages.front().first;
  CAF_CHECK_EQUAL(msg.header.from, bhdr.from);
  CAF_CHECK_EQUAL(msg.header.to, bhdr.to);
  int return_payload = 0;
  binary_deserializer bd(sys, msg.payload, msg.payload_size);
  bd(return_payload);
  CAF_CHECK_EQUAL(return_payload, payload);
}

CAF_TEST(timeouts) {
  // Should be an unexpected sequence number and lead to an error. Since
  // we start with 0, the 1 below should be out of order.
  ordering_header ohdr{1};
  basp_header bhdr{13, 42};
  int payload = 1337;
  CAF_MESSAGE("setup read event");
  anon_send(self, expect_atom::value, bhdr, payload);
  anon_send(self, send_atom::value, ohdr, bhdr, payload);
  exec_all();
  CAF_MESSAGE("trigger read event");
  auto& dummy = deref<dummy_basp_newb>(self);
  dummy.transport->max_consecutive_reads = 1;
  auto err = dummy.read_event();
  CAF_REQUIRE(!err);
  CAF_MESSAGE("trigger waiting timeouts");
  // Trigger timeout.
  sched.dispatch();
  // Handle received message.
  exec_all();
  // Message handler will check if the expected message was received.
}

CAF_TEST(message ordering) {
  CAF_MESSAGE("create data for two messges");
  // Message one.
  ordering_header ohdr_first{0};
  basp_header bhdr_first{10, 11};
  int payload_first = 100;
  // Message two.
  ordering_header ohdr_second{1};
  basp_header bhdr_second{12, 13};
  int payload_second = 101;
  CAF_MESSAGE("setup read events");
  anon_send(self, expect_atom::value, bhdr_first, payload_first);
  anon_send(self, expect_atom::value, bhdr_second, payload_second);
  exec_all();
  auto& dummy = deref<dummy_basp_newb>(self);
  dummy.transport->max_consecutive_reads = 1;
  auto& buf = dummy.transport->receive_buffer;
  CAF_MESSAGE("read second message first");
  {
    binary_serializer bs(sys, buf);
    bs(ohdr_second);
    bs(bhdr_second);
    bs(payload_second);
  }
  dummy.transport->received_bytes = buf.size();
  dummy.read_event();
  CAF_MESSAGE("followed by first message");
  buf.clear();
  {
    binary_serializer bs(sys, buf);
    bs(ohdr_first);
    bs(bhdr_first);
    bs(payload_first);
  }
  dummy.transport->received_bytes = buf.size();
  dummy.read_event();
}

CAF_TEST(write buf) {
  exec_all();
  basp_header bhdr{13, 42};
  int payload = 1337;
  CAF_MESSAGE("setup read event");
  anon_send(self, expect_atom::value, bhdr, payload);
  anon_send(self, send_atom::value, bhdr.from, bhdr.to, payload);
  exec_all();
  auto& dummy = deref<dummy_basp_newb>(self);
  dummy.transport->max_consecutive_reads = 1;
  dummy.handle_event(network::operation::read);
  // Message handler will check if the expected message was received.
}

CAF_TEST(newb acceptor) {
  CAF_MESSAGE("trigger read event on acceptor");
  // This will write a message into the receive buffer and trigger
  // a read event on the newly created newb.
  na->handle_event(network::operation::read);
  auto& dummy = dynamic_cast<acceptor_t&>(*na.get());
  CAF_CHECK(!dummy.spawned.empty());
}

CAF_TEST_FIXTURE_SCOPE_END()
