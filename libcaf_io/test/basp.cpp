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
#include <limits>
#include <iostream>
#include <condition_variable>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "caf/experimental/whereis.hpp"

#include "caf/io/network/interfaces.hpp"
#include "caf/io/network/test_multiplexer.hpp"

namespace std {

ostream& operator<<(ostream& out, const caf::io::basp::message_type& x) {
  return out << to_string(x);
}

template <class T>
ostream& operator<<(ostream& out, const caf::variant<caf::anything, T>& x) {
  using std::to_string;
  using caf::to_string;
  using caf::io::basp::to_string;
  if (get<caf::anything>(&x) != nullptr)
    return out << "*";
  return out << to_string(get<T>(x));
}

} // namespace std

namespace caf {

template <class T>
bool operator==(const variant<anything, T>& x, const T& y) {
  return get<anything>(&x) != nullptr || get<T>(x) == y;
}

template <class T>
bool operator==(const T& x, const variant<anything, T>& y) {
  return (y == x);
}

template <class T>
std::string to_string(const variant<anything, T>& x) {
  return get<anything>(&x) == nullptr ? std::string{"*"} : to_string(get<T>(x));
}

} // namespace caf

using namespace std;
using namespace caf;
using namespace caf::io;
using namespace caf::experimental;

namespace {

#define THROW_ON_UNEXPECTED(selfref)                                           \
  others >> [&] {                                                              \
    throw std::logic_error("unexpected message: "                              \
                           + to_string(selfref->current_message()));           \
  },                                                                           \
  after(std::chrono::seconds(0)) >> [&] {                                      \
    throw std::logic_error("unexpected timeout");                              \
  }

static constexpr uint32_t num_remote_nodes = 2;

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
    aut_ = mm->get_named_broker<basp_broker>(atom("BASP"));
    this_node_ = detail::singletons::get_node_id();
    CAF_MESSAGE("this node: " << to_string(this_node_));
    self_.reset(new scoped_actor);
    ahdl_ = accept_handle::from_int(1);
    mpx_->assign_tcp_doorman(aut_.get(), ahdl_);
    registry_ = detail::singletons::get_actor_registry();
    registry_->put((*self_)->id(),
                  actor_cast<abstract_actor_ptr>((*self_)->address()));
    // first remote node is everything of this_node + 1, then +2, etc.
    for (uint32_t i = 0; i < num_remote_nodes; ++i) {
      node_id::host_id_type tmp = this_node_.host_id();
      for (auto& c : tmp)
        c = static_cast<uint8_t>(c + i + 1);
      remote_node_[i] = node_id{this_node_.process_id() + i + 1, tmp};
      remote_hdl_[i] = connection_handle::from_int(i + 1);
      auto& ptr = pseudo_remote_[i];
      ptr.reset(new scoped_actor);
      // register all pseudo remote actors in the registry
      registry_->put((*ptr)->id(),
                    actor_cast<abstract_actor_ptr>((*ptr)->address()));
    }
    // make sure all init messages are handled properly
    mpx_->flush_runnables();
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

  void to_payload(binary_serializer&) {
    // end of recursion
  }

  template <class T, class... Ts>
  void to_payload(binary_serializer& bs, const T& x, const Ts&... xs) {
    bs << x;
    to_payload(bs, xs...);
  }

  template <class... Ts>
  void to_payload(buffer& buf, const Ts&... xs) {
    binary_serializer bs{std::back_inserter(buf), &get_namespace()};
    to_payload(bs, xs...);
  }

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
                    maybe<accept_handle> ax = none,
                    actor_id published_actor_id = invalid_actor_id,
                    set<string> published_actor_ifs = std::set<std::string>{}) {
    auto src = ax ? *ax : ahdl_;
    CAF_MESSAGE("connect remote node " << i << ", connection ID = " << (i + 1)
                << ", acceptor ID = " << src.id());
    auto hdl = remote_hdl(i);
    mpx_->add_pending_connect(src, hdl);
    mpx_->assign_tcp_scribe(aut(), hdl);
    mpx_->accept_connection(src);
    // technically, the server handshake arrives
    // before we send the client handshake
    auto m = mock(hdl,
                  {basp::message_type::client_handshake, 0, 0,
                   remote_node(i), this_node(),
                   invalid_actor_id, invalid_actor_id});
    if (published_actor_id != invalid_actor_id)
      m.expect(hdl,
               basp::message_type::server_handshake, any_vals, basp::version,
               this_node(), node_id{invalid_node_id},
               published_actor_id, invalid_actor_id,
               published_actor_id,
               published_actor_ifs);
    else
      m.expect(hdl,
               basp::message_type::server_handshake, any_vals, basp::version,
               this_node(), node_id{invalid_node_id},
               invalid_actor_id, invalid_actor_id);
    // upon receiving our client handshake, BASP will check
    // whether there is a SpawnServ actor on this node
    m.expect(hdl,
             basp::message_type::dispatch_message, any_vals,
             static_cast<uint64_t>(atom("SpawnServ")),
             this_node(), remote_node(i),
             any_vals, std::numeric_limits<actor_id>::max(),
             make_message(sys_atom::value, get_atom::value, "info"));
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
    auto src = registry_->get(hdr.source_actor);
    auto dest = registry_->get(hdr.dest_actor);
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
    mock_t& expect(connection_handle hdl,
                   variant<anything, basp::message_type> operation,
                   variant<anything, uint32_t> payload_len,
                   variant<anything, uint64_t> operation_data,
                   variant<anything, node_id> source_node,
                   variant<anything, node_id> dest_node,
                   variant<anything, actor_id> source_actor,
                   variant<anything, actor_id> dest_actor,
                   const Ts&... xs) {
      CAF_MESSAGE("expect " << num << ". sent message to be a "
                  << operation);
      buffer buf;
      this_->to_payload(buf, xs...);
      buffer& ob = this_->mpx()->output_buffer(hdl);
      CAF_MESSAGE("output buffer has " << ob.size() << " bytes");
      CAF_REQUIRE(ob.size() >= basp::header_size);
      basp::header hdr;
      { // lifetime scope of source
        auto source = this_->make_deserializer(ob);
        basp::read_hdr(source, hdr);
      }
      buffer payload;
      if (hdr.payload_len > 0) {
        CAF_REQUIRE(ob.size() >= (basp::header_size + hdr.payload_len));
        auto first = ob.begin() + basp::header_size;
        auto end = first + hdr.payload_len;
        payload.assign(first, end);
        CAF_MESSAGE("erase " << std::distance(ob.begin(), end)
                         << " bytes from output buffer");
        ob.erase(ob.begin(), end);
      } else {
        ob.erase(ob.begin(), ob.begin() + basp::header_size);
      }
      CAF_CHECK_EQUAL(operation, hdr.operation);
      CAF_CHECK_EQUAL(payload_len, hdr.payload_len);
      CAF_CHECK_EQUAL(operation_data, hdr.operation_data);
      CAF_CHECK_EQUAL(source_node, hdr.source_node);
      CAF_CHECK_EQUAL(dest_node, hdr.dest_node);
      CAF_CHECK_EQUAL(source_actor, hdr.source_actor);
      CAF_CHECK_EQUAL(dest_actor, hdr.dest_actor);
      CAF_REQUIRE(payload.size() == buf.size());
      CAF_REQUIRE(hexstr(payload) == hexstr(buf));
      ++num;
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
  basp::header expected{basp::message_type::server_handshake,
                        static_cast<uint32_t>(payload.size()),
                        basp::version,
                        this_node(), invalid_node_id,
                        invalid_actor_id, invalid_actor_id};
  CAF_CHECK(basp::valid(hdr));
  CAF_CHECK(basp::is_handshake(hdr));
  CAF_CHECK_EQUAL(to_string(hdr), to_string(expected));
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
                        self()->id(), invalid_actor_id};
  to_buf(expected_buf, expected, nullptr,
         self()->id(), set<string>{"caf::replies_to<@u16>::with<@u16>"});
  CAF_CHECK(hexstr(buf) == hexstr(expected_buf));
}

CAF_TEST(client_handshake_and_dispatch) {
  connect_node(0);
  // send a message via `dispatch` from node 0
  mock(remote_hdl(0),
       {basp::message_type::dispatch_message, 0, 0,
        remote_node(0), this_node(), pseudo_remote(0)->id(), self()->id()},
       make_message(1, 2, 3))
  .expect(remote_hdl(0),
          basp::message_type::announce_proxy_instance, uint32_t{0}, uint64_t{0},
          this_node(), remote_node(0),
          invalid_actor_id, pseudo_remote(0)->id());
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
  CAF_MESSAGE("exec message of forwarding proxy");
  mpx()->exec_runnable();
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
  connect_node(0);
  connect_node(1);
  auto msg = make_message(1, 2, 3);
  // send a message from node 0 to node 1, forwarded by this node
  mock(remote_hdl(0),
       {basp::message_type::dispatch_message, 0, 0,
        remote_node(0), remote_node(1),
        invalid_actor_id, pseudo_remote(1)->id()},
       msg)
  .expect(remote_hdl(1),
          basp::message_type::dispatch_message, any_vals, uint64_t{0},
          remote_node(0), remote_node(1),
          invalid_actor_id, pseudo_remote(1)->id(),
          msg);
}

CAF_TEST(publish_and_connect) {
  auto ax = accept_handle::from_int(4242);
  mpx()->provide_acceptor(4242, ax);
  publish(self(), 4242);
  mpx()->exec_runnable(); // process publish message in basp_broker
  connect_node(0, ax, self()->id());
}

CAF_TEST(remote_actor_and_send) {
  constexpr const char* lo = "localhost";
  CAF_MESSAGE("self: " << to_string(self()->address()));
  mpx()->provide_scribe(lo, 4242, remote_hdl(0));
  CAF_REQUIRE(mpx()->pending_scribes().count(make_pair(lo, 4242)) == 1);
  auto mm1 = get_middleman_actor();
  actor result;
  auto f = self()->sync_send(mm1, connect_atom::value,
                             lo, uint16_t{4242});
  // wait until BASP broker has received and processed the connect message
  while (! aut()->valid(remote_hdl(0)))
    mpx()->exec_runnable();
  CAF_REQUIRE(mpx()->pending_scribes().count(make_pair(lo, 4242)) == 0);
  // build a fake server handshake containing the id of our first pseudo actor
  CAF_MESSAGE("server handshake => client handshake + proxy announcement");
  auto na = registry()->named_actors();
  mock(remote_hdl(0),
       {basp::message_type::server_handshake, 0, basp::version,
        remote_node(0), invalid_node_id,
        pseudo_remote(0)->id(), invalid_actor_id},
       pseudo_remote(0)->id(),
       uint32_t{0})
  .expect(remote_hdl(0),
          basp::message_type::client_handshake, uint32_t{0}, uint64_t{0},
          this_node(), remote_node(0),
          invalid_actor_id, invalid_actor_id)
  .expect(remote_hdl(0),
          basp::message_type::dispatch_message, any_vals,
          static_cast<uint64_t>(atom("SpawnServ")),
          this_node(), remote_node(0),
          any_vals, std::numeric_limits<actor_id>::max(),
          make_message(sys_atom::value, get_atom::value, "info"))
  .expect(remote_hdl(0),
          basp::message_type::announce_proxy_instance, uint32_t{0}, uint64_t{0},
          this_node(), remote_node(0),
          invalid_actor_id, pseudo_remote(0)->id());
  // basp broker should've send the proxy
  f.await(
    [&](ok_atom, node_id nid, actor_addr res, std::set<std::string> ifs) {
      auto aptr = actor_cast<abstract_actor_ptr>(res);
      CAF_REQUIRE(aptr.downcast<forwarding_actor_proxy>() != nullptr);
      CAF_CHECK(get_namespace().get_all().size() == 1);
      CAF_CHECK(get_namespace().count_proxies(remote_node(0)) == 1);
      CAF_CHECK(nid == remote_node(0));
      CAF_CHECK(res.node() == remote_node(0));
      CAF_CHECK(res.id() == pseudo_remote(0)->id());
      CAF_CHECK(ifs.empty());
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
          basp::message_type::dispatch_message, any_vals, uint64_t{0},
          this_node(), remote_node(0),
          invalid_actor_id, pseudo_remote(0)->id(),
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
  auto testee_impl = [](event_based_actor* testee_self) -> behavior {
    return {
      others >> [=] {
        testee_self->quit();
        return testee_self->current_message();
      }
    };
  };
  connect_node(0);
  auto prx = get_namespace().get_or_put(remote_node(0), pseudo_remote(0)->id());
  mock()
  .expect(remote_hdl(0),
          basp::message_type::announce_proxy_instance, uint32_t{0}, uint64_t{0},
          this_node(), prx->node(),
          invalid_actor_id, prx->id());
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
  CAF_MESSAGE("wait until BASP broker writes to its output buffer");
  while (mpx()->output_buffer(remote_hdl(0)).empty())
    mpx()->exec_runnable(); // process forwarded message in basp_broker
  // output buffer must contain the reflected message
  mock()
  .expect(remote_hdl(0),
          basp::message_type::dispatch_message, any_vals, uint64_t{0},
          this_node(), prx->node(),
          testee->id(), prx->id(),
          msg);
}

CAF_TEST(indirect_connections) {
  // jupiter [remote hdl 0] -> mars [remote hdl 1] -> earth [this_node]
  // (this node receives a message from jupiter via mars and responds via mars)
  CAF_MESSAGE("self: " << to_string(self()->address()));
  auto ax = accept_handle::from_int(4242);
  mpx()->provide_acceptor(4242, ax);
  publish(self(), 4242);
  mpx()->exec_runnable(); // process publish message in basp_broker
  // connect to mars
  connect_node(1, ax, self()->id());
  // now, an actor from jupiter sends a message to us via mars
  mock(remote_hdl(1),
       {basp::message_type::dispatch_message, 0, 0,
        remote_node(0), this_node(),
        pseudo_remote(0)->id(), self()->id()},
       make_message("hello from jupiter!"))
  // this asks Jupiter if it has a 'SpawnServ'
  .expect(remote_hdl(1),
          basp::message_type::dispatch_message, any_vals,
          static_cast<uint64_t>(atom("SpawnServ")),
          this_node(), remote_node(0),
          any_vals, std::numeric_limits<actor_id>::max(),
          make_message(sys_atom::value, get_atom::value, "info"))
  // this tells Jupiter that Earth learned the address of one its actors
  .expect(remote_hdl(1),
          basp::message_type::announce_proxy_instance, uint32_t{0}, uint64_t{0},
          this_node(), remote_node(0),
          invalid_actor_id, pseudo_remote(0)->id());
  CAF_MESSAGE("receive message from jupiter");
  self()->receive(
    [](const std::string& str) -> std::string {
      CAF_CHECK(str == "hello from jupiter!");
      return "hello from earth!";
    },
    THROW_ON_UNEXPECTED(self())
  );
  mpx()->exec_runnable(); // process forwarded message in basp_broker
  mock()
  .expect(remote_hdl(1),
          basp::message_type::dispatch_message, any_vals, uint64_t{0},
          this_node(), remote_node(0),
          self()->id(), pseudo_remote(0)->id(),
          make_message("hello from earth!"));
}

CAF_TEST(automatic_connection) {
  // this tells our BASP broker to enable the automatic connection feature
  anon_send(aut(), ok_atom::value,
            "global.enable-automatic-connections", make_message(true));
  mpx()->exec_runnable(); // process publish message in basp_broker
  // jupiter [remote hdl 0] -> mars [remote hdl 1] -> earth [this_node]
  // (this node receives a message from jupiter via mars and responds via mars,
  //  but then also establishes a connection to jupiter directly)
  mpx()->provide_scribe("jupiter", 8080, remote_hdl(0));
  CAF_CHECK(mpx()->pending_scribes().count(make_pair("jupiter", 8080)) == 1);
  CAF_MESSAGE("self: " << to_string(self()->address()));
  auto ax = accept_handle::from_int(4242);
  mpx()->provide_acceptor(4242, ax);
  publish(self(), 4242);
  mpx()->exec_runnable(); // process publish message in basp_broker
  // connect to mars
  connect_node(1, ax, self()->id());
  CAF_CHECK_EQUAL(tbl().lookup_direct(remote_node(1)).id(), remote_hdl(1).id());
  // now, an actor from jupiter sends a message to us via mars
  mock(remote_hdl(1),
       {basp::message_type::dispatch_message, 0, 0,
        remote_node(0), this_node(),
        pseudo_remote(0)->id(), self()->id()},
       make_message("hello from jupiter!"))
  .expect(remote_hdl(1),
          basp::message_type::dispatch_message, any_vals,
          static_cast<uint64_t>(atom("SpawnServ")),
          this_node(), remote_node(0),
          any_vals, std::numeric_limits<actor_id>::max(),
          make_message(sys_atom::value, get_atom::value, "info"))
  .expect(remote_hdl(1),
          basp::message_type::dispatch_message, any_vals,
          static_cast<uint64_t>(atom("ConfigServ")),
          this_node(), remote_node(0),
          any_vals, // actor ID of an actor spawned by the BASP broker
          std::numeric_limits<actor_id>::max(),
          make_message(get_atom::value, "basp.default-connectivity"))
  .expect(remote_hdl(1),
          basp::message_type::announce_proxy_instance, uint32_t{0}, uint64_t{0},
          this_node(), remote_node(0),
          invalid_actor_id, pseudo_remote(0)->id());
  CAF_CHECK_EQUAL(mpx()->output_buffer(remote_hdl(1)).size(), 0);
  CAF_CHECK_EQUAL(tbl().lookup_indirect(remote_node(0)), remote_node(1));
  CAF_CHECK_EQUAL(tbl().lookup_indirect(remote_node(1)), invalid_node_id);
  auto connection_helper = abstract_actor::latest_actor_id();
  CAF_CHECK_EQUAL(mpx()->output_buffer(remote_hdl(1)).size(), 0);
  // create a dummy config server and respond to the name lookup
  CAF_MESSAGE("receive ConfigServ of jupiter");
  network::address_listing res;
  res[network::protocol::ipv4].push_back("jupiter");
  mock(remote_hdl(1),
       {basp::message_type::dispatch_message, 0, 0,
        this_node(), this_node(),
        invalid_actor_id, connection_helper},
       make_message(ok_atom::value, "basp.default-connectivity",
                    make_message(uint16_t{8080}, std::move(res))));
  // our connection helper should now connect to jupiter and
  // send the scribe handle over to the BASP broker
  mpx()->exec_runnable();
  CAF_CHECK_EQUAL(mpx()->output_buffer(remote_hdl(1)).size(), 0);
  CAF_CHECK(mpx()->pending_scribes().count(make_pair("jupiter", 8080)) == 0);
  // send handshake from jupiter
  mock(remote_hdl(0),
       {basp::message_type::server_handshake, 0, basp::version,
        remote_node(0), invalid_node_id,
        pseudo_remote(0)->id(), invalid_actor_id},
       pseudo_remote(0)->id(),
       uint32_t{0})
  .expect(remote_hdl(0),
          basp::message_type::client_handshake, uint32_t{0}, uint64_t{0},
          this_node(), remote_node(0),
          invalid_actor_id, invalid_actor_id);
  CAF_CHECK_EQUAL(tbl().lookup_indirect(remote_node(0)), invalid_node_id);
  CAF_CHECK_EQUAL(tbl().lookup_indirect(remote_node(1)), invalid_node_id);
  CAF_CHECK_EQUAL(tbl().lookup_direct(remote_node(0)).id(), remote_hdl(0).id());
  CAF_CHECK_EQUAL(tbl().lookup_direct(remote_node(1)).id(), remote_hdl(1).id());
  CAF_MESSAGE("receive message from jupiter");
  self()->receive(
    [](const std::string& str) -> std::string {
      CAF_CHECK(str == "hello from jupiter!");
      return "hello from earth!";
    },
    THROW_ON_UNEXPECTED(self())
  );
  mpx()->exec_runnable(); // process forwarded message in basp_broker
  CAF_MESSAGE("response message must take direct route now");
  mock()
  .expect(remote_hdl(0),
          basp::message_type::dispatch_message, any_vals, uint64_t{0},
          this_node(), remote_node(0),
          self()->id(), pseudo_remote(0)->id(),
          make_message("hello from earth!"));
  CAF_CHECK(mpx()->output_buffer(remote_hdl(1)).size() == 0);
}

CAF_TEST_FIXTURE_SCOPE_END()
