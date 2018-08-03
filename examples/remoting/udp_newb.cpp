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

// -- udp impls ----------------------------------------------------------------

struct udp_header {
  uint32_t payload_len;
  actor_id from;
  actor_id to;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun, udp_header& hdr) {
  return fun(meta::type_name("basp_header"), hdr.payload_len,
             hdr.from, hdr.to);
}

constexpr size_t udp_basp_header_len = sizeof(uint32_t) + sizeof(actor_id) * 2;

using sequence_type = uint16_t;

struct ordering_header {
  sequence_type seq;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun, ordering_header& hdr) {
  return fun(meta::type_name("ordering_header"), hdr.seq);
}

constexpr size_t udp_ordering_header_len = sizeof(sequence_type);

struct new_basp_message {
  udp_header header;
  char* payload;
  size_t payload_len;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun,
                                        new_basp_message& msg) {
  return fun(meta::type_name("new_basp_message"), msg.header,
             msg.payload_len);
}

struct basp {
  static constexpr size_t header_size = udp_basp_header_len;
  using message_type = new_basp_message;
  using result_type = optional<message_type>;
  io::network::newb<message_type>* parent;
  message_type msg;

  basp(io::network::newb<message_type>* parent) : parent(parent) {
    // nop
  }

  error read(char* bytes, size_t count) {
    // Read header.
    if (count < udp_basp_header_len) {
      CAF_LOG_DEBUG("not enought bytes for basp header");
      return sec::unexpected_message;
    }
    binary_deserializer bd{&parent->backend(), bytes, count};
    bd(msg.header);
    size_t payload_len = static_cast<size_t>(msg.header.payload_len);
    // Read payload.
    auto remaining = count - udp_basp_header_len;
    // TODO: Could be `!=` ?
    if (remaining < payload_len) {
      CAF_LOG_ERROR("not enough bytes remaining to fit payload");
      return sec::unexpected_message;
    }
    msg.payload = bytes + udp_basp_header_len;
    msg.payload_len = msg.header.payload_len;
    parent->handle(msg);
    return none;
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

template <class Next>
struct ordering {
  static constexpr size_t header_size = udp_ordering_header_len;
  using message_type = typename Next::message_type;
  using result_type = typename Next::result_type;
  sequence_type seq_read = 0;
  sequence_type seq_write = 0;
  size_t max_pending_messages = 10;
  std::chrono::milliseconds pending_to = std::chrono::milliseconds(100);
  io::network::newb<message_type>* parent;
  Next next;
  std::unordered_map<sequence_type, std::vector<char>> pending;

  ordering(io::network::newb<message_type>* parent)
      : parent(parent),
        next(parent) {
    // nop
  }

  error deliver_pending() {
    if (pending.empty())
      return none;
    while (pending.count(seq_read) > 0) {
      auto& buf = pending[seq_read];
      auto res = next.read(buf.data(), buf.size());
      pending.erase(seq_read);
      // TODO: Cancel timeout.
      if (res)
        return res;
    }
    return none;
  }

  error add_pending(char* bytes, size_t count, sequence_type seq) {
    pending[seq] = std::vector<char>(bytes + header_size, bytes + count);
    parent->set_timeout(pending_to, ordering_atom::value, seq);
    if (pending.size() > max_pending_messages) {
      seq_read = pending.begin()->first;
      return deliver_pending();
    }
    return none;
  }

  error read(char* bytes, size_t count) {
    if (count < header_size)
      return sec::unexpected_message;
    ordering_header hdr;
    binary_deserializer bd(&parent->backend(), bytes, count);
    bd(hdr);
    // TODO: Use the comparison function from BASP instance.
    if (hdr.seq == seq_read) {
      seq_read += 1;
      auto res = next.read(bytes + header_size, count - header_size);
      if (res)
        return res;
      return deliver_pending();
    } else if (hdr.seq > seq_read) {
      add_pending(bytes, count, hdr.seq);
      return none;
    }
    return none;
  }

  error timeout(atom_value atm, uint32_t id) {
    if (atm == ordering_atom::value) {
      error err = none;
      sequence_type seq = static_cast<sequence_type>(id);
      if (pending.count(seq) > 0) {
        seq_read = static_cast<sequence_type>(seq);
        err = deliver_pending();
      }
      return err;
    }
    return next.timeout(atm, id);
  }

  void write_header(io::network::byte_buffer& buf,
                    io::network::header_writer* hw) {
    binary_serializer bs(&parent->backend(), buf);
    bs(ordering_header{seq_write});
    seq_write += 1;
    next.write_header(buf, hw);
    return;
  }

  void prepare_for_sending(io::network::byte_buffer& buf,
                           size_t hstart, size_t offset, size_t plen) {
    next.prepare_for_sending(buf, hstart, offset + header_size, plen);
  }
};

struct udp_transport : public io::network::transport_policy {
  udp_transport()
      : maximum{std::numeric_limits<uint16_t>::max()},
        first_message{true},
        writing{false},
        written{0},
        offline_sum{0} {
    // nop
  }

  error read_some(io::network::event_handler* parent) override {
    CAF_LOG_TRACE(CAF_ARG(parent->fd()));
    memset(sender.address(), 0, sizeof(sockaddr_storage));
    io::network::socket_size_type len = sizeof(sockaddr_storage);
    auto buf_ptr = static_cast<io::network::socket_recv_ptr>(receive_buffer.data());
    auto buf_len = receive_buffer.size();
    auto sres = ::recvfrom(parent->fd(), buf_ptr, buf_len,
                           0, sender.address(), &len);
    if (io::network::is_error(sres, true)) {
      CAF_LOG_ERROR("recvfrom returned" << CAF_ARG(sres));
      return sec::runtime_error;
    } else if (io::network::would_block_or_temporarily_unavailable(
                                        io::network::last_socket_error())) {
      return sec::end_of_stream;
    }
    if (sres == 0)
      CAF_LOG_INFO("Received empty datagram");
    else if (sres > static_cast<io::network::signed_size_type>(buf_len))
      CAF_LOG_WARNING("recvfrom cut of message, only received "
                      << CAF_ARG(buf_len) << " of " << CAF_ARG(sres) << " bytes");
    received_bytes = (sres > 0) ? static_cast<size_t>(sres) : 0;
    *sender.length() = static_cast<size_t>(len);
    if (first_message) {
      endpoint = sender;
      first_message = false;
    }
    return none;
  }

  bool should_deliver() override {
    CAF_LOG_TRACE("");
    return received_bytes != 0 && sender == endpoint;
  }

  void prepare_next_read(io::network::event_handler*) override {
    received_bytes = 0;
    receive_buffer.resize(maximum);
  }

  void configure_read(io::receive_policy::config) override {
    // nop
  }

  error write_some(io::network::event_handler* parent) override {
    std::cerr << "sending on socket: " << parent->fd() << std::endl;
    using namespace caf::io::network;
    CAF_LOG_TRACE(CAF_ARG(parent->fd()) << CAF_ARG(send_buffer.size()));
    socket_size_type len = static_cast<socket_size_type>(*endpoint.clength());
    auto buf_ptr = reinterpret_cast<socket_send_ptr>(send_buffer.data() + written);
    auto buf_len = send_sizes.front();
    auto sres = ::sendto(parent->fd(), buf_ptr, buf_len,
                         0, endpoint.caddress(), len);
    std::cerr << "sent " << sres << " bytes to " << to_string(endpoint) << std::endl;
    if (is_error(sres, true)) {
      std::cerr << "sento failed: " << last_socket_error_as_string() << std::endl;
      std::abort();
      CAF_LOG_ERROR("sendto returned" << CAF_ARG(sres));
      return sec::runtime_error;
    }
    send_sizes.pop_front();
    written += (sres > 0) ? static_cast<size_t>(sres) : 0;
    auto remaining = send_buffer.size() - written;
    std::cerr << "wrote '" << written << "' got '" << remaining << "' bytes left to write" << std::endl;
    if (remaining == 0)
      prepare_next_write(parent);
    return none;
  }

  void prepare_next_write(io::network::event_handler* parent) override {
    written = 0;
    send_buffer.clear();
    send_sizes.clear();
    if (offline_buffer.empty()) {
      writing = false;
      parent->backend().del(io::network::operation::write,
                            parent->fd(), parent);
    } else {
      // Add size of last chunk.
      offline_sizes.push_back(offline_buffer.size() - offline_sum);
      // Switch buffers.
      send_buffer.swap(offline_buffer);
      send_sizes.swap(offline_sizes);
      // Reset sum.
      offline_sum = 0;
    }
  }

  io::network::byte_buffer& wr_buf() {
    if (!offline_buffer.empty()) {
      auto chunk_size = offline_buffer.size() - offline_sum;
      offline_sizes.push_back(chunk_size);
      offline_sum += chunk_size;
      std::cerr << "adding chunk '" << chunk_size << "' up to total of '" << offline_sum << std::endl;
    }
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
    auto res = io::network::new_remote_udp_endpoint_impl(host, port, preferred);
    if (!res)
      return std::move(res.error());
    endpoint = res->second;
    return res->first;
  }

  // State for reading.
  size_t maximum;
  bool first_message;

  // State for writing.
  bool writing;
  size_t written;
  size_t offline_sum;
  std::deque<size_t> send_sizes;
  std::deque<size_t> offline_sizes;

  // UDP endpoints.
  io::network::ip_endpoint endpoint;
  io::network::ip_endpoint sender;
};

template <class T>
struct udp_protocol
    : public io::network::protocol_policy<typename T::message_type> {
  T impl;

  udp_protocol(io::network::newb<typename T::message_type>* parent)
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

  void prepare_for_sending(io::network::byte_buffer& buf,
                           size_t hstart, size_t offset, size_t plen) override {
    impl.prepare_for_sending(buf, hstart, offset, plen);
  }
};

struct basp_newb : public io::network::newb<new_basp_message> {
  basp_newb(caf::actor_config& cfg, default_multiplexer& dm,
            native_socket sockfd)
      : newb<new_basp_message>(cfg, dm, sockfd) {
    // nop
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
          bs(udp_header{0, sender, receiver});
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


struct accept_udp
    : public io::network::accept_policy<new_basp_message> {
  expected<native_socket> create_socket(uint16_t port, const char* host,
                                        bool reuse = false) override {
    auto res = io::network::new_local_udp_endpoint_impl(port, host, reuse);
    if (!res)
      return std::move(res.error());
    return (*res).first;
  }

  std::pair<native_socket, io::network::transport_policy_ptr>
  accept(io::network::event_handler*) override {
    auto res = io::network::new_local_udp_endpoint_impl(0, nullptr);
    if (!res) {
      CAF_LOG_DEBUG("failed to create local endpoint");
      return {invalid_native_socket, nullptr};
    }
    auto sock = std::move(res->first);
    io::network::transport_policy_ptr ptr{new udp_transport};
    return {sock, std::move(ptr)};
  }

  void init(io::network::newb<new_basp_message>& n) override {
    n.start();
  }
};

template <class ProtocolPolicy>
struct udp_acceptor
    : public io::network::newb_acceptor<typename ProtocolPolicy::message_type> {
  using super = io::network::newb_acceptor<typename ProtocolPolicy::message_type>;

  udp_acceptor(default_multiplexer& dm, native_socket sockfd)
      : super(dm, sockfd) {
    // nop
  }

  expected<actor> create_newb(native_socket sockfd,
                              io::network::transport_policy_ptr pol) override {
    auto n = io::network::make_newb<basp_newb>(this->backend().system(), sockfd);
    auto ptr = caf::actor_cast<caf::abstract_actor*>(n);
    if (ptr == nullptr)
      return sec::runtime_error;
    auto& ref = dynamic_cast<basp_newb&>(*ptr);
    ref.transport = std::move(pol);
    ref.protocol.reset(new ProtocolPolicy(&ref));
    ref.responder = responder;
    // Read first message from this socket.
    ref.transport->prepare_next_read(this);
    ref.transport->read_some(this, *ref.protocol.get());
    // Subsequent messages will be read from `sockfd`.
    anon_send(responder, n);
    return n;
  }

  actor responder;
};

struct udp_test_broker_state {
  io::datagram_handle hdl;
};

// -- main ---------------------------------------------------------------------

void caf_main(actor_system& sys, const actor_system_config&) {
  using acceptor_t = udp_acceptor<udp_protocol<ordering<basp>>>;
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
  auto server_ptr = make_server_newb<acceptor_t, accept_udp>(sys, port, nullptr, true);
  server_ptr->responder = server_helper;

  aout(self) << "creating new client" << std::endl;
  auto client = make_client_newb<basp_newb, udp_transport, udp_protocol<ordering<basp>>>(sys, host, port);
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
