// Modes:
// - remote_actor: publish a cell actor at the server and wait for a connection
// - unpublish: publish a controller actor at the server, then have the client
//   trigger an unpublish operation and check that the server is no longer
//   reachable

#include <caf/io/middleman.hpp>

#include <caf/openssl/manager.hpp>
#include <caf/openssl/publish.hpp>
#include <caf/openssl/remote_actor.hpp>
#include <caf/openssl/unpublish.hpp>

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

CAF_BEGIN_TYPE_ID_BLOCK(openssl_test, caf::first_custom_type_id)

  CAF_ADD_TYPE_ID(openssl_test, (non_deserializable_t))

CAF_END_TYPE_ID_BLOCK(openssl_test)

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
      if (auto ok = caf::openssl::unpublish(actor{self}); !ok)
        return std::move(ok.error());
      return {};
    },
  };
}

struct config : actor_system_config {
  bool server = false;
  std::string host = "localhost";
  std::string mode;
  std::string path;
  uint16_t port = 0;
  config() {
    add_actor_type("cell", cell_impl);
    opt_group{custom_options_, "global"}
      .add(server, "server,s", "run in server mode")
      .add(mode, "mode,m", "set the test mode (what to test)")
      .add(host, "host,H", "set host (ignored in server mode)")
      .add(port, "port,p", "set port");
  }
};

int server(actor_system& sys, std::string_view mode, uint16_t port) {
  if (mode == "remote_actor") {
    auto cell = sys.spawn(cell_impl, 42);
    auto actual_port = caf::openssl::publish(cell, port);
    if (!actual_port) {
      std::cout << "failed to open port " << port << ": "
                << to_string(actual_port.error()) << '\n';
      return EXIT_FAILURE;
    }
    std::cout << "running on port " << *actual_port << '\n';
    return EXIT_SUCCESS;
  }
  if (mode == "unpublish") {
    auto actual_port = caf::openssl::publish(sys.spawn(controller_impl), port);
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
    auto res = fn();
    if (res)
      return res;
    total_delay += 50ms;
    if (total_delay > 2s) {
      std::cout << "failed to connect: " << to_string(res.error()) << std::endl;
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
  if (mode == "remote_actor") {
    auto cell = with_retry([&] { //
      return caf::openssl::remote_actor(sys, host, port);
    });
    auto cell2 = caf::openssl::remote_actor(sys, host, port);
    if (!cell2 || *cell != *cell2) {
      std::cout << "calling remote_actor twice must return the same handle\n";
      return EXIT_FAILURE;
    }
    return cell_tests(sys, *cell);
  }
  if (mode == "unpublish") {
    auto ctrl = with_retry([&] { //
      return caf::openssl::remote_actor(sys, host, port);
    });
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
      auto res = caf::openssl::remote_actor(sys, host, port);
      if (!res) {
        res = caf::actor{};
      } else {
        res = make_error(sec::runtime_error, "expected error");
      }
      return res;
    });
    std::cout << "unpublish success\n";
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

CAF_MAIN(caf::id_block::openssl_test, caf::io::middleman, caf::openssl::manager)
