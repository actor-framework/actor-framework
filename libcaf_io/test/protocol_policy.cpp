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

#include <cstdint>
#include <cstring>
#include <tuple>

#include "caf/test/dsl.hpp"

#include "caf/callback.hpp"
#include "caf/config.hpp"
#include "caf/detail/enum_to_string.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/network/default_multiplexer.hpp"
#include "caf/io/network/event_handler.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/mixin/behavior_changer.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/scheduled_actor.hpp"

using namespace caf;
using namespace caf::io;

using network::native_socket;
using network::default_multiplexer;

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
  virtual void init(network::event_handler&) = 0;
};

struct accept_policy_impl : accept_policy {
  std::pair<native_socket, transport_policy_ptr> accept() override {
    // TODO: For UDP read the message into a buffer. Create a new socket.
    //  Move the buffer into the transport policy as the new receive buffer.
    native_socket sock{13337};
    transport_policy_ptr ptr{new transport_policy};
    return {sock, std::move(ptr)};
  }

  void init(network::event_handler& eh) override {
    eh.handle_event(network::operation::read);
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
    return impl.write_header(buf, hw);
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

  newb(actor_config& cfg, default_multiplexer& dm, native_socket sockfd)
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
    CAF_REQUIRE(buf.empty());
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

  // Allow protocol policies to enqueue a data for sending. Probably required for
  // reliability to send ACKs. The problem is that only the headers of policies lower,
  // well closer to the transport, should write their headers. So we're facing a
  // similiar porblem to slicing here.
  // void (char* bytes, size_t count);

  /// Returns the `multiplexer` running this broker.
  network::multiplexer& backend() {
    return event_handler::backend();
  }

  virtual void handle(Message& msg) = 0;

  // Currently has to handle timeouts as well, see handler below.
  virtual behavior make_behavior() = 0; /*{
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

struct newb_acceptor : public network::event_handler {

  // -- constructors and destructors -------------------------------------------

  newb_acceptor(default_multiplexer& dm, native_socket sockfd)
      : event_handler(dm, sockfd) {
    // nop
  }

  // -- overridden modifiers of event handler ----------------------------------

  void handle_event(network::operation op) override {
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

  void removed_from_loop(network::operation) override {
    CAF_MESSAGE("remove from loop not implemented in newb acceptor");
    // TODO: implement
  }

  // -- members ----------------------------------------------------------------

  error read_event() {
    CAF_MESSAGE("read event on newb acceptor");
    native_socket sock;
    transport_policy_ptr transport;
    std::tie(sock, transport) = acceptor->accept();;
    auto en = create_newb(sock, std::move(transport));
    if (!en)
      return std::move(en.error());
    auto ptr = caf::actor_cast<caf::abstract_actor*>(*en);
    acceptor->init(dynamic_cast<event_handler&>(*ptr));
    return none;
  }

  virtual expected<actor> create_newb(native_socket sock, transport_policy_ptr pol) = 0;

  std::unique_ptr<accept_policy> acceptor;
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

  size_t write_header(byte_buffer& buf, header_writer* hw) {
    CAF_ASSERT(hw != nullptr);
    (*hw)(buf);
    return header_size;
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
      pending[hdr.seq_nr] = std::vector<char>(bytes + header_size, bytes + count);
      parent->set_timeout(std::chrono::seconds(2), ordering_atom::value, hdr.seq_nr);
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

  size_t write_header(byte_buffer& buf, header_writer* hw) {
    binary_serializer bs(&parent->backend(), buf);
    bs(ordering_header{seq_write});
    seq_write += 1;
    return header_size + next.write_header(buf, hw);
  }
};

// -- test classes -------------------------------------------------------------


template <class Newb>
actor make_newb(actor_system& sys, native_socket sockfd) {
  auto& mpx = dynamic_cast<default_multiplexer&>(sys.middleman().backend());
  actor_config acfg{&mpx};
  auto res = sys.spawn_impl<Newb, hidden + lazy_init>(acfg, mpx, sockfd);
  return actor_cast<actor>(res);
}

// TODO: I feel like this should include the ProtocolPolicy somehow.
template <class NewbAcceptor, class AcceptPolicy>
std::unique_ptr<newb_acceptor> make_newb_acceptor(actor_system& sys,
                                                  native_socket sockfd) {
  auto& mpx = dynamic_cast<default_multiplexer&>(sys.middleman().backend());
  std::unique_ptr<newb_acceptor> ptr{new NewbAcceptor(mpx, sockfd)};
  //std::unique_ptr<network::event_handler> ptr{new NewbAcceptor(mpx, sockfd)};
  //static_cast<NewbAcceptor*>(ptr)->acceptor.reset(new AcceptPolicy);
  ptr->acceptor.reset(new AcceptPolicy);
  return ptr;
}

struct dummy_basp_newb : newb<new_basp_message> {
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
        CAF_MESSAGE("get a write buffer");
        auto whdl = wr_buf(&hw);
        CAF_CHECK(whdl.buf != nullptr);
        CAF_CHECK(whdl.protocol != nullptr);
        CAF_MESSAGE("write the payload");
        binary_serializer bs(&backend(), *whdl.buf);
        bs(payload);
        std::swap(transport->receive_buffer, transport->send_buffer);
        transport->send_buffer.clear();
      },
      [=](send_atom, ordering_header& ohdr, basp_header& bhdr, int payload) {
        CAF_MESSAGE("send: ohdr = " << to_string(ohdr) << " bhdr = "
                    << to_string(bhdr) << " payload = " << payload);
        binary_serializer bs(&backend_, transport->receive_buffer);
        bs(ohdr);
        bs(bhdr);
        bs(payload);
      },
      [=](expect_atom, basp_header& bhdr, int payload) {
        expected.push_back(std::make_pair(bhdr, payload));
      }
    };
  }
};

template <class ProtocolPolicy>
struct dummy_basp_newb_acceptor : newb_acceptor {
  using message_tuple_t = std::tuple<ordering_header, basp_header, int>;

  dummy_basp_newb_acceptor(default_multiplexer& dm, native_socket sockfd)
      : newb_acceptor(dm, sockfd) {
    // nop
  }

  expected<actor> create_newb(native_socket sockfd, transport_policy_ptr pol) override {
    spawned.emplace_back(make_newb<dummy_basp_newb>(this->backend().system(), sockfd));
    auto ptr = caf::actor_cast<caf::abstract_actor*>(spawned.back());
    if (ptr == nullptr)
      return sec::runtime_error;
    auto& ref = dynamic_cast<dummy_basp_newb&>(*ptr);
    ref.transport.reset(pol.release());
    ref.protocol.reset(new ProtocolPolicy(&ref));
    // TODO: Call read_some using the buffer of the ref as a destination.
    binary_serializer bs(&backend(), ref.transport->receive_buffer);
    bs(get<0>(msg));
    bs(get<1>(msg));
    bs(get<2>(msg));
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
    load<io::middleman>();
  }
};

struct dm_fixture {
  using policy_t = protocol_policy_impl<ordering<basp_policy>>;
  using acceptor_t = dummy_basp_newb_acceptor<policy_t>;
  config cfg;
  actor_system sys;
  default_multiplexer& mpx;
  scheduler::test_coordinator& sched;
  actor self;
  std::unique_ptr<newb_acceptor> na;

  dm_fixture()
      : sys(cfg.parse(test::engine::argc(), test::engine::argv())),
        mpx(dynamic_cast<default_multiplexer&>(sys.middleman().backend())),
        sched(dynamic_cast<caf::scheduler::test_coordinator&>(sys.scheduler())) {
    self = make_newb<dummy_basp_newb>(sys, network::invalid_native_socket);
    auto& ref = deref<newb<new_basp_message>>(self);
    ref.transport.reset(new transport_policy);
    ref.protocol.reset(new protocol_policy_impl<ordering<basp_policy>>(&ref));
    na = make_newb_acceptor<acceptor_t, accept_policy_impl>(sys,
                                                network::invalid_native_socket);
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

CAF_TEST_FIXTURE_SCOPE(test_newb_creation, dm_fixture)

CAF_TEST(ordering and basp read event) {
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
  anon_send(self, expect_atom::value, bhdr, payload);
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
  anon_send(self, expect_atom::value, bhdr, payload);
  anon_send(self, send_atom::value, ohdr, bhdr, payload);
  exec_all();
  CAF_MESSAGE("trigger read event");
  auto err = deref<dummy_basp_newb>(self).read_event();
  CAF_REQUIRE(!err);
  CAF_MESSAGE("trigger waiting timeouts");
  // Trigger timeout.
  sched.dispatch();
  // Handle received message.
  exec_all();
  // Message handler will check if the expected message was received.
}

CAF_TEST(ordering and basp multiple messages) {
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
  auto& buf = dummy.transport->receive_buffer;
  CAF_MESSAGE("read second message first");
  to_buffer(ohdr_second, buf);
  to_buffer(bhdr_second, buf);
  to_buffer(payload_second, buf);
  dummy.read_event();
  CAF_MESSAGE("followed by first message");
  buf.clear();
  to_buffer(ohdr_first, buf);
  to_buffer(bhdr_first, buf);
  to_buffer(payload_first, buf);
  dummy.read_event();
}

CAF_TEST(ordering and basp write buf) {
  exec_all();
  basp_header bhdr{13, 42};
  int payload = 1337;
  CAF_MESSAGE("setup read event");
  anon_send(self, expect_atom::value, bhdr, payload);
  anon_send(self, send_atom::value, bhdr.from, bhdr.to, payload);
  exec_all();
  deref<dummy_basp_newb>(self).handle_event(network::operation::read);
  // Message handler will check if the expected message was received.
}

CAF_TEST(ordering and basp acceptor) {
  CAF_MESSAGE("trigger read event on acceptor");
  // This will write a message into the receive buffer and trigger
  // a read event on the newly created newb.
  na->handle_event(network::operation::read);
  auto& dummy = dynamic_cast<acceptor_t&>(*na.get());
  CAF_CHECK(!dummy.spawned.empty());
}

CAF_TEST_FIXTURE_SCOPE_END()
