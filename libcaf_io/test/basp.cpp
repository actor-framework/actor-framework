/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE io_basp
#include "caf/test/unit_test.hpp"

#include <array>
#include <mutex>
#include <memory>
#include <iostream>
#include <condition_variable>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "caf/io/network/test_multiplexer.hpp"

using namespace std;
using namespace caf;
using namespace caf::io;

namespace {

#define THROW_ON_UNEXPECTED(selfref)                                           \
  others >> [&] {                                                              \
    throw std::logic_error("unexpected message: "                              \
                           + to_string(selfref->current_message()));           \
  },                                                                           \
  after(std::chrono::seconds(0)) >> [&] {                                      \
    throw std::logic_error("unexpected timeout");                              \
  }

static constexpr size_t num_remote_nodes = 2;

using buffer = std::vector<char>;

string hexstr(const buffer& buf) {
  ostringstream oss;
  oss << hex;
  oss.fill('0');
  for (auto& c : buf) {
    oss.width(2);
    oss << int{c};
  }
  return oss.str();
}

class fixture {
public:
  fixture() {
    mpx_ = new network::test_multiplexer;
    set_middleman(mpx_);
    auto mm = middleman::instance();
    aut_ = mm->get_named_broker<basp_broker>(atom("_BASP"));
    this_node_ = detail::singletons::get_node_id();
    self_.reset(new scoped_actor);
    // run the initialization message of the BASP broker
    mpx_->exec_runnable();
    ahdl_ = accept_handle::from_int(1);
    mpx_->assign_tcp_doorman(aut_.get(), ahdl_);
    registry_ = detail::singletons::get_actor_registry();
    registry_->put((*self_)->id(),
                  actor_cast<abstract_actor_ptr>((*self_)->address()));
    // first remote node is everything of this_node + 1, then +2, etc.
    for (uint32_t i = 0; i < num_remote_nodes; ++i) {
      node_id::host_id_type tmp = this_node_.host_id();
      for (auto& c : tmp)
        c += static_cast<uint8_t>(i + 1);
      remote_node_[i] = node_id{this_node_.process_id() + i + 1, tmp};
      remote_hdl_[i] = connection_handle::from_int(i + 1);
      auto& ptr = pseudo_remote_[i];
      ptr.reset(new scoped_actor);
      // register all pseudo remote actors in the registry
      registry_->put((*ptr)->id(),
                    actor_cast<abstract_actor_ptr>((*ptr)->address()));
    }
  }

  uint32_t serialized_size(const message& msg) {
    buffer buf;
    binary_serializer bs{back_inserter(buf), &get_namespace()};
    bs << msg;
    return static_cast<uint32_t>(buf.size());
  }

  ~fixture() {
    this_node_ = invalid_node_id;
    self_.reset();
    for (auto& nid : remote_node_)
      nid = invalid_node_id;
    for (auto& ptr : pseudo_remote_)
      ptr.reset();
    await_all_actors_done();
    shutdown();
  }

  // our "virtual communication backend"
  network::test_multiplexer* mpx() {
    return mpx_;
  }

  // actor-under-test
  basp_broker* aut() {
    return aut_.get();
  }

  // our node ID
  node_id& this_node() {
    return this_node_;
  }

  // an actor reference representing a local actor
  scoped_actor& self() {
    return *self_;
  }

  // dummy remote node ID
  node_id& remote_node(size_t i) {
    return remote_node_[i];
  }

  // dummy remote node ID by connection
  node_id& remote_node(connection_handle hdl) {
    return remote_node_[static_cast<size_t>(hdl.id() - 1)];
  }

  // handle to a virtual connection
  connection_handle remote_hdl(size_t i) {
    return remote_hdl_[i];
  }

  // an actor reference representing a remote actor
  scoped_actor& pseudo_remote(size_t i) {
    return *pseudo_remote_[i];
  }

  // an actor reference representing a remote actor (by connection)
  scoped_actor& pseudo_remote(connection_handle hdl) {
    return *pseudo_remote_[static_cast<size_t>(hdl.id() - 1)];
  }

  // implementation of the Binary Actor System Protocol
  basp::instance& instance() {
    return aut()->state.instance;
  }

  // our routing table (filled by BASP)
  basp::routing_table& tbl() {
    return aut()->state.instance.tbl();
  }

  // access to proxy instances
  actor_namespace& get_namespace() {
    return aut()->state.get_namespace();
  }

  // stores the singleton pointer for convenience
  detail::actor_registry* registry() {
    return registry_;
  }

  using payload_writer = basp::instance::payload_writer;

  void to_buf(buffer& buf, basp::header& hdr, payload_writer* writer) {
    instance().write(buf, hdr, writer);
  }

  template <class T, class... Ts>
  void to_buf(buffer& buf, basp::header& hdr, payload_writer* writer,
              const T& x, const Ts&... xs) {
    auto pw = make_callback([&](serializer& sink) {
      if (writer)
        (*writer)(sink);
      sink << x;
    });
    to_buf(buf, hdr, &pw, xs...);
  }

  binary_deserializer make_deserializer(const buffer& buf) {
    return binary_deserializer{buf.data(), buf.size(), &get_namespace()};
  }

  std::pair<basp::header, buffer> from_buf(const buffer& buf) {
    basp::header hdr;
    auto bd = make_deserializer(buf);
    read_hdr(bd, hdr);
    buffer payload;
    if (hdr.payload_len > 0) {
      std::copy(buf.begin() + basp::header_size, buf.end(),
                std::back_inserter(payload));
    }
    return {hdr, std::move(payload)};
  }

  void connect_node(size_t i,
                    optional<accept_handle> ax,
                    actor_id published_actor_id,
                    set<string> published_actor_ifs = {}) {
    auto src = ax ? *ax : ahdl_;
    CAF_MESSAGE("connect remote node " << i << ", connection ID = " << (i + 1)
                << ", acceptor ID = " << src.id());
    auto hdl = remote_hdl(i);
    mpx_->add_pending_connect(src, hdl);
    mpx_->assign_tcp_scribe(aut(), hdl);
    mpx_->accept_connection(src);
    // technically, the server handshake arrives
    // before we send the client handshake
    mock(hdl,
         {basp::message_type::client_handshake, 0, 0,
          remote_node(i), this_node(),
          invalid_actor_id, invalid_actor_id})
    .expect(hdl,
            {basp::message_type::server_handshake, 0, basp::version,
             this_node(), invalid_node_id,
             invalid_actor_id, invalid_actor_id},
            published_actor_id,
            published_actor_ifs);
    // test whether basp instance correctly updates the
    // routing table upon receiving client handshakes
    auto path = tbl().lookup(remote_node(i));
    CAF_REQUIRE(path != none);
    CAF_CHECK(path->hdl == remote_hdl(i));
    CAF_CHECK(path->next_hop == remote_node(i));
  }

  std::pair<basp::header, buffer> read_from_out_buf(connection_handle hdl) {
    CAF_MESSAGE("read from output buffer for connection " << hdl.id());
    auto& buf = mpx_->output_buffer(hdl);
    CAF_REQUIRE(buf.size() >= basp::header_size);
    auto result = from_buf(buf);
    buf.erase(buf.begin(),
              buf.begin() + basp::header_size + result.first.payload_len);
    return result;
  }

  void dispatch_out_buf(connection_handle hdl) {
    basp::header hdr;
    buffer buf;
    std::tie(hdr, buf) = read_from_out_buf(hdl);
    CAF_MESSAGE("dispatch output buffer for connection " << hdl.id());
    CAF_REQUIRE(hdr.operation == basp::message_type::dispatch_message);
    message msg;
    auto source = make_deserializer(buf);
    msg.deserialize(source);
    auto registry = detail::singletons::get_actor_registry();
    auto src = registry->get(hdr.source_actor);
    auto dest = registry->get(hdr.dest_actor);
    CAF_REQUIRE(dest != nullptr);
    dest->enqueue(src ? src->address() : invalid_actor_addr,
                  message_id::make(), std::move(msg), nullptr);
  }

  class mock_t {
  public:
    mock_t(fixture* thisptr) : this_(thisptr) {
      // nop
    }

    mock_t(mock_t&&) = default;

    ~mock_t() {
      if (num > 1)
        CAF_MESSAGE("implementation under test responded with "
                    << (num - 1) << " BASP message" << (num > 2 ? "s" : ""));
    }

    template <class... Ts>
    mock_t& expect(connection_handle hdl, basp::header hdr, const Ts&... xs) {
      CAF_MESSAGE("expect " << num++ << ". sent message to be a "
                  << to_string(hdr.operation));
      buffer buf;
      this_->to_buf(buf, hdr, nullptr, xs...);
      buffer& ob = this_->mpx()->output_buffer(hdl);
      CAF_REQUIRE(buf.size() <= ob.size());
      auto first = ob.begin();
      auto last = ob.begin() + static_cast<ptrdiff_t>(buf.size());
      buffer cpy(first, last);
      ob.erase(first, last);
      basp::header tmp;
      { // lifetime scope of source
        auto source = this_->make_deserializer(cpy);
        basp::read_hdr(source, tmp);
      }
      CAF_REQUIRE(to_string(hdr) == to_string(tmp));
      CAF_REQUIRE(equal(buf.begin(), buf.begin() + basp::header_size,
                        cpy.begin()));
      buf.erase(buf.begin(), buf.begin() + basp::header_size);
      cpy.erase(cpy.begin(), cpy.begin() + basp::header_size);
      CAF_REQUIRE(hdr.payload_len == buf.size() && cpy.size() == buf.size());
      CAF_REQUIRE(hexstr(buf) == hexstr(cpy));
      return *this;
    }

  private:
    fixture* this_;
    size_t num = 1;
  };

  template <class... Ts>
  mock_t mock(connection_handle hdl, basp::header hdr, const Ts&... xs) {
    CAF_MESSAGE("virtually send " << to_string(hdr.operation));
    buffer buf;
    to_buf(buf, hdr, nullptr, xs...);
    mpx()->virtual_send(hdl, buf);
    return {this};
  }

  mock_t mock() {
    return {this};
  }

private:
  intrusive_ptr<basp_broker> aut_;
  accept_handle ahdl_;
  network::test_multiplexer* mpx_;
  node_id this_node_;
  unique_ptr<scoped_actor> self_;
  array<node_id, num_remote_nodes> remote_node_;
  array<connection_handle, num_remote_nodes> remote_hdl_;
  array<unique_ptr<scoped_actor>, num_remote_nodes> pseudo_remote_;
  detail::actor_registry* registry_;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(basp_tests, fixture)

CAF_TEST(empty_server_handshake) {
  // test whether basp instance correctly sends a
  // server handshake whene there's no actor published
  buffer buf;
  instance().write_server_handshake(buf, none);
  basp::header hdr;
  buffer payload;
  std::tie(hdr, payload) = from_buf(buf);
  auto is_zero = [](char c) {
    return c == 0;
  };
  basp::header expected{basp::message_type::server_handshake,
                        sizeof(actor_id) + sizeof(uint32_t),
                        basp::version,
                        this_node(), invalid_node_id,
                        invalid_actor_id, invalid_actor_id};
  CAF_CHECK(basp::valid(hdr));
  CAF_CHECK(basp::is_handshake(hdr));
  CAF_CHECK(hdr == expected);
  CAF_CHECK(all_of(payload.begin(), payload.end(), is_zero));
}

CAF_TEST(non_empty_server_handshake) {
  // test whether basp instance correctly sends a
  // server handshake with published actors
  buffer buf;
  instance().add_published_actor(4242, self().address(),
                                 {"caf::replies_to<@u16>::with<@u16>"});
  instance().write_server_handshake(buf, 4242);
  buffer expected_buf;
  basp::header expected{basp::message_type::server_handshake, 0, basp::version,
                        this_node(), invalid_node_id,
                        invalid_actor_id, invalid_actor_id};
  to_buf(expected_buf, expected, nullptr,
         self()->id(), set<string>{"caf::replies_to<@u16>::with<@u16>"});
  CAF_CHECK(hexstr(buf) == hexstr(expected_buf));
}

CAF_TEST(client_handshake_and_dispatch) {
  connect_node(0, none, invalid_actor_id, {});
  // send a message via `dispatch` from node 0
  mock(remote_hdl(0),
       {basp::message_type::dispatch_message, 0, 0,
        remote_node(0), this_node(), pseudo_remote(0)->id(), self()->id()},
       make_message(1, 2, 3))
  .expect(remote_hdl(0),
          {basp::message_type::announce_proxy_instance, 0, 0,
           this_node(), remote_node(0),
           invalid_actor_id, pseudo_remote(0)->id()});
  // must've created a proxy for our remote actor
  CAF_REQUIRE(get_namespace().count_proxies(remote_node(0)) == 1);
  // must've send remote node a message that this proxy is monitored now
  // receive the message
  self()->receive(
    [](int a, int b, int c) {
      CAF_CHECK(a == 1);
      CAF_CHECK(b == 2);
      CAF_CHECK(c == 3);
      return a + b + c;
    },
    THROW_ON_UNEXPECTED(self())
  );
  // check for message forwarded by `forwarding_actor_proxy`
  mpx()->exec_runnable(); // exec the message of our forwarding proxy
  dispatch_out_buf(remote_hdl(0)); // deserialize and send message from out buf
  pseudo_remote(0)->receive(
    [](int i) {
      CAF_CHECK(i == 6);
    },
    THROW_ON_UNEXPECTED(pseudo_remote(0))
  );
}

CAF_TEST(message_forwarding) {
  // connect two remote nodes
  connect_node(0, none, invalid_actor_id, {});
  connect_node(1, none, invalid_actor_id, {});
  auto msg = make_message(1, 2, 3);
  // send a message from node 0 to node 1, forwarded by this node
  mock(remote_hdl(0),
       {basp::message_type::dispatch_message, 0, 0,
        remote_node(0), remote_node(1),
        invalid_actor_id, pseudo_remote(1)->id()},
       msg)
  .expect(remote_hdl(1),
          {basp::message_type::dispatch_message, 0, 0,
           remote_node(0), remote_node(1),
           invalid_actor_id, pseudo_remote(1)->id()},
          msg);
}

CAF_TEST(publish_and_connect) {
  auto ax = accept_handle::from_int(4242);
  mpx()->provide_acceptor(4242, ax);
  publish(self(), 4242);
  mpx()->exec_runnable(); // process publish message in basp_broker
  connect_node(0, ax, self()->id(), {});
}

CAF_TEST(remote_actor_and_send) {
  CAF_MESSAGE("self: " << to_string(self()->address()));
  mpx()->provide_scribe("localhost", 4242, remote_hdl(0));
  CAF_CHECK(mpx()->pending_scribes().count(make_pair("localhost", 4242)) == 1);
  auto mm = get_middleman_actor();
  actor result;
  auto f = self()->sync_send(mm, get_atom::value, "localhost", uint16_t{4242});
  mpx()->exec_runnable(); // process message in basp_broker
  CAF_CHECK(mpx()->pending_scribes().count(make_pair("localhost", 4242)) == 0);
  // build a fake server handshake containing the id of our first pseudo actor
  CAF_TEST_VERBOSE("server handshake => client handshake + proxy announcement");
  mock(remote_hdl(0),
       {basp::message_type::server_handshake, 0, basp::version,
        remote_node(0), invalid_node_id,
        invalid_actor_id, invalid_actor_id},
       pseudo_remote(0)->id(),
       uint32_t{0})
  .expect(remote_hdl(0),
          {basp::message_type::client_handshake, 0, 0,
           this_node(), remote_node(0),
           invalid_actor_id, invalid_actor_id})
  .expect(remote_hdl(0),
          {basp::message_type::announce_proxy_instance, 0, 0,
           this_node(), remote_node(0),
           invalid_actor_id, pseudo_remote(0)->id()});
  // basp broker should've send the proxy
  f.await(
    [&](ok_atom, actor_addr res) {
      auto aptr = actor_cast<abstract_actor_ptr>(res);
      CAF_REQUIRE(aptr.downcast<forwarding_actor_proxy>() != nullptr);
      CAF_CHECK(get_namespace().get_all().size() == 1);
      CAF_CHECK(get_namespace().count_proxies(remote_node(0)) == 1);
      CAF_CHECK(res.node() == remote_node(0));
      CAF_CHECK(res.id() == pseudo_remote(0)->id());
      auto proxy = get_namespace().get(remote_node(0), pseudo_remote(0)->id());
      CAF_REQUIRE(proxy != nullptr);
      CAF_REQUIRE(proxy->address() == res);
      result = actor_cast<actor>(res);
    },
    [&](error_atom, std::string& msg) {
      throw logic_error(std::move(msg));
    }
  );
  CAF_MESSAGE("send message to proxy");
  anon_send(actor_cast<actor>(result), 42);
  mpx()->exec_runnable(); // process forwarded message in basp_broker
  mock()
  .expect(remote_hdl(0),
          {basp::message_type::dispatch_message, 0, 0,
           this_node(), remote_node(0),
           invalid_actor_id, pseudo_remote(0)->id()},
          make_message(42));
  auto msg = make_message("hi there!");
  CAF_MESSAGE("send message via BASP (from proxy)");
  mock(remote_hdl(0),
       {basp::message_type::dispatch_message, 0, 0,
        remote_node(0), this_node(),
        pseudo_remote(0)->id(), self()->id()},
       make_message("hi there!"));
  self()->receive(
    [&](const string& str) {
      CAF_CHECK(to_string(self()->current_sender()) == to_string(result));
      CAF_CHECK(self()->current_sender() == result);
      CAF_CHECK(str == "hi there!");
    },
    THROW_ON_UNEXPECTED(self())
  );
}

CAF_TEST(actor_serialize_and_deserialize) {
  auto testee_impl = [](event_based_actor* self) -> behavior {
    return {
      others >> [=] {
        self->quit();
        return self->current_message();
      }
    };
  };
  connect_node(0, none, invalid_actor_id, {});
  auto prx = get_namespace().get_or_put(remote_node(0), pseudo_remote(0)->id());
  mock()
  .expect(remote_hdl(0),
          {basp::message_type::announce_proxy_instance, 0, 0,
           this_node(), prx->node(),
           invalid_actor_id, prx->id()});
  CAF_CHECK(prx->node() == remote_node(0));
  CAF_CHECK(prx->id() == pseudo_remote(0)->id());
  auto testee = spawn(testee_impl);
  registry()->put(testee->id(), testee->address());
  CAF_MESSAGE("send message via BASP (from proxy)");
  auto msg = make_message(prx->address());
  mock(remote_hdl(0),
       {basp::message_type::dispatch_message, 0, 0,
        prx->node(), this_node(),
        prx->id(), testee->id()},
       msg);
  // testee must've responded (process forwarded message in BASP broker)
  CAF_MESSAGE("exec runnable, i.e., handle response from testee");
  mpx()->exec_runnable(); // process forwarded message in basp_broker
  // output buffer must contain the reflected message
  mock()
  .expect(remote_hdl(0),
          {basp::message_type::dispatch_message, 0, 0,
           this_node(), prx->node(),
           testee->id(), prx->id()},
          msg);
}

CAF_TEST_FIXTURE_SCOPE_END()
