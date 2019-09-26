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

#define CAF_SUITE endpoint_manager

#include "caf/net/endpoint_manager.hpp"

#include "caf/test/dsl.hpp"

#include "host_fixture.hpp"

#include "caf/byte.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/make_actor.hpp"
#include "caf/net/actor_proxy_impl.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/serializer_impl.hpp"
#include "caf/span.hpp"

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
  static expected<std::vector<byte>> serialize(actor_system& sys,
                                               const type_erased_tuple& x) {
    std::vector<byte> result;
    serializer_impl<std::vector<byte>> sink{sys, result};
    if (auto err = message::save(sink, x))
      return err;
    return result;
  }
};

class dummy_transport {
public:
  using application_type = dummy_application;

  dummy_transport(stream_socket handle, std::shared_ptr<std::vector<byte>> data)
    : handle_(handle), data_(data), read_buf_(1024) {
    // nop
  }

  stream_socket handle() {
    return handle_;
  }

  template <class Manager>
  error init(Manager& manager) {
    auto test_bytes = as_bytes(make_span(hello_test));
    buf_.insert(buf_.end(), test_bytes.begin(), test_bytes.end());
    CAF_CHECK(manager.mask_add(operation::read_write));
    return none;
  }

  template <class Manager>
  bool handle_read_event(Manager&) {
    auto res = read(handle_, make_span(read_buf_));
    if (auto num_bytes = get_if<size_t>(&res)) {
      data_->insert(data_->end(), read_buf_.begin(),
                    read_buf_.begin() + *num_bytes);
      return true;
    }
    return get<sec>(res) == sec::unavailable_or_would_block;
  }

  template <class Manager>
  bool handle_write_event(Manager& mgr) {
    for (auto x = mgr.next_message(); x != nullptr; x = mgr.next_message()) {
      auto& payload = x->payload;
      buf_.insert(buf_.end(), payload.begin(), payload.end());
    }
    auto res = write(handle_, make_span(buf_));
    if (auto num_bytes = get_if<size_t>(&res)) {
      buf_.erase(buf_.begin(), buf_.begin() + *num_bytes);
      return buf_.size() > 0;
    }
    return get<sec>(res) == sec::unavailable_or_would_block;
  }

  void handle_error(sec) {
    // nop
  }

  template <class Manager>
  void resolve(Manager& mgr, const uri& locator, const actor& listener) {
    actor_id aid = 42;
    auto hid = "0011223344556677889900112233445566778899";
    auto nid = unbox(make_node_id(42, hid));
    actor_config cfg;
    auto p = make_actor<actor_proxy_impl, strong_actor_ptr>(aid, nid,
                                                            &mgr.system(), cfg,
                                                            &mgr);
    std::string path{locator.path().begin(), locator.path().end()};
    anon_send(listener, resolve_atom::value, std::move(path), p);
  }

  template <class Manager>
  void timeout(Manager&, atom_value, uint64_t) {
    // nop
  }

  template <class Parent>
  void new_proxy(Parent&, const node_id&, actor_id) {
    // nop
  }

  template <class Parent>
  void local_actor_down(Parent&, const node_id&, actor_id, error) {
    // nop
  }

private:
  stream_socket handle_;

  std::shared_ptr<std::vector<byte>> data_;

  std::vector<byte> read_buf_;

  std::vector<byte> buf_;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(endpoint_manager_tests, fixture)

CAF_TEST(send and receive) {
  std::vector<byte> read_buf(1024);
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 1u);
  auto buf = std::make_shared<std::vector<byte>>();
  auto sockets = unbox(make_stream_socket_pair());
  nonblocking(sockets.second, true);
  CAF_CHECK_EQUAL(read(sockets.second, make_span(read_buf)),
                  sec::unavailable_or_would_block);
  auto guard = detail::make_scope_guard([&] { close(sockets.second); });
  auto mgr = make_endpoint_manager(mpx, sys,
                                   dummy_transport{sockets.first, buf});
  CAF_CHECK_EQUAL(mgr->init(), none);
  mpx->handle_updates();
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 2u);
  CAF_CHECK_EQUAL(write(sockets.second, as_bytes(make_span(hello_manager))),
                  hello_manager.size());
  run();
  CAF_CHECK_EQUAL(string_view(reinterpret_cast<char*>(buf->data()),
                              buf->size()),
                  hello_manager);
  CAF_CHECK_EQUAL(read(sockets.second, make_span(read_buf)), hello_test.size());
  CAF_CHECK_EQUAL(string_view(reinterpret_cast<char*>(read_buf.data()),
                              hello_test.size()),
                  hello_test);
}

CAF_TEST(resolve and proxy communication) {
  std::vector<byte> read_buf(1024);
  auto buf = std::make_shared<std::vector<byte>>();
  auto sockets = unbox(make_stream_socket_pair());
  nonblocking(sockets.second, true);
  auto guard = detail::make_scope_guard([&] { close(sockets.second); });
  auto mgr = make_endpoint_manager(mpx, sys,
                                   dummy_transport{sockets.first, buf});
  CAF_CHECK_EQUAL(mgr->init(), none);
  mpx->handle_updates();
  run();
  CAF_CHECK_EQUAL(read(sockets.second, make_span(read_buf)), hello_test.size());
  mgr->resolve(unbox(make_uri("test:id/42")), self);
  run();
  self->receive(
    [&](resolve_atom, const std::string&, const strong_actor_ptr& p) {
      CAF_MESSAGE("got a proxy, send a message to it");
      self->send(actor_cast<actor>(p), "hello proxy!");
    },
    after(std::chrono::seconds(0)) >>
      [&] { CAF_FAIL("manager did not respond with a proxy."); });
  run();
  auto read_res = read(sockets.second, make_span(read_buf));
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
