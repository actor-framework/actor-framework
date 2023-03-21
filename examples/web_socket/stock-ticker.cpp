// Pseudo "Stock Ticker" that publishes random updates once per second via
// WebSocket feed.

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/cow_string.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/json_writer.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/web_socket/with.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/span.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <utility>

using namespace std::literals;

// -- constants ----------------------------------------------------------------

static constexpr uint16_t default_port = 8080;

static constexpr size_t default_max_connections = 128;

// -- custom types -------------------------------------------------------------

namespace stock {

struct info {
  std::string symbol;
  std::string currency;
  double current;
  double open;
  double high;
  double low;
};

template <class Inspector>
bool inspect(Inspector& f, info& x) {
  return f.object(x).fields(f.field("symbol", x.symbol),
                            f.field("currency", x.currency),
                            f.field("open", x.open), f.field("high", x.high),
                            f.field("low", x.low));
}

} // namespace stock

// -- actor for generating a random feed ---------------------------------------

struct random_feed_state {
  using trait = caf::net::web_socket::default_trait;
  using frame = caf::net::web_socket::frame;
  using accept_event = trait::accept_event<>;

  random_feed_state(caf::event_based_actor* selfptr,
                    trait::acceptor_resource<> events,
                    caf::timespan update_interval)
    : self(selfptr), val_dist(0, 100000), index_dist(0, 19) {
    // Init random number generator.
    std::random_device rd;
    rng.seed(rd());
    // Fill vector with some stuff.
    for (size_t i = 0; i < 20; ++i) {
      std::uniform_int_distribution<int> char_dist{'A', 'Z'};
      std::string symbol;
      for (size_t j = 0; j < 5; ++j)
        symbol += static_cast<char>(char_dist(rng));
      auto val = next_value();
      infos.emplace_back(
        stock::info{std::move(symbol), "USD", val, val, val, val});
    }
    // Create the feed to push updates once per second.
    writer.skip_object_type_annotation(true);
    feed = self->make_observable()
             .interval(update_interval)
             .map([this](int64_t) {
               writer.reset();
               auto& x = update();
               if (!writer.apply(x)) {
                 std::cerr << "*** failed to generate JSON: "
                           << to_string(writer.get_error()) << '\n';
                 return frame{};
               }
               return frame{writer.str()};
             })
             .filter([](const frame& x) {
               // Just in case: drop frames that failed to generate JSON.
               return x.is_text();
             })
             .share();
    // Subscribe once to start the feed immediately and to keep it running.
    feed.for_each([n = 1](const frame&) mutable {
      std::cout << "*** tick " << n++ << "\n";
    });
    // Add each incoming WebSocket listener to the feed.
    auto n = std::make_shared<int>(0);
    events
      .observe_on(self) //
      .for_each([this, n](const accept_event& ev) {
        std::cout << "*** added listener (n = " << ++*n << ")\n";
        auto [pull, push] = ev.data();
        pull.observe_on(self)
          .do_finally([n] { //
            std::cout << "*** removed listener (n = " << --*n << ")\n";
          })
          .subscribe(std::ignore);
        feed.subscribe(push);
      });
  }

  // Picks a random stock, assigns a new value to it, and returns it.
  stock::info& update() {
    auto& x = infos[index_dist(rng)];
    auto val = next_value();
    x.current = val;
    x.high = std::max(x.high, val);
    x.low = std::min(x.high, val);
    return x;
  }

  double next_value() {
    return val_dist(rng) / 100.0;
  }

  caf::event_based_actor* self;
  caf::flow::observable<frame> feed;
  caf::json_writer writer;
  std::vector<stock::info> infos;
  std::minstd_rand rng;
  std::uniform_int_distribution<int> val_dist;
  std::uniform_int_distribution<size_t> index_dist;
};

using random_feed_impl = caf::stateful_actor<random_feed_state>;

// -- configuration setup ------------------------------------------------------

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<uint16_t>("port,p", "port to listen for incoming connections")
      .add<size_t>("max-connections,m", "limit for concurrent clients")
      .add<caf::timespan>("interval,i", "update interval");
    ;
    opt_group{custom_options_, "tls"} //
      .add<std::string>("key-file,k", "path to the private key file")
      .add<std::string>("cert-file,c", "path to the certificate file");
  }
};

// -- main ---------------------------------------------------------------------

int caf_main(caf::actor_system& sys, const config& cfg) {
  namespace http = caf::net::http;
  namespace ssl = caf::net::ssl;
  namespace ws = caf::net::web_socket;
  // Read the configuration.
  auto interval = caf::get_or(cfg, "interval", caf::timespan{1s});
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
    = ws::with(sys)
        // Optionally enable TLS.
        .context(ssl::context::enable(key_file && cert_file)
                   .and_then(ssl::emplace_server(ssl::tls::v1_2))
                   .and_then(ssl::use_private_key_file(key_file, pem))
                   .and_then(ssl::use_certificate_file(cert_file, pem)))
        // Bind to the user-defined port.
        .accept(port)
        // Limit how many clients may be connected at any given time.
        .max_connections(max_connections)
        // Add handler for incoming connections.
        .on_request([](ws::acceptor<>& acc, const http::request_header&) {
          // Ignore all header fields and accept the connection.
          acc.accept();
        })
        // When started, run our worker actor to handle incoming connections.
        .start([&sys, interval](trait::acceptor_resource<> events) {
          sys.spawn<random_feed_impl>(std::move(events), interval);
        });
  // Report any error to the user.
  if (!server) {
    std::cerr << "*** unable to run at port " << port << ": "
              << to_string(server.error()) << '\n';
    return EXIT_FAILURE;
  }
  // Note: the actor system will keep the application running for as long as the
  // workers are still alive.
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
