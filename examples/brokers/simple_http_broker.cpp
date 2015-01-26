#include <iostream>
#include <chrono>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using std::cout;
using std::cerr;
using std::endl;

using namespace caf;
using namespace caf::io;

const char http_ok[] = R"__(HTTP/1.1 200 OK
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
  self->delayed_send(self, std::chrono::seconds(1), atom("tick"));
  return {
    [=](const new_connection_msg& ncm) {
      auto worker = self->fork(connection_worker, ncm.handle);
      self->monitor(worker);
      self->link_to(worker);
    },
    [=](const down_msg&) {
      ++*counter;
    },
    on(atom("tick")) >> [=] {
      aout(self) << "Finished " << *counter << " requests per second." << endl;
      *counter = 0;
      self->delayed_send(self, std::chrono::seconds(1), atom("tick"));
    },
    others() >> [=] {
      aout(self) << "unexpected: " << to_string(self->last_dequeued()) << endl;
    }
  };
}

optional<uint16_t> as_u16(const std::string& str) {
  return static_cast<uint16_t>(stoul(str));
}

int main(int argc, const char **argv) {
  message_builder{argv + 1, argv + argc}.apply({
    on("-p", as_u16) >> [&](uint16_t port) {
      cout << "*** run in server mode listen on: " << port << endl;
      cout << "*** to quit the program, simply press <enter>" << endl;
      auto sever_actor = spawn_io_server(server, port);
      // wait for any input
      std::string dummy;
      std::getline(std::cin, dummy);
      // kill server
      anon_send_exit(sever_actor, exit_reason::user_shutdown);
    },
    others() >> [] {
      cerr << "use with '-p PORT' as server on port" << endl;
    }
  });
  await_all_actors_done();
  shutdown();
}
