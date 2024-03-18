// Simple HTTP/WebSocket server that sends predefined text snippets
// (philosophers quotes) to the client. Clients may either ask for a single
// quote via HTTP GET request or for all quotes of a selected philosopher by
// connecting via WebSocket.

#include "caf/net/http/with.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/web_socket/switch_protocol.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/cow_string.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/span.hpp"

#include <atomic>
#include <cassert>
#include <csignal>
#include <iostream>
#include <random>
#include <utility>

using namespace std::literals;

// -- constants ----------------------------------------------------------------

static constexpr uint16_t default_port = 8080;

static constexpr size_t default_max_connections = 128;

static constexpr std::string_view epictetus[] = {
  "Wealth consists not in having great possessions, but in having few wants.",
  "Don't explain your philosophy. Embody it.",
  "First say to yourself what you would be; and then do what you have to do.",
  "It's not what happens to you, but how you react to it that matters.",
  "If you want to improve, be content to be thought foolish and stupid.",
  "He who laughs at himself never runs out of things to laugh at.",
  "It is impossible for a man to learn what he thinks he already knows.",
  "Circumstances don't make the man, they only reveal him to himself.",
  "People are not disturbed by things, but by the views they take of them.",
  "Only the educated are free.",
};

static constexpr std::string_view seneca[] = {
  "Luck is what happens when preparation meets opportunity.",
  "All cruelty springs from weakness.",
  "We suffer more often in imagination than in reality.",
  "Difficulties strengthen the mind, as labor does the body.",
  "If a man knows not to which port he sails, no wind is favorable.",
  "It is the power of the mind to be unconquerable.",
  "No man was ever wise by chance.",
  "He suffers more than necessary, who suffers before it is necessary.",
  "I shall never be ashamed of citing a bad author if the line is good.",
  "Only time can heal what reason cannot.",
};

static constexpr std::string_view plato[] = {
  "Love is a serious mental disease.",
  "The measure of a man is what he does with power.",
  "Ignorance, the root and stem of every evil.",
  "Those who tell the stories rule society.",
  "You should not honor men more than truth.",
  "When men speak ill of thee, live so as nobody may believe them.",
  "The beginning is the most important part of the work.",
  "Necessity is the mother of invention.",
  "The greatest wealth is to live content with little.",
  "Beauty lies in the eyes of the beholder.",
};

// -- configuration setup ------------------------------------------------------

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<uint16_t>("port,p", "port to listen for incoming connections")
      .add<size_t>("max-connections,m", "limit for concurrent clients");
    opt_group{custom_options_, "tls"} //
      .add<std::string>("key-file,k", "path to the private key file")
      .add<std::string>("cert-file,c", "path to the certificate file");
  }

  caf::settings dump_content() const override {
    auto result = actor_system_config::dump_content();
    caf::put_missing(result, "port", default_port);
    caf::put_missing(result, "max-connections", default_max_connections);
    return result;
  }
};

// -- helper functions ---------------------------------------------------------

// Returns a list of philosopher quotes by path.
caf::span<const std::string_view> quotes_by_name(std::string_view path) {
  if (path == "epictetus")
    return caf::make_span(epictetus);
  else if (path == "seneca")
    return caf::make_span(seneca);
  else if (path == "plato")
    return caf::make_span(plato);
  else
    return {};
}

// Chooses a random quote from a list of quotes.
struct pick_random {
public:
  pick_random() : engine_(std::random_device{}()) {
    // nop
  }

  std::string_view operator()(caf::span<const std::string_view> quotes) {
    assert(!quotes.empty());
    auto dis = std::uniform_int_distribution<size_t>{0, quotes.size() - 1};
    return quotes[dis(engine_)];
  }

private:
  std::minstd_rand engine_;
};

std::string not_found_str(std::string_view name) {
  auto result = "Name '"s;
  result += name;
  result += "' not found. Try 'epictetus', 'seneca' or 'plato'.";
  return result;
}

// -- main ---------------------------------------------------------------------

namespace {

std::atomic<bool> shutdown_flag;

void set_shutdown_flag(int) {
  shutdown_flag = true;
}

} // namespace

int caf_main(caf::actor_system& sys, const config& cfg) {
  namespace http = caf::net::http;
  namespace ssl = caf::net::ssl;
  namespace ws = caf::net::web_socket;
  // Do a regular shutdown for CTRL+C and SIGTERM.
  signal(SIGTERM, set_shutdown_flag);
  signal(SIGINT, set_shutdown_flag);
  // Read the configuration.
  auto port = caf::get_or(cfg, "port", default_port);
  auto pem = ssl::format::pem;
  auto key_file = caf::get_as<std::string>(cfg, "tls.key-file");
  auto cert_file = caf::get_as<std::string>(cfg, "tls.cert-file");
  auto max_connections = caf::get_or(cfg, "max-connections",
                                     default_max_connections);
  if (!key_file != !cert_file) {
    std::cerr << "*** inconsistent TLS config: declare neither file or both\n";
    return EXIT_FAILURE;
  }
  // Open up a TCP port for incoming connections and start the server.
  using trait = ws::default_trait;
  auto server
    = http::with(sys)
        // Optionally enable TLS.
        .context(ssl::context::enable(key_file && cert_file)
                   .and_then(ssl::emplace_server(ssl::tls::v1_2))
                   .and_then(ssl::use_private_key_file(key_file, pem))
                   .and_then(ssl::use_certificate_file(cert_file, pem)))
        // Bind to the user-defined port.
        .accept(port)
        // Limit how many clients may be connected at any given time.
        .max_connections(max_connections)
        // On "/quote/<arg>", we pick one random quote for the client.
        .route("/quote/<arg>", http::method::get,
               [](http::responder& res, std::string name) {
                 auto quotes = quotes_by_name(name);
                 if (quotes.empty()) {
                   res.respond(http::status::not_found, "text/plain",
                               not_found_str(name));
                 } else {
                   pick_random f;
                   res.respond(http::status::ok, "text/plain", f(quotes));
                 }
               })
        // --(rst-switch_protocol-begin)--
        // On "/ws/quotes/<arg>", we switch the protocol to WebSocket.
        .route("/ws/quotes/<arg>", http::method::get,
               ws::switch_protocol()
                 // Check that the client asks for a known philosopher.
                 .on_request(
                   [](ws::acceptor<caf::cow_string>& acc, std::string name) {
                     auto quotes = quotes_by_name(name);
                     if (quotes.empty()) {
                       auto err = caf::error{caf::sec::invalid_argument,
                                             not_found_str(name)};
                       acc.reject(std::move(err));
                     } else {
                       // Forward the name to the WebSocket worker.
                       acc.accept(caf::cow_string{std::move(name)});
                     }
                   })
                 // Spawn a worker for the WebSocket clients.
                 .on_start(
                   [&sys](trait::acceptor_resource<caf::cow_string> events) {
                     // Spawn a worker that reads from `events`.
                     using event_t = trait::accept_event<caf::cow_string>;
                     sys.spawn([events](caf::event_based_actor* self) {
                       // Each WS connection has a pull/push buffer pair.
                       self->make_observable()
                         .from_resource(events) //
                         .for_each([self](const event_t& ev) mutable {
                           // Forward the quotes to the client.
                           auto [pull, push, name] = ev.data();
                           auto quotes = quotes_by_name(name);
                           assert(!quotes.empty()); // Checked in on_request.
                           self->make_observable()
                             .from_container(quotes)
                             .map([](std::string_view quote) {
                               return ws::frame{quote};
                             })
                             .subscribe(push);
                           // We ignore whatever the client may send to us.
                           pull.observe_on(self).subscribe(std::ignore);
                         });
                     });
                   }))
        .route("/status", http::method::get,
               [](http::responder& res) {
                 res.respond(http::status::no_content);
               })
        // --(rst-switch_protocol-end)--
        // Run with the configured routes.
        .start();
  // Report any error to the user.
  if (!server) {
    std::cerr << "*** unable to run at port " << port << ": "
              << to_string(server.error()) << '\n';
    return EXIT_FAILURE;
  }
  // Wait for CTRL+C or SIGTERM.
  while (!shutdown_flag)
    std::this_thread::sleep_for(250ms);
  std::cerr << "*** shutting down\n";
  server->dispose();
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
