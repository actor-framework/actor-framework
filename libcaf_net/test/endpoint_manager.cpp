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

#define CAF_SUITE endpoint_manager

#include "caf/net/endpoint_manager.hpp"

#include "caf/test/dsl.hpp"

#include "host_fixture.hpp"

#include "caf/detail/scope_guard.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/stream_socket.hpp"

using namespace caf;
using namespace caf::net;

namespace {

string_view hello_manager{"hello manager!"};

string_view hello_test{"hello test!"};

struct fixture : test_coordinator_fixture<>, host_fixture {
  fixture() {
    mpx = std::make_shared<multiplexer>();
    if (auto err = mpx->init())
      CAF_FAIL("mpx->init failed: " << sys.render(err));
  }

  multiplexer_ptr mpx;
};

class dummy_application {};

class dummy_transport {
public:
  dummy_transport(stream_socket handle, std::shared_ptr<std::vector<char>> data)
    : handle_(handle), data_(data), read_buf_(1024) {
    // nop
  }

  stream_socket handle() {
    return handle_;
  }

  template <class Manager>
  error init(Manager& manager) {
    write_buf_.insert(write_buf_.end(), hello_test.begin(), hello_test.end());
    CAF_CHECK(manager.mask_add(operation::read_write));
    return none;
  }

  template <class Manager>
  bool handle_read_event(Manager&) {
    auto res = read(handle_, read_buf_.data(), read_buf_.size());
    if (auto num_bytes = get_if<size_t>(&res)) {
      data_->insert(data_->end(), read_buf_.begin(),
                    read_buf_.begin() + *num_bytes);
      return true;
    }
    return get<sec>(res) == sec::unavailable_or_would_block;
  }

  template <class Manager>
  bool handle_write_event(Manager&) {
    auto res = write(handle_, write_buf_.data(), write_buf_.size());
    if (auto num_bytes = get_if<size_t>(&res)) {
      write_buf_.erase(write_buf_.begin(), write_buf_.begin() + *num_bytes);
      return write_buf_.size() > 0;
    }
    return get<sec>(res) == sec::unavailable_or_would_block;
  }

  template <class Manager>
  void handle_error(Manager&, sec) {
    // nop
  }

  template <class Manager>
  void resolve(Manager&, std::string path, actor listener) {
    anon_send(listener, resolve_atom::value, std::move(path),
              make_error(sec::feature_disabled));
  }

  template <class Manager>
  void timeout(Manager&, atom_value, uint64_t) {
    // nop
  }

private:
  stream_socket handle_;

  std::shared_ptr<std::vector<char>> data_;

  std::vector<char> read_buf_;

  std::vector<char> write_buf_;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(endpoint_manager_tests, fixture)

CAF_TEST(send and receive) {
  std::vector<char> read_buf(1024);
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 1u);
  auto buf = std::make_shared<std::vector<char>>();
  auto sockets = unbox(make_stream_socket_pair());
  nonblocking(sockets.second, true);
  CAF_CHECK_EQUAL(read(sockets.second, read_buf.data(), read_buf.size()),
                  sec::unavailable_or_would_block);
  auto guard = detail::make_scope_guard([&] { close(sockets.second); });
  auto mgr = make_endpoint_manager(mpx, sys,
                                   dummy_transport{sockets.first, buf},
                                   dummy_application{});
  CAF_CHECK_EQUAL(mgr->init(), none);
  mpx->handle_updates();
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 2u);
  CAF_CHECK_EQUAL(write(sockets.second, hello_manager.data(),
                        hello_manager.size()),
                  hello_manager.size());
  while (mpx->poll_once(false))
    ; // Repeat.
  CAF_CHECK_EQUAL(string_view(buf->data(), buf->size()), hello_manager);
  CAF_CHECK_EQUAL(read(sockets.second, read_buf.data(), read_buf.size()),
                  hello_test.size());
  CAF_CHECK_EQUAL(string_view(read_buf.data(), hello_test.size()), hello_test);
}

CAF_TEST_FIXTURE_SCOPE_END()
