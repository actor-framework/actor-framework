#include "caf/io/network/newb.hpp"

#include "caf/config.hpp"
#include "caf/logger.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/detail/call_cfun.hpp"

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

using io::network::native_socket;
using io::network::invalid_native_socket;
using io::network::default_multiplexer;
using io::network::last_socket_error_as_string;

namespace {

constexpr auto ipv4 = caf::io::network::protocol::ipv4;
//constexpr auto ipv6 = caf::io::network::protocol::ipv6;

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

// -- atoms --------------------------------------------------------------------

using ordering_atom = atom_constant<atom("ordering")>;
using send_atom = atom_constant<atom("send")>;
using quit_atom = atom_constant<atom("quit")>;

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
                       reinterpret_cast<io::network::setsockopt_ptr>(&off),
                       static_cast<io::network::socket_size_type>(sizeof(off))));
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
                         reinterpret_cast<io::network::setsockopt_ptr>(&on),
                         static_cast<io::network::socket_size_type>(sizeof(on))));
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
                 static_cast<io::network::socket_size_type>(sizeof(sa))));
  return sguard.release();
}

expected<native_socket> new_tcp_acceptor_impl(uint16_t port, const char* addr,
                                              bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(port) << ", addr = " << (addr ? addr : "nullptr"));
  auto addrs = io::network::interfaces::server_address(port, addr);
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

// -- tcp impls ----------------------------------------------------------------

struct tcp_basp_header {
  uint32_t payload_len;
  actor_id from;
  actor_id to;
};

constexpr size_t tcp_basp_header_len = sizeof(uint32_t) + sizeof(actor_id) * 2;

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun, tcp_basp_header& hdr) {
  return fun(meta::type_name("tcp_basp_header"),
             hdr.payload_len, hdr.from, hdr.to);
}

struct new_tcp_basp_message {
  tcp_basp_header header;
  char* payload;
  size_t payload_len;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun,
                                        new_tcp_basp_message& msg) {
  return fun(meta::type_name("new_tcp_basp_message"), msg.header,
             msg.payload_len);
}

struct tcp_basp {
  static constexpr size_t header_size = sizeof(tcp_basp_header);
  using message_type = new_tcp_basp_message;
  using result_type = optional<message_type>;
  io::network::newb<message_type>* parent;
  message_type msg;
  bool expecting_header = true;

  tcp_basp(io::network::newb<message_type>* parent) : parent(parent) {
    // nop
  }

  error read_header(char* bytes, size_t count) {
    if (count < tcp_basp_header_len) {
      CAF_LOG_DEBUG("buffer contains " << count << " bytes of expected "
                    << tcp_basp_header_len);
      return sec::unexpected_message;
    }
    binary_deserializer bd{&parent->backend(), bytes, count};
    bd(msg.header);
    CAF_LOG_DEBUG("read header " << CAF_ARG(msg.header));
    size_t size = static_cast<size_t>(msg.header.payload_len);
    parent->configure_read(io::receive_policy::exactly(size));
    expecting_header = false;
    return none;
  }

  error read_payload(char* bytes, size_t count) {
    if (count < msg.header.payload_len) {
      CAF_LOG_DEBUG("buffer contains " << count << " bytes of expected "
                    << msg.header.payload_len);
      return sec::unexpected_message;
    }
    msg.payload = bytes;
    msg.payload_len = msg.header.payload_len;
    parent->handle(msg);
    expecting_header = true;
    parent->configure_read(io::receive_policy::exactly(tcp_basp_header_len));
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

  size_t write_header(io::network::byte_buffer& buf,
                      io::network::header_writer* hw) {
    CAF_ASSERT(hw != nullptr);
    (*hw)(buf);
    return header_size;
  }

  void prepare_for_sending(io::network::byte_buffer& buf,
                           size_t hstart, size_t plen) {
    stream_serializer<charbuf> out{&parent->backend(), buf.data() + hstart,
                                   sizeof(uint32_t)};
    auto len = static_cast<uint32_t>(plen);
    out(len);
  }
};

struct tcp_transport_policy : public io::network::transport_policy {
  tcp_transport_policy()
      : read_threshold{0},
        collected{0},
        maximum{0},
        writing{false},
        written{0} {
    // nop
  }

  error read_some(io::network::event_handler* parent) override {
    CAF_LOG_TRACE("");
    size_t len = receive_buffer.size() - collected;
    receive_buffer.resize(len);
    void* buf = receive_buffer.data() + collected;
    auto sres = ::recv(parent->fd(),
                       reinterpret_cast<io::network::socket_recv_ptr>(buf),
                       len, io::network::no_sigpipe_io_flag);
    if (io::network::is_error(sres, true) || sres == 0) {
      // recv returns 0 when the peer has performed an orderly shutdown
      return sec::runtime_error;
    }
    size_t result = (sres > 0) ? static_cast<size_t>(sres) : 0;
    collected += result;
    received_bytes = collected;
    return none;
  }

  bool should_deliver() override {
    CAF_LOG_DEBUG(CAF_ARG(collected) << CAF_ARG(read_threshold));
    return collected >= read_threshold;
  }

  void prepare_next_read(io::network::event_handler*) override {
    collected = 0;
    received_bytes = 0;
    switch (rd_flag) {
      case io::receive_policy_flag::exactly:
        if (receive_buffer.size() != maximum)
          receive_buffer.resize(maximum);
        read_threshold = maximum;
        break;
      case io::receive_policy_flag::at_most:
        if (receive_buffer.size() != maximum)
          receive_buffer.resize(maximum);
        read_threshold = 1;
        break;
      case io::receive_policy_flag::at_least: {
        // read up to 10% more, but at least allow 100 bytes more
        auto maximumsize = maximum + std::max<size_t>(100, maximum / 10);
        if (receive_buffer.size() != maximumsize)
          receive_buffer.resize(maximumsize);
        read_threshold = maximum;
        break;
      }
    }
  }

  void configure_read(io::receive_policy::config config) override {
    rd_flag = config.first;
    maximum = config.second;
  }

  error write_some(io::network::event_handler* parent) override {
    CAF_LOG_TRACE("");
    const void* buf = send_buffer.data() + written;
    auto len = send_buffer.size() - written;
    auto sres = ::send(parent->fd(),
                       reinterpret_cast<io::network::socket_send_ptr>(buf),
                       len, io::network::no_sigpipe_io_flag);
    if (io::network::is_error(sres, true))
      return sec::runtime_error;
    size_t result = (sres > 0) ? static_cast<size_t>(sres) : 0;
    written += result;
    auto remaining = send_buffer.size() - written;
    if (remaining == 0)
      prepare_next_write(parent);
    return none;
  }

  void prepare_next_write(io::network::event_handler* parent) override {
    written = 0;
    send_buffer.clear();
    if (offline_buffer.empty()) {
      parent->backend().del(io::network::operation::write,
                            parent->fd(), parent);
      writing = false;
    } else {
      send_buffer.swap(offline_buffer);
    }
  }

  io::network::byte_buffer& wr_buf() {
    return offline_buffer;
  }

  void flush(io::network::event_handler* parent) override {
    CAF_ASSERT(parent != nullptr);
    CAF_LOG_TRACE(CAF_ARG(offline_buffer.size()));
    if (!offline_buffer.empty() && !writing) {
      parent->backend().add(io::network::operation::write,
                            parent->fd(), parent);
      writing = true;
      prepare_next_write(parent);
    }
  }

  // State for reading.
  size_t read_threshold;
  size_t collected;
  size_t maximum;
  io::receive_policy_flag rd_flag;

  // State for writing.
  bool writing;
  size_t written;
};

template <class T>
struct tcp_protocol_policy
    : public io::network::protocol_policy<typename T::message_type> {
  T impl;

  tcp_protocol_policy(io::network::newb<typename T::message_type>* parent)
      : impl(parent) {
    // nop
  }

  error read(char* bytes, size_t count) override {
    return impl.read(bytes, count);
  }

  error timeout(atom_value atm, uint32_t id) override {
    return impl.timeout(atm, id);
  }

  void write_header(io::network::byte_buffer& buf,
                    io::network::header_writer* hw) override {
    impl.write_header(buf, hw);
  }

  void prepare_for_sending(io::network::byte_buffer& buf, size_t hstart,
                           size_t plen) override {
    impl.prepare_for_sending(buf, hstart, plen);
  }
};

struct tcp_basp_newb : io::network::newb<new_tcp_basp_message> {
  tcp_basp_newb(caf::actor_config& cfg, default_multiplexer& dm,
                native_socket sockfd)
      : newb<new_tcp_basp_message>(cfg, dm, sockfd) {
    // nop
  }

  void handle(new_tcp_basp_message& msg) override {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_LOG_TRACE("");
    std::string res;
    binary_deserializer bd(&backend(), msg.payload, msg.payload_len);
    bd(res);
    send(responder, res);
  }

  behavior make_behavior() override {
    set_default_handler(print_and_drop);
    return {
      // Must be implemented at the moment, will be cought by the broker in a
      // later implementation.
      [=](atom_value atm, uint32_t id) {
        protocol->timeout(atm, id);
      },
      [=](send_atom, actor_id sender, actor_id receiver, std::string payload) {
        auto hw = caf::make_callback([&](io::network::byte_buffer& buf) -> error {
          binary_serializer bs(&backend(), buf);
          bs(tcp_basp_header{0, sender, receiver});
          return none;
        });
        auto whdl = wr_buf(&hw);
        CAF_ASSERT(whdl.buf != nullptr);
        CAF_ASSERT(whdl.protocol != nullptr);
        binary_serializer bs(&backend(), *whdl.buf);
        bs(payload);
      },
      [=](quit_atom) {
        // Remove from multiplexer loop.
        stop();
        // Quit actor.
        quit();
      }
    };
  }

  actor responder;
};


struct tcp_accept_policy
    : public io::network::accept_policy<new_tcp_basp_message> {
  virtual std::pair<native_socket, io::network::transport_policy_ptr>
    accept(io::network::event_handler* parent) {
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

  virtual void init(io::network::newb<new_tcp_basp_message>& n) {
    n.start();
  }
};

template <class ProtocolPolicy>
struct tcp_basp_acceptor
    : public io::network::newb_acceptor<typename ProtocolPolicy::message_type> {
  using super = io::network::newb_acceptor<typename ProtocolPolicy::message_type>;


  static expected<native_socket> create_socket(uint16_t port, const char* host,
                                               bool reuse = false) {
    return new_tcp_acceptor_impl(port, host, reuse);
  }

  tcp_basp_acceptor(default_multiplexer& dm, native_socket sockfd)
      : super(dm, sockfd) {
    // nop
  }

  expected<actor> create_newb(native_socket sockfd,
                              io::network::transport_policy_ptr pol) override {
    CAF_LOG_DEBUG("creating new basp tcp newb");
    auto n = io::network::make_newb<tcp_basp_newb>(this->backend().system(),
                                                   sockfd);
    auto ptr = caf::actor_cast<caf::abstract_actor*>(n);
    if (ptr == nullptr)
      return sec::runtime_error;
    auto& ref = dynamic_cast<tcp_basp_newb&>(*ptr);
    ref.transport = std::move(pol);
    ref.protocol.reset(new ProtocolPolicy(&ref));
    ref.responder = responder;
    // This should happen somewhere else?
    ref.configure_read(io::receive_policy::exactly(tcp_basp_header_len));
    anon_send(responder, n);
    return n;
  }

  actor responder;
};

struct tcp_test_broker_state {
  tcp_basp_header hdr;
  bool expecting_header = true;
};

void caf_main(actor_system& sys, const actor_system_config&) {
  using tcp_protocol_policy_t = tcp_protocol_policy<tcp_basp>;
  using tcp_accept_policy_t = tcp_accept_policy;
  using tcp_newb_acceptor_t = tcp_basp_acceptor<tcp_protocol_policy_t>;
  using tcp_transport_policy_t = tcp_transport_policy;

  const char* host = "localhost";
  const uint16_t port = 12345;

  scoped_actor main_actor{sys};
  actor newb_actor;
  auto testing = [&](io::stateful_broker<tcp_test_broker_state>* self,
                     io::connection_handle hdl, actor) -> behavior {
    CAF_ASSERT(hdl != io::invalid_connection_handle);
    self->configure_read(hdl, io::receive_policy::exactly(tcp_basp_header_len));
    self->state.expecting_header = true;
    return {
      [=](send_atom, std::string str) {
        CAF_LOG_DEBUG("sending '" << str << "'");
        io::network::byte_buffer buf;
        binary_serializer bs(self->system(), buf);
        tcp_basp_header hdr{0, 1, 2};
        bs(hdr);
        auto header_len = buf.size();
        CAF_ASSERT(header_len == tcp_basp_header_len);
        bs(str);
        hdr.payload_len = static_cast<uint32_t>(buf.size() - header_len);
        stream_serializer<charbuf> out{self->system(), buf.data(),
                                       sizeof(hdr.payload_len)};
        out(hdr.payload_len);
        CAF_LOG_DEBUG("header len: " << header_len
                    << ", packet_len: " << buf.size()
                    << ", header: " << to_string(hdr));
        self->write(hdl, buf.size(), buf.data());
        self->flush(hdl);
      },
      [=](quit_atom) {
        CAF_LOG_DEBUG("test broker shutting down");
        self->quit();
      },
      [=](io::new_data_msg& msg) {
        auto& s = self->state;
        size_t next_len = tcp_basp_header_len;
        binary_deserializer bd(self->system(), msg.buf);
        if (s.expecting_header) {
          bd(s.hdr);
          next_len = s.hdr.payload_len;
          s.expecting_header = false;
        } else {
          std::string str;
          bd(str);
          CAF_LOG_DEBUG("received '" << str << "'");
          std::reverse(std::begin(str), std::end(str));
          io::network::byte_buffer buf;
          binary_serializer bs(self->system(), buf);
          tcp_basp_header hdr{0, 1, 2};
          bs(hdr);
          auto header_len = buf.size();
          CAF_ASSERT(header_len == tcp_basp_header_len);
          bs(str);
          hdr.payload_len = static_cast<uint32_t>(buf.size() - header_len);
          stream_serializer<charbuf> out{self->system(), buf.data(),
                                         sizeof(hdr.payload_len)};
          out(hdr.payload_len);
          CAF_LOG_DEBUG("header len: " << header_len
                      << ", packet_len: " << buf.size()
                      << ", header: " << to_string(hdr));
          self->write(hdl, buf.size(), buf.data());
          self->flush(hdl);
//          self->send(m, quit_atom::value);
        }
        self->configure_read(msg.handle, io::receive_policy::exactly(next_len));
      }
    };
  };
  auto helper_actor = sys.spawn([&](event_based_actor* self, actor m) -> behavior {
    return {
      [=](const std::string& str) {
        CAF_LOG_DEBUG("received '" << str << "'");
        self->send(m, quit_atom::value);
      },
      [=](actor a) {
        CAF_LOG_DEBUG("got new newb handle");
        self->send(m, a);
      },
      [=](quit_atom) {
        CAF_LOG_DEBUG("helper shutting down");
        self->quit();
      }
    };
  }, main_actor);
  CAF_LOG_DEBUG("creating new acceptor");
  auto newb_acceptor_ptr
    = io::network::make_newb_acceptor<tcp_newb_acceptor_t, tcp_accept_policy_t>(sys, port);
  dynamic_cast<tcp_newb_acceptor_t*>(newb_acceptor_ptr.get())->responder
    = helper_actor;
  CAF_LOG_DEBUG("connecting from 'old-style' broker");
  auto exp = sys.middleman().spawn_client(testing, host, port, main_actor);
  CAF_ASSERT(exp);
  auto test_broker = std::move(*exp);
  main_actor->receive(
    [&](actor a) {
      newb_actor = a;
    }
  );
  CAF_LOG_DEBUG("sending message to newb");
  main_actor->send(test_broker, send_atom::value, "hello world");
  std::this_thread::sleep_for(std::chrono::seconds(1));
  main_actor->receive(
    [](quit_atom) {
      CAF_LOG_DEBUG("check");
    }
  );
  CAF_LOG_DEBUG("sending message from newb");
  main_actor->send(newb_actor, send_atom::value, actor_id{3}, actor_id{4},
                   "dlrow olleh");
  main_actor->receive(
    [](quit_atom) {
      CAF_LOG_DEBUG("check");
    }
  );
  CAF_LOG_DEBUG("shutting everything down");
  newb_acceptor_ptr->stop();
  anon_send(newb_actor, quit_atom::value);
  anon_send(helper_actor, quit_atom::value);
  anon_send(test_broker, quit_atom::value);
  sys.await_all_actors_done();
  CAF_LOG_DEBUG("done");
}

} // namespace anonymous

CAF_MAIN(io::middleman);
