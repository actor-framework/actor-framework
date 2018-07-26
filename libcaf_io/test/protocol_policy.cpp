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
#include <tuple>

#include "caf/test/dsl.hpp"

#include "caf/callback.hpp"
#include "caf/config.hpp"
#include "caf/scheduled_actor.hpp"

#include "caf/io/middleman.hpp"

#include "caf/io/network/event_handler.hpp"
#include "caf/io/network/default_multiplexer.hpp"
#include "caf/io/network/native_socket.hpp"

#include "caf/detail/enum_to_string.hpp"

#include "caf/mixin/sender.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/behavior_changer.hpp"

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

} // namespace <anonymous>

namespace caf {

template <class T>
class behavior_type_of<newb<T>> {
public:
  using type = behavior;
};

} // namespace caf

namespace {

// -- atoms --------------------------------------------------------------------

using expect_atom = atom_constant<atom("expect")>;
using ordering_atom = atom_constant<atom("ordering")>;
using send_atom = atom_constant<atom("send")>;

// -- aliases ------------------------------------------------------------------

using byte_buffer = std::vector<char>;
using header_writer = caf::callback<byte_buffer&>;

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

// -- message types ------------------------------------------------------------

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
    // TODO: there was something we wanted to do here ...
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

  virtual error timeout(atom_value, uint32_t) = 0;

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

  error timeout(atom_value atm, uint32_t id) override {
    return impl.timeout(atm, id);
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
struct newb : public extend<scheduled_actor, newb<Message>>::template
                     with<mixin::sender, mixin::requester,
                          mixin::behavior_changer>,
              public dynamically_typed_actor_base,
              public network::event_handler {
  using super = typename extend<scheduled_actor, newb<Message>>::
    template with<mixin::sender, mixin::requester, mixin::behavior_changer>;

  using signatures = none_t;

  // -- constructors and destructors -------------------------------------------

  newb(actor_config& cfg, io::network::default_multiplexer& dm, native_socket sockfd)
      : super(cfg),
        event_handler(dm, sockfd) {
    // nop
  }

  newb() = default;
  newb(newb<Message>&&) = default;

  ~newb() override {
    // nop
  }

  // -- overridden modifiers of abstract_actor ---------------------------------

  void enqueue(mailbox_element_ptr ptr, execution_unit*) override {
    CAF_PUSH_AID(this->id());
    scheduled_actor::enqueue(std::move(ptr), &backend());
  }

  void enqueue(strong_actor_ptr src, message_id mid, message msg,
               execution_unit*) override {
    enqueue(make_mailbox_element(std::move(src), mid, {}, std::move(msg)),
            &backend());
  }

  resumable::subtype_t subtype() const override {
    return resumable::io_actor;
  }

  // -- overridden modifiers of local_actor ------------------------------------

  void launch(execution_unit* eu, bool lazy, bool hide) override {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_ASSERT(eu != nullptr);
    CAF_ASSERT(eu == &backend());
    CAF_LOG_TRACE(CAF_ARG(lazy) << CAF_ARG(hide));
    // add implicit reference count held by middleman/multiplexer
    if (!hide)
      super::register_at_system();
    if (lazy && super::mailbox().try_block())
      return;
    intrusive_ptr_add_ref(super::ctrl());
    eu->exec_later(this);
  }

  void initialize() override {
    CAF_LOG_TRACE("");
    init_newb();
    auto bhvr = make_behavior();
    CAF_LOG_DEBUG_IF(!bhvr, "make_behavior() did not return a behavior:"
                             << CAF_ARG(this->has_behavior()));
    if (bhvr) {
      // make_behavior() did return a behavior instead of using become()
      CAF_LOG_DEBUG("make_behavior() did return a valid behavior");
      this->become(std::move(bhvr));
    }
  }

  bool cleanup(error&& reason, execution_unit* host) override {
    CAF_LOG_TRACE(CAF_ARG(reason));
    // TODO: Ask policies, close socket.
    return local_actor::cleanup(std::move(reason), host);
  }

  // -- overridden modifiers of resumable --------------------------------------

  network::multiplexer::runnable::resume_result resume(execution_unit* ctx,
                                                       size_t mt) override {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_ASSERT(ctx != nullptr);
    CAF_ASSERT(ctx == &backend());
    return scheduled_actor::resume(ctx, mt);
  }

  // -- overridden modifiers of event handler ----------------------------------

  void handle_event(network::operation op) override {
    CAF_PUSH_AID_FROM_PTR(this);
    switch (op) {
      case io::network::operation::read:
        read_event();
        break;
      case io::network::operation::write:
        write_event();
        break;
      case io::network::operation::propagate_error:
        handle_error();
    }
  }

  void removed_from_loop(network::operation op) override {
    CAF_PUSH_AID_FROM_PTR(this);
    std::cout << "removing myself from the loop for "
              << to_string(op) << std::endl;
  }

  // -- members ----------------------------------------------------------------

  write_handle wr_buf(header_writer* hw) {
    // TODO: We somehow need to tell the transport policy how much we've
    // written to enable it to split the buffer into datagrams.
    auto& buf = transport->wr_buf();
    auto header_offset = protocol->write_header(buf, hw);
    return {protocol.get(), &buf, header_offset};
  }

  // Send
  void flush() {
    // TODO: send message
  }

  error read_event() {
    return transport->read_some(*protocol);
  }

  void write_event() {
    CAF_MESSAGE("got write event to handle: not implemented");
    // transport->write_some();
  }

  void handle_error() {
    CAF_CRITICAL("got error to handle: not implemented");
  }

  // Protocol policies can set timeouts using a custom message.
  template<class Rep = int, class Period = std::ratio<1>>
  void set_timeout(std::chrono::duration<Rep, Period> timeout,
                   atom_value atm, uint32_t id) {
    CAF_MESSAGE("sending myself a timeout");
    this->delayed_send(this, timeout, atm, id);
    // TODO: Use actor clock.
    // TODO: Make this system messages and handle them separately.
  }

  // Allow protocol policies to enqueue a data for sending.
  // Probably required for reliability.
  // void direct_enqueue(char* bytes, size_t count);

  virtual void handle(Message& msg) {
    using tmp_t = mailbox_element_vals<Message>;
    tmp_t tmp{strong_actor_ptr{}, make_message_id(),
              mailbox_element::forwarding_stack{},
              msg};
    super::activate(&backend(), tmp);
  }

  /// Returns the `multiplexer` running this broker.
  network::multiplexer& backend() {
    return event_handler::backend();
  }

  // Currently has to handle timeouts as well, see handler below:
  virtual behavior make_behavior() = 0; /*{
    std::cout << "creating newb behavior" << std::endl;
    return {
      [=](atom_value atm, uint32_t id) {
        protocol->timeout(atm, id);
      }
    };
  }*/

  void init_newb() {
    CAF_LOG_TRACE("");
    super::setf(super::is_initialized_flag);
  }

  /// @cond PRIVATE

  template <class... Ts>
  void eq_impl(message_id mid, strong_actor_ptr sender,
               execution_unit* ctx, Ts&&... xs) {
    enqueue(make_mailbox_element(std::move(sender), mid,
                                 {}, std::forward<Ts>(xs)...),
            ctx);
  }

  /// @endcond

  // -- policies ---------------------------------------------------------------

  std::unique_ptr<transport_policy> transport;
  std::unique_ptr<protocol_policy<Message>> protocol;
};

template <class ProtocolPolicy>
struct newb_acceptor : public network::event_handler {
  std::unique_ptr<accept_policy> acceptor;

  void handle_event(network::operation op) {
    switch (op) {
      case network::operation::read:
        read_event();
        break;
      case network::operation::write:
        // nop
        break;
      case network::operation::propagate_error:
        CAF_MESSAGE("acceptor got error operation");
        break;
    }
  }

  void remove_from_loop(network::operation) {
    CAF_MESSAGE("remove from loop not implemented in newb acceptor");
    // TODO: implement
  }

  error read_event() {
    CAF_MESSAGE("read event on newb acceptor");
    native_socket sock;
    transport_policy_ptr transport;
    std::tie(sock, transport) = acceptor->accept();;
    auto n = create_newb(sock, std::move(transport));
    acceptor->init(n);
    return n;
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
    CAF_MESSAGE("ordering read, count = " << count);
    uint32_t seq;
    binary_deserializer bd(&parent->backend(), bytes, count);
    bd(seq);
    CAF_MESSAGE("seq = " << seq << ", seq_read = " << seq_read);
    // TODO: Use the comparison function from BASP instance.
    if (seq == seq_read) {
      seq_read += 1;
      auto res = next.read(bytes + header_size, count - header_size);
      if (res)
        return res;
      return deliver_pending();
    } else if (seq > seq_read) {
      pending[seq] = std::vector<char>(bytes + header_size, bytes + count);
      parent->set_timeout(std::chrono::seconds(2), ordering_atom::value, seq);
      return none;
    }
    // Is late, drop it. TODO: Should this return an error?
    return none;
  }

  error timeout(atom_value atm, uint32_t id) {
    if (atm == ordering_atom::value) {
      error err;
      if (pending.count(id) > 0) {
        CAF_MESSAGE("found pending message");
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


template <class Newb>
actor make_newb(actor_system& sys, native_socket sockfd) {
  auto& mpx = dynamic_cast<network::default_multiplexer&>(sys.middleman().backend());
  actor_config acfg{&mpx};
  auto res = sys.spawn_impl<Newb, hidden + lazy_init>(acfg, mpx, sockfd);
  return actor_cast<actor>(res);
}

template <class NewbAcceptor, class AcceptPolicy>
NewbAcceptor make_newb_acceptor() {
  NewbAcceptor na;
  na.acceptor.reset(new accept_policy_impl);
  return na;
}

struct dummy_basp_newb : newb<new_basp_message> {
  using expexted_t = std::tuple<ordering_header, basp_header, int>;
  std::vector<std::pair<atom_value, uint32_t>> timeout_messages;
  std::vector<std::pair<new_basp_message, std::vector<char>>> messages;
  std::deque<expexted_t> expected;

  dummy_basp_newb(caf::actor_config& cfg, io::network::default_multiplexer& dm,
                  native_socket sockfd)
      : newb<new_basp_message>(cfg, dm, sockfd) {
    // nop
  }

  void handle(new_basp_message& msg) override {
    CAF_MESSAGE("handling new basp message = " << to_string(msg));
    CAF_ASSERT(!expected.empty());
    auto& e = expected.front();
    CAF_CHECK_EQUAL(msg.header.from, get<1>(e).from);
    CAF_CHECK_EQUAL(msg.header.to, get<1>(e).to);
    int pl;
    binary_deserializer bd(&backend_, msg.payload, msg.payload_size);
    bd(pl);
    CAF_CHECK_EQUAL(pl, get<2>(e));
    std::vector<char> payload{msg.payload, msg.payload + msg.payload_size};
    messages.emplace_back(msg, payload);
    messages.back().first.payload = messages.back().second.data();
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
      // Append message to a buffer for checking the contents.
      [=](new_basp_message& msg) {
        CAF_MESSAGE("new basp message received = " << to_string(msg));
        CAF_ASSERT(!expected.empty());
        auto& e = expected.front();
        CAF_CHECK_EQUAL(msg.header.from, get<1>(e).from);
        CAF_CHECK_EQUAL(msg.header.to, get<1>(e).to);
        int pl;
        binary_deserializer bd(&backend_, msg.payload, msg.payload_size);
        bd(pl);
        CAF_CHECK_EQUAL(pl, get<2>(e));
        std::vector<char> payload{msg.payload, msg.payload + msg.payload_size};
        messages.emplace_back(msg, payload);
        messages.back().first.payload = messages.back().second.data();
      },
      [=](send_atom, ordering_header& ohdr, basp_header& bhdr, int payload) {
        CAF_MESSAGE("send: ohdr = " << to_string(ohdr) << " bhdr = "
                    << to_string(bhdr) << " payload = " << payload);
        binary_serializer bs(&backend_, transport->receive_buffer);
        bs(ohdr);
        bs(bhdr);
        bs(payload);
      },
      [=](expect_atom, ordering_header& ohdr, basp_header& bhdr, int payload) {
        expected.push_back(std::make_tuple(ohdr, bhdr, payload));
      }
    };
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

class config : public actor_system_config {
public:
  config() {
    set("scheduler.policy", atom("testing"));
    set("logger.inline-output", true);
    set("middleman.manual-multiplexing", true);
    set("middleman.attach-utility-actors", true);
    load<io::middleman>();
  }
};

struct dm_fixture {
  config cfg;
  actor_system sys;
  network::default_multiplexer& mpx;
  caf::scheduler::test_coordinator& sched;
  actor self;

  dm_fixture()
      : sys(cfg.parse(test::engine::argc(), test::engine::argv())),
        mpx(dynamic_cast<network::default_multiplexer&>(sys.middleman().backend())),
        sched(dynamic_cast<caf::scheduler::test_coordinator&>(sys.scheduler())) {
    self = make_newb<dummy_basp_newb>(sys, network::invalid_native_socket);
    auto& ref = deref<newb<new_basp_message>>(self);
    ref.transport.reset(new transport_policy);
    ref.protocol.reset(new protocol_policy_impl<ordering<basp_policy>>(&ref));
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

  // -- serialization ----------------------------------------------------------

  template <class T>
  void to_buffer(ordering_header& hdr, T& x) {
    binary_serializer bs(sys, x);
    bs(hdr);
  }

  template <class T>
  void to_buffer(basp_header& hdr, T& x) {
    binary_serializer bs(sys, x);
    bs(hdr);
  }

  template <class T, class U>
  void to_buffer(U value, T& x) {
    binary_serializer bs(sys, x);
    bs(value);
  }

  template <class T>
  void from_buffer(T& x, size_t offset, ordering_header& hdr) {
    binary_deserializer bd(sys, x.data() + offset, sizeof(ordering_header));
    bd(hdr);
  }

  template <class T>
  void from_buffer(T& x, size_t offset, basp_header& hdr) {
    binary_deserializer bd(sys, x.data() + offset, sizeof(basp_header));
    bd(hdr);
  }

  template <class T>
  void from_buffer(char* x, T& value) {
    binary_deserializer bd(sys, x, sizeof(T));
    bd(value);
  }
};

} // namespace <anonymous>

/*
CAF_TEST_FIXTURE_SCOPE(protocol_policy_tests, datagram_fixture)

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
*/

CAF_TEST_FIXTURE_SCOPE(test_newb_creation, dm_fixture)

CAF_TEST(ordering and basp read event) {
  exec_all();
  CAF_MESSAGE("create some values for our buffer");
  ordering_header ohdr{0};
  basp_header bhdr{13, 42};
  int payload = 1337;
  anon_send(self, expect_atom::value, ohdr, bhdr, payload);
  exec_all();
  CAF_MESSAGE("copy them into the buffer");
  auto& dummy = deref<dummy_basp_newb>(self);
  auto& buf = dummy.transport->receive_buffer;
  // Write data to buffer.
  to_buffer(ohdr, buf);
  to_buffer(bhdr, buf);
  to_buffer(payload, buf);
  CAF_MESSAGE("trigger a read event");
  auto err = dummy.read_event();
  CAF_REQUIRE(!err);
  CAF_MESSAGE("check the basp header and payload");
  CAF_REQUIRE(!dummy.messages.empty());
  auto& msg = dummy.messages.front().first;
  CAF_CHECK_EQUAL(msg.header.from, bhdr.from);
  CAF_CHECK_EQUAL(msg.header.to, bhdr.to);
  int return_payload = 0;
  from_buffer(msg.payload, return_payload);
  CAF_CHECK_EQUAL(return_payload, payload);
}

CAF_TEST(ordering and basp message passing) {
  exec_all();
  ordering_header ohdr{0};
  basp_header bhdr{13, 42};
  int payload = 1337;
  CAF_MESSAGE("setup read event");
  anon_send(self, expect_atom::value, ohdr, bhdr, payload);
  anon_send(self, send_atom::value, ohdr, bhdr, payload);
  exec_all();
  auto& dummy = deref<dummy_basp_newb>(self);
  dummy.handle_event(network::operation::read);
  CAF_MESSAGE("check the basp header and payload");
  auto& msg = dummy.messages.front().first;
  CAF_CHECK_EQUAL(msg.header.from, bhdr.from);
  CAF_CHECK_EQUAL(msg.header.to, bhdr.to);
  int return_payload = 0;
  from_buffer(msg.payload, return_payload);
  CAF_CHECK_EQUAL(return_payload, payload);
}


CAF_TEST(ordering and basp read event with timeout) {
  // Should be an unexpected sequence number and lead to an error. Since
  // we start with 0, the 1 below should be out of order.
  ordering_header ohdr{1};
  basp_header bhdr{13, 42};
  int payload = 1337;
  CAF_MESSAGE("setup read event");
  anon_send(self, expect_atom::value, ohdr, bhdr, payload);
  anon_send(self, send_atom::value, ohdr, bhdr, payload);
  exec_all();
  auto& dummy = deref<dummy_basp_newb>(self);
  CAF_MESSAGE("trigger read event");
  auto err = dummy.read_event();
  CAF_REQUIRE(!err);
  CAF_MESSAGE("trigger waiting timeouts");
  // Trigger timeout.
  sched.dispatch();
  // Handle received message.
  exec_all();
  //CAF_MESSAGE("check if we have a pending timeout now");
  //CAF_REQUIRE(!dummy.timeout_messages.empty());
  //auto& timeout_pair = dummy.timeout_messages.back();
  //CAF_CHECK_EQUAL(timeout_pair.first, ordering_atom::value);
  //CAF_CHECK_EQUAL(timeout_pair.second, ohdr.seq_nr);
  //CAF_REQUIRE(!dummy.messages.empty());
  //CAF_MESSAGE("check delivered message");
  //auto& msg = dummy.messages.front().first;
  //CAF_CHECK_EQUAL(msg.header.from, bhdr.from);
  //CAF_CHECK_EQUAL(msg.header.to, bhdr.to);
  //int return_payload = 0;
  //CAF_CHECK_EQUAL(return_payload, payload);
}


CAF_TEST_FIXTURE_SCOPE_END()
