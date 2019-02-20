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

#define CAF_SUITE io_basp_proxy

#include "caf/io/basp/proxy.hpp"

#include "caf/test/dsl.hpp"

#include <deque>

#include "caf/binary_deserializer.hpp"
#include "caf/io/basp/header.hpp"
#include "caf/make_actor.hpp"
#include "caf/send.hpp"

using std::string;
using buffer = std::vector<char>;

using namespace caf;

namespace {

// -- convenience structs for holding direct or routed messages ----------------

struct direct_msg {
  io::basp::header hdr;
  mailbox_element::forwarding_stack stages;
  message content;

  static direct_msg from(actor_system& sys, const io::basp::header& hdr,
                         const buffer& buf) {
    direct_msg result;
    result.hdr = hdr;
    binary_deserializer source{sys, buf};
    if (result.hdr.operation != io::basp::message_type::direct_message)
      CAF_FAIL("direct message != " << to_string(result.hdr.operation));
    if (result.hdr.payload_len != buf.size())
      CAF_FAIL("BASP header has invalid payload size: expected "
               << buf.size() << ", got " << result.hdr.payload_len);
    if (auto err = source(result.stages, result.content))
      CAF_FAIL("failed to deserialize payload: " << sys.render(err));
    return result;
  }
};

struct routed_msg {
  io::basp::header hdr;
  mailbox_element::forwarding_stack stages;
  message content;
  node_id src;
  node_id dst;

  static routed_msg from(actor_system& sys, const io::basp::header& hdr,
                         const buffer& buf) {
    routed_msg result;
    result.hdr = hdr;
    binary_deserializer source{sys, buf};
    if (result.hdr.operation != io::basp::message_type::routed_message)
      CAF_FAIL("routed message != " << to_string(result.hdr.operation));
    if (result.hdr.payload_len != buf.size())
      CAF_FAIL("BASP header has invalid payload size: expected "
               << buf.size() << ", got " << result.hdr.payload_len);
    if (auto err = source(result.src, result.dst, result.stages,
                          result.content))
      CAF_FAIL("failed to deserialize payload: " << sys.render(err));
    return result;
  }
};

// -- fake dispatcher that mimics a BASP broker --------------------------------

/// A dispatcher that simply exposes everything it receives via a FIFO queue.
struct dispatcher_state {
  struct item {
    strong_actor_ptr sender;
    io::basp::header hdr;
    buffer buf;
  };

  std::deque<item> items;

  item next() {
    auto result = std::move(items.front());
    items.pop_front();
    return result;
  }
};

using dispatcher_type = stateful_actor<dispatcher_state>;

behavior fake_dispatcher(dispatcher_type* self) {
  self->set_default_handler(drop);
  return {[=](strong_actor_ptr receiver, io::basp::header hdr, buffer& buf) {
    using item = dispatcher_state::item;
    self->state.items.emplace_back(item{receiver, hdr, std::move(buf)});
  }};
}

// -- simple dummy actor for testing message delivery --------------------------

class dummy_actor:public event_based_actor {
public:
  dummy_actor(actor_config& cfg) : event_based_actor(cfg) {
    //nop
  }

  behavior make_behavior() override {
    return {[] {}};
  }
};

// -- fixture setup ------------------------------------------------------------

struct fixture : test_coordinator_fixture<> {
  fixture()
    : mars(42, "0011223344556677889900112233445566778899"),
      jupiter(23, "99887766554433221100998877665544332211FF") {
    dispatcher = sys.spawn<lazy_init>(fake_dispatcher);
    actor_config cfg;
    aut = make_actor<io::basp::proxy, actor>(42, mars, &sys, cfg, dispatcher);
    expect((monitor_atom, strong_actor_ptr), to(dispatcher));
  }

  ~fixture() {
    anon_send_exit(dispatcher, exit_reason::user_shutdown);
  }

  dispatcher_state::item next_item() {
    return deref<dispatcher_type>(dispatcher).state.next();
  }

  /// Gets the next item from the dispatcher and deserializes a direct_message
  /// from the buffer content.
  direct_msg next_direct_msg() {
    auto item = next_item();
    if (item.sender != aut)
      CAF_FAIL("message is not directed at our actor-under-test");
    return direct_msg::from(sys, item.hdr, item.buf);
  }

  /// Gets the next item from the dispatcher and deserializes a routed_message
  /// from the buffer content.
  routed_msg next_routed_msg() {
    auto item = next_item();
    if (item.sender != aut)
      CAF_FAIL("message is not directed at our actor-under-test");
    return routed_msg::from(sys, item.hdr, item.buf);
  }

  node_id mars;
  node_id jupiter;
  actor dispatcher;
  actor aut;
};

} // namespace <anonymous>

// -- unit tests ---------------------------------------------------------------

CAF_TEST_FIXTURE_SCOPE(proxy_tests, fixture)

CAF_TEST(forward anonymous message) {
  anon_send(aut, "hello proxy!");
  expect((std::string), to(aut));
  expect((strong_actor_ptr, io::basp::header, buffer), to(dispatcher));
  auto msg = next_direct_msg();
  CAF_CHECK_EQUAL(msg.stages.size(), 0u);
  CAF_CHECK_EQUAL(to_string(msg.content), R"__(("hello proxy!"))__");
}

CAF_TEST(forward message from local actor) {
  self->send(aut, "hi there!");
  expect((std::string), to(aut));
  expect((strong_actor_ptr, io::basp::header, buffer), to(dispatcher));
  auto msg = next_direct_msg();
  CAF_CHECK_EQUAL(msg.stages.size(), 0u);
  CAF_CHECK_EQUAL(to_string(msg.content), R"__(("hi there!"))__");
}

CAF_TEST(forward message from remote actor) {
  actor_config cfg;
  auto testee = make_actor<dummy_actor>(42, jupiter, &sys, cfg);
  send_as(testee, aut, "hello from jupiter!");
  expect((std::string), to(aut));
  expect((strong_actor_ptr, io::basp::header, buffer), to(dispatcher));
  auto msg = next_routed_msg();
  CAF_CHECK_EQUAL(msg.src, jupiter);
  CAF_CHECK_EQUAL(msg.dst, mars);
  CAF_CHECK_EQUAL(msg.stages.size(), 0u);
  CAF_CHECK_EQUAL(to_string(msg.content), R"__(("hello from jupiter!"))__");
}

CAF_TEST_FIXTURE_SCOPE_END()
