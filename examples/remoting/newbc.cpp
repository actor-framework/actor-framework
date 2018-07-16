#include <mutex>
#include <thread>
#include <iostream>
#include <condition_variable>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "caf/io/network/newb.hpp"
#include "caf/io/network/default_multiplexer.hpp"

using namespace caf;

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
  io::network::default_multiplexer mpx{&system};
//  std::thread t([&]() {
//    std::cout << "starting multiplexer" << std::endl;
//    mpx.run();
//  });
  auto backend_supervisor = mpx.make_supervisor();
  std::thread t;
  // The only backend that returns a `nullptr` by default is the
  // `test_multiplexer` which does not have its own thread but uses the main
  // thread instead. Other backends can set `middleman_detach_multiplexer` to
  // false to suppress creation of the supervisor.
  if (backend_supervisor != nullptr) {
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
  }

  actor_config cfg{&mpx};
//  io::network::newb n{cfg, mpx, -1};
//  n.eq_impl(make_message_id(message_priority::normal), nullptr, nullptr, 1);
//  auto n = make_actor<io::network::newb>(system.next_actor_id(), system.node(),+
//                                         &system, cfg, mpx, -1);
  auto res = system.spawn_impl<io::network::newb, hidden>(cfg, mpx, -1);
//  auto m = caf::io::make_middleman_actor(system, res);
  auto n = actor_cast<actor>(res);
  anon_send(n, 1);
  t.join();
}

} // namespace <anonymous>

CAF_MAIN(io::middleman)
