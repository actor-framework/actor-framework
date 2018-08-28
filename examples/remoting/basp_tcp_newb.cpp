#include "caf/all.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/detail/call_cfun.hpp"
#include "caf/io/network/newb.hpp"
#include "caf/logger.hpp"
#include "caf/policy/newb_tcp.hpp"

using namespace caf;

using io::network::native_socket;
using io::network::invalid_native_socket;
using io::network::default_multiplexer;

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

  behavior make_behavior() override {
    set_default_handler(print_and_drop);
    return {
      [=](new_basp_message& msg) {
        CAF_LOG_TRACE("");
        std::string res;
        binary_deserializer bd(&backend(), msg.payload, msg.payload_len);
        bd(res);
        send(responder, res);
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
  using caf::io::network::make_server_newb;
  using caf::io::network::make_client_newb;
  using caf::policy::accept_tcp;
  using caf::policy::tcp_transport;
  using caf::policy::tcp_protocol;
  using acceptor_t = tcp_acceptor<tcp_protocol<basp>>;
  const char* host = "localhost";
  const uint16_t port = 12345;
  scoped_actor self{sys};

  auto running = [=](event_based_actor* self, std::string name,
                     actor, actor b) -> behavior {
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
  auto init = [=](event_based_actor* self, std::string name,
                  actor m) -> behavior {
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
  auto server_ptr = make_server_newb<acceptor_t, accept_tcp>(sys, port, nullptr,
                                                             true);
  server_ptr->responder = server_helper;

  aout(self) << "creating new client" << std::endl;
  auto client = make_client_newb<basp_newb, tcp_transport,
                                 tcp_protocol<basp>>(sys, host, port);
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
