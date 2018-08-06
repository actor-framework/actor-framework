#include "caf/io/network/newb.hpp"

#include "caf/config.hpp"
#include "caf/logger.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/detail/call_cfun.hpp"
#include "caf/policy/newb_udp.hpp"

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
  using caf::io::network::make_server_newb;
  using caf::io::network::make_client_newb;
  using caf::policy::udp_transport;
  using caf::policy::accept_udp;
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
