// Simple WebSocket server that sends local files from a working directory to
// the clients.

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/cow_string.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/web_socket/accept.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/span.hpp"

#include <cassert>
#include <iostream>
#include <utility>

using namespace std::literals;

static constexpr uint16_t default_port = 8080;

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

caf::span<const std::string_view> quotes_by_path(std::string_view path) {
  if (path == "/epictetus")
    return caf::make_span(epictetus);
  else if (path == "/seneca")
    return caf::make_span(seneca);
  else if (path == "/plato")
    return caf::make_span(plato);
  else
    return {};
}

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<uint16_t>("port,p", "port to listen for incoming connections");
  }
};

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

int caf_main(caf::actor_system& sys, const config& cfg) {
  namespace cn = caf::net;
  namespace ws = cn::web_socket;
  // Open up a TCP port for incoming connections.
  auto port = caf::get_or(cfg, "port", default_port);
  cn::tcp_accept_socket fd;
  if (auto maybe_fd = cn::make_tcp_accept_socket({caf::ipv4_address{}, port})) {
    std::cout << "*** started listening for incoming connections on port "
              << port << '\n';
    fd = std::move(*maybe_fd);
  } else {
    std::cerr << "*** unable to open port " << port << ": "
              << to_string(maybe_fd.error()) << '\n';
    return EXIT_FAILURE;
  }
  // Convenience type aliases.
  using event_t = ws::accept_event_t<caf::cow_string>;
  // Create buffers to signal events from the WebSocket server to the worker.
  auto [wres, sres] = ws::make_accept_event_resources<caf::cow_string>();
  // Spin up a worker to handle the events.
  auto worker = sys.spawn([worker_res = wres](caf::event_based_actor* self) {
    // For each buffer pair, we create a new flow ...
    self->make_observable()
      .from_resource(worker_res)
      .for_each([self, f = pick_random{}](const event_t& event) mutable {
        // ... that pushes one random quote to the client.
        auto [pull, push, path] = event.data();
        auto quotes = quotes_by_path(path);
        auto quote = quotes.empty() ? "Try /epictetus, /seneca or /plato."
                                    : f(quotes);
        self->make_observable().just(ws::frame{quote}).subscribe(push);
        // Note: we simply drop `pull` here, which will close the buffer.
      });
  });
  // Callback for incoming WebSocket requests.
  auto on_request = [](const caf::settings& hdr, auto& req) {
    // The hdr parameter is a dictionary with fields from the WebSocket
    // handshake such as the path. This is only field we care about here.
    auto path = caf::get_or(hdr, "web-socket.path", "/");
    req.accept(caf::cow_string{std::move(path)});
  };
  // Set everything in motion.
  ws::accept(sys, fd, std::move(sres), on_request);
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
