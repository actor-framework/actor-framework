// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE io_basp_worker

#include "caf/io/basp/worker.hpp"

#include "io-test.hpp"

#include "caf/actor_cast.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/actor_system.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/io/basp/message_queue.hpp"
#include "caf/io/network/test_multiplexer.hpp"
#include "caf/make_actor.hpp"
#include "caf/proxy_registry.hpp"

using namespace caf;

namespace {

behavior testee_impl() {
  return {
    [](ok_atom) {
      // nop
    },
  };
}

struct config : actor_system_config {
  config() {
    test_coordinator_fixture<>::init_config(*this);
    load<io::middleman>();
  }
};

class mock_actor_proxy : public actor_proxy {
public:
  explicit mock_actor_proxy(actor_config& cfg) : actor_proxy(cfg) {
    // nop
  }

  bool enqueue(mailbox_element_ptr, execution_unit*) override {
    CAF_FAIL("mock_actor_proxy::enqueue called");
    return false;
  }

  void kill_proxy(execution_unit*, error) override {
    // nop
  }
};

class mock_proxy_registry_backend : public proxy_registry::backend {
public:
  mock_proxy_registry_backend(actor_system& sys) : sys_(sys) {
    // nop
  }

  strong_actor_ptr make_proxy(node_id nid, actor_id aid) override {
    actor_config cfg;
    return make_actor<mock_actor_proxy, strong_actor_ptr>(aid, nid, &sys_, cfg);
  }

  void set_last_hop(node_id*) override {
    // nop
  }

private:
  actor_system& sys_;
};

struct fixture : test_coordinator_fixture<config> {
  detail::worker_hub<io::basp::worker> hub;
  io::basp::message_queue queue;
  mock_proxy_registry_backend proxies_backend;
  proxy_registry proxies;
  node_id last_hop;
  actor testee;

  fixture() : proxies_backend(sys), proxies(sys, proxies_backend) {
    auto tmp = make_node_id(123, "0011223344556677889900112233445566778899");
    last_hop = unbox(std::move(tmp));
    testee = sys.spawn<lazy_init>(testee_impl);
    sys.registry().put(testee.id(), testee);
    run();
  }

  ~fixture() {
    sys.registry().erase(testee.id());
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(deliver serialized message) {
  MESSAGE("create the BASP worker");
  CAF_REQUIRE_EQUAL(hub.peek(), nullptr);
  hub.add_new_worker(queue, proxies);
  CAF_REQUIRE_NOT_EQUAL(hub.peek(), nullptr);
  auto w = hub.pop();
  MESSAGE("create a fake message + BASP header");
  byte_buffer payload;
  std::vector<strong_actor_ptr> stages;
  binary_serializer sink{sys, payload};
  auto msg = make_message(ok_atom_v);
  if (!sink.apply(stages) || !sink.apply(msg))
    CAF_FAIL("unable to serialize message: " << sink.get_error());
  io::basp::header hdr{io::basp::message_type::direct_message,
                       0,
                       static_cast<uint32_t>(payload.size()),
                       make_message_id().integer_value(),
                       42,
                       testee.id()};
  MESSAGE("launch worker");
  w->launch(last_hop, hdr, payload);
  sched.run_once();
  expect((ok_atom), from(_).to(testee));
}

END_FIXTURE_SCOPE()
