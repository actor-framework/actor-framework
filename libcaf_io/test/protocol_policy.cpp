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

//#include "caf/io/protocol_policy.hpp"

#include <cstdint>
#include <cstring>

#include "caf/test/dsl.hpp"

#include "caf/abstract_actor.hpp"
#include "caf/config.hpp"

#include "caf/io/network/native_socket.hpp"
// TODO: {receive_buffer => byte_buffer} (and use `byte` instead of `char`)
#include "caf/io/network/receive_buffer.hpp"

#include "caf/detail/enum_to_string.hpp"

using namespace caf;
using namespace caf::io;

using network::native_socket;

namespace {

// -- forward declarations -----------------------------------------------------

struct dummy_device;
struct newb_base;
struct protocol_policy_base;

template <class T>
struct protocol_policy;

template <class T>
struct newb;

// -- atoms --------------------------------------------------------------------

using ordering_atom = atom_constant<atom("ordering")>;

// -- aliases ------------------------------------------------------------------

using byte_buffer = network::receive_buffer;

// -- dummy headers ------------------------------------------------------------

struct basp_header {
  actor_id from;
  actor_id to;
};

struct ordering_header {
  uint32_t seq_nr;
};

// -- message types ------------------------------------------------------------

struct new_basp_message {
  basp_header header;
  const char* payload;
  size_t payload_size;
};

// -- transport policy ---------------------------------------------------------

struct transport_policy {
  virtual ~transport_policy() {
    // nop
  }

  virtual error write_some(network::native_socket) {
    return none;
  }

  byte_buffer* wr_buf() {
    return &send_buffer;
  }


  template <class T>
  optional<T> read_some(newb<T>* parent, protocol_policy<T>& policy) {
    auto err = read_some();
    if (err) {
      // Call something on parent?
      return none;
    }
    return policy.read(parent, receive_buffer.data(), receive_buffer.size());
  }

  virtual error read_some() {
    return none;
  }

  byte_buffer receive_buffer;
  byte_buffer send_buffer;
};

using transport_policy_ptr = std::unique_ptr<transport_policy>;

// -- accept policy ------------------------------------------------------------

struct accept_policy {
  virtual ~accept_policy() {
    // nop
  }
  virtual std::pair<native_socket, transport_policy_ptr> accept() = 0;
  virtual void init(newb_base&) = 0;
};

// -- protocol policies --------------------------------------------------------

struct protocol_policy_base {
  virtual ~protocol_policy_base() {
    // nop
  }

  virtual void write_header(byte_buffer& buf, size_t offset) = 0;

  virtual size_t offset() const noexcept = 0;
};

template <class T>
struct protocol_policy : protocol_policy_base {
  virtual ~protocol_policy() override {
    // nop
  }

  virtual optional<T> read(newb<T>* parent, char* bytes, size_t count) = 0;

  virtual optional<T> timeout(newb<T>* parent, caf::message& msg) = 0;

  void write_header(byte_buffer&, size_t) override {
    // nop
  }

};

template <class T>
using protocol_policy_ptr = std::unique_ptr<protocol_policy<T>>;

struct basp_policy {
  static constexpr size_t header_size = sizeof(basp_header);
  static constexpr size_t offset = header_size;
  using type = new_basp_message;
  using result_type = optional<new_basp_message>;
  //newb<type>* parent;

  result_type read(newb<type>*, char* bytes, size_t count) {
    new_basp_message msg;
    memcpy(&msg.header.from, bytes, sizeof(msg.header.from));
    memcpy(&msg.header.to, bytes + sizeof(msg.header.from), sizeof(msg.header.to));
    msg.payload = bytes + header_size;
    msg.payload_size = count - header_size;
    return msg;
  }

  result_type timeout(newb<type>*, caf::message&) {
    return none;
  }
};

template <class Next>
struct ordering {
  static constexpr size_t header_size = sizeof(ordering_header);
  static constexpr size_t offset = Next::offset + header_size;
  using type = typename Next::type;
  using result_type = typename Next::result_type;
  uint32_t next_seq = 0;
  Next next;
  std::unordered_map<uint32_t, std::vector<char>> pending;

  result_type read(newb<type>* parent, char* bytes, size_t count) {
    // TODO: What to do if we want to deliver multiple messages? For
    //       example when our buffer a missing message arrives and we
    //       can just deliver everything in our buffer?
    uint32_t seq;
    memcpy(&seq, bytes, sizeof(seq));
    if (seq != next_seq) {
      CAF_MESSAGE("adding message to pending ");
      // TODO: Only works if we have datagrams.
      pending[seq] = std::vector<char>(bytes + header_size, bytes + count);
      parent->set_timeout(std::chrono::seconds(2),
                          caf::make_message(ordering_atom::value, seq));
      return none;
    }
    next_seq++;
    return next.read(parent, bytes + header_size, count - header_size);
  }

  result_type timeout(newb<type>* parent, caf::message& msg) {
    // TODO: Same as above.
    auto matched = false;
    result_type was_pending = none;
    msg.apply([&](ordering_atom, uint32_t seq) {
      matched = true;
      if (pending.count(seq) > 0) {
        CAF_MESSAGE("found pending message");
        auto& buf = pending[seq];
        was_pending = next.read(parent, buf.data(), buf.size());
      }
    });
    if (matched)
      return was_pending;
    return next.timeout(parent, msg);

  }
};

template <class T>
struct protocol_policy_impl : protocol_policy<typename T::type> {
  T impl;
  protocol_policy_impl() : impl() {
    // nop
  }

  typename T::result_type read(newb<typename T::type>* parent, char* bytes,
                               size_t count) override {
    return impl.read(parent, bytes, count);
  }

  size_t offset() const noexcept override {
    return T::offset;
  }

  typename T::result_type timeout(newb<typename T::type>* parent,
                                  caf::message& msg) override {
    return impl.timeout(parent, msg);
  }
};

// -- new broker classes -------------------------------------------------------

/// Returned by funtion wr_buf
struct write_handle {
  protocol_policy_base* protocol;
  byte_buffer* buf;
  size_t header_offset;

  ~write_handle() {
    protocol->write_header(*buf, header_offset);
    // TODO: maybe trigger transport policy
  }
};

template <class Message>
struct newb {
  std::unique_ptr<transport_policy> transport;
  std::unique_ptr<protocol_policy<Message>> protocol;

  virtual ~newb() {
    // nop
  }

  write_handle wr_buf() {
    auto buf = transport->wr_buf();
    auto header_size = protocol->offset();
    auto header_offset = buf->size();
    buf->resize(buf->size() + header_size);
    return {protocol.get(), buf, header_offset};
  }

  // Send
  void flush() {

  }

  error read_event() {
    auto maybe_msg = transport->read_some(this, *protocol);
    if (!maybe_msg)
      return make_error(sec::unexpected_message);
    // TODO: create message on the stack and call message handler
    handle(*maybe_msg);
    return none;
  }

  void write_event() {
    // transport->write_some();
  }

  // Protocol policies can set timeouts using a custom message.
  template<class Rep = int, class Period = std::ratio<1>>
  void set_timeout(std::chrono::duration<Rep, Period>, caf::message msg) {
    // TODO: Once this is an actor send yourself a delayed messge.
    //       And on receict call ...
    set_timeout_impl(std::move(msg));
  }

  // Called on self when a timeout is received.
  error timeout_event(caf::message& msg) {
    auto maybe_msg = protocol->timeout(this, msg);
    if (!maybe_msg)
      return make_error(sec::unexpected_message);
    handle(*maybe_msg);
    return none;
  }

  // Allow protocol policies to enqueue a data for sending.
  // Probably required for reliability.
  // void direct_enqueue(char* bytes, size_t count);

  virtual void handle(Message& msg) = 0;

  // TODO: Only here for some first tests.
  virtual void set_timeout_impl(caf::message) = 0;
};

struct basp_newb : newb<new_basp_message> {
  void handle(new_basp_message&) override {
    // nop
  }
};

/*
behavior my_broker(newb<new_data_msg>* self) {
  // nop ...
}

template <class AcceptPolicy, class ProtocolPolicy>
struct newb_acceptor {
  std::unique_ptr<AcceptPolicy> acceptor;

  // read = accept
  error read_event() {
    auto [sock, trans_pol] = acceptor.accept();
    auto worker = sys.middleman.spawn_client<ProtocolPolicy>(
      sock, std::move(trans_pol), fork_behavior);
    acceptor.init(worker);
  }
};
*/

// client: sys.middleman().spawn_client<protocol_policy>(sock,
//                        std::move(transport_protocol_policy_impl), my_client);
// server: sys.middleman().spawn_server<protocol_policy>(sock,
//                        std::move(accept_protocol_policy_impl), my_server);


// -- test classes -------------------------------------------------------------

struct dummy_basp_newb : newb<new_basp_message> {
  void handle(new_basp_message& received_msg) override {
    msg = received_msg;
  }
  void set_timeout_impl(caf::message msg) override {
    timeout_messages.emplace_back(std::move(msg));
  }
  std::vector<caf::message> timeout_messages;
  new_basp_message msg;
};

struct fixture {
  dummy_basp_newb self;

  fixture() {
    self.transport.reset(new transport_policy);
    self.protocol.reset(new protocol_policy_impl<ordering<basp_policy>>);
  }
  scoped_execution_unit context;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(protocol_policy_tests, fixture)

CAF_TEST(ordering and basp read event) {
  // Read process:
  //  - read event happens
  //  - read_event() called on newb
  //  - read_some(protocol&) called on transport policy
  //  - transport policies reads from network layer
  //  - calls read on protocol policy
  //  - the basp policy fills the expected result message with data
  CAF_MESSAGE("create some values for our buffer");
  ordering_header ohdr{0};
  basp_header bhdr{13, 42};
  int payload = 1337;
  CAF_MESSAGE("copy them into the buffer");
  auto& buf = self.transport->receive_buffer;
  // Make sure the buffer is big enough.
  buf.resize(sizeof(ordering_header)
              + sizeof(basp_header)
              + sizeof(payload));
  // Track an offset for writing.
  size_t offset = 0;
  memcpy(buf.data() + offset, &ohdr, sizeof(ordering_header));
  offset += sizeof(ordering_header);
  memcpy(buf.data() + offset, &bhdr, sizeof(basp_header));
  offset += sizeof(basp_header);
  memcpy(buf.data() + offset, &payload, sizeof(payload));
  CAF_MESSAGE("trigger a read event");
  auto err = self.read_event();
  CAF_REQUIRE(!err);
  CAF_MESSAGE("check the basp header and payload");
  CAF_CHECK_EQUAL(self.msg.header.from, bhdr.from);
  CAF_CHECK_EQUAL(self.msg.header.to, bhdr.to);
  CAF_CHECK_EQUAL(self.msg.payload_size, sizeof(payload));
  int return_payload = 0;
  memcpy(&return_payload, self.msg.payload, self.msg.payload_size);
  CAF_CHECK_EQUAL(return_payload, payload);
}

CAF_TEST(ordering and basp read event with timeout) {
  // Read process:
  //  - read event happens
  //  - read_event() called on newb
  //  - read_some(protocol&) called on transport policy
  //  - transport policies reads from network layer
  //  - calls read on protocol policy
  //  - the ordering policy sets a timeout
  //  -
  CAF_MESSAGE("create some values for our buffer");
  // Should be an unexpected sequence number and lead to an error.
  ordering_header ohdr{1};
  basp_header bhdr{13, 42};
  int payload = 1337;
  CAF_MESSAGE("copy them into the buffer");
  auto& buf = self.transport->receive_buffer;
  // Make sure the buffer is big enough.
  buf.resize(sizeof(ordering_header)
              + sizeof(basp_header)
              + sizeof(payload));
  // Track an offset for writing.
  size_t offset = 0;
  memcpy(buf.data() + offset, &ohdr, sizeof(ordering_header));
  offset += sizeof(ordering_header);
  memcpy(buf.data() + offset, &bhdr, sizeof(basp_header));
  offset += sizeof(basp_header);
  memcpy(buf.data() + offset, &payload, sizeof(payload));
  CAF_MESSAGE("trigger a read event, expecting an error");
  auto err = self.read_event();
  CAF_REQUIRE(err);
  CAF_MESSAGE("check if we have a pending timeout now");
  CAF_REQUIRE(!self.timeout_messages.empty());
  auto& timeout_msg = self.timeout_messages.back();
  auto read_message = false;
  timeout_msg.apply([&](ordering_atom, uint32_t seq) {
    if (seq == ohdr.seq_nr)
      read_message = true;
  });
  CAF_REQUIRE(read_message);
  CAF_MESSAGE("trigger timeout");
  err = self.timeout_event(timeout_msg);
  CAF_REQUIRE(!err);
  CAF_MESSAGE("check delivered message");
  CAF_CHECK_EQUAL(self.msg.header.from, bhdr.from);
  CAF_CHECK_EQUAL(self.msg.header.to, bhdr.to);
  CAF_CHECK_EQUAL(self.msg.payload_size, sizeof(payload));
  int return_payload = 0;
  memcpy(&return_payload, self.msg.payload, self.msg.payload_size);
  CAF_CHECK_EQUAL(return_payload, payload);
}

CAF_TEST(ordering and basp write event) {
  CAF_MESSAGE("Not implemented");
}

CAF_TEST_FIXTURE_SCOPE_END()
