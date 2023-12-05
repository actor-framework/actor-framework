// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.multiplexer

#include "caf/net/multiplexer.hpp"

#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/byte_buffer.hpp"
#include "caf/span.hpp"

#include "net-test.hpp"

#include <new>
#include <string_view>
#include <tuple>

using namespace caf;

namespace {

using shared_count = std::shared_ptr<std::atomic<size_t>>;

class mock_event_layer : public net::socket_event_layer {
public:
  // -- constructors, destructors, and assignment operators --------------------

  mock_event_layer(net::stream_socket fd, std::string name, shared_count count)
    : name(std::move(name)), fd_(fd), count_(count) {
    MESSAGE("created new mock event layer");
    ++*count_;
    rd_buf_.resize(1024);
  }

  ~mock_event_layer() {
    MESSAGE("destroyed mock event layer");
    --*count_;
  }

  // -- factories --------------------------------------------------------------

  static auto make(net::stream_socket fd, std::string name,
                   shared_count count) {
    return std::make_unique<mock_event_layer>(fd, std::move(name),
                                              std::move(count));
  }

  // -- testing DSL ------------------------------------------------------------

  void send(std::string_view x) {
    auto x_bytes = as_bytes(make_span(x));
    wr_buf_.insert(wr_buf_.end(), x_bytes.begin(), x_bytes.end());
  }

  std::string receive() {
    std::string result(reinterpret_cast<char*>(rd_buf_.data()), rd_buf_pos_);
    rd_buf_pos_ = 0;
    return result;
  }

  // -- implementation of socket_event_layer -----------------------------------

  error start(net::socket_manager* mgr) override {
    mgr_ = mgr;
    return none;
  }

  net::socket handle() const override {
    return fd_;
  }

  void handle_read_event() override {
    if (read_capacity() < 1024)
      rd_buf_.resize(rd_buf_.size() + 2048);
    auto res = read(fd_, make_span(read_position_begin(), read_capacity()));
    if (res > 0) {
      CAF_ASSERT(res > 0);
      rd_buf_pos_ += res;
    } else if (res == 0 || !net::last_socket_error_is_temporary()) {
      mgr_->deregister();
    }
  }

  void handle_write_event() override {
    if (wr_buf_.size() == 0) {
      mgr_->deregister_writing();
    } else if (auto res = write(fd_, wr_buf_); res > 0) {
      wr_buf_.erase(wr_buf_.begin(), wr_buf_.begin() + res);
      if (wr_buf_.size() == 0)
        mgr_->deregister_writing();
    } else if (res == 0 || !net::last_socket_error_is_temporary()) {
      mgr_->deregister();
    }
  }

  void abort(const error& reason) override {
    abort_reason = reason;
  }

  std::string name;

  error abort_reason;

private:
  std::byte* read_position_begin() {
    return rd_buf_.data() + rd_buf_pos_;
  }

  std::byte* read_position_end() {
    return rd_buf_.data() + rd_buf_.size();
  }

  size_t read_capacity() const {
    return rd_buf_.size() - rd_buf_pos_;
  }
  net::stream_socket fd_;

  shared_count count_;

  size_t rd_buf_pos_ = 0;

  byte_buffer wr_buf_;

  byte_buffer rd_buf_;

  net::socket_manager* mgr_ = nullptr;
};

struct fixture {
  fixture() : mpx(net::multiplexer::make(nullptr)) {
    manager_count = std::make_shared<std::atomic<size_t>>(0);
    mpx->set_thread_id();
  }

  ~fixture() {
    mpx->shutdown();
    exhaust();
    REQUIRE_EQ(*manager_count, 0u);
  }

  void exhaust() {
    mpx->apply_updates();
    while (mpx->poll_once(false))
      ; // Repeat.
  }

  void apply_updates() {
    mpx->apply_updates();
  }

  std::pair<mock_event_layer*, net::socket_manager_ptr>
  make_manager(net::stream_socket fd, std::string name) {
    auto mock = mock_event_layer::make(fd, std::move(name), manager_count);
    auto mock_ptr = mock.get();
    auto mgr = net::socket_manager::make(mpx.get(), std::move(mock));
    std::ignore = mgr->start();
    return {mock_ptr, std::move(mgr)};
  }

  void init() {
    if (auto err = mpx->init())
      FAIL("mpx->init failed: " << err);
    exhaust();
  }

  shared_count manager_count;

  net::multiplexer_ptr mpx;
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("the multiplexer has no socket managers after default construction") {
  GIVEN("a default constructed multiplexer") {
    WHEN("querying the number of socket managers") {
      THEN("the result is 0") {
        CHECK_EQ(mpx->num_socket_managers(), 0u);
      }
    }
  }
}

SCENARIO("the multiplexer constructs the pollset updater while initializing") {
  GIVEN("an initialized multiplexer") {
    WHEN("querying the number of socket managers") {
      THEN("the result is 1") {
        CHECK_EQ(mpx->num_socket_managers(), 0u);
        CHECK_EQ(mpx->init(), none);
        exhaust();
        CHECK_EQ(mpx->num_socket_managers(), 1u);
      }
    }
  }
}

SCENARIO("socket managers can register for read and write operations") {
  GIVEN("an initialized multiplexer") {
    init();
    WHEN("socket managers register for read and write operations") {
      auto [alice_fd, bob_fd] = unbox(net::make_stream_socket_pair());
      auto [alice, alice_mgr] = make_manager(alice_fd, "Alice");
      auto [bob, bob_mgr] = make_manager(bob_fd, "Bob");
      alice_mgr->register_reading();
      bob_mgr->register_reading();
      apply_updates();
      CHECK_EQ(mpx->num_socket_managers(), 3u);
      THEN("the multiplexer runs callbacks on socket activity") {
        alice->send("Hello Bob!");
        alice_mgr->register_writing();
        exhaust();
        CHECK_EQ(bob->receive(), "Hello Bob!");
      }
    }
  }
}

SCENARIO("a multiplexer terminates its thread after shutting down") {
  GIVEN("a multiplexer running in its own thread and some socket managers") {
    init();
    auto go_time = std::make_shared<barrier>(2);
    auto mpx_thread = std::thread{[this, go_time] {
      mpx->set_thread_id();
      go_time->arrive_and_wait();
      mpx->run();
    }};
    go_time->arrive_and_wait();
    auto [alice_fd, bob_fd] = unbox(net::make_stream_socket_pair());
    auto [alice, alice_mgr] = make_manager(alice_fd, "Alice");
    auto [bob, bob_mgr] = make_manager(bob_fd, "Bob");
    alice_mgr->register_reading();
    bob_mgr->register_reading();
    WHEN("calling shutdown on the multiplexer") {
      mpx->shutdown();
      THEN("the thread terminates and all socket managers get shut down") {
        mpx_thread.join();
        CHECK(alice_mgr->disposed());
        CHECK(bob_mgr->disposed());
      }
    }
  }
}

// TODO: re-implement handovers
//
// SCENARIO("a multiplexer allows managers to perform socket handovers") {
//   GIVEN("an initialized multiplexer") {
//     init();
//     WHEN("socket manager triggers a handover") {
//       auto [alice_fd, bob_fd] = unbox(make_stream_socket_pair());
//       auto alice = make_manager(alice_fd, "Alice");
//       auto bob = make_manager(bob_fd, "Bob");
//       alice->register_reading();
//       bob->register_reading();
//       apply_updates();
//       CHECK_EQ(mpx->num_socket_managers(), 3u);
//       THEN("the multiplexer swaps out the socket managers for the socket") {
//         alice->send("Hello Bob!");
//         alice->register_writing();
//         exhaust();
//         CHECK_EQ(bob->receive(), "Hello Bob!");
//         bob->trigger_handover = true;
//         alice->send("Hello Carl!");
//         alice->register_writing();
//         bob->register_reading();
//         exhaust();
//         CHECK_EQ(bob->receive(), "");
//         CHECK_EQ(bob->handle(), invalid_socket);
//         if (CHECK_NE(bob->next, nullptr)) {
//           auto carl = bob->next;
//           CHECK_EQ(carl->handle(), socket{bob_fd});
//           carl->register_reading();
//           exhaust();
//           CHECK_EQ(carl->name, "Carl");
//           CHECK_EQ(carl->receive(), "Hello Carl!");
//         }
//       }
//     }
//   }
// }

END_FIXTURE_SCOPE()
