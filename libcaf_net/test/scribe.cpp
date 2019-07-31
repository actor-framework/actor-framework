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

#define CAF_SUITE scribe_policy

#include <caf/net/endpoint_manager_impl.hpp>
#include <caf/net/multiplexer.hpp>
#include <caf/net/make_endpoint_manager.hpp>
#include "caf/policy/scribe_policy.hpp"

#include "caf/test/dsl.hpp"
#include "host_fixture.hpp"

using namespace caf;
using namespace caf::net;
using namespace caf::policy;

namespace {

string_view hello_manager{"hello manager!"};
string_view hello_test{"hello test!"};

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

struct dummy_application {
  dummy_application(std::shared_ptr<std::vector<char>> buf) : buf_(buf) {};

  ~dummy_application() = default;

  template<class Transport>
  void prepare(std::unique_ptr<endpoint_manager::message>,
               Transport&, socket_manager&) {
  }

  template<class Transport>
  void process(std::vector<char> payload, Transport&,
               socket_manager&) {
    buf_->clear();
    buf_->insert(buf_->begin(), payload.begin(), payload.end());
  }

  template<class Transport>
  void resolve(Transport&, const std::string&, actor) {
    // nop
  }

  template<class Transport>
  void timeout(Transport&, atom_value, uint64_t) {
    // nop
  }

  void handle_error(caf::sec) {
    // nop
  }

  static caf::expected<std::vector<char>> serialize(actor_system&,
                                                    const type_erased_tuple&) {
    return std::vector<char>(0);
  }

  std::shared_ptr<std::vector<char>> buf_;
};

CAF_TEST_FIXTURE_SCOPE(scribe_tests, fixture)

CAF_TEST(send) {
  std::vector<char> read_buf(1024);
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 1u);
  auto buf = std::make_shared<std::vector<char>>();
  auto sockets = unbox(make_stream_socket_pair());
  nonblocking(sockets.second, true);
  CAF_CHECK_EQUAL(read(sockets.second, read_buf.data(), read_buf.size()),
                  sec::unavailable_or_would_block);
  auto guard = detail::make_scope_guard([&] { close(sockets.second); });

  CAF_MESSAGE("make new endpoint");
  scribe_policy scribe{sockets.first};
  scribe.configure_read(net::receive_policy::exactly(hello_manager.size()));

  auto mgr = make_endpoint_manager(mpx, sys, scribe,
                                   dummy_application{buf});
  CAF_CHECK_EQUAL(mgr->init(), none);
  mpx->handle_updates();
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 2u);

  CAF_MESSAGE("transfer data from second to first socket");

  CAF_CHECK_EQUAL(write(sockets.second, hello_manager.data(),
                        hello_manager.size()),
                  hello_manager.size());

  CAF_MESSAGE("receive transferred data");
  run();
  CAF_CHECK_EQUAL(string_view(buf->data(), buf->size()), hello_manager);
}

CAF_TEST_FIXTURE_SCOPE_END()

} // namespace