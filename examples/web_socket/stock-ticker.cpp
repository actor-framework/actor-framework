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
#include "caf/net/stream_transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/net/web_socket/accept.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/span.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <utility>

using namespace std::literals;

static constexpr uint16_t default_port = 8080;

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

struct random_feed_state {
  using frame = caf::net::web_socket::frame;
  using accept_event = caf::net::web_socket::accept_event_t<>;

  random_feed_state(caf::event_based_actor* selfptr,
                    caf::async::consumer_resource<accept_event> events,
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

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<uint16_t>("port,p", "port to listen for incoming connections")
      .add<caf::timespan>("interval,i", "update interval");
  }
};

int caf_main(caf::actor_system& sys, const config& cfg) {
  namespace cn = caf::net;
  namespace ws = cn::web_socket;
  // Open up a TCP port for incoming connections.
  auto port = caf::get_or(cfg, "port", default_port);
  cn::tcp_accept_socket fd;
  if (auto maybe_fd = cn::make_tcp_accept_socket({caf::ipv4_address{}, port},
                                                 true)) {
    std::cout << "*** started listening for incoming connections on port "
              << port << '\n';
    fd = std::move(*maybe_fd);
  } else {
    std::cerr << "*** unable to open port " << port << ": "
              << to_string(maybe_fd.error()) << '\n';
    return EXIT_FAILURE;
  }
  // Create buffers to signal events from the WebSocket server to the worker.
  auto [wres, sres] = ws::make_accept_event_resources();
  // Spin up a worker to handle the events.
  auto interval = caf::get_or(cfg, "interval", caf::timespan{1s});
  auto worker = sys.spawn<random_feed_impl>(std::move(wres), interval);
  // Callback for incoming WebSocket requests.
  auto on_request = [](const caf::settings&, auto& req) { req.accept(); };
  // Set everything in motion.
  ws::accept(sys, fd, std::move(sres), on_request);
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
