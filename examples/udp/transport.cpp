// Simple demonstration of data transfer over raw UDP.

#include "caf/net/datagram_transport.hpp"
#include "caf/net/ip.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/udp_datagram_socket.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/log/net.hpp"

#include <csignal>
#include <iostream>
#include <memory>
#include <utility>

using namespace caf;
using namespace caf::net;

using namespace std::literals;

namespace {

std::atomic<bool> shutdown_flag;

// Implementation of logger interface for our custom component "app".
namespace app {

constexpr std::string_view component = "app";

template <class... Ts>
void debug(caf::format_string_with_location fmt_str, Ts&&... args) {
  caf::logger::log(caf::log::level::debug, component, fmt_str,
                   std::forward<Ts>(args)...);
}

template <class... Ts>
void info(caf::format_string_with_location fmt_str, Ts&&... args) {
  caf::logger::log(caf::log::level::info, component, fmt_str,
                   std::forward<Ts>(args)...);
}

template <class... Ts>
void error(caf::format_string_with_location fmt_str, Ts&&... args) {
  caf::logger::log(caf::log::level::error, component, fmt_str,
                   std::forward<Ts>(args)...);
}

} // namespace app

} // namespace

// -- configuration setup ------------------------------------------------------

struct config : actor_system_config {
  config() {
    opt_group{custom_options_, "global"}
      .add<std::string>("address,a", "Address to send payload to")
      .add<std::string>("port,p", "Port to send payload to")
      .add<uint16_t>("listen,l", "Port to listen for incoming data");
  }
};

// -- utility functions --------------------------------------------------------

// Read lines from standard input. Run as detached actor.
void reader(event_based_actor* self, actor sink, ip_endpoint dest) {
  std::string line;
  while (std::getline(std::cin, line))
    self->send(sink, dest, line);
  // Stop the application when reading EOF.
  shutdown_flag = true;
}

// Print lines to standard output.
behavior printer(event_based_actor* self) {
  return {[](ip_endpoint ep, std::string message) {
            app::info("caf", "received {} bytes from {}:{}", message.size(),
                      ep.address(), ep.port());
            std::cout << message << std::endl;
          },
          [self](const message&) {
            app::error("caf", "received unexpected message");
            self->quit();
          }};
}

// Send payload over udp.
error send_payload(actor_system& sys, std::string addr, uint16_t port) {
  auto tg = logger::trace("caf", "Trying to send data to {}:{}", addr, port);
  auto print_actor = sys.spawn(printer);
  // Simplified case, we use only the first resolved address.
  auto addrs = ip::resolve(addr);
  if (addrs.empty())
    return make_error(sec::runtime_error, "No resolved ip address");
  auto dest = ip_endpoint{addrs[0], port};
  auto local = ip_endpoint{make_ipv4_address(127, 0, 0, 1), 0};
  auto sock = make_udp_datagram_socket(local);
  if (!sock) {
    std::cerr << "*** failed to open udp with: " << std::endl;
    return make_error(sec::runtime_error, "Can't establish UDP socket");
  }
  app::debug("caf", "Sending data to {}", dest);
  // Start multiplexer.
  auto& mpx = sys.network_manager().mpx();
  auto transport = std::make_unique<caf::net::datagram_transport>(*sock, sys,
                                                                  &mpx,
                                                                  print_actor);
  auto transport_ptr = transport.get();
  mpx.start(caf::net::socket_manager::make(&mpx, std::move(transport)));
  auto read_actor = sys.spawn<detached>(reader, transport_ptr->actor_handle(),
                                        dest);
  // Wait for CTRL+C or SIGTERM.
  while (!shutdown_flag)
    std::this_thread::sleep_for(250ms);
  anon_send_exit(print_actor, exit_reason::user_shutdown);
  // Stop read actor.
  std::cin.setstate(std::ios_base::failbit);
  anon_send_exit(transport_ptr->actor_handle(), exit_reason::user_shutdown);
  return error{};
}

// Listen for data over udp.
error listen_for_payload(actor_system& sys, ip_endpoint ep) {
  auto tg = logger::trace("caf", "Trying to connect to {}", ep);
  auto print_actor = sys.spawn(printer);
  auto sock = make_udp_datagram_socket(ep);
  if (!sock) {
    std::cerr << "*** failed to open udp with error code: "
              << detail::to_underlying(last_socket_error()) << std::endl;
    return make_error(sec::runtime_error, "Can't open UDP socket");
  }
  // Start multiplexer.
  auto& mpx = sys.network_manager().mpx();
  auto transport = std::make_unique<caf::net::datagram_transport>(*sock, sys,
                                                                  &mpx,
                                                                  print_actor);
  auto transport_ptr = transport.get();
  mpx.start(caf::net::socket_manager::make(&mpx, std::move(transport)));
  // Wait for CTRL+C or SIGTERM.
  while (!shutdown_flag)
    std::this_thread::sleep_for(250ms);
  anon_send_exit(print_actor, exit_reason::user_shutdown);
  anon_send_exit(transport_ptr->actor_handle(), exit_reason::user_shutdown);
  return error{};
}

// -- main ---------------------------------------------------------------------

int caf_main(caf::actor_system& sys, const config& cfg) {
  signal(SIGTERM, [](int) { shutdown_flag = true; });
  signal(SIGINT, [](int) { shutdown_flag = true; });
  // If the listen port is set up, we will open a UDP socket at localhost:port
  // and wait.
  if (auto maybe_port = get_as<uint16_t>(cfg, "listen")) {
    auto port = *maybe_port;
    auto ep = ip_endpoint{make_ipv4_address(127, 0, 0, 1), port};
    auto err = listen_for_payload(sys, ep);
    if (err)
      return EXIT_FAILURE;
    return EXIT_SUCCESS;
  }
  // Otherwise, send payload to address/port via UDP.
  auto maybe_port = get_as<uint16_t>(cfg, "port");
  auto maybe_address = get_as<std::string>(cfg, "address");
  if (!maybe_port || !maybe_address) {
    std::cerr << "*** missing port or address" << std::endl;
    return EXIT_FAILURE;
  }
  auto err = send_payload(sys, *maybe_address, *maybe_port);
  if (err)
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
