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

#include "caf/callback.hpp"
#include "caf/config.hpp"
#include "caf/detail/call_cfun.hpp"
#include "caf/detail/enum_to_string.hpp"
#include "caf/detail/socket_guard.hpp"
#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/network/default_multiplexer.hpp"
#include "caf/io/network/event_handler.hpp"
#include "caf/io/network/interfaces.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/mixin/behavior_changer.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/scheduled_actor.hpp"

#ifdef CAF_WINDOWS
# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
# endif // WIN32_LEAN_AND_MEAN
# ifndef NOMINMAX
#   define NOMINMAX
# endif
# ifdef CAF_MINGW
#   undef _WIN32_WINNT
#   undef WINVER
#   define _WIN32_WINNT WindowsVista
#   define WINVER WindowsVista
#   include <w32api.h>
# endif
# include <io.h>
# include <windows.h>
# include <winsock2.h>
# include <ws2ipdef.h>
# include <ws2tcpip.h>
#else
# include <unistd.h>
# include <arpa/inet.h>
# include <cerrno>
# include <fcntl.h>
# include <netdb.h>
# include <netinet/in.h>
# include <netinet/ip.h>
# include <netinet/tcp.h>
# include <sys/socket.h>
# include <sys/types.h>
# ifdef CAF_POLL_MULTIPLEXER
#   include <poll.h>
# elif defined(CAF_EPOLL_MULTIPLEXER)
#   include <sys/epoll.h>
# else
#   error "neither CAF_POLL_MULTIPLEXER nor CAF_EPOLL_MULTIPLEXER defined"
# endif
#endif

using namespace caf;
using namespace caf::io;

using network::native_socket;
using network::invalid_native_socket;
using network::default_multiplexer;
using network::last_socket_error_as_string;

namespace {

// -- forward declarations -----------------------------------------------------

struct dummy_device;
struct newb_base;
struct protocol_policy_base;

template <class T>
struct protocol_policy;

template <class T>
struct newb;

constexpr auto ipv4 = caf::io::network::protocol::ipv4;
constexpr auto ipv6 = caf::io::network::protocol::ipv6;

auto addr_of(sockaddr_in& what) -> decltype(what.sin_addr)& {
  return what.sin_addr;
}

auto family_of(sockaddr_in& what) -> decltype(what.sin_family)& {
  return what.sin_family;
}

auto port_of(sockaddr_in& what) -> decltype(what.sin_port)& {
  return what.sin_port;
}

auto addr_of(sockaddr_in6& what) -> decltype(what.sin6_addr)& {
  return what.sin6_addr;
}

auto family_of(sockaddr_in6& what) -> decltype(what.sin6_family)& {
  return what.sin6_family;
}

auto port_of(sockaddr_in6& what) -> decltype(what.sin6_port)& {
  return what.sin6_port;
}

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
using shutdown_atom = atom_constant<atom("shutdown")>;

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

  virtual error write_some(network::event_handler*) {
    return none;
  }

  virtual error read_some(network::event_handler*) {
    return none;
  }

  virtual bool should_deliver() {
    return true;
  }

  virtual void prepare_next_read(network::event_handler*) {
    // nop
  }

  virtual void prepare_next_write(network::event_handler*) {
    // nop
  }

  virtual void configure_read(receive_policy::config) {
    // nop
  }

  byte_buffer& wr_buf() {
    return offline_buffer;
  }

  template <class T>
  error read_some(network::event_handler* parent, protocol_policy<T>& policy) {
    auto res = read_some(parent);
    if (!res && should_deliver()) {
      res = policy.read(receive_buffer.data(), receive_buffer.size());
      prepare_next_read(parent);
    }
    return res;
  }

  byte_buffer offline_buffer;
  byte_buffer receive_buffer;
  byte_buffer send_buffer;
};

using transport_policy_ptr = std::unique_ptr<transport_policy>;

// -- accept policy ------------------------------------------------------------

struct accept_policy {
  virtual ~accept_policy() {
    // nop
  }

  virtual std::pair<native_socket, transport_policy_ptr>
  accept(network::event_handler*) = 0;

  virtual void init(network::event_handler&) = 0;
};

// -- protocol policies --------------------------------------------------------

struct protocol_policy_base {
  virtual ~protocol_policy_base() {
    // nop
  }

  /// TODO: Come up with something better than a write here?
  /// Write header into buffer. Use push back to append only.
  virtual void write_header(byte_buffer&, header_writer*) = 0;

  //virtual size_t offset() const noexcept = 0;

  virtual void prepare_for_sending(byte_buffer&, size_t, size_t) = 0;
};

template <class T>
struct protocol_policy : protocol_policy_base {
  using message_type = T;
  virtual ~protocol_policy() override {
    // nop
  }

  virtual error read(char* bytes, size_t count) = 0;

  virtual error timeout(atom_value, uint32_t) = 0;
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

  /*
  size_t offset() const noexcept override {
    return T::offset;
  }
  */

  error timeout(atom_value atm, uint32_t id) override {
    return impl.timeout(atm, id);
  }

  void write_header(byte_buffer& buf, header_writer* hw) override {
    return impl.write_header(buf, hw);
  }

  void prepare_for_sending(byte_buffer&, size_t, size_t) override {
    return;
  }
};

// -- new broker classes -------------------------------------------------------

/// @relates newb
/// Returned by funtion wr_buf of newb.
struct write_handle {
  protocol_policy_base* protocol;
  byte_buffer* buf;
  size_t header_start;
  size_t header_len;

  ~write_handle() {
    // Can we calculate added bytes for datagram things?
    auto payload_size = buf->size() - (header_start + header_len);
    protocol->prepare_for_sending(*buf, header_start, payload_size);
  }
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
    auto hstart = buf.size();
    protocol->write_header(buf, hw);
    auto hlen = buf.size() - hstart;
    return {protocol.get(), &buf, hstart, hlen};
  }

  // Send
  void flush() {
    // TODO: send message
  }

  error read_event() {
    return transport->read_some(this, *protocol);
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
    close_read_channel();
    // TODO: implement
  }

  // -- members ----------------------------------------------------------------

  error read_event() {
    CAF_MESSAGE("read event on newb acceptor");
    native_socket sock;
    transport_policy_ptr transport;
    std::tie(sock, transport) = acceptor->accept(this);;
    auto en = create_newb(sock, std::move(transport));
    if (!en)
      return std::move(en.error());
    auto ptr = caf::actor_cast<caf::abstract_actor*>(*en);
    acceptor->init(dynamic_cast<event_handler&>(*ptr));
    return none;
  }

  void activate() {
    event_handler::activate();
  }

  virtual expected<actor> create_newb(native_socket sock, transport_policy_ptr pol) = 0;

  std::unique_ptr<accept_policy> acceptor;
};

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

  void write_header(byte_buffer& buf, header_writer* hw) {
    CAF_ASSERT(hw != nullptr);
    (*hw)(buf);
    return;
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

  void write_header(byte_buffer& buf, header_writer* hw) {
    binary_serializer bs(&parent->backend(), buf);
    bs(ordering_header{seq_write});
    seq_write += 1;
    next.write_header(buf, hw);
    return;
  }
};

// -- network code -------------------------------------------------------------

expected<void> set_inaddr_any(native_socket, sockaddr_in& sa) {
  sa.sin_addr.s_addr = INADDR_ANY;
  return unit;
}

expected<void> set_inaddr_any(native_socket fd, sockaddr_in6& sa) {
  sa.sin6_addr = in6addr_any;
  // also accept ipv4 requests on this socket
  int off = 0;
  CALL_CFUN(res, detail::cc_zero, "setsockopt",
            setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
                       reinterpret_cast<network::setsockopt_ptr>(&off),
                       static_cast<network::socket_size_type>(sizeof(off))));
  return unit;
}

template <int Family, int SockType = SOCK_STREAM>
expected<native_socket> new_ip_acceptor_impl(uint16_t port, const char* addr,
                                             bool reuse_addr, bool any) {
  static_assert(Family == AF_INET || Family == AF_INET6, "invalid family");
  CAF_LOG_TRACE(CAF_ARG(port) << ", addr = " << (addr ? addr : "nullptr"));
  CALL_CFUN(fd, detail::cc_valid_socket, "socket", socket(Family, SockType, 0));
  // sguard closes the socket in case of exception
  detail::socket_guard sguard{fd};
  if (reuse_addr) {
    int on = 1;
    CALL_CFUN(tmp1, detail::cc_zero, "setsockopt",
              setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                         reinterpret_cast<network::setsockopt_ptr>(&on),
                         static_cast<network::socket_size_type>(sizeof(on))));
  }
  using sockaddr_type =
    typename std::conditional<
      Family == AF_INET,
      sockaddr_in,
      sockaddr_in6
    >::type;
  sockaddr_type sa;
  memset(&sa, 0, sizeof(sockaddr_type));
  family_of(sa) = Family;
  if (any)
    set_inaddr_any(fd, sa);
  CALL_CFUN(tmp, detail::cc_one, "inet_pton",
            inet_pton(Family, addr, &addr_of(sa)));
  port_of(sa) = htons(port);
  CALL_CFUN(res, detail::cc_zero, "bind",
            bind(fd, reinterpret_cast<sockaddr*>(&sa),
                 static_cast<network::socket_size_type>(sizeof(sa))));
  return sguard.release();
}

expected<native_socket> new_tcp_acceptor_impl(uint16_t port, const char* addr,
                                              bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(port) << ", addr = " << (addr ? addr : "nullptr"));
  auto addrs = network::interfaces::server_address(port, addr);
  auto addr_str = std::string{addr == nullptr ? "" : addr};
  if (addrs.empty())
    return make_error(sec::cannot_open_port, "No local interface available",
                      addr_str);
  bool any = addr_str.empty() || addr_str == "::" || addr_str == "0.0.0.0";
  auto fd = invalid_native_socket;
  for (auto& elem : addrs) {
    auto hostname = elem.first.c_str();
    auto p = elem.second == ipv4
           ? new_ip_acceptor_impl<AF_INET>(port, hostname, reuse_addr, any)
           : new_ip_acceptor_impl<AF_INET6>(port, hostname, reuse_addr, any);
    if (!p) {
      CAF_LOG_DEBUG(p.error());
      continue;
    }
    fd = *p;
    break;
  }
  if (fd == invalid_native_socket) {
    CAF_LOG_WARNING("could not open tcp socket on:" << CAF_ARG(port)
                    << CAF_ARG(addr_str));
    return make_error(sec::cannot_open_port, "tcp socket creation failed",
                      port, addr_str);
  }
  detail::socket_guard sguard{fd};
  CALL_CFUN(tmp2, detail::cc_zero, "listen", listen(fd, SOMAXCONN));
  // ok, no errors so far
  CAF_LOG_DEBUG(CAF_ARG(fd));
  return sguard.release();
}

// -- create newbs -------------------------------------------------------------

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
                                                  uint16_t port,
                                                  const char* addr = nullptr,
                                                  bool reuse_addr = false) {
  auto sockfd = new_tcp_acceptor_impl(port, addr, reuse_addr);
  if (!sockfd) {
    CAF_LOG_DEBUG("Could not open " << CAF_ARG(port) << CAF_ARG(addr));
    return nullptr;
  }
  auto& mpx = dynamic_cast<default_multiplexer&>(sys.middleman().backend());
  std::unique_ptr<newb_acceptor> ptr{new NewbAcceptor(mpx, *sockfd)};
  ptr->acceptor.reset(new AcceptPolicy);
  ptr->activate();
  return ptr;
}

// -- tcp impls ----------------------------------------------------------------

struct tcp_basp_header {
  uint32_t payload_len;
  actor_id from;
  actor_id to;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun, tcp_basp_header& hdr) {
  return fun(meta::type_name("tcp_basp_header"),
             hdr.payload_len, hdr.from, hdr.to);
}

struct new_tcp_basp_message {
  tcp_basp_header header;
  char* payload;
  size_t payload_size;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun, new_tcp_basp_message& msg) {
  return fun(meta::type_name("new_tcp_basp_message"), msg.header, msg.payload_size);
}

struct tcp_basp {
  static constexpr size_t header_size = sizeof(basp_header);
  using message_type = new_tcp_basp_message;
  using result_type = optional<message_type>;
  newb<message_type>* parent;
  message_type msg;
  bool expecting_header = true;

  tcp_basp(newb<message_type>* parent) : parent(parent) {
    // nop
  }

  error read_header(char* bytes, size_t count) {
    if (count < header_size) {
      CAF_MESSAGE("data left in packet to small to contain the basp header");
      return sec::unexpected_message;
    }
    binary_deserializer bd(&parent->backend(), bytes, count);
    bd(msg.header);
    expecting_header = false;
    return none;
  }

  error read_payload(char* bytes, size_t count) {
    if (count < msg.header.payload_len) {
      return sec::unexpected_message;
    }
    msg.payload = bytes;
    msg.payload_size = msg.header.payload_len;
    parent->handle(msg);
    expecting_header = true;
    return none;
  }

  error read(char* bytes, size_t count) {
    if (expecting_header)
      return read_header(bytes, count);
    else
      return read_payload(bytes, count);
  }

  error timeout(atom_value, uint32_t) {
    return none;
  }

  size_t write_header(byte_buffer& buf, header_writer* hw) {
    CAF_ASSERT(hw != nullptr);
    (*hw)(buf);
    return header_size;
  }

  void prepare_for_sending(byte_buffer& buf, size_t hstart, size_t plen) {
    stream_serializer<charbuf> out{&parent->backend(), buf.data() + hstart,
                                   sizeof(tcp_basp_header::payload_len)};
    auto len = static_cast<uint32_t>(plen);
    out(len);
  }
};

struct tcp_transport_policy : public transport_policy {
  error write_some(network::event_handler* parent) override {
    const void* buf = send_buffer.data();
    auto len = send_buffer.size();
    auto sres = ::send(parent->fd(), reinterpret_cast<network::socket_send_ptr>(buf),
                       len, network::no_sigpipe_io_flag);
    if (network::is_error(sres, true))
      return sec::runtime_error;
    //result = (sres > 0) ? static_cast<size_t>(sres) : 0;
    return none;
  }

  byte_buffer& wr_buf() {
    return send_buffer;
  }

  error read_some(network::event_handler* parent) override {
    size_t len = 1024;
    receive_buffer.resize(len);
    void* buf = receive_buffer.data();
    auto sres = ::recv(parent->fd(), reinterpret_cast<network::socket_recv_ptr>(buf),
                       len, network::no_sigpipe_io_flag);
    if (network::is_error(sres, true) || sres == 0) {
      // recv returns 0  when the peer has performed an orderly shutdown
      return sec::runtime_error;
    }
    // auto result = (sres > 0) ? static_cast<size_t>(sres) : 0;
    return none;
  }

  bool should_deliver() override {
    return collected >= read_threshold;
  }

  void prepare_next_read(network::event_handler*) override {
    collected = 0;
    switch (rd_flag) {
      case receive_policy_flag::exactly:
        if (receive_buffer.size() != maximum)
          receive_buffer.resize(maximum);
        read_threshold = maximum;
        break;
      case receive_policy_flag::at_most:
        if (receive_buffer.size() != maximum)
          receive_buffer.resize(maximum);
        read_threshold = 1;
        break;
      case receive_policy_flag::at_least: {
        // read up to 10% more, but at least allow 100 bytes more
        auto maximumsize = maximum + std::max<size_t>(100, maximum / 10);
        if (receive_buffer.size() != maximumsize)
          receive_buffer.resize(maximumsize);
        read_threshold = maximum;
        break;
      }
    }
  }

  void prepare_next_write(network::event_handler* parent) override {
    written = 0;
    send_buffer.clear();
    if (offline_buffer.empty()) {
      writing = false;
      parent->backend().del(network::operation::write, parent->fd(), parent);
    } else {
      send_buffer.swap(offline_buffer);
    }
  }

  void configure_read(receive_policy::config config) override {
    rd_flag = config.first;
    maximum = config.second;
  }

  // State for reading.
  size_t read_threshold;
  size_t collected;
  size_t maximum;
  receive_policy_flag rd_flag;

  // State for writing.
  bool writing;
  size_t written;
};

template <class T>
struct tcp_protocol_policy : protocol_policy<typename T::message_type> {
  T impl;

  tcp_protocol_policy(newb<typename T::message_type>* parent) : impl(parent) {
    // nop
  }

  error read(char* bytes, size_t count) override {
    return impl.read(bytes, count);
  }

  /*
  size_t offset() const noexcept override {
    return T::offset;
  }
  */

  error timeout(atom_value atm, uint32_t id) override {
    return impl.timeout(atm, id);
  }

  void write_header(byte_buffer& buf, header_writer* hw) override {
    impl.write_header(buf, hw);
  }

  void prepare_for_sending(byte_buffer& buf, size_t hstart,
                           size_t plen) override {
    impl.prepare_for_sending(buf, hstart, plen);
  }
};

struct tcp_basp_newb : newb<new_tcp_basp_message> {
  tcp_basp_newb(caf::actor_config& cfg, default_multiplexer& dm,
                  native_socket sockfd)
      : newb<new_tcp_basp_message>(cfg, dm, sockfd) {
    // nop
  }

  void handle(new_tcp_basp_message&) override {
    // TODO:
    //  - parse the payload of the message
    //  - send it somewhere
    //  - drop bytes from the buffer?
  }

  behavior make_behavior() override {
    set_default_handler(print_and_drop);
    return {
      // Must be implemented at the moment, will be cought by the broker in a
      // later implementation.
      [=](atom_value atm, uint32_t id) {
        protocol->timeout(atm, id);
      },
      [=](send_atom, actor_id sender, actor_id receiver, int payload) {
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
      }
    };
  }

  actor responder;
};


struct tcp_accept_policy : public accept_policy {
  virtual std::pair<native_socket, transport_policy_ptr>
    accept(network::event_handler* parent) {
    using namespace io::network;
    sockaddr_storage addr;
    std::memset(&addr, 0, sizeof(addr));
    socket_size_type addrlen = sizeof(addr);
    auto result = ::accept(parent->fd(), reinterpret_cast<sockaddr*>(&addr),
                           &addrlen);
    if (result == invalid_native_socket) {
      auto err = last_socket_error();
      if (!would_block_or_temporarily_unavailable(err)) {
        return {invalid_native_socket, nullptr};
      }
    }
    transport_policy_ptr ptr{new tcp_transport_policy};
    return {result, std::move(ptr)};
  }

  virtual void init(network::event_handler&) {

  }
};

template <class ProtocolPolicy>
struct tcp_basp_acceptor : newb_acceptor {
  tcp_basp_acceptor(default_multiplexer& dm, native_socket sockfd)
      : newb_acceptor(dm, sockfd) {
    // nop
  }

  expected<actor> create_newb(native_socket sockfd, transport_policy_ptr pol) override {
    CAF_MESSAGE("creating new basp tcp newb");
    auto n = make_newb<tcp_basp_newb>(this->backend().system(), sockfd);
    auto ptr = caf::actor_cast<caf::abstract_actor*>(n);
    if (ptr == nullptr)
      return sec::runtime_error;
    auto& ref = dynamic_cast<tcp_basp_newb&>(*ptr);
    ref.transport = std::move(pol);
    ref.protocol.reset(new ProtocolPolicy(&ref));
    ref.responder = responder;
    anon_send(responder, n);
    return n;
  }

  actor responder;
};

// -- test classes -------------------------------------------------------------

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
        std::swap(transport->receive_buffer, transport->offline_buffer);
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

struct accept_policy_impl : accept_policy {
  std::pair<native_socket, transport_policy_ptr>
    accept(network::event_handler*) override {
    // TODO: For UDP read the message into a buffer. Create a new socket.
    //  Move the buffer into the transport policy as the new receive buffer.
    transport_policy_ptr ptr{new transport_policy};
    return {invalid_native_socket, std::move(ptr)};
  }

  void init(network::event_handler& eh) override {
    eh.handle_event(network::operation::read);
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

class io_config : public actor_system_config {
public:
  io_config() {
    load<io::middleman>();
  }
};

struct fixture {
  using protocol_policy_t = tcp_protocol_policy<tcp_basp>;
  using accept_policy_t = tcp_accept_policy;
  using newb_acceptor_t = tcp_basp_acceptor<protocol_policy_t>;
  using transport_policy_t = tcp_transport_policy;

  config cfg;
  actor_system sys;
  default_multiplexer& mpx;
  scheduler::test_coordinator& sched;

  const char* host = "localhost";
  const uint16_t port = 12345;

  // -- constructor ------------------------------------------------------------

  fixture()
      : sys(cfg.parse(test::engine::argc(), test::engine::argv())),
        mpx(dynamic_cast<default_multiplexer&>(sys.middleman().backend())),
        sched(dynamic_cast<caf::scheduler::test_coordinator&>(sys.scheduler())) {
    // nop
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
    // Create newb.
    self = make_newb<dummy_basp_newb>(sys, network::invalid_native_socket);
    auto& ref = deref<newb<new_basp_message>>(self);
    ref.transport.reset(new transport_policy);
    ref.protocol.reset(new protocol_policy_impl<ordering<basp_policy>>(&ref));
    // Create acceptor.
    auto& mpx = dynamic_cast<default_multiplexer&>(sys.middleman().backend());
    std::unique_ptr<newb_acceptor> ptr{new acceptor_t(mpx, invalid_native_socket)};
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

CAF_TEST_FIXTURE_SCOPE(tcp_newbs, fixture)

CAF_TEST(accept test) {
  actor newb_actor;
  auto tester = [](broker* self, connection_handle hdl) -> behavior {
    return {
      [=](send_atom) {
        byte_buffer buf;
        self->write(hdl, buf.size(), buf.data());
      },
      [=](shutdown_atom) {
        self->quit();
      }
    };
  };
  auto helper = sys.spawn([&](event_based_actor* self) -> behavior {
    self->set_default_handler(print_and_drop);
    return {
      [&](int i) {
        CAF_MESSAGE("Got int message " << i);
      },
      [&](actor a) {
        CAF_MESSAGE("Got new newb handle");
        newb_actor = a;
      }
    };
  });
  exec_all();
  CAF_MESSAGE("creating new acceptor");
  auto ptr = make_newb_acceptor<newb_acceptor_t, accept_policy_t>(sys, port);
  dynamic_cast<newb_acceptor_t*>(ptr.get())->responder = helper;
  exec_all();
  CAF_MESSAGE("connecting from 'old' broker");
  auto eb = sys.middleman().spawn_client(tester, host, port);
  CAF_CHECK(eb);
  auto e = std::move(*eb);
  exec_all();
  anon_send_exit(e, exit_reason::user_shutdown);
  anon_send_exit(helper, exit_reason::user_shutdown);
  ptr->passivate();
  exec_all();
  // Not a good solution but the newbs currently don't shut down cleanly.
  sys.await_actors_before_shutdown(false);
}

CAF_TEST_FIXTURE_SCOPE_END();
