// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.http.router

#include "caf/net/http/router.hpp"

#include "net-test.hpp"

#include "caf/net/multiplexer.hpp"

using namespace caf;
using namespace std::literals;

namespace http = caf::net::http;

using http::make_route;
using http::responder;
using http::router;

class mock_server : public http::lower_layer {
public:
  mock_server(net::multiplexer* mpx) : mpx_(mpx) {
    // nop
  }

  net::multiplexer& mpx() noexcept {
    return *mpx_;
  }

  bool can_send_more() const noexcept {
    return false;
  }

  bool is_reading() const noexcept {
    return false;
  }

  void write_later() {
    // nop
  }

  void shutdown() {
    // nop
  }

  void request_messages() {
    // nop
  }

  void suspend_reading() {
    // nop
  }

  void begin_header(http::status) {
    // nop
  }

  void add_header_field(std::string_view, std::string_view) {
    // nop
  }

  bool end_header() {
    return true;
  }

  bool send_payload(const_byte_span) {
    return true;
  }

  bool send_chunk(const_byte_span) {
    return true;
  }

  bool send_end_of_chunks() {
    return true;
  }

  void switch_protocol(std::unique_ptr<net::octet_stream::upper_layer>) {
    // nop
  }

private:
  net::multiplexer* mpx_;
};

struct fixture : test_coordinator_fixture<> {
  fixture() : mpx(net::multiplexer::make(nullptr)), serv(mpx.get()) {
    mpx->set_thread_id();
    std::ignore = rt.start(&serv);
  }

  net::multiplexer_ptr mpx;
  mock_server serv;
  std::string req;
  http::request_header hdr;
  http::router rt;
  std::vector<config_value> args;

  void set_get_request(std::string path) {
    req = "GET " + path + " HTTP/1.1\r\n"
          + "Host: localhost:8090\r\n"
            "User-Agent: AwesomeLib/1.0\r\n"
            "Accept-Encoding: gzip\r\n\r\n";
    auto [status, err_msg] = hdr.parse(req);
    REQUIRE(status == http::status::ok);
  }

  void set_post_request(std::string path) {
    req = "POST " + path + " HTTP/1.1\r\n"
          + "Host: localhost:8090\r\n"
            "User-Agent: AwesomeLib/1.0\r\n"
            "Accept-Encoding: gzip\r\n\r\n";
    auto [status, err_msg] = hdr.parse(req);
    REQUIRE(status == http::status::ok);
  }

  template <class... Ts>
  auto make_args(Ts... xs) {
    std::vector<config_value> result;
    (result.emplace_back(xs), ...);
    return result;
  }
};

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("routes must have one <arg> entry per argument") {
  GIVEN("a make_route call that has fewer arguments than the callback") {
    WHEN("evaluating the factory call") {
      THEN("the factory produces an error") {
        auto res1 = make_route("/", [](responder&, int) {});
        CHECK_EQ(res1, sec::invalid_argument);
        auto res2 = make_route("/<arg>", [](responder&, int, int) {});
        CHECK_EQ(res2, sec::invalid_argument);
      }
    }
  }
  GIVEN("a make_route call that has more arguments than the callback") {
    WHEN("evaluating the factory call") {
      THEN("the factory produces an error") {
        auto res1 = make_route("/<arg>/<arg>", [](responder&) {});
        CHECK_EQ(res1, sec::invalid_argument);
        auto res2 = make_route("/<arg>/<arg>", [](responder&, int) {});
        CHECK_EQ(res2, sec::invalid_argument);
      }
    }
  }
  GIVEN("a make_route call with the matching number of arguments") {
    WHEN("evaluating the factory call") {
      THEN("the factory produces a valid callback") {
        if (auto res = make_route("/", [](responder&) {}); CHECK(res)) {
          set_get_request("/");
          CHECK((*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          CHECK(!(*res)->exec(hdr, {}, &rt));
        }
        if (auto res = make_route("/foo/bar", http::method::get,
                                  [](responder&) {});
            CHECK(res)) {
          set_get_request("/");
          CHECK(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo");
          CHECK(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar/baz");
          CHECK(!(*res)->exec(hdr, {}, &rt));
          set_post_request("/foo/bar");
          CHECK(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          CHECK((*res)->exec(hdr, {}, &rt));
        }
        if (auto res = make_route(
              "/<arg>", [this](responder&, int x) { args = make_args(x); });
            CHECK(res)) {
          set_get_request("/");
          CHECK(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          CHECK(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/42");
          if (CHECK((*res)->exec(hdr, {}, &rt)))
            CHECK_EQ(args, make_args(42));
        }
        if (auto res
            = make_route("/foo/<arg>/bar",
                         [this](responder&, int x) { args = make_args(x); });
            CHECK(res)) {
          set_get_request("/");
          CHECK(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          CHECK(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/123/bar");
          if (CHECK((*res)->exec(hdr, {}, &rt)))
            CHECK_EQ(args, make_args(123));
        }
        if (auto res = make_route("/foo/<arg>/bar",
                                  [this](responder&, std::string x) {
                                    args = make_args(x);
                                  });
            CHECK(res)) {
          set_get_request("/");
          CHECK(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          CHECK(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/my-arg/bar");
          if (CHECK((*res)->exec(hdr, {}, &rt)))
            CHECK_EQ(args, make_args("my-arg"s));
        }
        if (auto res = make_route("/<arg>/<arg>/<arg>",
                                  [this](responder&, int x, bool y, int z) {
                                    args = make_args(x, y, z);
                                  });
            CHECK(res)) {
          set_get_request("/");
          CHECK(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          CHECK(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/1/true/3?foo=bar");
          if (CHECK((*res)->exec(hdr, {}, &rt)))
            CHECK_EQ(args, make_args(1, true, 3));
        }
      }
    }
  }
}

END_FIXTURE_SCOPE()
