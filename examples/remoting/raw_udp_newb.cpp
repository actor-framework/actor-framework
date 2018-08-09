#include "caf/all.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/detail/call_cfun.hpp"
#include "caf/io/network/newb.hpp"
#include "caf/logger.hpp"
#include "caf/policy/newb_udp.hpp"

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

struct raw_udp {
  using message_type = new_data;
  using result_type = optional<message_type>;
  io::network::newb<message_type>* parent;
  message_type msg;

  raw_udp(io::network::newb<message_type>* parent) : parent(parent) {
    // nop
  }

  error read(char* bytes, size_t count) {
    std::cerr << "read on raw UDP with " << count << " byted" << std::endl;
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

  ~raw_newb() override {
    std::cerr << "terminating newb" << std::endl;
    CAF_LOG_TRACE("");
  }

  void handle(message_type& msg) override {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_LOG_TRACE("");
    char res = *msg.payload;
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
struct udp_acceptor
    : public io::network::newb_acceptor<typename ProtocolPolicy::message_type> {
  using super = io::network::newb_acceptor<typename ProtocolPolicy::message_type>;

  udp_acceptor(default_multiplexer& dm, native_socket sockfd)
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
    ref.transport = std::move(pol);
    ref.protocol.reset(new ProtocolPolicy(&ref));
    ref.responder = responder;
    // Read first message from this socket
    ref.transport->prepare_next_read(this);
    ref.transport->read_some(this, *ref.protocol.get());
    // TODO: Just a workaround.
    anon_send(responder, n);
    return n;
  }

  actor responder;
};

void caf_main(actor_system& sys, const actor_system_config&) {
  using caf::io::network::make_server_newb;
  using caf::io::network::make_client_newb;
  using caf::policy::accept_udp;
  using caf::policy::udp_transport;
  using caf::policy::udp_protocol;
  using acceptor_t = udp_acceptor<udp_protocol<raw_udp>>;
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
  auto server_ptr = make_server_newb<acceptor_t, accept_udp>(sys, port, nullptr,
                                                             true);
  server_ptr->responder = server_helper;

  aout(self) << "creating new client" << std::endl;
  auto client = make_client_newb<raw_newb, udp_transport,
                                 udp_protocol<raw_udp>>(sys, host, port);
  self->send(client, responder_atom::value, client_helper);

  self->send(client_helper, send_atom::value, 'a');
  self->send(server_helper, send_atom::value, 'b');

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
