#include <mutex>
#include <thread>
#include <iostream>
#include <condition_variable>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "caf/io/network/newb.hpp"
#include "caf/io/network/default_multiplexer.hpp"

using namespace caf;
using namespace caf::io::network;

namespace {

class config : public actor_system_config {
public:
  uint16_t port = 0;
  std::string host = "localhost";
  bool server_mode = false;

  config() {
    opt_group{custom_options_, "global"}
    .add(port, "port,p", "set port")
    .add(host, "host,H", "set host (ignored in server mode)")
    .add(server_mode, "server-mode,s", "enable server mode");
  }
};

void caf_main(actor_system& system, const config&) {
  default_multiplexer mpx{&system};
  // Setup thread to run the multiplexer.
  std::thread t;
  std::atomic<bool> init_done{false};
  std::mutex mtx;
  std::condition_variable cv;
  t = std::thread{[&] {
    system.thread_started();
    std::cout << "starting multiplexer" << std::endl;
    {
      std::unique_lock<std::mutex> guard{mtx};
      mpx.thread_id(std::this_thread::get_id());
      init_done = true;
      cv.notify_one();
    }
    mpx.run();
    system.thread_terminates();
  }};
  std::unique_lock<std::mutex> guard{mtx};
  while (init_done == false)
    cv.wait(guard);
  // Create an event handling actor to run in the multiplexer.
  actor_config cfg{&mpx};
  auto n = make_newb<detail::protocol_policy,
                     detail::mutating_policy>(system, cfg, mpx, -1);
  anon_send(n, 1);
  t.join();
}

} // namespace <anonymous>

CAF_MAIN(io::middleman)
