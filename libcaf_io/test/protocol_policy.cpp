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
#include "caf/callback.hpp"
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
using header_writer = caf::callback<byte_buffer&>;

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

  byte_buffer& wr_buf() {
    return send_buffer;
  }

  template <class T>
  error read_some(protocol_policy<T>& policy) {
    auto err = read_some();
    if (err)
      return err;
    return policy.read(receive_buffer.data(), receive_buffer.size());
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

struct accept_policy_impl : accept_policy {
  std::pair<native_socket, transport_policy_ptr> accept() override {
    native_socket sock{13337};
    transport_policy_ptr ptr{new transport_policy};
    return {sock, std::move(ptr)};
  }

  void init(newb_base&) override {
    // nop
  }
};

// -- protocol policies --------------------------------------------------------

struct protocol_policy_base {
  virtual ~protocol_policy_base() {
    // nop
  }

//  virtual void write_header(byte_buffer& buf, size_t offset) = 0;

  virtual size_t offset() const noexcept = 0;
};

template <class T>
struct protocol_policy : protocol_policy_base {
  using message_type = T;
  virtual ~protocol_policy() override {
    // nop
  }

  virtual error read(char* bytes, size_t count) = 0;

  virtual error timeout(caf::message& msg) = 0;

  /// TODO: Come up with something better than a write here?
  /// Write header into buffer. Use push back to append only.
  virtual size_t write_header(byte_buffer&, header_writer*) = 0;
};

template <class T>
using protocol_policy_ptr = std::unique_ptr<protocol_policy<T>>;

template <class T>
struct protocol_policy_impl : protocol_policy<typename T::message_type> {
  T impl;

  protocol_policy_impl(newb<typename T::message_type>* parent) : impl(parent) {
    // nop
  }

  error read(char* bytes, size_t count) override {
    return impl.read(bytes, count);
  }

  size_t offset() const noexcept override {
    return T::offset;
  }

  error timeout(caf::message& msg) override {
    return impl.timeout(msg);
  }

  size_t write_header(byte_buffer& buf, header_writer* hw) override {
    return impl.write_header(buf, 0, hw);
  }
};

// -- new broker classes -------------------------------------------------------

/// @relates newb
/// Returned by funtion wr_buf of newb.
struct write_handle {
  protocol_policy_base* protocol;
  byte_buffer* buf;
  size_t header_offset;

  /*
  ~write_handle() {
    // TODO: maybe trigger transport policy for ... what again?
    // Can we calculate added bytes for datagram things?
  }
  */
};

template <class Message>
struct newb {
  std::unique_ptr<transport_policy> transport;
  std::unique_ptr<protocol_policy<Message>> protocol;

  newb() = default;
  newb(newb<Message>&&) = default;

  virtual ~newb() {
    // nop
  }

  write_handle wr_buf(header_writer* hw) {
    // TODO: We somehow need to tell the transport policy how much we've
    // written to enable it to split the buffer into datagrams.
    auto& buf = transport->wr_buf();
    auto header_offset = protocol->write_header(buf, hw);
    return {protocol.get(), &buf, header_offset};
  }

  // Send
  void flush() {

  }

  error read_event() {
    return transport->read_some(*protocol);
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
    return protocol->timeout(msg);
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

template <class ProtocolPolicy>
struct newb_acceptor {
  std::unique_ptr<accept_policy> acceptor;

  error read_event() {
    CAF_MESSAGE("read event on newb acceptor");
    auto res = acceptor->accept();
    native_socket sock{res.first};
    transport_policy_ptr transport{std::move(res.second)};
//    std::tie(sock, transport) = acceptor->accept();;
    return create_newb(sock, std::move(transport));
  }

  virtual error create_newb(native_socket sock, transport_policy_ptr pol) = 0;
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

// -- policies -----------------------------------------------------------------

/// @relates protocol_policy
/// Protocol policy layer for the BASP application protocol.
struct basp_policy {
  static constexpr size_t header_size = sizeof(basp_header);
  static constexpr size_t offset = header_size;
  using message_type = new_basp_message;
  using result_type = optional<new_basp_message>;
  newb<message_type>* parent;

  basp_policy(newb<message_type>* parent) : parent(parent) {
    // nop
  }

  error read(char* bytes, size_t count) {
    new_basp_message msg;
    memcpy(&msg.header.from, bytes, sizeof(msg.header.from));
    memcpy(&msg.header.to, bytes + sizeof(msg.header.from), sizeof(msg.header.to));
    msg.payload = bytes + header_size;
    msg.payload_size = count - header_size;
    // Using the result for messages has some problems, so ...
    parent->handle(msg);
    return none;
  }

  error timeout(caf::message&) {
    return none;
  }

  size_t write_header(byte_buffer& buf, size_t offset, header_writer* hw) {
    CAF_ASSERT(hw != nullptr);
    (*hw)(buf);
    return offset + header_size;
  }
};

/// @relates protocol_policy
/// Protocol policy layer for ordering.
template <class Next>
struct ordering {
  static constexpr size_t header_size = sizeof(ordering_header);
  static constexpr size_t offset = Next::offset + header_size;
  using message_type = typename Next::message_type;
  using result_type = typename Next::result_type;
  uint32_t seq_read = 0;
  uint32_t seq_write = 0;
  newb<message_type>* parent;
  Next next;
  std::unordered_map<uint32_t, std::vector<char>> pending;

  ordering(newb<message_type>* parent) : parent(parent), next(parent) {
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
    uint32_t seq;
    memcpy(&seq, bytes, sizeof(seq));
    // TODO: Use the comparison function from BASP instance.
    if (seq == seq_read) {
      seq_read += 1;
      auto res = next.read(bytes + header_size, count - header_size);
      if (res)
        return res;
      return deliver_pending();
    } else if (seq > seq_read) {
      pending[seq] = std::vector<char>(bytes + header_size, bytes + count);
      parent->set_timeout(std::chrono::seconds(2),
                          caf::make_message(ordering_atom::value, seq));
      return none;
    }
    // Is late, drop it. TODO: Should this return an error?
    return none;
  }

  error timeout(caf::message& msg) {
    // TODO: Can't we just set the seq_read to the sequence number and call
    //       deliver messages?
    error err = none;
    msg.apply([&](ordering_atom, uint32_t seq) {
      if (pending.count(seq) > 0) {
        CAF_MESSAGE("found pending message");
        auto& buf = pending[seq];
        err = next.read(buf.data(), buf.size());
        seq_read = seq + 1;
      }
    });
    if (err)
      return err;
    return deliver_pending();
  }

  size_t write_header(byte_buffer& buf, size_t offset, header_writer* hw) {
    std::array<char, sizeof (seq_write)> tmp;
    memcpy(tmp.data(), &seq_write, sizeof(seq_write));
    seq_write += 1;
    for (auto& c : tmp)
      buf.push_back(c);
    return next.write_header(buf, offset + header_size, hw);
  }
};

// -- test classes -------------------------------------------------------------

struct dummy_basp_newb : newb<new_basp_message> {
  std::vector<caf::message> timeout_messages;
  std::vector<std::pair<new_basp_message, std::vector<char>>> messages;

  void handle(new_basp_message& msg) override {
    std::vector<char> payload{msg.payload, msg.payload + msg.payload_size};
    messages.emplace_back(msg, payload);
    messages.back().first.payload = messages.back().second.data();
  }

  void set_timeout_impl(caf::message msg) override {
    timeout_messages.emplace_back(std::move(msg));
  }
};

template <class ProtocolPolicy>
struct dummy_basp_newb_acceptor : newb_acceptor<ProtocolPolicy> {
  std::vector<dummy_basp_newb> spawned;

  error create_newb(native_socket, transport_policy_ptr pol) override {
    spawned.emplace_back();
    auto& n = spawned.back();
    n.transport.reset(pol.release());
    n.protocol.reset( new ProtocolPolicy(&n));
    return none;
  }
};

struct datagram_fixture {
  dummy_basp_newb self;

  datagram_fixture() {
    self.transport.reset(new transport_policy);
    self.protocol.reset(new protocol_policy_impl<ordering<basp_policy>>(&self));
  }
  scoped_execution_unit context;
};

struct stream_fixture {
  dummy_basp_newb self;

  stream_fixture() {
    self.transport.reset(new transport_policy);
    self.protocol.reset(new protocol_policy_impl<basp_policy>(&self));
  }
  scoped_execution_unit context;
};

struct acceptor_fixture {
  dummy_basp_newb_acceptor<protocol_policy_impl<ordering<basp_policy>>> self;

  acceptor_fixture() {
    self.acceptor.reset(new accept_policy_impl);
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(protocol_policy_tests, datagram_fixture)

CAF_TEST(ordering and basp read event) {
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
  auto& msg = self.messages.front().first;
  CAF_CHECK_EQUAL(msg.header.from, bhdr.from);
  CAF_CHECK_EQUAL(msg.header.to, bhdr.to);
  CAF_CHECK_EQUAL(msg.payload_size, sizeof(payload));
  int return_payload = 0;
  memcpy(&return_payload, msg.payload, msg.payload_size);
  CAF_CHECK_EQUAL(return_payload, payload);
}

CAF_TEST(ordering and basp read event with timeout) {
  CAF_MESSAGE("create some values for our buffer");
  // Should be an unexpected sequence number and lead to an error. Since
  // we start with 0, the 1 below should be out of order.
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
  CAF_MESSAGE("trigger a read event");
  auto err = self.read_event();
  CAF_REQUIRE(!err);
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
  auto& msg = self.messages.front().first;
  CAF_CHECK_EQUAL(msg.header.from, bhdr.from);
  CAF_CHECK_EQUAL(msg.header.to, bhdr.to);
  CAF_CHECK_EQUAL(msg.payload_size, sizeof(payload));
  int return_payload = 0;
  memcpy(&return_payload, msg.payload, msg.payload_size);
  CAF_CHECK_EQUAL(return_payload, payload);
}

CAF_TEST(ordering and basp multiple messages) {
  // Should enqueue the first message out of order as above, followed by the
  // missing message. The result should be both messages in the receive buffer
  // in the right order.
  // The problem is that our API cannot currently express that. Simply returning
  // a vector of `new_basp_message` objects doesn't work as the objects just
  // include a pointer to the buffer. This makes sense as we want to avoid
  // copying everything around. It would be much easier to just call `handle`
  // on the newb since we already have the reference and so on ...
  CAF_MESSAGE("create data for two messges");
  // Message one.
  ordering_header ohdr_first{0};
  basp_header bhdr_first{10, 11};
  int payload_first = 100;
  // Message two.
  ordering_header ohdr_second{1};
  basp_header bhdr_second{12, 13};
  int payload_second = 101;
  auto& buf = self.transport->receive_buffer;
  // Make sure the buffer is big enough.
  buf.resize(sizeof(ordering_header)
              + sizeof(basp_header)
              + sizeof(payload_first));
  CAF_MESSAGE("create event for the second message first");
  // Track an offset for writing.
  size_t offset = 0;
  memcpy(buf.data() + offset, &ohdr_second, sizeof(ordering_header));
  offset += sizeof(ordering_header);
  memcpy(buf.data() + offset, &bhdr_second, sizeof(basp_header));
  offset += sizeof(basp_header);
  memcpy(buf.data() + offset, &payload_second, sizeof(payload_second));
  CAF_MESSAGE("trigger a read event, expecting an error");
  auto err = self.read_event();
  CAF_REQUIRE(!err);
  CAF_MESSAGE("check if we have a pending timeout now");
  CAF_REQUIRE(!self.timeout_messages.empty());
  auto& timeout_msg = self.timeout_messages.back();
  auto expected_timeout = false;
  timeout_msg.apply([&](ordering_atom, uint32_t seq) {
    if (seq == ohdr_second.seq_nr)
      expected_timeout = true;
  });
  CAF_REQUIRE(expected_timeout);
  CAF_MESSAGE("create event for the first message");
  // Track an offset for writing.
  offset = 0;
  memcpy(buf.data() + offset, &ohdr_first, sizeof(ordering_header));
  offset += sizeof(ordering_header);
  memcpy(buf.data() + offset, &bhdr_first, sizeof(basp_header));
  offset += sizeof(basp_header);
  memcpy(buf.data() + offset, &payload_first, sizeof(payload_first));
  // This not clean.
  CAF_MESSAGE("trigger a read event");
  err = self.read_event();
  CAF_REQUIRE(!err);
  CAF_CHECK(self.messages.size() == 2);
  CAF_MESSAGE("check first message");
  auto& msg = self.messages.front().first;
  CAF_CHECK_EQUAL(msg.header.from, bhdr_first.from);
  CAF_CHECK_EQUAL(msg.header.to, bhdr_first.to);
  CAF_CHECK_EQUAL(msg.payload_size, sizeof(payload_first));
  int return_payload = 0;
  memcpy(&return_payload, msg.payload, msg.payload_size);
  CAF_CHECK_EQUAL(return_payload, payload_first);
  CAF_MESSAGE("check second message");
  msg = self.messages.back().first;
  CAF_CHECK_EQUAL(msg.header.from, bhdr_second.from);
  CAF_CHECK_EQUAL(msg.header.to, bhdr_second.to);
  CAF_CHECK_EQUAL(msg.payload_size, sizeof(payload_second));
  return_payload = 0;
  memcpy(&return_payload, msg.payload, msg.payload_size);
  CAF_CHECK_EQUAL(return_payload, payload_second);
}

CAF_TEST(ordering and basp write buf) {
  basp_header bhdr{13, 42};
  int payload = 1337;
  CAF_MESSAGE("create a callback to write the BASP header");
  auto hw = caf::make_callback([&](byte_buffer& buf) -> error {
    std::array<char, sizeof (bhdr)> tmp;
    memcpy(tmp.data(), &bhdr.from, sizeof(bhdr.from));
    memcpy(tmp.data() + sizeof(bhdr.from), &bhdr.to, sizeof(bhdr.to));
    for (char& c : tmp)
      buf.push_back(c);
    return none;
  });
  CAF_MESSAGE("get a write buffer");
  auto whdl = self.wr_buf(&hw);
  CAF_REQUIRE(whdl.buf != nullptr);
  CAF_REQUIRE(whdl.header_offset == sizeof(basp_header) + sizeof(ordering_header));
  CAF_REQUIRE(whdl.protocol != nullptr);
  CAF_MESSAGE("write the payload");
  std::array<char, sizeof(payload)> tmp;
  memcpy(tmp.data(), &payload, sizeof(payload));
  for (auto c : tmp)
    whdl.buf->push_back(c);
  CAF_MESSAGE("swap send and receive buffer of the payload");
  std::swap(self.transport->receive_buffer, self.transport->send_buffer);
  CAF_MESSAGE("trigger a read event");
  auto err = self.read_event();
  CAF_REQUIRE(!err);
  CAF_MESSAGE("check the basp header and payload");
  auto& msg = self.messages.front().first;
  CAF_CHECK_EQUAL(msg.header.from, bhdr.from);
  CAF_CHECK_EQUAL(msg.header.to, bhdr.to);
  CAF_CHECK_EQUAL(msg.payload_size, sizeof(payload));
  int return_payload = 0;
  memcpy(&return_payload, msg.payload, msg.payload_size);
  CAF_CHECK_EQUAL(return_payload, payload);
}

CAF_TEST(slicing and basp) {
  // TODO: Come up with a way to implement this with policies. What is missing
  //  in the API? The problem here is that this is not just a write_header call
  //  when acquireing the write buffer. There is more to it.
  // TOUGHTS: For receipt this is located somewhere between ordering /
  //  reliability and BASP. For sending it would need to split messages into
  //  multiple parts after the BASP header was written but before ordering or
  //  reliability add their headers.
  CAF_MESSAGE("not implemented");
}

CAF_TEST_FIXTURE_SCOPE_END()

CAF_TEST_FIXTURE_SCOPE(stream_policy_tests, stream_fixture)

CAF_TEST(basp and streaming) {
  // TODO: As far as I can tell atm, we would need to reimplement the BASP
  //  policy as it works differently on datgrams and streams. Needs two reads
  //  for streaming: once for the header and once for the payload.
  // IDEA: We could let the policy track itself if it expects payload. When it
  //  receives a message it dezerializes the header and tests if it still has a
  //  payload of the expected size. Or something like that.
  //  Easiest way is to implement BASP twice.
  CAF_MESSAGE("not implemented");
}

CAF_TEST_FIXTURE_SCOPE_END()

CAF_TEST_FIXTURE_SCOPE(acceptor_policy_tests, acceptor_fixture)

CAF_TEST(ordering and basp acceptor) {
  CAF_MESSAGE("trigger read event on acceptor");
  self.read_event();
  CAF_MESSAGE("test if accept was successful");
  // TODO: implement init to initialize the newb?
  CAF_CHECK(!self.spawned.empty());
  auto& bn = self.spawned.front();
  CAF_MESSAGE("create some values for the newb");
  ordering_header ohdr{0};
  basp_header bhdr{13, 42};
  int payload = 1337;
  CAF_MESSAGE("copy them into the buffer");
  auto& buf = bn.transport->receive_buffer;
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
  auto err = bn.read_event();
  CAF_REQUIRE(!err);
  CAF_MESSAGE("check the basp header and payload");
  auto& msg = bn.messages.front().first;
  CAF_CHECK_EQUAL(msg.header.from, bhdr.from);
  CAF_CHECK_EQUAL(msg.header.to, bhdr.to);
  CAF_CHECK_EQUAL(msg.payload_size, sizeof(payload));
  int return_payload = 0;
  memcpy(&return_payload, msg.payload, msg.payload_size);
  CAF_CHECK_EQUAL(return_payload, payload);
}

CAF_TEST_FIXTURE_SCOPE_END()
