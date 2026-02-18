// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms or copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/socket_manager.hpp"

#include "caf/test/scenario.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/fwd.hpp"
#include "caf/telemetry/gauge.hpp"
#include "caf/telemetry/metric_registry.hpp"

#include <source_location>
#include <span>
#include <string_view>

using namespace caf;
using namespace std::literals;

namespace {

using loc_t = std::source_location;

struct fixture {
  using stream_socket = caf::net::stream_socket;

  using socket_guard_t = caf::net::socket_guard<caf::net::stream_socket>;

  static caf::actor_system_config& init(caf::actor_system_config& cfg) {
    cfg.load<caf::net::middleman>();
    return cfg;
  }

  fixture() : sys(init(cfg)) {
    // Create a pair of connected sockets.
    auto handles = caf::net::make_stream_socket_pair();
    if (!handles) {
      auto str = caf::detail::format("failed to create socket pair: {}",
                                     handles.error());
      throw std::logic_error(str);
    }
    fd1.reset(handles->first);
    fd2.reset(handles->second);
    // Set a 1s timeout for next_message().
    if (auto err = caf::net::receive_timeout(fd2.get(), 1s); err.valid()) {
      auto str = caf::detail::format("failed to set socket timeout: {}", err);
      throw std::logic_error(str);
    }
    if (auto err = caf::net::allow_sigpipe(fd2.get(), false); err.valid()) {
      auto str = caf::detail::format("failed to disable sigpipe: {}", err);
      throw std::logic_error(str);
    }
  }

  caf::actor_system_config cfg;

  caf::actor_system sys;

  socket_guard_t fd1;

  socket_guard_t fd2;

  auto& metrics() {
    return sys.metrics();
  }

  /// Starts a connection on top of an LPF transport.
  template <class EventLayer>
  net::socket_manager_ptr
  start(socket_guard_t& fd, loc_t loc = loc_t::current()) {
    auto layer = std::make_unique<EventLayer>(fd.get(), &metrics());
    auto* mpx = sys.network_manager().mpx_ptr();
    auto mgr = caf::net::socket_manager::make(mpx, std::move(layer));
    if (!mpx->start(mgr)) {
      auto& this_runnable = caf::test::runnable::current();
      this_runnable.fail({"failed to start the socket manager", loc});
    }
    fd.release();
    return mgr;
  }

  /// Receives a text frame from the socket.
  auto receive_text(socket_guard_t& fd, loc_t loc = loc_t::current()) {
    auto& this_runnable = caf::test::runnable::current();
    std::vector<std::byte> buf;
    buf.resize(4);
    if (caf::net::read(fd.get(), std::span{buf}) != 4) {
      this_runnable.fail({"failed to read message size: {}", loc},
                         caf::net::last_socket_error_as_string());
    }
    auto len = uint32_t{0};
    std::memcpy(&len, buf.data(), 4);
    len = caf::detail::from_network_order(len);
    buf.resize(len);
    if (caf::net::read(fd.get(), std::span{buf}) != static_cast<int32_t>(len)) {
      this_runnable.fail({"failed to read message payload: {}", loc},
                         caf::net::last_socket_error_as_string());
    }
    return std::string{reinterpret_cast<const char*>(buf.data()), len};
  }

  bool read_returns_0(socket_guard_t& fd) {
    std::vector<std::byte> buf;
    buf.resize(4);
    return caf::net::read(fd.get(), std::span{buf}) == 0;
  }

  /// Sends a text frame to the socket.
  auto send_text(socket_guard_t& fd, std::string_view str,
                 loc_t loc = loc_t::current()) {
    auto& this_runnable = caf::test::runnable::current();
    std::vector<std::byte> buf;
    auto len = static_cast<uint32_t>(str.size());
    len = caf::detail::to_network_order(len);
    buf.resize(4);
    std::memcpy(buf.data(), &len, 4);
    auto bytes = std::as_bytes(std::span{str});
    buf.insert(buf.end(), bytes.begin(), bytes.end());
    auto written = caf::net::write(fd.get(), std::span{buf});
    if (written != static_cast<ptrdiff_t>(buf.size())) {
      this_runnable.fail({"failed to write message: {}", loc},
                         caf::net::last_socket_error_as_string());
    }
  }
};

/// Event layer that shuts down the manager when data is received.
class test_event_layer : public net::socket_event_layer {
public:
  test_event_layer(net::stream_socket fd, telemetry::metric_registry* metrics)
    : fd_(fd), metrics_(metrics) {
    metrics_->counter_singleton("test", "constructed", "test")->inc();
  }

  ~test_event_layer() override {
    metrics_->counter_singleton("test", "destructed", "test")->inc();
  }

  error start(net::socket_manager* mgr) override {
    mgr_ = mgr;
    metrics_->counter_singleton("test", "started", "test")->inc();
    return {};
  }

  net::socket handle() const override {
    return fd_;
  }

  void handle_read_event() override {
    // nop
  }

  void handle_write_event() override {
    // nop
  }

  void abort(const error&) override {
    // nop
  }

protected:
  net::stream_socket fd_;
  telemetry::metric_registry* metrics_;
  net::socket_manager* mgr_ = nullptr;
};

} // namespace

WITH_FIXTURE(fixture) {

/// Event layer that shuts down the manager when data is received.
class shutdown_on_read_layer : public test_event_layer {
public:
  using super = test_event_layer;

  using super::super;

  error start(net::socket_manager* mgr) override {
    std::ignore = super::start(mgr);
    mgr->register_reading();
    return {};
  }

  void handle_read_event() override {
    metrics_->counter_singleton("test", "read-events", "test")->inc();
    mgr_->shutdown();
  }
};

SCENARIO("shutting down on read event cleans up the manager") {
  GIVEN("an event layer that shuts down the manager on read event") {
    auto mgr = start<shutdown_on_read_layer>(fd1);
    check_metric_eq(metrics(), "test", "constructed", 1);
    check_metric_eq(metrics(), "test", "started", 1);
    WHEN("data is sent to the socket") {
      caf::net::write(fd2.get(), std::as_bytes(std::span{"hello"sv}));
      THEN("the manager shuts down after the read event") {
        check_metric_eq(metrics(), "test", "read-events", 1);
        check(read_returns_0(fd2));
        check_metric_eq(metrics(), "test", "destructed", 1);
      }
    }
  }
}

/// Event layer that shuts down the manager when data is received.
class shutdown_on_write_layer : public test_event_layer {
public:
  using super = test_event_layer;

  using super::super;

  error start(net::socket_manager* mgr) override {
    std::ignore = super::start(mgr);
    mgr->register_writing();
    return {};
  }

  void handle_write_event() override {
    metrics_->counter_singleton("test", "write-events", "test")->inc();
    mgr_->shutdown();
  }
};

SCENARIO("shutting down on write event cleans up the manager") {
  GIVEN("an event layer that shuts down the manager on write event") {
    auto mgr = start<shutdown_on_write_layer>(fd1);
    check_metric_eq(metrics(), "test", "constructed", 1);
    check_metric_eq(metrics(), "test", "started", 1);
    WHEN("the test layer receives a write event") {
      check_metric_eq(metrics(), "test", "write-events", 1);
      THEN("the manager shuts down") {
        check(read_returns_0(fd2));
        check_metric_eq(metrics(), "test", "destructed", 1);
      }
    }
  }
}

/// Event layer that shuts down the manager in a delayed action.
class shutdown_on_delayed_action_layer : public test_event_layer {
public:
  using super = test_event_layer;

  using super::super;

  error start(net::socket_manager* mgr) override {
    std::ignore = super::start(mgr);
    mgr->delay(make_single_shot_action([this] {
      metrics_->counter_singleton("test", "delayed-action", "test")->inc();
      mgr_->shutdown();
    }));
    return {};
  }
};

SCENARIO("shutting down on delayed action cleans up the manager") {
  GIVEN("an event layer that shuts down the manager in a delayed action") {
    auto mgr = start<shutdown_on_delayed_action_layer>(fd1);
    check_metric_eq(metrics(), "test", "constructed", 1);
    check_metric_eq(metrics(), "test", "started", 1);
    WHEN("the test layer runs the delayed action") {
      check_metric_eq(metrics(), "test", "delayed-action", 1);
      THEN("the manager shuts down") {
        check(read_returns_0(fd2));
        check_metric_eq(metrics(), "test", "destructed", 1);
      }
    }
  }
}

/// Event layer that shuts down the manager in a delayed action.
class shutdown_on_scheduled_action_layer : public test_event_layer {
public:
  using super = test_event_layer;

  using super::super;

  error start(net::socket_manager* mgr) override {
    std::ignore = super::start(mgr);
    mgr->schedule(make_single_shot_action([this] {
      metrics_->counter_singleton("test", "scheduled-action", "test")->inc();
      mgr_->shutdown();
    }));
    return {};
  }
};

SCENARIO("shutting down on scheduled action cleans up the manager") {
  GIVEN("an event layer that shuts down the manager in a scheduled action") {
    auto mgr = start<shutdown_on_scheduled_action_layer>(fd1);
    check_metric_eq(metrics(), "test", "constructed", 1);
    check_metric_eq(metrics(), "test", "started", 1);
    WHEN("the test layer runs the scheduled action") {
      check_metric_eq(metrics(), "test", "scheduled-action", 1);
      THEN("the manager shuts down") {
        check(read_returns_0(fd2));
        check_metric_eq(metrics(), "test", "destructed", 1);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)
