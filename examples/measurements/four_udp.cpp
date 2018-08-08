#include "caf/io/network/newb.hpp"

#include "caf/logger.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/detail/call_cfun.hpp"
#include "caf/policy/newb_basp.hpp"
#include "caf/policy/newb_ordering.hpp"
#include "caf/policy/newb_udp.hpp"

using namespace caf;

using caf::io::network::default_multiplexer;
using caf::io::network::invalid_native_socket;
using caf::io::network::make_client_newb;
using caf::io::network::make_server_newb;
using caf::io::network::native_socket;
using caf::policy::accept_udp;
using caf::policy::udp_protocol;
using caf::policy::udp_transport;

namespace {

using ordering_atom = atom_constant<atom("ordering")>;

constexpr size_t chunk_size = 1024; //8192; //128; //1024;

struct dummy_transport : public io::network::transport_policy {
  dummy_transport() {
    // nop
  }

  inline error read_some(io::network::event_handler*) override {
    return none;
  }

  inline bool should_deliver() override {
    return true;
  }

  void prepare_next_read(io::network::event_handler*) override {
    received_bytes = 0;
    receive_buffer.resize(maximum);
  }

  inline void configure_read(io::receive_policy::config) override {
    // nop
  }

  inline error write_some(io::network::event_handler* parent) override {
    written += send_sizes.front();
    send_sizes.pop_front();
    auto remaining = send_buffer.size() - written;
    count += 1;
    if (remaining == 0)
      prepare_next_write(parent);
    return none;
  }

  void prepare_next_write(io::network::event_handler*) override {
    written = 0;
    send_buffer.clear();
    send_sizes.clear();
    if (offline_buffer.empty()) {
      writing = false;
    } else {
      offline_sizes.push_back(offline_buffer.size() - offline_sum);
       // Switch buffers.
      send_buffer.swap(offline_buffer);
      send_sizes.swap(offline_sizes);
      // Reset sum.
      offline_sum = 0;
    }
  }

  io::network::byte_buffer& wr_buf() override {
    if (!offline_buffer.empty()) {
      auto chunk_size = offline_buffer.size() - offline_sum;
      offline_sizes.push_back(chunk_size);
      offline_sum += chunk_size;
    }
    return offline_buffer;
  }

  void flush(io::network::event_handler* parent) override {
    if (!offline_buffer.empty() && !writing) {
      writing = true;
      prepare_next_write(parent);
    }
  }

  expected<io::network::native_socket>
  connect(const std::string&, uint16_t,
          optional<io::network::protocol::network> = none) override {
    return invalid_native_socket;
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
};


struct raw_newb : public io::network::newb<caf::policy::new_basp_message> {
  using message_type = caf::policy::new_basp_message;

  raw_newb(caf::actor_config& cfg, default_multiplexer& dm,
            native_socket sockfd)
      : newb<message_type>(cfg, dm, sockfd) {
    // nop
    CAF_LOG_TRACE("");
  }

  void handle(message_type&) override {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_LOG_TRACE("");
  }

  behavior make_behavior() override {
    set_default_handler(print_and_drop);
    return {
      // Must be implemented at the moment, will be cought by the broker in a
      // later implementation.
      [=](atom_value atm, uint32_t id) {
        protocol->timeout(atm, id);
      }
    };
  }
};

class config : public actor_system_config {
public:
  int iterations = 10;

  config() {
    opt_group{custom_options_, "global"}
    .add(iterations, "iterations,i", "set iterations");
  }
};

void caf_main(actor_system& sys, const config& cfg) {
  using clock = std::chrono::system_clock;
  using resolution = std::chrono::milliseconds;
  auto n = io::network::make_newb<raw_newb>(sys, invalid_native_socket);
  auto ptr = caf::actor_cast<caf::abstract_actor*>(n);
  auto& ref = dynamic_cast<raw_newb&>(*ptr);
  ref.transport.reset(new dummy_transport);
  ref.protocol.reset(new udp_protocol<policy::ordering<caf::policy::datagram_basp>>(&ref));
  auto start = clock::now();
  for (int i = 0; i < cfg.iterations; ++i) {
    auto hw = caf::make_callback([&](io::network::byte_buffer& buf) -> error {
      binary_serializer bs(sys, buf);
      bs(policy::basp_header{0, actor_id{}, actor_id{}});
      return none;
    });
    auto whdl = ref.wr_buf(&hw);
    CAF_ASSERT(whdl.buf != nullptr);
    CAF_ASSERT(whdl.protocol != nullptr);
    binary_serializer bs(sys, *whdl.buf);

    auto start = whdl.buf->size();
    whdl.buf->resize(start + chunk_size);
    std::fill(whdl.buf->begin() + start, whdl.buf->end(), 'a');
  }
  auto end = clock::now();
  auto ticks = std::chrono::duration_cast<resolution>(end - start).count();
  std::cout << cfg.iterations << ", " << ticks << std::endl;
}

} // namespace anonymous

CAF_MAIN(io::middleman);
