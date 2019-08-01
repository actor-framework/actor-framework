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

#include "caf/net/endpoint_manager.hpp"
#include <caf/policy/scribe.hpp>

#include "caf/test/dsl.hpp"

#include "host_fixture.hpp"

#include "caf/binary_serializer.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/make_actor.hpp"
#include "caf/net/actor_proxy_impl.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/stream_socket.hpp"

using namespace caf;
using namespace caf::net;

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

class dummy_application {
public:
  dummy_application(std::shared_ptr<std::vector<char>> rec_buf)
    : rec_buf_(std::move(rec_buf)) {
    // nop
  };

  ~dummy_application() = default;

  template <class Transport>
  void prepare(std::unique_ptr<endpoint_manager::message> msg, Transport& transport) {
    auto& buf = transport.wr_buf();
    auto& msg_buf = msg->payload;
    buf.resize(buf.size()+msg_buf.size());
    buf.insert(buf.end(), msg_buf.begin(), msg_buf.end());
  }

  template <class Transport>
  void process(std::vector<char> payload, Transport&, socket_manager&) {
    rec_buf_->clear();
    rec_buf_->insert(rec_buf_->begin(), payload.begin(), payload.end());
  }

  template <class Manager>
  void resolve(Manager& manager, const std::string& path, actor listener) {
    actor_id aid = 42;
    node_id nid{42, "00112233445566778899aa00112233445566778899aa"};
    actor_config cfg;
    auto p = make_actor<actor_proxy_impl, strong_actor_ptr>(aid, nid,
                                                            &manager.system(), cfg,
                                                            &manager);
    anon_send(listener, resolve_atom::value, std::move(path), p);
  }

  template <class Transport>
  void timeout(Transport&, atom_value, uint64_t) {
    // nop
  }

  void handle_error(caf::sec) {
    // nop
  }

  static expected<std::vector<char>> serialize(actor_system& sys,
                                               const type_erased_tuple& x) {
    std::vector<char> result;
    binary_serializer sink{sys, result};
    if (auto err = message::save(sink, x))
      return err;
    return result;
  }

private:
  std::shared_ptr<std::vector<char>> rec_buf_;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(endpoint_manager_tests, fixture)
/*
CAF_TEST(receive) {
  std::vector<char> read_buf(1024);
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 1u);
  auto buf = std::make_shared<std::vector<char>>();
  auto sockets = unbox(make_stream_socket_pair());
  nonblocking(sockets.second, true);
  CAF_CHECK_EQUAL(read(sockets.second, read_buf.data(), read_buf.size()),
                  sec::unavailable_or_would_block);
  auto guard = detail::make_scope_guard([&] { close(sockets.second); });

  CAF_MESSAGE("configure scribe_policy");
  policy::scribe scribe{sockets.first};
  scribe.configure_read(net::receive_policy::exactly(hello_manager.size()));
  auto mgr = make_endpoint_manager(mpx, sys,
                                   scribe,
                                   dummy_application{buf});
  CAF_CHECK_EQUAL(mgr->init(), none);
  mpx->handle_updates();
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 2u);

  CAF_MESSAGE("sending data to scribe_policy");
  CAF_CHECK_EQUAL(write(sockets.second, hello_manager.data(),
                        hello_manager.size()),
                  hello_manager.size());
  run();
  CAF_CHECK_EQUAL(string_view(buf->data(), buf->size()), hello_manager);
}*/

CAF_TEST(resolve and proxy communication) {
  std::vector<char> read_buf(1024);
  auto buf = std::make_shared<std::vector<char>>();
  auto sockets = unbox(make_stream_socket_pair());
  nonblocking(sockets.second, true);
  auto guard = detail::make_scope_guard([&] { close(sockets.second); });
  auto mgr = make_endpoint_manager(mpx, sys,
                                   policy::scribe{sockets.first},
                                   dummy_application{buf});
  CAF_CHECK_EQUAL(mgr->init(), none);
  mpx->handle_updates();
  run();
  /*CAF_CHECK_EQUAL(read(sockets.second, read_buf.data(), read_buf.size()),
                  hello_test.size());*/
  mgr->resolve("/id/42", self);
  run();
  self->receive(
    [&](resolve_atom, const std::string&, const strong_actor_ptr& p) {
      CAF_MESSAGE("got a proxy, send a message to it");
      self->send(actor_cast<actor>(p), "hello proxy!");
    },
    after(std::chrono::seconds(0)) >>
                                   [&] { CAF_FAIL("manager did not respond with a proxy."); });
  run();
  auto read_res = read(sockets.second, read_buf.data(), read_buf.size());
  if (!holds_alternative<size_t>(read_res)) {
    CAF_ERROR("read() returned an error: " << sys.render(get<sec>(read_res)));
    return;
  }
  read_buf.resize(get<size_t>(read_res));
  CAF_MESSAGE("receive buffer contains " << read_buf.size() << " bytes");
  message msg;
  binary_deserializer source{sys, read_buf};
  CAF_CHECK_EQUAL(source(msg), none);
  if (msg.match_elements<std::string>())
    CAF_CHECK_EQUAL(msg.get_as<std::string>(0), "hello proxy!");
  else
    CAF_ERROR("expected a string, got: " << to_string(msg));
}

CAF_TEST_FIXTURE_SCOPE_END()
