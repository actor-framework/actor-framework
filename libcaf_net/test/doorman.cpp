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

#include "caf/net/endpoint_manager.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/policy/doorman.hpp"

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
  }

  bool handle_io_event() override {
    mpx->handle_updates();
    return mpx->poll_once(false);
  }

  multiplexer_ptr mpx;
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
  bool handle_read_event(Transport&) {
    return false;
  }

  template <class Transport>
  bool handle_write_event(Transport&) {
    return false;
  }

  template <class Transport>
  void resolve(Transport&, std::string path, actor listener) {
    anon_send(listener, resolve_atom::value, "the resolved path is still " + path);
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

  dummy_application make() const {
    return dummy_application{};
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(doorman_tests, fixture)

CAF_TEST(doorman creation) {
  auto acceptor = unbox(make_accept_socket(0, nullptr, false));
  auto mgr = make_endpoint_manager(mpx, sys,
                                   policy::doorman{acceptor},
                                   dummy_application_factory{});
}

CAF_TEST_FIXTURE_SCOPE_END()
