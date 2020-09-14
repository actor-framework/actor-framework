/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE net.web_socket

#include "caf/net/web_socket.hpp"

#include "caf/net/test/host_fixture.hpp"
#include "caf/test/dsl.hpp"

#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"

using namespace caf;

namespace {

using byte_span = span<const byte>;

struct app {
  std::vector<std::string> lines;

  template <class LowerLayer>
  error init(LowerLayer&, const settings&) {
    return none;
  }

  template <class LowerLayer>
  bool prepare_send(LowerLayer&) {
    return true;
  }

  template <class LowerLayer>
  bool done_sending(LowerLayer&) {
    return true;
  }

  template <class LowerLayer>
  void abort(LowerLayer&, const error& reason) {
    CAF_FAIL("app::abort called: " << reason);
  }

  template <class LowerLayer>
  ptrdiff_t consume(LowerLayer& down, byte_span buffer, byte_span) {
    constexpr auto nl = byte{'\n'};
    auto e = buffer.end();
    if (auto i = std::find(buffer.begin(), e, nl); i != e) {
      std::string str;
      auto string_size = static_cast<size_t>(std::distance(buffer.begin(), e));
      str.reserve(string_size);
      auto num_bytes = string_size + 1; // Also consume the newline character.
      std::transform(buffer.begin(), i, std::back_inserter(str),
                     [](byte x) { return static_cast<char>(x); });
      return num_bytes + consume(down, buffer.subspan(num_bytes), {});
    }
    return 0;
  }
};

struct fixture : host_fixture {
  fixture() {
    using namespace caf::net;
    mpx = std::make_shared<multiplexer>();
    mpx->set_thread_id();
    std::tie(sock.self, sock.mgr) = unbox(make_stream_socket_pair());
    auto ptr = make_socket_manager<app, web_socket, stream_transport>(sock.mgr,
                                                                      mpx);
    settings cfg;
    if (auto err = ptr->init(cfg))
      CAF_FAIL("initializing the socket manager failed: " << err);
    mgr = ptr;
  }

  ~fixture() {
    close(sock.self);
  }

  net::multiplexer_ptr mpx;

  net::socket_manager_ptr mgr;

  struct {
    net::stream_socket self;
    net::stream_socket mgr;
  } sock;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(web_socket_tests, fixture)

CAF_TEST(todo) {

}

CAF_TEST_FIXTURE_SCOPE_END()
