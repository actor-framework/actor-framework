#include "caf/io/middleman.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <iostream>

using namespace caf;
using namespace std::literals;

static constexpr auto max_batch_delay = 50ms;
static constexpr auto max_batch_size = 10u;
static constexpr auto max_buffered = 50u;
static constexpr auto demand_threshold = 5u;

struct point {
  int32_t x;
  int32_t y;
};

template <class Inspector>
bool inspect(Inspector& f, point& x) {
  return f.object(x).fields(f.field("x", x.x), f.field("y", x.y));
}

CAF_BEGIN_TYPE_ID_BLOCK(stream_driver, first_custom_type_id)
  CAF_ADD_TYPE_ID(stream_driver, (point))
CAF_END_TYPE_ID_BLOCK(stream_driver)

struct config : actor_system_config {
  bool server_mode = false;
  std::string host = "localhost";
  uint16_t port = 0;
  config() {
    opt_group{custom_options_, "global"}
      .add(server_mode, "server-mode,s", "run in server mode")
      .add(host, "host,H", "set host (ignored in server mode)")
      .add(port, "port,p", "set port");
  }
};

behavior producer(event_based_actor* self) {
  return {
    [self](get_atom) {
      return self->make_observable()
        .iota(1)
        .take(9)
        .map([](int x) {
          return point{x, x * x};
        })
        .to_stream("points", max_batch_delay, max_batch_size);
    },
  };
}

void consumer(event_based_actor* self, const actor& src) {
  self->request(src, infinite, get_atom_v).then([self](const stream& in) {
    self->observe_as<point>(in, max_buffered, demand_threshold)
      .for_each([](const point& x) { std::cout << deep_to_string(x) << '\n'; });
  });
}

int server(actor_system& sys, uint16_t port) {
  auto src = sys.spawn(producer);
  auto actual_port = sys.middleman().publish(src, port);
  if (!actual_port) {
    std::cout << "failed to open port " << port << ": "
              << to_string(actual_port.error()) << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int client(actor_system& sys, const std::string& host, uint16_t port) {
  auto attempt = 0;
  auto src = sys.middleman().remote_actor(host, port);
  if (!src) {
    if (++attempt == 8) {
      std::cout << "could not connect to server\n";
      return EXIT_FAILURE;
    }
    std::this_thread::sleep_for(125ms);
    src = sys.middleman().remote_actor(host, port);
  }
  sys.spawn(consumer, *src);
  return EXIT_SUCCESS;
}

int caf_main(actor_system& sys, const config& cfg) {
  if (cfg.server_mode)
    return server(sys, cfg.port);
  else
    return client(sys, cfg.host, cfg.port);
}

CAF_MAIN(id_block::stream_driver, io::middleman)
