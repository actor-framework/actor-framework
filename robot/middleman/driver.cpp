// Modes:
// - remote_actor: publish a cell actor at the server and wait for a connection
// - remote_spawn: open a port at the server and have the client spawn a cell
//   remotely
// - remote_lookup: open a port at the server and register a cell actor at the
//   registry, then have the client look up the actor remotely
// - unpublish: publish a controller actor at the server, then have the client
//   trigger an unpublish operation and check that the server is no longer
//   reachable
// - monitor_node: publish a controller actor at the server, then have the
//   client trigger a shutdown of the server and check that CAF sends a
//   node_down message to the client
// - prometheus: configure the server to export Prometheus metrics via HTTP; no
//   client setup, because the test will simply use HTTP GET on the server
// - rendesvous/ping/pong: publish an actor handle cell at the server, then have
//   the pong client "register" a pong actor at the cell and the ping client
//   retrieves the pong actor handle from the cell and sends a message to it

#include <caf/io/middleman.hpp>
#include <caf/io/publish.hpp>

#include <caf/actor_system.hpp>
#include <caf/actor_system_config.hpp>
#include <caf/caf_main.hpp>
#include <caf/event_based_actor.hpp>
#include <caf/expected.hpp>
#include <caf/scoped_actor.hpp>
#include <caf/stateful_actor.hpp>
#include <caf/type_id.hpp>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

struct non_deserializable_t {
  template <typename Inspector>
  friend bool inspect(Inspector&, non_deserializable_t&) {
    return !Inspector::is_loading;
  }
};

CAF_BEGIN_TYPE_ID_BLOCK(io_test, caf::first_custom_type_id)

  CAF_ADD_TYPE_ID(io_test, (non_deserializable_t))

CAF_END_TYPE_ID_BLOCK(io_test)

using namespace caf;
using namespace std::literals;

behavior cell_impl(int32_t init) {
  auto value = std::make_shared<int32_t>(init);
  return {
    [value](get_atom) { return *value; },
    [value](put_atom, int32_t new_value) { *value = new_value; },
  };
}

behavior actor_hdl_cell_impl() {
  auto value = std::make_shared<caf::actor>();
  return {
    [value](get_atom) { return *value; },
    [value](put_atom, caf::actor& new_value) { *value = std::move(new_value); },
  };
}

behavior controller_impl(event_based_actor* self) {
  return {
    [self](ok_atom) -> result<void> {
      if (auto ok = self->system().middleman().unpublish(actor{self}); !ok)
        return std::move(ok.error());
      return {};
    },
  };
}

struct config : actor_system_config {
  bool server = false;
  std::string host = "localhost";
  std::string mode;
  uint16_t port = 0;
  config() {
    set("caf.middleman.heartbeat-interval", "20ms");
    add_actor_type("cell", cell_impl);
    opt_group{custom_options_, "global"}
      .add(server, "server,s", "run in server mode")
      .add(mode, "mode,m", "set the test mode (what to test)")
      .add(host, "host,H", "set host (ignored in server mode)")
      .add(port, "port,p", "set port");
  }
};

int server(actor_system& sys, std::string_view mode, uint16_t port) {
  auto& mm = sys.middleman();
  if (mode == "remote_actor") {
    auto cell = sys.spawn(cell_impl, 42);
    auto actual_port = io::publish(cell, port);
    if (!actual_port) {
      std::cout << "failed to open port " << port << ": "
                << to_string(actual_port.error()) << '\n';
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
  if (mode == "remote_spawn") {
    auto actual_port = mm.open(port);
    if (!actual_port) {
      std::cout << "failed to open port " << port << ": "
                << to_string(actual_port.error()) << '\n';
      return EXIT_FAILURE;
    }
    for (;;) // Simply wait until Robot terminates this process.
      std::this_thread::sleep_for(360s);
    return EXIT_SUCCESS;
  }
  if (mode == "remote_lookup") {
    auto cell = sys.spawn(cell_impl, 23);
    sys.registry().put("cell", cell);
    auto actual_port = mm.open(port);
    if (!actual_port) {
      std::cout << "failed to open port " << port << ": "
                << to_string(actual_port.error()) << '\n';
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
  if (mode == "unpublish" || mode == "monitor_node"
      || mode == "deserialization_error") {
    auto actual_port = io::publish(sys.spawn(controller_impl), port);
    if (!actual_port) {
      std::cout << "failed to open port " << port << ": "
                << to_string(actual_port.error()) << '\n';
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
  if (mode == "prometheus") {
    for (;;) // Simply wait until Robot terminates this process.
      std::this_thread::sleep_for(360s);
    return EXIT_SUCCESS;
  }
  if (mode == "rendesvous") {
    auto cell = sys.spawn(actor_hdl_cell_impl);
    auto actual_port = io::publish(cell, port);
    if (!actual_port) {
      std::cout << "failed to open port " << port << ": "
                << to_string(actual_port.error()) << '\n';
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
  std::cout << "unknown mode: " << mode << '\n';
  return EXIT_FAILURE;
}

template <class Fn>
auto with_retry(Fn fn) {
  auto total_delay = 0ms;
  for (;;) {
    if (auto res = fn())
      return res;
    total_delay += 50ms;
    if (total_delay > 2s) {
      std::cout << "failed to connect" << std::endl;
      abort();
    }
    std::this_thread::sleep_for(50ms);
  }
}

std::optional<int32_t> read_cell_value(scoped_actor& self, const actor& cell) {
  std::optional<int32_t> result;
  self->request(cell, 5s, get_atom_v)
    .receive([&result](int32_t value) { result = value; },
             [](error& err) {
               std::cout << "error: " << to_string(err) << '\n';
             });
  return result;
}

int cell_tests(actor_system& sys, const actor& cell) {
  scoped_actor self{sys};
  self->monitor(cell);
  if (auto res = read_cell_value(self, cell)) {
    std::cout << "cell value 1: " << *res << std::endl;
    self->send(cell, put_atom_v, *res + 7);
  } else {
    return EXIT_FAILURE;
  }
  if (auto res = read_cell_value(self, cell)) {
    std::cout << "cell value 2: " << *res << std::endl;
  } else {
    return EXIT_FAILURE;
  }
  self->send_exit(cell, exit_reason::user_shutdown);
  self->receive([&](const down_msg&) { std::cout << "cell down\n"; },
                after(5s) >> [&] { std::cout << "timeout\n"; });
  return EXIT_SUCCESS;
}

void purge_cache(actor_system& sys, const std::string& host, uint16_t port) {
  auto mm_hdl = actor_cast<actor>(sys.middleman().actor_handle());
  anon_send(mm_hdl, delete_atom_v, host, port);
}

int client(actor_system& sys, std::string_view mode, const std::string& host,
           uint16_t port) {
  auto& mm = sys.middleman();
  if (mode == "remote_actor") {
    auto cell = with_retry([&] { return mm.remote_actor(host, port); });
    auto cell2 = mm.remote_actor(host, port);
    if (!cell2 || *cell != *cell2) {
      std::cout << "calling remote_actor twice must return the same handle\n";
      return EXIT_FAILURE;
    }
    return cell_tests(sys, *cell);
  }
  if (mode == "remote_spawn") {
    auto nid = with_retry([&] { return mm.connect(host, port); });
    auto nid2 = mm.connect(host, port); // with cache
    if (!nid2 || *nid != *nid2) {
      std::cout << "calling connect twice must return the same node ID\n";
      return EXIT_FAILURE;
    }
    purge_cache(sys, host, port);
    auto nid3 = mm.connect(host, port); // without cache
    if (!nid3 || *nid != *nid3) {
      std::cout << "calling connect twice must return the same node ID\n";
      return EXIT_FAILURE;
    }
    auto cell = mm.remote_spawn<actor>(*nid, "cell", make_message(int32_t{7}),
                                       5s);
    if (!cell) {
      std::cout << "remote spawn failed: " << to_string(cell.error()) << "\n";
      return EXIT_FAILURE;
    }
    return cell_tests(sys, *cell);
  }
  if (mode == "remote_lookup") {
    auto nid = with_retry([&] { return mm.connect(host, port); });
    auto cell = mm.remote_lookup("cell", *nid);
    if (!cell) {
      std::cout << "remote_lookup failed\n";
      return EXIT_FAILURE;
    }
    return cell_tests(sys, actor_cast<actor>(cell));
  }
  if (mode == "unpublish") {
    auto ctrl = with_retry([&] { return mm.remote_actor(host, port); });
    bool unpublished = false;
    scoped_actor self{sys};
    self->request(*ctrl, 5s, ok_atom_v)
      .receive([&unpublished] { unpublished = true; },
               [](const error& reason) {
                 std::cout << "failed to unpublish: " << to_string(reason)
                           << "\n";
               });
    if (!unpublished) {
      return EXIT_FAILURE;
    }
    with_retry([&] {
      purge_cache(sys, host, port);
      return !mm.remote_actor(host, port);
    });
    std::cout << "unpublish success\n";
    return EXIT_SUCCESS;
  }
  if (mode == "monitor_node") {
    auto ctrl = with_retry([&] { return mm.remote_actor(host, port); });
    scoped_actor self{sys};
    self->monitor(ctrl->node());
    self->send_exit(*ctrl, exit_reason::kill);
    self->receive([](const node_down_msg&) { std::cout << "server down\n"; },
                  after(5s) >> [&] { std::cout << "timeout\n"; });
    return EXIT_SUCCESS;
  }
  if (mode == "deserialization_error") {
    auto ctrl = with_retry([&] { return mm.remote_actor(host, port); });
    caf::scoped_actor self{sys};
    self->request(*ctrl, 5s, non_deserializable_t{})
      .receive([] { std::cout << "server accepted the message?\n"; },
               [](const error& reason) {
                 std::cout << "error: " << to_string(reason) << "\n";
               });
    return EXIT_SUCCESS;
  }
  if (mode == "pong") {
    auto cell = with_retry([&] { return mm.remote_actor(host, port); });
    auto pong = sys.spawn([](caf::event_based_actor* self) {
      return caf::behavior{
        [self](caf::ping_atom) {
          self->quit();
          return caf::pong_atom_v;
        },
      };
    });
    scoped_actor self{sys};
    self->send(*cell, caf::put_atom_v, pong);
    self->wait_for(pong);
    return EXIT_SUCCESS;
  }
  if (mode == "ping") {
    auto cell = with_retry([&] { return mm.remote_actor(host, port); });
    auto ping = sys.spawn([reg = *cell](caf::event_based_actor* self) {
      self->set_down_handler([self](const caf::down_msg&) {
        std::cout << "pong down\n";
        self->quit();
      });
      // Waiting 50ms here gives the pong process a bit of time, but also makes
      // sure that we trigger at least one BASP heartbeat message in the
      // meantime to have coverage on the heartbeat logic as well.
      self->delayed_send(reg, 50ms, caf::get_atom_v);
      return caf::behavior{
        [self, reg](const actor& pong) {
          if (!pong) {
            self->delayed_send(reg, 50ms, caf::get_atom_v);
            return;
          }
          self->monitor(pong);
          self->send(pong, caf::ping_atom_v);
        },
        [](caf::pong_atom) { std::cout << "got pong" << std::endl; },
      };
    });
    return EXIT_SUCCESS;
  }
  std::cout << "unknown mode: " << mode << '\n';
  return EXIT_FAILURE;
}

int caf_main(actor_system& sys, const config& cfg) {
  if (cfg.server)
    return server(sys, cfg.mode, cfg.port);
  else
    return client(sys, cfg.mode, cfg.host, cfg.port);
}

CAF_MAIN(caf::id_block::io_test, caf::io::middleman)
