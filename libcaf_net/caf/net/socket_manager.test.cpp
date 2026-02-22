// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms or copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/socket_manager.hpp"

#include "caf/test/scenario.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/socket.hpp"
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
    caf::test::runnable::current().current_metric_registry(&sys.metrics());
    // Create a pair of connected sockets.
    auto handles = caf::net::make_stream_socket_pair();
    if (!handles) {
      auto str = caf::detail::format("failed to create socket pair: {}",
                                     handles.error());
      CAF_RAISE_ERROR(std::logic_error, str.c_str());
    }
    fd1.reset(handles->first);
    fd2.reset(handles->second);
    // Set a 1s timeout for next_message().
    if (auto err = caf::net::receive_timeout(fd2.get(), 1s); err.valid()) {
      auto str = caf::detail::format("failed to set socket timeout: {}", err);
      CAF_RAISE_ERROR(std::logic_error, str.c_str());
    }
    if (auto err = caf::net::allow_sigpipe(fd2.get(), false); err.valid()) {
      auto str = caf::detail::format("failed to disable sigpipe: {}", err);
      CAF_RAISE_ERROR(std::logic_error, str.c_str());
    }
  }

  caf::actor_system_config cfg;

  caf::actor_system sys;

  socket_guard_t fd1;

  socket_guard_t fd2;

  /// Starts a connection on top of an LPF transport.
  template <class EventLayer>
  net::socket_manager_ptr
  start(socket_guard_t& fd, loc_t loc = loc_t::current()) {
    auto layer = std::make_unique<EventLayer>(fd.get(), &sys.metrics());
    auto* mpx = sys.network_manager().mpx_ptr();
    auto mgr = caf::net::socket_manager::make(mpx, std::move(layer));
    if (!mpx->start(mgr)) {
      auto& this_runnable = caf::test::runnable::current();
      this_runnable.fail({"failed to start the socket manager", loc});
    }
    fd.release();
    return mgr;
  }

  /// Checks whether the peer closes the connection.
  bool connection_closes(socket_guard_t& fd) {
    std::vector<std::byte> buf;
    buf.resize(4);
    auto res = caf::net::read(fd.get(), std::span{buf});
    if (res == 0) {
      return true;
    }
    if (res > 0) {
      auto& reporter = caf::test::reporter::instance();
      reporter.println(caf::log::level::warning,
                       "read returned {} bytes, expected 0 (EOF)", res);
      return false;
    }
    if (caf::net::last_socket_error() == std::errc::connection_reset) {
      return true;
    }
    auto err_str = caf::net::last_socket_error_as_string();
    auto& reporter = caf::test::reporter::instance();
    reporter.println(caf::log::level::warning, "read failed: {}", err_str);
    return false;
  }
};

/// Base class for test event layers.
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

SCENARIO("shutting down on read event") {
  GIVEN("an event layer that shuts down the manager on read event") {
    start<shutdown_on_read_layer>(fd1);
    require_metric_eq("test", "constructed", 1);
    require_metric_eq("test", "started", 1);
    WHEN("data is sent to the socket") {
      caf::net::write(fd2.get(), std::as_bytes(std::span{"hello"sv}));
      THEN("the manager shuts down after the read event") {
        require_metric_eq("test", "read-events", 1);
        require_metric_eq("test", "destructed", 1);
        check(connection_closes(fd2));
      }
    }
  }
}

/// Event layer that schedules shutdown via delay() in handle_read_event.
class shutdown_on_read_delayed_layer : public test_event_layer {
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
    mgr_->delay(make_single_shot_action([this] { mgr_->shutdown(); }));
    mgr_->deregister_reading();
  }
};

SCENARIO("shutting down on delayed action from read event") {
  GIVEN("an event layer that delays shutdown on read event") {
    start<shutdown_on_read_delayed_layer>(fd1);
    require_metric_eq("test", "constructed", 1);
    require_metric_eq("test", "started", 1);
    WHEN("data is sent to the socket") {
      caf::net::write(fd2.get(), std::as_bytes(std::span{"hello"sv}));
      THEN("the manager shuts down after the read event") {
        require_metric_eq("test", "read-events", 1);
        require_metric_eq("test", "destructed", 1);
        check(connection_closes(fd2));
      }
    }
  }
}

/// Event layer that schedules shutdown via schedule() in handle_read_event.
class shutdown_on_read_scheduled_layer : public test_event_layer {
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
    mgr_->schedule(make_single_shot_action([this] { mgr_->shutdown(); }));
    mgr_->deregister_reading();
  }
};

SCENARIO("shutting down on scheduled action from read event") {
  GIVEN("an event layer that schedules shutdown on read event") {
    start<shutdown_on_read_scheduled_layer>(fd1);
    require_metric_eq("test", "constructed", 1);
    require_metric_eq("test", "started", 1);
    WHEN("data is sent to the socket") {
      caf::net::write(fd2.get(), std::as_bytes(std::span{"hello"sv}));
      THEN("the manager shuts down after the read event") {
        require_metric_eq("test", "read-events", 1);
        require_metric_eq("test", "destructed", 1);
        check(connection_closes(fd2));
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

SCENARIO("shutting down on write event") {
  GIVEN("an event layer that shuts down the manager on write event") {
    start<shutdown_on_write_layer>(fd1);
    require_metric_eq("test", "constructed", 1);
    require_metric_eq("test", "started", 1);
    WHEN("the test layer receives a write event") {
      require_metric_eq("test", "write-events", 1);
      THEN("the manager shuts down") {
        check(connection_closes(fd2));
        require_metric_eq("test", "destructed", 1);
      }
    }
  }
}

/// Event layer that schedules shutdown via delay() in handle_write_event.
class shutdown_on_write_delayed_layer : public test_event_layer {
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
    mgr_->delay(make_single_shot_action([this] { mgr_->shutdown(); }));
    mgr_->deregister_writing();
  }
};

SCENARIO("shutting down on delayed action from write event") {
  GIVEN("an event layer that delays shutdown on write event") {
    start<shutdown_on_write_delayed_layer>(fd1);
    require_metric_eq("test", "constructed", 1);
    require_metric_eq("test", "started", 1);
    WHEN("the test layer receives a write event") {
      require_metric_eq("test", "write-events", 1);
      THEN("the manager shuts down") {
        check(connection_closes(fd2));
        require_metric_eq("test", "destructed", 1);
      }
    }
  }
}

/// Event layer that schedules shutdown via schedule() in handle_write_event.
class shutdown_on_write_scheduled_layer : public test_event_layer {
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
    mgr_->schedule(make_single_shot_action([this] { mgr_->shutdown(); }));
    mgr_->deregister_writing();
  }
};

SCENARIO("shutting down on scheduled action from write event") {
  GIVEN("an event layer that schedules shutdown on write event") {
    start<shutdown_on_write_scheduled_layer>(fd1);
    require_metric_eq("test", "constructed", 1);
    require_metric_eq("test", "started", 1);
    WHEN("the test layer receives a write event") {
      require_metric_eq("test", "write-events", 1);
      THEN("the manager shuts down") {
        check(connection_closes(fd2));
        require_metric_eq("test", "destructed", 1);
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

SCENARIO("shutting down on delayed action") {
  GIVEN("an event layer that shuts down the manager in a delayed action") {
    start<shutdown_on_delayed_action_layer>(fd1);
    require_metric_eq("test", "constructed", 1);
    require_metric_eq("test", "started", 1);
    WHEN("the test layer runs the delayed action") {
      require_metric_eq("test", "delayed-action", 1);
      THEN("the manager shuts down") {
        check(connection_closes(fd2));
        require_metric_eq("test", "destructed", 1);
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

SCENARIO("shutting down on scheduled action") {
  GIVEN("an event layer that shuts down the manager in a scheduled action") {
    start<shutdown_on_scheduled_action_layer>(fd1);
    require_metric_eq("test", "constructed", 1);
    require_metric_eq("test", "started", 1);
    WHEN("the test layer runs the scheduled action") {
      require_metric_eq("test", "scheduled-action", 1);
      THEN("the manager shuts down") {
        check(connection_closes(fd2));
        require_metric_eq("test", "destructed", 1);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)
