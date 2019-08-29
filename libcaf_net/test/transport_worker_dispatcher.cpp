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

#define CAF_SUITE transport_worker_dispatcher

#include "caf/net/transport_worker_dispatcher.hpp"

#include "caf/test/dsl.hpp"

#include "host_fixture.hpp"

#include "caf/make_actor.hpp"
#include "caf/monitorable_actor.hpp"
#include "caf/node_id.hpp"
#include "caf/uri.hpp"

using namespace caf;
using namespace caf::net;

namespace {

struct dummy_actor : public monitorable_actor {
  dummy_actor(actor_config& cfg) : monitorable_actor(cfg) {
    // nop
  }

  void enqueue(mailbox_element_ptr, execution_unit*) override {
    // nop
  }
};

class dummy_application {
public:
  dummy_application(std::shared_ptr<std::vector<byte>> rec_buf, uint8_t id)
    : rec_buf_(std::move(rec_buf)),
      id_(id){
        // nop
      };

  ~dummy_application() = default;

  template <class Parent>
  error init(Parent&) {
    return none;
  }

  template <class Transport>
  void write_message(Transport&, std::unique_ptr<endpoint_manager::message>) {
    rec_buf_->push_back(static_cast<byte>(id_));
  }

  template <class Parent>
  void handle_data(Parent&, span<const byte>) {
    rec_buf_->push_back(static_cast<byte>(id_));
  }

  template <class Manager>
  void resolve(Manager&, const std::string&, actor) {
    // nop
  }

  template <class Transport>
  void timeout(Transport&, atom_value, uint64_t) {
    // nop
  }

  void handle_error(sec) {
    // nop
  }

  static expected<std::vector<byte>> serialize(actor_system&,
                                               const type_erased_tuple&) {
    return std::vector<byte>{};
    // nop
  }

private:
  std::shared_ptr<std::vector<byte>> rec_buf_;
  uint8_t id_;
};

struct dummy_application_factory {
public:
  using application_type = dummy_application;

  dummy_application_factory(std::shared_ptr<std::vector<byte>> buf)
    : buf_(buf), application_cnt_(0) {
    // nop
  }

  dummy_application make() {
    return dummy_application{buf_, application_cnt_++};
  }

private:
  std::shared_ptr<std::vector<byte>> buf_;
  uint8_t application_cnt_;
};

struct testdata {
  testdata(uint8_t worker_id, node_id id, ip_endpoint ep)
    : worker_id(worker_id), nid(id), ep(ep) {
    // nop
  }

  uint8_t worker_id;
  node_id nid;
  ip_endpoint ep;
};

struct dummy {
  using transport_type = dummy;

  using application_type = dummy_application;
};

// TODO: switch to std::operator""s when switching to C++14
ip_endpoint operator"" _ep(const char* cstr, size_t cstr_len) {
  ip_endpoint ep;
  string_view str(cstr, cstr_len);
  if (auto err = parse(str, ep))
    CAF_FAIL("parse returned error: " << err);
  return ep;
}

uri operator"" _u(const char* cstr, size_t cstr_len) {
  uri result;
  string_view str{cstr, cstr_len};
  auto err = parse(str, result);
  if (err)
    CAF_FAIL("error while parsing " << str << ": " << to_string(err));
  return result;
}

struct fixture : host_fixture {
  using dispatcher_type = transport_worker_dispatcher<dummy_application_factory,
                                                      ip_endpoint>;

  fixture()
    : buf{std::make_shared<std::vector<byte>>()},
      dispatcher{dummy_application_factory{buf}} {
    // nop
  }

  std::unique_ptr<net::endpoint_manager::message>
  make_dummy_message(node_id nid) {
    actor_id aid = 42;
    actor_config cfg;
    auto p = make_actor<dummy_actor, strong_actor_ptr>(aid, nid, &sys, cfg);
    std::vector<byte> payload;
    auto strong_actor = actor_cast<strong_actor_ptr>(p);
    mailbox_element::forwarding_stack stack;
    auto elem = make_mailbox_element(std::move(strong_actor),
                                     make_message_id(12345), std::move(stack),
                                     make_message());
    return detail::make_unique<endpoint_manager::message>(std::move(elem),
                                                          payload);
  }

  template <class Application, class IdType>
  void add_new_workers(
    transport_worker_dispatcher<Application, IdType>& dispatcher) {
    for (auto& data : test_data) {
      dispatcher.add_new_worker(data.nid, data.ep);
    }
  }

  void test_write_message(testdata& testcase) {
    auto msg = make_dummy_message(testcase.nid);
    if (!msg->msg->sender)
      CAF_FAIL("sender is null");
    dispatcher.add_new_worker(testcase.nid, "[::1]:1"_ep);
    dispatcher.write_message(dummy, std::move(msg));
  }

  actor_system_config cfg{};
  actor_system sys{cfg};

  std::shared_ptr<std::vector<byte>> buf;
  dispatcher_type dispatcher;
  dummy dummy;

  std::vector<testdata> test_data{
    {0, make_node_id("http:file"_u), "[::1]:1"_ep},
    {1, make_node_id("http:file?a=1&b=2"_u), "[fe80::2:34]:12345"_ep},
    {2, make_node_id("http:file#42"_u), "[1234::17]:4444"_ep},
    {3, make_node_id("http:file?a=1&b=2#42"_u), "[2332::1]:12"_ep},
  };
};

#define CHECK_HANDLE_DATA(dispatcher, dummy, testcase, buf)                    \
  dispatcher.handle_data(dummy, span<byte>{}, testcase.ep);                    \
  CAF_CHECK_EQUAL(buf->size(), 1u);                                            \
  CAF_CHECK_EQUAL(static_cast<byte>(testcase.worker_id), buf->at(0));          \
  buf->clear();

#define CHECK_WRITE_MESSAGE(dispatcher, dummy, testcase, buf)                  \
  test_write_message(testcase);                                                \
  CAF_CHECK_EQUAL(buf->size(), 1u);                                            \
  CAF_CHECK_EQUAL(static_cast<byte>(testcase.worker_id), buf->at(0));          \
  buf->clear();

} // namespace

CAF_TEST_FIXTURE_SCOPE(transport_worker_dispatcher_test, fixture)

CAF_TEST(handle_data) {
  CHECK_HANDLE_DATA(dispatcher, dummy, test_data.at(0), buf);
  CHECK_HANDLE_DATA(dispatcher, dummy, test_data.at(1), buf);
  CHECK_HANDLE_DATA(dispatcher, dummy, test_data.at(2), buf);
  CHECK_HANDLE_DATA(dispatcher, dummy, test_data.at(3), buf);
}

CAF_TEST(write_message) {
  CHECK_WRITE_MESSAGE(dispatcher, dummy, test_data.at(0), buf);
  CHECK_WRITE_MESSAGE(dispatcher, dummy, test_data.at(1), buf);
  CHECK_WRITE_MESSAGE(dispatcher, dummy, test_data.at(2), buf);
  CHECK_WRITE_MESSAGE(dispatcher, dummy, test_data.at(3), buf);
}

CAF_TEST_FIXTURE_SCOPE_END()
