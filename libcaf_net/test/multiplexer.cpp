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

#define CAF_SUITE multiplexer

#include "caf/net/multiplexer.hpp"

#include "caf/net/test/host_fixture.hpp"
#include "caf/test/dsl.hpp"

#include <new>
#include <tuple>

#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/span.hpp"

using namespace caf;
using namespace caf::net;

namespace {

class dummy_manager : public socket_manager {
public:
  dummy_manager(size_t& manager_count, stream_socket handle,
                multiplexer_ptr parent)
    : socket_manager(handle, std::move(parent)),
      count_(manager_count),
      rd_buf_pos_(0) {
    ++count_;
    rd_buf_.resize(1024);
  }

  ~dummy_manager() {
    --count_;
  }

  stream_socket handle() const noexcept {
    return socket_cast<stream_socket>(handle_);
  }

  bool handle_read_event() override {
    if (read_capacity() < 1024)
      rd_buf_.resize(rd_buf_.size() + 2048);
    auto num_bytes = read(handle(),
                          make_span(read_position_begin(), read_capacity()));
    if (num_bytes > 0) {
      CAF_ASSERT(num_bytes > 0);
      rd_buf_pos_ += num_bytes;
      return true;
    }
    return num_bytes < 0 && last_socket_error_is_temporary();
  }

  bool handle_write_event() override {
    if (wr_buf_.size() == 0)
      return false;
    auto num_bytes = write(handle(), wr_buf_);
    if (num_bytes > 0) {
      wr_buf_.erase(wr_buf_.begin(), wr_buf_.begin() + num_bytes);
      return wr_buf_.size() > 0;
    }
    return num_bytes < 0 && last_socket_error_is_temporary();
  }

  void handle_error(sec code) override {
    CAF_FAIL("handle_error called with code " << code);
  }

  void send(string_view x) {
    auto x_bytes = as_bytes(make_span(x));
    wr_buf_.insert(wr_buf_.end(), x_bytes.begin(), x_bytes.end());
  }

  std::string receive() {
    std::string result(reinterpret_cast<char*>(rd_buf_.data()), rd_buf_pos_);
    rd_buf_pos_ = 0;
    return result;
  }

private:
  byte* read_position_begin() {
    return rd_buf_.data() + rd_buf_pos_;
  }

  byte* read_position_end() {
    return rd_buf_.data() + rd_buf_.size();
  }

  size_t read_capacity() const {
    return rd_buf_.size() - rd_buf_pos_;
  }

  size_t& count_;

  size_t rd_buf_pos_;

  byte_buffer wr_buf_;

  byte_buffer rd_buf_;
};

using dummy_manager_ptr = intrusive_ptr<dummy_manager>;

struct fixture : host_fixture {
  fixture() : manager_count(0), mpx(std::make_shared<multiplexer>()) {
    mpx->set_thread_id();
  }

  ~fixture() {
    mpx.reset();
    CAF_REQUIRE_EQUAL(manager_count, 0u);
  }

  void exhaust() {
    while (mpx->poll_once(false))
      ; // Repeat.
  }

  size_t manager_count;

  multiplexer_ptr mpx;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(multiplexer_tests, fixture)

CAF_TEST(default construction) {
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 0u);
}

CAF_TEST(init) {
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 0u);
  CAF_REQUIRE_EQUAL(mpx->init(), none);
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 1u);
  mpx->close_pipe();
  exhaust();
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 0u);
  // Calling run must have no effect now.
  mpx->run();
}

CAF_TEST(send and receive) {
  CAF_REQUIRE_EQUAL(mpx->init(), none);
  auto sockets = unbox(make_stream_socket_pair());
  auto alice = make_counted<dummy_manager>(manager_count, sockets.first, mpx);
  auto bob = make_counted<dummy_manager>(manager_count, sockets.second, mpx);
  alice->register_reading();
  bob->register_reading();
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 3u);
  alice->send("hello bob");
  alice->register_writing();
  exhaust();
  CAF_CHECK_EQUAL(bob->receive(), "hello bob");
}

CAF_TEST(shutdown) {
  std::mutex m;
  std::condition_variable cv;
  bool thread_id_set = false;
  auto run_mpx = [&] {
    std::unique_lock<std::mutex> lk(m);
    mpx->set_thread_id();
    thread_id_set = true;
    lk.unlock();
    cv.notify_one();
    mpx->run();
  };
  CAF_REQUIRE_EQUAL(mpx->init(), none);
  auto sockets = unbox(make_stream_socket_pair());
  auto alice = make_counted<dummy_manager>(manager_count, sockets.first, mpx);
  auto bob = make_counted<dummy_manager>(manager_count, sockets.second, mpx);
  alice->register_reading();
  bob->register_reading();
  CAF_REQUIRE_EQUAL(mpx->num_socket_managers(), 3u);
  std::thread mpx_thread{run_mpx};
  std::unique_lock<std::mutex> lk(m);
  cv.wait(lk, [&] { return thread_id_set; });
  mpx->shutdown();
  mpx_thread.join();
  CAF_REQUIRE_EQUAL(mpx->num_socket_managers(), 0u);
}

CAF_TEST_FIXTURE_SCOPE_END()
