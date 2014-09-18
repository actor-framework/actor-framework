#include <iostream>

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

actor_ostream out(broker* self, const char* role) {
  return aout(self) << "[" << role << ":" << self->id() << "] ";
}

actor_ostream wout(broker* self) {
  return out(self, "worker");
}

actor_ostream sout(broker* self) {
  return out(self, "server");
}

behavior connection_worker(broker* self, connection_handle hdl) {
  self->configure_read(hdl, receive_policy::at_most(1024));
  return {
    [=](const new_data_msg& msg) {
      wout(self) << "received HTTP request, send http_ok" << endl;
      self->write(msg.handle, cstr_size(http_ok), http_ok);
      self->quit();
    },
    [=](const connection_closed_msg&) {
      wout(self) << "connection closed by remote host, quit" << endl;
      self->quit();
    }
  };
}

behavior server(broker* self) {
  sout(self) << "running" << endl;
  return {
    [=](const new_connection_msg& ncm) {
      auto worker = self->fork(connection_worker, ncm.handle);
      self->monitor(worker);
      self->link_to(worker);
      sout(self) << "forked connection, handle ID: " << ncm.handle.id()
                 << ", worker ID: " << worker.id() << endl;
    },
    [=](const down_msg& dm) {
      sout(self) << "worker with ID " << dm.source.id() << " is done" << endl;
    },
    others() >> [=] {
      sout(self) << "unexpected: " << to_string(self->last_dequeued()) << endl;
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
