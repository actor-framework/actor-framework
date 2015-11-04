#include <iostream>
#include <chrono>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using std::cout;
using std::cerr;
using std::endl;

using namespace caf;
using namespace caf::io;

using tick_atom = atom_constant<atom("tick")>;

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
  self->delayed_send(self, std::chrono::seconds(1), tick_atom::value);
  return {
    [=](const new_connection_msg& ncm) {
      auto worker = self->fork(connection_worker, ncm.handle);
      self->monitor(worker);
      self->link_to(worker);
    },
    [=](const down_msg&) {
      ++*counter;
    },
    [=](tick_atom) {
      aout(self) << "Finished " << *counter << " requests per second." << endl;
      *counter = 0;
      self->delayed_send(self, std::chrono::seconds(1), tick_atom::value);
    },
    others >> [=] {
      aout(self) << "unexpected: " << to_string(self->current_message()) << endl;
    }
  };
}

maybe<uint16_t> as_u16(const std::string& str) {
  return static_cast<uint16_t>(stoul(str));
}

int main(int argc, const char** argv) {
  uint16_t port = 0;
  auto res = message_builder(argv + 1, argv + argc).extract_opts({
    {"port,p", "set port", port},
  });
  if (! res.error.empty()) {
    cerr << res.error << endl;
    return 1;
  }
  if (res.opts.count("help") > 0) {
    cout << res.helptext << endl;
    return 0;
  }
  if (! res.remainder.empty()) {
    // not all CLI arguments could be consumed
    cerr << "*** too many arguments" << endl << res.helptext << endl;
    return 1;
  }
  if (res.opts.count("port") == 0) {
    cerr << "*** no port given" << endl << res.helptext << endl;
    return 1;
  }
  cout << "*** run in server mode listen on: " << port << endl;
  cout << "*** to quit the program, simply press <enter>" << endl;
  auto server_actor = spawn_io_server(server, port);
  // wait for any input
  std::string dummy;
  std::getline(std::cin, dummy);
  // kill server
  anon_send_exit(server_actor, exit_reason::user_shutdown);
  await_all_actors_done();
  shutdown();
}
