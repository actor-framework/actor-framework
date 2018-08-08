#include "caf/io/network/newb.hpp"

#include "caf/logger.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/detail/call_cfun.hpp"
#include "caf/policy/newb_tcp.hpp"

using namespace caf;

using caf::io::network::default_multiplexer;
using caf::io::network::invalid_native_socket;
using caf::io::network::make_client_newb;
using caf::io::network::make_server_newb;
using caf::io::network::native_socket;
using caf::policy::accept_tcp;
using caf::policy::tcp_protocol;
using caf::policy::tcp_transport;

namespace {

using interval_atom = atom_constant<atom("interval")>;
using ordering_atom = atom_constant<atom("ordering")>;
using send_atom = atom_constant<atom("send")>;
using quit_atom = atom_constant<atom("quit")>;
using responder_atom = atom_constant<atom("responder")>;

constexpr size_t chunk_size = 1024; //128; //8192; //1024;

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

struct basp_newb : public io::network::newb<new_basp_message> {
  using message_type = new_basp_message;

  basp_newb(caf::actor_config& cfg, default_multiplexer& dm,
            native_socket sockfd)
      : newb<message_type>(cfg, dm, sockfd),
        running(true),
        interval_counter(0),
        received_messages(0),
        interval(5000) {
    // nop
    CAF_LOG_TRACE("");
  }

  void handle(message_type& msg) override {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_LOG_TRACE("");
    if (msg.payload_len == 1) {
      // nop
    } else {
      received_messages += 1;
      if (received_messages % 1000 == 0)
        std::cout << "received " << received_messages << " messages" << std::endl;
      // nop
    }
  }

  behavior make_behavior() override {
    set_default_handler(print_and_drop);
    return {
      // Must be implemented at the moment, will be cought by the broker in a
      // later implementation.
      [=](atom_value atm, uint32_t id) {
        protocol->timeout(atm, id);
      },
      [=](send_atom, char c) {
        if (running) {
          delayed_send(this, interval, send_atom::value, char((c + 1) % 256));
          auto hw = caf::make_callback([&](io::network::byte_buffer& buf) -> error {
            binary_serializer bs(&backend(), buf);
            bs(basp_header{0, id(), actor_id{}});
            return none;
          });
          auto whdl = wr_buf(&hw);
          CAF_ASSERT(whdl.buf != nullptr);
          CAF_ASSERT(whdl.protocol != nullptr);
          binary_serializer bs(&backend(), *whdl.buf);
          auto start = whdl.buf->size();
          whdl.buf->resize(start + chunk_size);
          std::fill(whdl.buf->begin() + start, whdl.buf->end(), c);
        }
      },
      [=](responder_atom, actor r) {
        std::cout << "got responder assigned" << std::endl;
        responder = r;
        send(r, this);
      },
      [=](interval_atom) {
        if (running) {
          delayed_send(this, std::chrono::seconds(1), interval_atom::value);
          interval_counter += 1;
          data.emplace_back(interval,
                            transport->count,
                            transport->offline_buffer.size());
          if (interval_counter % 10 == 0) {
            auto cnt = interval.count();
            auto dec = cnt > 1000 ? 1000 : (cnt > 100 ? 100 : 10);
            interval -= std::chrono::microseconds(dec);
          }
          transport->count = 0;
          if (interval.count() <= 0)
            running = false;
        } else {
          std::map<size_t, std::vector<size_t>> aggregate;
          for (auto& t : data) {
            auto expected = (1000000 / get<0>(t).count());
            aggregate[expected].push_back(get<1>(t));
          }
          for (auto& p : aggregate) {
            std::cerr << p.first;
            for (auto v : p.second)
              std::cerr << ", " << v;
            std::cerr << std::endl;
          }
          send(this, quit_atom::value);
        }
      },
      [=](quit_atom) {
        std::cout << "got quit message" << std::endl;
        // Remove from multiplexer loop.
        stop();
        // Quit actor.
        quit();
        send(responder, quit_atom::value);
      }
    };
  }

  bool running;
  actor responder;
  uint32_t interval_counter;
  uint32_t received_messages;
  std::chrono::microseconds interval;
  // values: measurement point, current interval, messages sent in interval, offline buffer size
  std::vector<std::tuple<std::chrono::microseconds, size_t, size_t>> data;
};

template <class ProtocolPolicy>
struct tcp_acceptor
    : public io::network::newb_acceptor<typename ProtocolPolicy::message_type> {
  using super = io::network::newb_acceptor<typename ProtocolPolicy::message_type>;

  tcp_acceptor(default_multiplexer& dm, native_socket sockfd)
      : super(dm, sockfd) {
    // nop
  }

  expected<actor> create_newb(native_socket sockfd,
                              io::network::transport_policy_ptr pol) override {
    CAF_LOG_TRACE(CAF_ARG(sockfd));
    std::cout << "creating newb" << std::endl;
    auto n = io::network::make_newb<basp_newb>(this->backend().system(), sockfd);
    auto ptr = caf::actor_cast<caf::abstract_actor*>(n);
    if (ptr == nullptr)
      return sec::runtime_error;
    auto& ref = dynamic_cast<basp_newb&>(*ptr);
    ref.transport = std::move(pol);
    ref.protocol.reset(new ProtocolPolicy(&ref));
    ref.responder = responder;
    ref.configure_read(io::receive_policy::exactly(basp_header_len));
    anon_send(responder, n);
    return n;
  }

  actor responder;
};

class config : public actor_system_config {
public:
  uint16_t port = 12345;
  std::string host = "127.0.0.1";
  bool is_server = false;

  config() {
    opt_group{custom_options_, "global"}
    .add(port, "port,P", "set port")
    .add(host, "host,H", "set host")
    .add(is_server, "server,s", "set server");
  }
};

void caf_main(actor_system& sys, const config& cfg) {
  using acceptor_t = tcp_acceptor<tcp_protocol<basp>>;
  const char* host = cfg.host.c_str();
  const uint16_t port = cfg.port;
  scoped_actor self{sys};

  auto running = [=](event_based_actor* self, std::string,
                     actor m, actor) -> behavior {
    return {
      [=](quit_atom) {
        self->send(m, quit_atom::value);
      }
    };
  };
  auto init = [=](event_based_actor* self, std::string name,
                  actor m) -> behavior {
    self->set_default_handler(skip);
    return {
      [=](actor b) {
        std::cout << "[" << name << "] got broker, let's do this" << std::endl;
        self->become(running(self, name, m, b));
        self->set_default_handler(print_and_drop);
      }
    };
  };

  auto dummy_broker = [](io::broker*) -> behavior {
    return {
      [](io::new_connection_msg&) {
        std::cout << "got new connection" << std::endl;
      }
    };
  };

  auto name = cfg.is_server ? "server" : "client";
  auto helper = sys.spawn(init, name, self);

  actor nb;
  auto await_done = [&]() {
    self->receive(
      [&](quit_atom) {
        std::cout << "done" << std::endl;
      }
    );
  };
  if (cfg.is_server) {
    std::cout << "creating new server" << std::endl;
    auto server_ptr = make_server_newb<acceptor_t, accept_tcp>(sys, port, nullptr,
                                                               true);
    //server_ptr->responder = helper;
    //std::cout << "creating new client" << std::endl;
    //auto client = make_client_newb<basp_newb, tcp_transport,
                                   //tcp_protocol<raw_tcp>>(sys, host, port);
    // If I don't do this, our newb acceptor will never get events ...
    auto b = sys.middleman().spawn_server(dummy_broker, port + 1);
    await_done();
  } else {
    std::cout << "creating new client" << std::endl;
    auto client = make_client_newb<basp_newb, tcp_transport,
                                   tcp_protocol<basp>>(sys, host, port);
    self->send(client, responder_atom::value, helper);
    self->send(client, send_atom::value, char(0));
    self->send(client, interval_atom::value);
    await_done();
  }
}

} // namespace anonymous

CAF_MAIN(io::middleman);
