/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/io/all.hpp"
#include "caf/io/network/test_multiplexer.hpp"

#include "caf/test/dsl.hpp"

namespace {

/// A fixture containing all required state to simulate a single CAF node.
template <class BaseFixture =
            test_coordinator_fixture<caf::actor_system_config>>
class test_node_fixture : public BaseFixture {
public:
  using super = BaseFixture;

  using exec_all_nodes_fun = std::function<void ()>;

  exec_all_nodes_fun exec_all_nodes;
  caf::io::middleman& mm;
  caf::io::network::test_multiplexer& mpx;

  /// @param fun A function object for delegating to the parent's `exec_all`.
  test_node_fixture(exec_all_nodes_fun fun)
      : exec_all_nodes(std::move(fun)),
        mm(this->sys.middleman()),
        mpx(dynamic_cast<caf::io::network::test_multiplexer&>(mm.backend())) {
    // nop
  }

  // Convenience function for transmitting all "network" traffic and running
  // all executables on this node.
  void exec_all() {
    while (mpx.try_exec_runnable() || mpx.read_data()
           || mpx.try_accept_connection() || this->sched.try_run_once()) {
      // rince and repeat
    }
  }

  /// Convenience function for calling `mm.publish` and requiring a valid
  /// result.
  template <class Handle>
  uint16_t publish(Handle whom, uint16_t port, const char* in = nullptr,
                   bool reuse = false) {
    this->sched.inline_next_enqueue();
    auto res = mm.publish(whom, port, in, reuse);
    CAF_REQUIRE(res);
    return *res;
  }

  /// Convenience function for calling `mm.remote_actor` and requiring a valid
  /// result.
  template <class Handle = caf::actor>
  Handle remote_actor(std::string host, uint16_t port) {
    this->sched.inline_next_enqueue();
    this->sched.after_next_enqueue(exec_all_nodes);
    auto res = mm.remote_actor<Handle>(std::move(host), port);
    CAF_REQUIRE(res);
    return *res;
  }

private:
  caf::io::basp_broker* get_basp_broker() {
    auto hdl = mm.named_broker<caf::io::basp_broker>(caf::atom("BASP"));
    return dynamic_cast<caf::io::basp_broker*>(
      caf::actor_cast<caf::abstract_actor*>(hdl));
  }
};

template <class Iterator>
void exec_all_fixtures(Iterator first, Iterator last) {
  using fixture_ptr = caf::detail::decay_t<decltype(*first)>;
  auto advance = [](fixture_ptr x) {
    return x->sched.try_run_once() || x->mpx.read_data()
           || x->mpx.try_exec_runnable() || x->mpx.try_accept_connection();
  };
  auto trigger_timeouts = [](fixture_ptr x) {
    auto& sched = x->sched;
    sched.clock().current_time += x->credit_round_interval;
    sched.dispatch();
  };
  for (;;) {
    // Exhaust all messages in the system.
    while (std::any_of(first, last, advance))
      ; // repeat
    // Try to "revive" the system by dispatching timeouts.
    std::for_each(first, last, trigger_timeouts);
    // Stop if the timeouts didn't cause new activity.
    if (std::none_of(first, last, advance))
      return;
  }
}


/// Binds `test_coordinator_fixture<Config>` to `test_node_fixture`.
template <class Config = caf::actor_system_config>
using test_node_fixture_t = test_node_fixture<test_coordinator_fixture<Config>>;

/// Base fixture for simulated network settings with any number of CAF nodes.
template <class PlanetType>
class fake_network_fixture_base {
public:
  using planets_vector = std::vector<PlanetType*>;

  using connection_handle = caf::io::connection_handle;

  using accept_handle = caf::io::accept_handle;

  fake_network_fixture_base(planets_vector xs) : planets_(std::move(xs)) {
    // nop
  }

  /// Returns a unique acceptor handle.
  accept_handle next_accept_handle() {
    return accept_handle::from_int(++hdl_id_);
  }

  /// Returns a unique connection handle.
  connection_handle next_connection_handle() {
    return connection_handle::from_int(++hdl_id_);
  }

  /// Prepare a connection from `client` (calls `remote_actor`) to `server`
  /// (calls `publish`).
  /// @returns randomly picked connection handles for the server and the client.
  std::pair<connection_handle, connection_handle>
  prepare_connection(PlanetType& server, PlanetType& client,
                     std::string host, uint16_t port,
                     accept_handle server_accept_hdl) {
    auto server_hdl = next_connection_handle();
    auto client_hdl = next_connection_handle();
    server.mpx.prepare_connection(server_accept_hdl, server_hdl, client.mpx,
                                  std::move(host), port, client_hdl);
    return std::make_pair(server_hdl, client_hdl);
  }

  /// Prepare a connection from `client` (calls `remote_actor`) to `server`
  /// (calls `publish`).
  /// @returns randomly picked connection handles for the server and the client.
  std::pair<connection_handle, connection_handle>
  prepare_connection(PlanetType& server, PlanetType& client,
                     std::string host, uint16_t port) {
    return prepare_connection(server, client, std::move(host), port,
                              next_accept_handle());
  }

  // Convenience function for transmitting all "network" traffic (no new
  // connections are accepted).
  void network_traffic() {
    auto f = [](PlanetType* x) {
      return x->mpx.try_exec_runnable() || x->mpx.read_data();
    };
    while (std::any_of(std::begin(planets_), std::end(planets_), f))
      ; // repeat
  }

  // Convenience function for transmitting all "network" traffic, trying to
  // accept all pending connections, and running all broker and regular actor
  // messages.
  void exec_all() {
    CAF_LOG_TRACE("");
    exec_all_fixtures(std::begin(planets_), std::end(planets_));
  }

  /// Type-erased callback for calling `exec_all`.
  std::function<void ()> exec_all_callback() {
    return [&] { exec_all(); };
  }

  void loop_after_next_enqueue(PlanetType& planet) {
    planet.sched.after_next_enqueue(exec_all_callback());
  }

private:
  int64_t hdl_id_ = 0;
  std::vector<PlanetType*> planets_;
};

/// A simple fixture that includes two nodes (`earth` and `mars`) that can
/// connect to each other.
template <class BaseFixture =
            test_coordinator_fixture<caf::actor_system_config>>
class point_to_point_fixture
    : public fake_network_fixture_base<test_node_fixture<BaseFixture>> {
public:
  using planet_type = test_node_fixture<BaseFixture>;

  using super = fake_network_fixture_base<planet_type>;

  planet_type earth;
  planet_type mars;

  point_to_point_fixture()
    : super({&earth, &mars}),
      earth(this->exec_all_callback()),
      mars(this->exec_all_callback()) {
    // Run initialization code.
    this->exec_all();
  }
};

/// Binds `test_coordinator_fixture<Config>` to `point_to_point_fixture`.
template <class Config = caf::actor_system_config>
using point_to_point_fixture_t =
  point_to_point_fixture<test_coordinator_fixture<Config>>;

/// A simple fixture that includes three nodes (`earth`, `mars`, and `jupiter`)
/// that can connect to each other.
template <class BaseFixture =
            test_coordinator_fixture<caf::actor_system_config>>
class belt_fixture
    : public fake_network_fixture_base<test_node_fixture<BaseFixture>> {
public:
  using planet_type = test_node_fixture<BaseFixture>;

  using super = fake_network_fixture_base<planet_type>;

  planet_type earth;
  planet_type mars;
  planet_type jupiter;

  belt_fixture()
    : super({&earth, &mars, &jupiter}),
      earth(this->exec_all_callback()),
      mars(this->exec_all_callback()),
      jupiter(this->exec_all_callback()) {
    // nop
  }
};

/// Binds `test_coordinator_fixture<Config>` to `belt_fixture`.
template <class Config = caf::actor_system_config>
using belt_fixture_t =
  belt_fixture<test_coordinator_fixture<Config>>;

}// namespace <anonymous>

#define expect_on(where, types, fields)                                        \
  CAF_MESSAGE(#where << ": expect" << #types << "." << #fields);               \
  expect_clause< CAF_EXPAND(CAF_DSL_LIST types) >{where . sched} . fields

#define disallow_on(where, types, fields)                                      \
  CAF_MESSAGE(#where << ": disallow" << #types << "." << #fields);             \
  disallow_clause< CAF_EXPAND(CAF_DSL_LIST types) >{where . sched} . fields
