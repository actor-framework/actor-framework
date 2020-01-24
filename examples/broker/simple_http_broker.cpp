#include <iostream>
#include <chrono>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using std::cout;
using std::cerr;
using std::endl;

using namespace caf;
using namespace caf::io;

namespace {

constexpr const char http_ok[] = R"__(HTTP/1.1 200 OK
Content-Type: text/plain
Connection: keep-alive
Transfer-Encoding: chunked

d
Hi there! :)

0


)__";

template <size_t Size>
constexpr size_t cstr_size(const char (&)[Size]) {
  return Size;
}

behavior connection_worker(broker* self, connection_handle hdl) {
  self->configure_read(hdl, receive_policy::at_most(1024));
  return {
    [=](const new_data_msg& msg) {
      self->write(msg.handle, cstr_size(http_ok), http_ok);
      self->quit();
    },
    [=](const connection_closed_msg&) {
      self->quit();
    }
  };
}

behavior server(broker* self) {
  auto counter = std::make_shared<int>(0);
  self->set_down_handler([=](down_msg&) {
    ++*counter;
  });
  self->delayed_send(self, std::chrono::seconds(1), tick_atom_v);
  return {
    [=](const new_connection_msg& ncm) {
      auto worker = self->fork(connection_worker, ncm.handle);
      self->monitor(worker);
      self->link_to(worker);
    },
    [=](tick_atom) {
      aout(self) << "Finished " << *counter << " requests per second." << endl;
      *counter = 0;
      self->delayed_send(self, std::chrono::seconds(1), tick_atom_v);
    }
  };
}

class config : public actor_system_config {
public:
  uint16_t port = 0;

  config() {
    opt_group{custom_options_, "global"}
    .add(port, "port,p", "set port");
  }
};

void caf_main(actor_system& system, const config& cfg) {
  auto server_actor = system.middleman().spawn_server(server, cfg.port);
  if (!server_actor) {
    cerr << "*** cannot spawn server: "
         << system.render(server_actor.error()) << endl;
    return;
  }
  cout << "*** listening on port " << cfg.port << endl;
  cout << "*** to quit the program, simply press <enter>" << endl;
  // wait for any input
  std::string dummy;
  std::getline(std::cin, dummy);
  // kill server
  anon_send_exit(*server_actor, exit_reason::user_shutdown);
}

} // namespace

CAF_MAIN(io::middleman)
