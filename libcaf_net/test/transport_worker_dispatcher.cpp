// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE transport_worker_dispatcher

#include "caf/net/transport_worker_dispatcher.hpp"

#include "caf/net/test/host_fixture.hpp"
#include "caf/test/dsl.hpp"

#include "caf/byte_buffer.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/make_actor.hpp"
#include "caf/monitorable_actor.hpp"
#include "caf/node_id.hpp"
#include "caf/uri.hpp"

using namespace caf;
using namespace caf::net;

namespace {

using byte_buffer_ptr = std::shared_ptr<byte_buffer>;

constexpr string_view hello_test = "hello_test";

struct dummy_actor : public monitorable_actor {
  dummy_actor(actor_config& cfg) : monitorable_actor(cfg) {
    // nop
  }

  void enqueue(mailbox_element_ptr, execution_unit*) override {
    // nop
  }

  void setup_metrics() {
    // nop
  }
};

class dummy_application {
public:
  dummy_application(byte_buffer_ptr rec_buf, uint8_t id)
    : rec_buf_(std::move(rec_buf)),
      id_(id){
        // nop
      };

  ~dummy_application() = default;

  template <class Parent>
  error init(Parent&) {
    rec_buf_->push_back(static_cast<byte>(id_));
    return none;
  }

  template <class Parent>
  error write_message(Parent& parent,
                      std::unique_ptr<endpoint_manager_queue::message> ptr) {
    rec_buf_->push_back(static_cast<byte>(id_));
    auto data = ptr->msg->content().get_as<byte_buffer>(0);
    parent.write_packet(data);
    return none;
  }

  template <class Parent>
  error handle_data(Parent&, span<const byte>) {
    rec_buf_->push_back(static_cast<byte>(id_));
    return none;
  }

  template <class Manager>
  void resolve(Manager&, const std::string&, actor) {
    rec_buf_->push_back(static_cast<byte>(id_));
  }

  template <class Transport>
  void timeout(Transport&, const std::string&, uint64_t) {
    rec_buf_->push_back(static_cast<byte>(id_));
  }

  void handle_error(sec) {
    rec_buf_->push_back(static_cast<byte>(id_));
  }

private:
  byte_buffer_ptr rec_buf_;
  uint8_t id_;
};

struct dummy_application_factory {
public:
  using application_type = dummy_application;

  dummy_application_factory(byte_buffer_ptr buf)
    : buf_(std::move(buf)), application_cnt_(0) {
    // nop
  }

  dummy_application make() {
    return dummy_application{buf_, application_cnt_++};
  }

private:
  byte_buffer_ptr buf_;
  uint8_t application_cnt_;
};

struct dummy_transport {
  using transport_type = dummy_transport;

  using factory_type = dummy_application_factory;

  using application_type = dummy_application;

  dummy_transport(actor_system& sys, byte_buffer_ptr buf)
    : sys_(sys), buf_(std::move(buf)) {
    // nop
  }

  template <class IdType>
  void write_packet(IdType, span<byte_buffer*> buffers) {
    for (auto buf : buffers)
      buf_->insert(buf_->end(), buf->begin(), buf->end());
  }

  actor_system& system() {
    return sys_;
  }

  transport_type& transport() {
    return *this;
  }

  byte_buffer next_header_buffer() {
    return {};
  }

  byte_buffer next_payload_buffer() {
    return {};
  }

private:
  actor_system& sys_;
  byte_buffer_ptr buf_;
};

struct testdata {
  testdata(uint8_t worker_id, node_id id, ip_endpoint ep)
    : worker_id(worker_id), nid(std::move(id)), ep(ep) {
    // nop
  }

  uint8_t worker_id;
  node_id nid;
  ip_endpoint ep;
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
  using dispatcher_type
    = transport_worker_dispatcher<dummy_application_factory, ip_endpoint>;

  fixture()
    : buf{std::make_shared<byte_buffer>()},
      dispatcher{dummy_application_factory{buf}},
      dummy{sys, buf} {
    add_new_workers();
  }

  std::unique_ptr<net::endpoint_manager_queue::message>
  make_dummy_message(node_id nid) {
    actor_id aid = 42;
    auto test_span = as_bytes(make_span(hello_test));
    byte_buffer payload(test_span.begin(), test_span.end());
    actor_config cfg;
    auto p = make_actor<dummy_actor, strong_actor_ptr>(aid, nid, &sys, cfg);
    auto receiver = actor_cast<strong_actor_ptr>(p);
    if (!receiver)
      CAF_FAIL("failed to cast receiver to a strong_actor_ptr");
    mailbox_element::forwarding_stack stack;
    auto elem = make_mailbox_element(nullptr, make_message_id(12345),
                                     std::move(stack), make_message(payload));
    return detail::make_unique<endpoint_manager_queue::message>(std::move(elem),
                                                                receiver);
  }

  bool contains(byte x) {
    return std::count(buf->begin(), buf->end(), x) > 0;
  }

  void add_new_workers() {
    for (auto& data : test_data) {
      auto worker = dispatcher.add_new_worker(dummy, data.nid, data.ep);
      if (!worker)
        CAF_FAIL("add_new_worker returned an error: " << worker.error());
    }
    buf->clear();
  }

  void test_write_message(testdata& testcase) {
    auto msg = make_dummy_message(testcase.nid);
    if (!msg->receiver)
      CAF_FAIL("receiver is null");
    CAF_MESSAGE(CAF_ARG(msg));
    dispatcher.write_message(dummy, std::move(msg));
  }

  actor_system_config cfg{};
  actor_system sys{cfg};

  byte_buffer_ptr buf;
  dispatcher_type dispatcher;
  dummy_transport dummy;

  std::vector<testdata> test_data{
    {0, make_node_id("http:file"_u), "[::1]:1"_ep},
    {1, make_node_id("http:file?a=1&b=2"_u), "[fe80::2:34]:12345"_ep},
    {2, make_node_id("http:file#42"_u), "[1234::17]:4444"_ep},
    {3, make_node_id("http:file?a=1&b=2#42"_u), "[2332::1]:12"_ep},
  };
};

#define CHECK_HANDLE_DATA(testcase)                                            \
  CAF_CHECK_EQUAL(                                                             \
    dispatcher.handle_data(dummy, span<const byte>{}, testcase.ep), none);     \
  CAF_CHECK_EQUAL(buf->size(), 1u);                                            \
  CAF_CHECK_EQUAL(static_cast<byte>(testcase.worker_id), buf->at(0));          \
  buf->clear();

#define CHECK_WRITE_MESSAGE(testcase)                                          \
  test_write_message(testcase);                                                \
  CAF_CHECK_EQUAL(buf->size(), hello_test.size() + 1u);                        \
  CAF_CHECK_EQUAL(static_cast<byte>(testcase.worker_id), buf->at(0));          \
  CAF_CHECK_EQUAL(                                                             \
    memcmp(buf->data() + 1, hello_test.data(), hello_test.size()), 0);         \
  buf->clear();

} // namespace

CAF_TEST_FIXTURE_SCOPE(transport_worker_dispatcher_test, fixture)

CAF_TEST(init) {
  dispatcher_type dispatcher{dummy_application_factory{buf}};
  CAF_CHECK_EQUAL(dispatcher.init(dummy), none);
}

CAF_TEST(handle_data) {
  CHECK_HANDLE_DATA(test_data.at(0));
  CHECK_HANDLE_DATA(test_data.at(1));
  CHECK_HANDLE_DATA(test_data.at(2));
  CHECK_HANDLE_DATA(test_data.at(3));
}

CAF_TEST(write_message write_packet) {
  CHECK_WRITE_MESSAGE(test_data.at(0));
  CHECK_WRITE_MESSAGE(test_data.at(1));
  CHECK_WRITE_MESSAGE(test_data.at(2));
  CHECK_WRITE_MESSAGE(test_data.at(3));
}

CAF_TEST(resolve) {
  // TODO think of a test for this
}

CAF_TEST(handle_error) {
  dispatcher.handle_error(sec::unavailable_or_would_block);
  CAF_CHECK_EQUAL(buf->size(), 4u);
  CAF_CHECK(contains(byte(0)));
  CAF_CHECK(contains(byte(1)));
  CAF_CHECK(contains(byte(2)));
  CAF_CHECK(contains(byte(3)));
}

CAF_TEST_FIXTURE_SCOPE_END()
