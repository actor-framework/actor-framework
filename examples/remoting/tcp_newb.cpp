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

// -- atoms --------------------------------------------------------------------

using ordering_atom = atom_constant<atom("ordering")>;
using send_atom = atom_constant<atom("send")>;
using quit_atom = atom_constant<atom("quit")>;
using responder_atom = atom_constant<atom("responder")>;

// -- tcp impls ----------------------------------------------------------------

struct basp_header {
  uint32_t payload_len;
  actor_id from;
  actor_id to;
};

constexpr size_t basp_header_len = sizeof(uint32_t) + sizeof(actor_id) * 2;

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun, basp_header& hdr) {
  return fun(meta::type_name("tcp_basp_header"),
             hdr.payload_len, hdr.from, hdr.to);
}

struct new_basp_message {
  basp_header header;
  char* payload;
  size_t payload_len;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun,
                                        new_basp_message& msg) {
  return fun(meta::type_name("tcp_new_basp_message"), msg.header,
             msg.payload_len);
}

struct basp {
  static constexpr size_t header_size = basp_header_len;
  using message_type = new_basp_message;
  using result_type = optional<message_type>;
  io::network::newb<message_type>* parent;
  message_type msg;
  bool expecting_header = true;

  basp(io::network::newb<message_type>* parent) : parent(parent) {
    // TODO: this is dangerous ...
    // Maybe we need an init function that is called with `start()`?
    parent->configure_read(io::receive_policy::exactly(basp_header_len));
  }

  error read_header(char* bytes, size_t count) {
    if (count < basp_header_len)
      return sec::unexpected_message;
    binary_deserializer bd{&parent->backend(), bytes, count};
    bd(msg.header);
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
    parent->configure_read(io::receive_policy::exactly(basp_header_len));
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
                           size_t hstart, size_t offset, size_t plen) {
    stream_serializer<charbuf> out{&parent->backend(),
                                   buf.data() + hstart + offset,
                                   sizeof(uint32_t)};
    auto len = static_cast<uint32_t>(plen);
    out(len);
  }
};

struct tcp_transport : public io::network::transport_policy {
  tcp_transport()
      : read_threshold{0},
        collected{0},
        maximum{0},
        writing{false},
        written{0} {
    // nop
  }

  error read_some(io::network::event_handler* parent) override {
    CAF_LOG_TRACE("");
    std::cerr << "read some called" << std::endl;
    size_t len = receive_buffer.size() - collected;
    receive_buffer.resize(len);
    void* buf = receive_buffer.data() + collected;
    auto sres = ::recv(parent->fd(),
                       reinterpret_cast<io::network::socket_recv_ptr>(buf),
                       len, io::network::no_sigpipe_io_flag);
    if (io::network::is_error(sres, true) || sres == 0) {
      std::cerr << "read some error" << std::endl;
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

  expected<native_socket>
  connect(const std::string& host, uint16_t port,
          optional<io::network::protocol::network> preferred = none) override {
    auto res = io::network::new_tcp_connection(host, port, preferred);
    if (!res)
      std::cerr << "failed to create new TCP connection" << std::endl;
    return res;
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
struct tcp_protocol
    : public io::network::protocol_policy<typename T::message_type> {
  T impl;

  tcp_protocol(io::network::newb<typename T::message_type>* parent)
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
                           size_t offset, size_t plen) override {
    impl.prepare_for_sending(buf, hstart, offset, plen);
  }
};

struct basp_newb : public io::network::newb<new_basp_message> {
  basp_newb(caf::actor_config& cfg, default_multiplexer& dm,
            native_socket sockfd)
      : newb<new_basp_message>(cfg, dm, sockfd) {
    // nop
    CAF_LOG_TRACE("");
    std::cerr << "constructing newb" << std::endl;
  }

  ~basp_newb() {
    std::cerr << "terminating newb" << std::endl;
    CAF_LOG_TRACE("");
  }

  void handle(new_basp_message& msg) override {
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
          bs(basp_header{0, sender, receiver});
          return none;
        });
        auto whdl = wr_buf(&hw);
        CAF_ASSERT(whdl.buf != nullptr);
        CAF_ASSERT(whdl.protocol != nullptr);
        binary_serializer bs(&backend(), *whdl.buf);
        bs(payload);
      },
      [=](responder_atom, actor r) {
        aout(this) << "got responder assigned" << std::endl;
        responder = r;
        send(r, this);
      },
      [=](quit_atom) {
        aout(this) << "got quit message" << std::endl;
        // Remove from multiplexer loop.
        stop();
        // Quit actor.
        quit();
      }
    };
  }

  actor responder;
};


struct accept_tcp : public io::network::accept_policy<new_basp_message> {
  expected<native_socket> create_socket(uint16_t port, const char* host,
                                        bool reuse = false) override {
    return io::network::new_tcp_acceptor_impl(port, host, reuse);
  }

  std::pair<native_socket, io::network::transport_policy_ptr>
    accept(io::network::event_handler* parent) override {
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
    std::cerr << "accepted connection" << std::endl;
    transport_policy_ptr ptr{new tcp_transport};
    return {result, std::move(ptr)};
  }

  void init(io::network::newb<new_basp_message>& n) override {
    n.start();
  }
};

template <class ProtocolPolicy>
struct tcp_acceptor
    : public io::network::newb_acceptor<typename ProtocolPolicy::message_type> {
  using super = io::network::newb_acceptor<typename ProtocolPolicy::message_type>;

  tcp_acceptor(default_multiplexer& dm, native_socket sockfd)
      : super(dm, sockfd) {
    CAF_LOG_TRACE("");
    std::cerr << "constructing newb acceptor" << std::endl;
    // nop
  }

  ~tcp_acceptor() {
    CAF_LOG_TRACE("");
    std::cerr << "terminating newb acceptor" << std::endl;
  }

  expected<actor> create_newb(native_socket sockfd,
                              io::network::transport_policy_ptr pol) override {
    CAF_LOG_TRACE(CAF_ARG(sockfd));
    std::cerr << "acceptor creating new newb" << std::endl;
    auto n = io::network::make_newb<basp_newb>(this->backend().system(), sockfd);
    auto ptr = caf::actor_cast<caf::abstract_actor*>(n);
    if (ptr == nullptr)
      return sec::runtime_error;
    auto& ref = dynamic_cast<basp_newb&>(*ptr);
    // TODO: Transport has to be assigned before protocol ... which sucks.
    //  (basp protocol calls configure read which accesses the transport.)
    ref.transport = std::move(pol);
    ref.protocol.reset(new ProtocolPolicy(&ref));
    ref.responder = responder;
    // TODO: Just a workaround.
    anon_send(responder, n);
    return n;
  }

  actor responder;
};

struct tcp_test_broker_state {
  basp_header hdr;
  bool expecting_header = true;
};

void caf_main(actor_system& sys, const actor_system_config&) {
  using acceptor_t = tcp_acceptor<tcp_protocol<basp>>;
  using io::network::make_server_newb;
  using io::network::make_client_newb;
  const char* host = "localhost";
  const uint16_t port = 12345;
  scoped_actor self{sys};

  auto running = [=](event_based_actor* self, std::string name, actor , actor b) -> behavior {
    return {
      [=](std::string str) {
        aout(self) << "[" << name << "] received '" << str << "'" << std::endl;
      },
      [=](send_atom, std::string str) {
        aout(self) << "[" << name << "] sending '" << str << "'" << std::endl;
        self->send(b, send_atom::value, self->id(), actor_id{}, str);
      },
    };
  };
  auto init = [=](event_based_actor* self, std::string name, actor m) -> behavior {
    self->set_default_handler(skip);
    return {
      [=](actor b) {
        aout(self) << "[" << name << "] got broker, let's do this" << std::endl;
        self->become(running(self, name, m, b));
        self->set_default_handler(print_and_drop);
      }
    };
  };

  auto server_helper = sys.spawn(init, "s", self);
  auto client_helper = sys.spawn(init, "c", self);

  aout(self) << "creating new server" << std::endl;
  auto server_ptr = make_server_newb<acceptor_t, accept_tcp>(sys, port, nullptr, true);
  server_ptr->responder = server_helper;

  aout(self) << "creating new client" << std::endl;
  auto client = make_client_newb<basp_newb, tcp_transport, tcp_protocol<basp>>(sys, host, port);
  self->send(client, responder_atom::value, client_helper);

  self->send(client_helper, send_atom::value, "hallo");
  self->send(server_helper, send_atom::value, "hallo");

  self->receive(
    [&](quit_atom) {
      aout(self) << "check" << std::endl;
    }
  );

  /*
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
  */
}

} // namespace anonymous

CAF_MAIN(io::middleman);
