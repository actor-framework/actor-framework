#include "caf/io/network/newb.hpp"

#include "caf/all.hpp"
#include "caf/logger.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/detail/call_cfun.hpp"
#include "caf/policy/newb_tcp.hpp"

using namespace caf;

using io::network::native_socket;
using io::network::invalid_native_socket;
using io::network::default_multiplexer;

namespace {

using ordering_atom = atom_constant<atom("ordering")>;
using send_atom = atom_constant<atom("send")>;
using quit_atom = atom_constant<atom("quit")>;
using responder_atom = atom_constant<atom("responder")>;

struct new_data {
  char* payload;
  size_t payload_len;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun, new_data& data) {
  return fun(meta::type_name("new_data"), data.payload_len);
}

struct raw_tcp {
  using message_type = new_data;
  using result_type = optional<message_type>;
  io::network::newb<message_type>* parent;
  message_type msg;

  raw_tcp(io::network::newb<message_type>* parent) : parent(parent) {
    // nop
    parent->configure_read(io::receive_policy::exactly(1000));
  }

  error read(char* bytes, size_t count) {
    msg.payload = bytes;
    msg.payload_len = count;
    parent->handle(msg);
    return none;
  }

  error timeout(atom_value, uint32_t) {
    return none;
  }

  size_t write_header(io::network::byte_buffer&,
                      io::network::header_writer*) {
    return 0;
  }

  void prepare_for_sending(io::network::byte_buffer&, size_t, size_t, size_t) {
    // nop
  }
};

struct raw_newb : public io::network::newb<new_data> {
  using message_type = new_data;

  raw_newb(caf::actor_config& cfg, default_multiplexer& dm,
            native_socket sockfd)
      : newb<message_type>(cfg, dm, sockfd) {
    // nop
    CAF_LOG_TRACE("");
    std::cerr << "constructing newb" << std::endl;
  }

  ~raw_newb() {
    std::cerr << "terminating newb" << std::endl;
    CAF_LOG_TRACE("");
  }

  behavior make_behavior() override {
    set_default_handler(print_and_drop);
    return {
      [=](message_type& msg) {
        CAF_LOG_TRACE("");
        char res = *msg.payload;
        send(responder, res);
      },
      [=](send_atom, char c) {
        auto whdl = wr_buf(nullptr);
        CAF_ASSERT(whdl.buf != nullptr);
        CAF_ASSERT(whdl.protocol != nullptr);
        whdl.buf->resize(1000);
        std::fill(whdl.buf->begin(), whdl.buf->end(), c);
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
    auto n = io::network::make_newb<raw_newb>(this->backend().system(), sockfd);
    auto ptr = caf::actor_cast<caf::abstract_actor*>(n);
    if (ptr == nullptr)
      return sec::runtime_error;
    auto& ref = dynamic_cast<raw_newb&>(*ptr);
    // TODO: Transport has to be assigned before protocol ... which sucks.
    //  (basp protocol calls configure read which accesses the transport.)
    ref.transport = std::move(pol);
    ref.protocol.reset(new ProtocolPolicy(&ref));
    ref.responder = responder;
    ref.configure_read(io::receive_policy::exactly(1000));
    // TODO: Just a workaround.
    anon_send(responder, n);
    return n;
  }

  actor responder;
};

void caf_main(actor_system& sys, const actor_system_config&) {
  using caf::io::network::make_server_newb;
  using caf::io::network::make_client_newb;
  using caf::policy::accept_tcp;
  using caf::policy::tcp_transport;
  using caf::policy::tcp_protocol;
  using acceptor_t = tcp_acceptor<tcp_protocol<raw_tcp>>;
  const char* host = "localhost";
  const uint16_t port = 12345;
  scoped_actor self{sys};

  auto running = [=](event_based_actor* self, std::string name,
                     actor, actor b) -> behavior {
    return {
      [=](char c) {
        aout(self) << "[" << name << "] received '" << c << "'" << std::endl;
      },
      [=](send_atom, char c) {
        aout(self) << "[" << name << "] sending '" << c << "'" << std::endl;
        self->send(b, send_atom::value, c);
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
  auto client = make_client_newb<raw_newb, tcp_transport,
                                 tcp_protocol<raw_tcp>>(sys, host, port);
  self->send(client, responder_atom::value, client_helper);

  self->send(client_helper, send_atom::value, 'a');
  self->send(server_helper, send_atom::value, 'b');

  self->receive(
    [&](quit_atom) {
      aout(self) << "check" << std::endl;
    }
  );
}

} // namespace anonymous

CAF_MAIN(io::middleman);
