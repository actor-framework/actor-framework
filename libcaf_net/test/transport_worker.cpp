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

#define CAF_SUITE transport_worker

#include "caf/net/transport_worker.hpp"

#include "caf/test/dsl.hpp"

#include "host_fixture.hpp"

#include "caf/byte.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/make_actor.hpp"
#include "caf/net/actor_proxy_impl.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/serializer_impl.hpp"
#include "caf/span.hpp"

using namespace caf;
using namespace caf::net;

namespace {

constexpr string_view hello_test = "hello test!";

struct application_result {
  bool initialized;
  std::vector<byte> data_buffer;
  std::string resolve_path;
  actor resolve_listener;
  atom_value timeout_value;
  uint64_t timeout_id;
  sec err;
};

struct transport_result {
  std::vector<byte> packet_buffer;
  ip_endpoint ep;
};

class dummy_application {
public:
  dummy_application(std::shared_ptr<application_result> res)
    : res_(std::move(res)){
      // nop
    };

  ~dummy_application() = default;

  template <class Parent>
  error init(Parent&) {
    res_->initialized = true;
    return none;
  }

  template <class Transport>
  void write_message(Transport& transport,
                     std::unique_ptr<endpoint_manager::message> msg) {
    transport.write_packet(span<byte>{}, msg->payload);
  }

  template <class Parent>
  void handle_data(Parent&, span<const byte> data) {
    auto& buf = res_->data_buffer;
    buf.clear();
    buf.insert(buf.begin(), data.begin(), data.end());
  }

  template <class Parent>
  void resolve(Parent&, const std::string& path, actor listener) {
    res_->resolve_path = path;
    res_->resolve_listener = std::move(listener);
  }

  template <class Parent>
  void timeout(Parent&, atom_value value, uint64_t id) {
    res_->timeout_value = value;
    res_->timeout_id = id;
  }

  void handle_error(sec err) {
    res_->err = err;
  }

  static expected<std::vector<byte>> serialize(actor_system& sys,
                                               const type_erased_tuple& x) {
    std::vector<byte> result;
    serializer_impl<std::vector<byte>> sink{sys, result};
    if (auto err = message::save(sink, x))
      return err;
    return result;
  }

private:
  std::shared_ptr<application_result> res_;
};

class dummy_transport {
public:
  using transport_type = dummy_transport;

  using application_type = dummy_application;

  dummy_transport(std::shared_ptr<transport_result> res) : res_(res) {
    // nop
  }

  void write_packet(span<const byte> header, span<const byte> payload,
                    ip_endpoint ep) {
    auto& buf = res_->packet_buffer;
    buf.insert(buf.begin(), header.begin(), header.end());
    buf.insert(buf.begin(), payload.begin(), payload.end());
    res_->ep = ep;
  }

private:
  std::shared_ptr<transport_result> res_;
};

struct fixture : test_coordinator_fixture<>, host_fixture {
  using worker_type = transport_worker<dummy_application, ip_endpoint>;

  fixture()
    : transport_results{std::make_shared<transport_result>()},
      application_results{std::make_shared<application_result>()},
      transport(transport_results),
      worker{dummy_application{application_results}} {
    mpx = std::make_shared<multiplexer>();
    if (auto err = mpx->init())
      CAF_FAIL("mpx->init failed: " << sys.render(err));
    if (auto err = parse("[::1]:12345", ep))
      CAF_FAIL("parse returned an error: " << err);
    worker = worker_type{dummy_application{application_results}, ep};
  }

  bool handle_io_event() override {
    mpx->handle_updates();
    return mpx->poll_once(false);
  }

  multiplexer_ptr mpx;
  std::shared_ptr<transport_result> transport_results;
  std::shared_ptr<application_result> application_results;
  dummy_transport transport;
  worker_type worker;
  ip_endpoint ep;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(endpoint_manager_tests, fixture)

CAF_TEST(construction and initialization) {
  CAF_CHECK_EQUAL(worker.init(transport), none);
  CAF_CHECK_EQUAL(application_results->initialized, true);
}

CAF_TEST(handle_data) {
  auto test_span = make_span(reinterpret_cast<byte*>(
                               const_cast<char*>(hello_test.data())),
                             hello_test.size());
  worker.handle_data(transport, test_span);
  auto& buf = application_results->data_buffer;
  string_view result{reinterpret_cast<char*>(buf.data()), buf.size()};
  CAF_CHECK_EQUAL(result, hello_test);
}

CAF_TEST(write_message) {
  actor act;
  auto strong_actor = actor_cast<strong_actor_ptr>(act);
  mailbox_element::forwarding_stack stack;
  auto msg = make_message();
  auto elem = make_mailbox_element(strong_actor, make_message_id(12345), stack,
                                   msg);
  auto test_span = make_span(reinterpret_cast<byte*>(
                               const_cast<char*>(hello_test.data())),
                             hello_test.size());
  std::vector<byte> payload(test_span.begin(), test_span.end());
  auto message = detail::make_unique<endpoint_manager::message>(std::move(elem),
                                                                payload);
  worker.write_message(transport, std::move(message));
  auto& buf = transport_results->packet_buffer;
  string_view result{reinterpret_cast<char*>(buf.data()), buf.size()};
  CAF_CHECK_EQUAL(result, hello_test);
  CAF_CHECK_EQUAL(transport_results->ep, ep);
}

CAF_TEST(resolve) {
  worker.resolve(transport, "foo", self);
  CAF_CHECK_EQUAL(application_results->resolve_path, "foo");
  CAF_CHECK_EQUAL(application_results->resolve_listener, self);
}

CAF_TEST(timeout) {
  worker.timeout(transport, atom("bar"), 42u);
  CAF_CHECK_EQUAL(application_results->timeout_value, atom("bar"));
  CAF_CHECK_EQUAL(application_results->timeout_id, 42u);
}

CAF_TEST(handle_error) {
  worker.handle_error(sec::feature_disabled);
  CAF_CHECK_EQUAL(application_results->err, sec::feature_disabled);
}

CAF_TEST_FIXTURE_SCOPE_END()
