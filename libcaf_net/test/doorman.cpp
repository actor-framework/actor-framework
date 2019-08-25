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

#define CAF_SUITE doorman

#include "caf/policy/doorman.hpp"

#include "caf/net/endpoint_manager.hpp"
#include "caf/net/ip.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/uri.hpp"

#include "caf/test/dsl.hpp"

#include "host_fixture.hpp"

using namespace caf;
using namespace caf::net;

namespace {

struct fixture : test_coordinator_fixture<>, host_fixture {
  fixture() {
    mpx = std::make_shared<multiplexer>();
    if (auto err = mpx->init())
      CAF_FAIL("mpx->init failed: " << sys.render(err));
    auth.port = 0;
    auth.host = std::string{"0.0.0.0"};
  }

  bool handle_io_event() override {
    mpx->handle_updates();
    return mpx->poll_once(false);
  }

  multiplexer_ptr mpx;
  uri::authority_type auth;
};

class dummy_application {
public:
  static expected<std::vector<char>> serialize(actor_system& sys,
                                               const type_erased_tuple& x) {
    std::vector<char> result;
    binary_serializer sink{sys, result};
    if (auto err = message::save(sink, x))
      return err;
    return result;
  }

  template <class Transport>
  error init(Transport&) {
    return none;
  }

  template <class Transport>
  void write_message(Transport& transport,
                     std::unique_ptr<endpoint_manager::message> msg) {
    transport.write_packet(msg->payload);
  }

  template <class Parent>
  void handle_data(Parent&, span<char>) {
    // nop
  }

  template <class Transport>
  void resolve(Transport&, std::string path, actor listener) {
    anon_send(listener, resolve_atom::value,
              "the resolved path is still " + path);
  }

  template <class Transport>
  void timeout(Transport&, atom_value, uint64_t) {
    // nop
  }

  void handle_error(sec) {
    // nop
  }
};

class dummy_application_factory {
public:
  static expected<std::vector<char>> serialize(actor_system& sys,
                                               const type_erased_tuple& x) {
    return dummy_application::serialize(sys, x);
  }

  template <class Parent>
  error init(Parent&) {
    return none;
  }

  dummy_application make() const {
    return dummy_application{};
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(doorman_tests, fixture)

CAF_TEST(tcp connect) {
  auto acceptor = unbox(make_accept_socket(auth, false));
  auto port = unbox(local_port(socket_cast<network_socket>(acceptor)));
  CAF_MESSAGE("opened acceptor on port " << port);
  uri::authority_type dst;
  dst.port = port;
  dst.host = std::string{"localhost"};
  auto con_fd = unbox(make_connected_socket(dst));
  auto acc_fd = unbox(accept(acceptor));
  CAF_MESSAGE("accepted connection");
  close(con_fd);
  close(acc_fd);
  close(acceptor);
}

CAF_TEST(doorman accept) {
  auto acceptor = unbox(make_accept_socket(auth, false));
  auto port = unbox(local_port(socket_cast<network_socket>(acceptor)));
  CAF_MESSAGE("opened acceptor on port " << port);
  auto mgr = make_endpoint_manager(mpx, sys, policy::doorman{acceptor},
                                   dummy_application_factory{});
  CAF_CHECK_EQUAL(mgr->init(), none);
  handle_io_event();
  auto before = mpx->num_socket_managers();
  CAF_MESSAGE("connecting to doorman");
  uri::authority_type dst;
  dst.port = port;
  dst.host = std::string{"localhost"};
  auto fd = unbox(make_connected_socket(dst));
  CAF_MESSAGE("waiting for connection");
  while (mpx->num_socket_managers() != before + 1)
    handle_io_event();
  CAF_MESSAGE("connected");
  close(acceptor);
  close(fd);
}

CAF_TEST_FIXTURE_SCOPE_END()
