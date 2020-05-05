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

#define CAF_SUITE net.doorman

#include "caf/net/doorman.hpp"

#include "caf/binary_serializer.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/ip.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/uri.hpp"

#include "caf/test/dsl.hpp"

#include "caf/net/test/host_fixture.hpp"

using namespace caf;
using namespace caf::net;
using namespace std::literals::string_literals;

namespace {

struct fixture : test_coordinator_fixture<>, host_fixture {
  fixture() {
    mpx = std::make_shared<multiplexer>();
    if (auto err = mpx->init())
      CAF_FAIL("mpx->init failed: " << err);
    mpx->set_thread_id();
    CAF_CHECK_EQUAL(mpx->num_socket_managers(), 1u);
    auth.port = 0;
    auth.host = "0.0.0.0"s;
  }

  bool handle_io_event() override {
    return mpx->poll_once(false);
  }

  multiplexer_ptr mpx;
  uri::authority_type auth;
};

class dummy_application {
public:
  static expected<std::vector<byte>> serialize(actor_system& sys,
                                               const message& x) {
    std::vector<byte> result;
    binary_serializer sink{sys, result};
    if (auto err = x.save(sink))
      return err.value();
    return result;
  }

  template <class Parent>
  error init(Parent&) {
    return none;
  }

  template <class Transport>
  void write_message(Transport& transport,
                     std::unique_ptr<endpoint_manager_queue::message> msg) {
    if (auto payload = serialize(transport.system(), msg->msg->payload))
      transport.write_packet(*payload);
    else
      CAF_FAIL("serializing failed: " << payload.error());
  }

  template <class Parent>
  error handle_data(Parent&, span<const byte>) {
    return none;
  }

  template <class Parent>
  void resolve(Parent&, string_view path, const actor& listener) {
    anon_send(listener, resolve_atom_v,
              "the resolved path is still "
                + std::string(path.begin(), path.end()));
  }

  template <class Parent>
  void timeout(Parent&, const std::string&, uint64_t) {
    // nop
  }

  template <class Parent>
  void new_proxy(Parent&, actor_id) {
    // nop
  }

  template <class Parent>
  void local_actor_down(Parent&, actor_id, error) {
    // nop
  }

  void handle_error(sec) {
    // nop
  }
};

class dummy_application_factory {
public:
  using application_type = dummy_application;

  static expected<std::vector<byte>> serialize(actor_system& sys,
                                               const message& x) {
    return dummy_application::serialize(sys, x);
  }

  template <class Parent>
  error init(Parent&) {
    return none;
  }

  application_type make() const {
    return dummy_application{};
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(doorman_tests, fixture)

CAF_TEST(doorman accept) {
  auto acceptor = unbox(make_tcp_accept_socket(auth, false));
  auto port = unbox(local_port(socket_cast<network_socket>(acceptor)));
  auto acceptor_guard = make_socket_guard(acceptor);
  CAF_MESSAGE("opened acceptor on port " << port);
  auto mgr = make_endpoint_manager(
    mpx, sys,
    doorman<dummy_application_factory>{acceptor_guard.release(),
                                       dummy_application_factory{}});
  CAF_CHECK_EQUAL(mgr->init(), none);
  auto before = mpx->num_socket_managers();
  CAF_CHECK_EQUAL(before, 2u);
  uri::authority_type dst;
  dst.port = port;
  dst.host = "localhost"s;
  CAF_MESSAGE("connecting to doorman on: " << dst);
  auto conn = make_socket_guard(unbox(make_connected_tcp_stream_socket(dst)));
  CAF_MESSAGE("waiting for connection");
  while (mpx->num_socket_managers() != before + 1)
    run();
  CAF_MESSAGE("connected");
}

CAF_TEST_FIXTURE_SCOPE_END()
