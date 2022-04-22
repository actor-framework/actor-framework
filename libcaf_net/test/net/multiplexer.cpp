// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.multiplexer

#include "caf/net/multiplexer.hpp"

#include "net-test.hpp"

#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/span.hpp"

#include <new>
#include <string_view>
#include <tuple>

using namespace caf;
using namespace caf::net;

namespace {

using shared_atomic_count = std::shared_ptr<std::atomic<size_t>>;

class dummy_manager : public socket_manager {
public:
  // -- constructors, destructors, and assignment operators --------------------

  dummy_manager(stream_socket handle, multiplexer* parent, std::string name,
                shared_atomic_count count)
    : socket_manager(handle, parent), name(std::move(name)), count_(count) {
    MESSAGE("created new dummy manager");
    ++*count_;
    rd_buf_.resize(1024);
  }

  ~dummy_manager() {
    MESSAGE("destroyed dummy manager");
    --*count_;
  }

  // -- properties -------------------------------------------------------------

  stream_socket handle() const noexcept {
    return socket_cast<stream_socket>(handle_);
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

  // -- interface functions ----------------------------------------------------

  error init(const settings&) override {
    return none;
  }

  read_result handle_read_event() override {
    if (trigger_handover) {
      MESSAGE(name << " triggered a handover");
      return read_result::handover;
    }
    if (read_capacity() < 1024)
      rd_buf_.resize(rd_buf_.size() + 2048);
    auto num_bytes = read(handle(),
                          make_span(read_position_begin(), read_capacity()));
    if (num_bytes > 0) {
      CAF_ASSERT(num_bytes > 0);
      rd_buf_pos_ += num_bytes;
      return read_result::again;
    } else if (num_bytes < 0 && last_socket_error_is_temporary()) {
      return read_result::again;
    } else {
      return read_result::stop;
    }
  }

  read_result handle_buffered_data() override {
    return read_result::again;
  }

  read_result handle_continue_reading() override {
    return read_result::again;
  }

  write_result handle_write_event() override {
    if (trigger_handover) {
      MESSAGE(name << " triggered a handover");
      return write_result::handover;
    }
    if (wr_buf_.size() == 0)
      return write_result::stop;
    auto num_bytes = write(handle(), wr_buf_);
    if (num_bytes > 0) {
      wr_buf_.erase(wr_buf_.begin(), wr_buf_.begin() + num_bytes);
      return wr_buf_.size() > 0 ? write_result::again : write_result::stop;
    }
    return num_bytes < 0 && last_socket_error_is_temporary()
             ? write_result::again
             : write_result::stop;
  }

  write_result handle_continue_writing() override {
    return write_result::again;
  }

  void handle_error(sec code) override {
    FAIL("handle_error called with code " << code);
  }

  socket_manager_ptr make_next_manager(socket handle) override {
    if (next != nullptr)
      FAIL("asked to do handover twice!");
    next = make_counted<dummy_manager>(socket_cast<stream_socket>(handle), mpx_,
                                       "Carl", count_);
    if (auto err = next->init(settings{}))
      FAIL("next->init failed: " << err);
    return next;
  }

  // --

  bool trigger_handover = false;

  intrusive_ptr<dummy_manager> next;

  std::string name;

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

  shared_atomic_count count_;

  size_t rd_buf_pos_ = 0;

  byte_buffer wr_buf_;

  byte_buffer rd_buf_;
};

using dummy_manager_ptr = intrusive_ptr<dummy_manager>;

struct fixture {
  fixture() : mpx(nullptr) {
    manager_count = std::make_shared<std::atomic<size_t>>(0);
    mpx.set_thread_id();
  }

  ~fixture() {
    mpx.shutdown();
    exhaust();
    REQUIRE_EQ(*manager_count, 0u);
  }

  void exhaust() {
    mpx.apply_updates();
    while (mpx.poll_once(false))
      ; // Repeat.
  }

  void apply_updates() {
    mpx.apply_updates();
  }

  auto make_manager(stream_socket fd, std::string name) {
    return make_counted<dummy_manager>(fd, &mpx, std::move(name),
                                       manager_count);
  }

  void init() {
    if (auto err = mpx.init())
      FAIL("mpx.init failed: " << err);
    exhaust();
  }

  shared_atomic_count manager_count;

  multiplexer mpx;
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("the multiplexer has no socket managers after default construction") {
  GIVEN("a default constructed multiplexer") {
    WHEN("querying the number of socket managers") {
      THEN("the result is 0") {
        CHECK_EQ(mpx.num_socket_managers(), 0u);
      }
    }
  }
}

SCENARIO("the multiplexer constructs the pollset updater while initializing") {
  GIVEN("an initialized multiplexer") {
    WHEN("querying the number of socket managers") {
      THEN("the result is 1") {
        CHECK_EQ(mpx.num_socket_managers(), 0u);
        CHECK_EQ(mpx.init(), none);
        exhaust();
        CHECK_EQ(mpx.num_socket_managers(), 1u);
      }
    }
  }
}

SCENARIO("socket managers can register for read and write operations") {
  GIVEN("an initialized multiplexer") {
    init();
    WHEN("socket managers register for read and write operations") {
      auto [alice_fd, bob_fd] = unbox(make_stream_socket_pair());
      auto alice = make_manager(alice_fd, "Alice");
      auto bob = make_manager(bob_fd, "Bob");
      alice->register_reading();
      bob->register_reading();
      apply_updates();
      CHECK_EQ(mpx.num_socket_managers(), 3u);
      THEN("the multiplexer runs callbacks on socket activity") {
        alice->send("Hello Bob!");
        alice->register_writing();
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
      mpx.set_thread_id();
      go_time->arrive_and_wait();
      mpx.run();
    }};
    go_time->arrive_and_wait();
    auto [alice_fd, bob_fd] = unbox(make_stream_socket_pair());
    auto alice = make_manager(alice_fd, "Alice");
    auto bob = make_manager(bob_fd, "Bob");
    alice->register_reading();
    bob->register_reading();
    WHEN("calling shutdown on the multiplexer") {
      mpx.shutdown();
      THEN("the thread terminates and all socket managers get shut down") {
        mpx_thread.join();
        CHECK(alice->read_closed());
        CHECK(bob->read_closed());
      }
    }
  }
}

SCENARIO("a multiplexer allows managers to perform socket handovers") {
  GIVEN("an initialized multiplexer") {
    init();
    WHEN("socket manager triggers a handover") {
      auto [alice_fd, bob_fd] = unbox(make_stream_socket_pair());
      auto alice = make_manager(alice_fd, "Alice");
      auto bob = make_manager(bob_fd, "Bob");
      alice->register_reading();
      bob->register_reading();
      apply_updates();
      CHECK_EQ(mpx.num_socket_managers(), 3u);
      THEN("the multiplexer swaps out the socket managers for the socket") {
        alice->send("Hello Bob!");
        alice->register_writing();
        exhaust();
        CHECK_EQ(bob->receive(), "Hello Bob!");
        bob->trigger_handover = true;
        alice->send("Hello Carl!");
        alice->register_writing();
        bob->register_reading();
        exhaust();
        CHECK_EQ(bob->receive(), "");
        CHECK_EQ(bob->handle(), invalid_socket);
        if (CHECK_NE(bob->next, nullptr)) {
          auto carl = bob->next;
          CHECK_EQ(carl->handle(), socket{bob_fd});
          carl->register_reading();
          exhaust();
          CHECK_EQ(carl->name, "Carl");
          CHECK_EQ(carl->receive(), "Hello Carl!");
        }
      }
    }
  }
}

END_FIXTURE_SCOPE()
