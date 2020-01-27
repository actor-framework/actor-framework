/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE io_basp_worker

#include "caf/io/basp/worker.hpp"

#include "caf/test/dsl.hpp"

#include "caf/actor_cast.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/actor_system.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/io/basp/message_queue.hpp"
#include "caf/make_actor.hpp"
#include "caf/proxy_registry.hpp"

using namespace caf;

namespace {

behavior testee_impl() {
  return {[](ok_atom) {
    // nop
  }};
}

class mock_actor_proxy : public actor_proxy {
public:
  explicit mock_actor_proxy(actor_config& cfg) : actor_proxy(cfg) {
    // nop
  }

  void enqueue(mailbox_element_ptr, execution_unit*) override {
    CAF_FAIL("mock_actor_proxy::enqueue called");
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

struct fixture : test_coordinator_fixture<> {
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
  }

  ~fixture() {
    sys.registry().erase(testee.id());
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(worker_tests, fixture)

CAF_TEST(deliver serialized message) {
  CAF_MESSAGE("create the BASP worker");
  CAF_REQUIRE_EQUAL(hub.peek(), nullptr);
  hub.add_new_worker(queue, proxies);
  CAF_REQUIRE_NOT_EQUAL(hub.peek(), nullptr);
  auto w = hub.pop();
  CAF_MESSAGE("create a fake message + BASP header");
  byte_buffer payload;
  std::vector<strong_actor_ptr> stages;
  binary_serializer sink{sys, payload};
  if (auto err = sink(stages, make_message(ok_atom_v)))
    CAF_FAIL("unable to serialize message: " << sys.render(err));
  io::basp::header hdr{io::basp::message_type::direct_message,
                       0,
                       static_cast<uint32_t>(payload.size()),
                       make_message_id().integer_value(),
                       42,
                       testee.id()};
  CAF_MESSAGE("launch worker");
  w->launch(last_hop, hdr, payload);
  sched.run_once();
  expect((ok_atom), from(_).to(testee));
}

CAF_TEST_FIXTURE_SCOPE_END()
