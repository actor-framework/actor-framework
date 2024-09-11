// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/router.hpp"

#include "caf/test/scenario.hpp"

#include "caf/net/multiplexer.hpp"

#include "caf/detail/source_location.hpp"

using namespace caf;
using namespace std::literals;

namespace http = caf::net::http;

using http::make_route;
using http::responder;
using http::router;

namespace {

struct fixture {
  std::string req;
  http::request_header hdr;
  http::router rt;
  std::vector<config_value> args;

  template <class... Ts>
  auto make_args(Ts... xs) {
    std::vector<config_value> result;
    (result.emplace_back(xs), ...);
    return result;
  }
};

} // namespace

WITH_FIXTURE(fixture) {

SCENARIO("routes must have one 'arg' entry per argument") {
  auto set_get_request
    = [this](std::string path,
             detail::source_location loc = detail::source_location::current()) {
        req = "GET " + path + " HTTP/1.1\r\n"
              + "Host: localhost:8090\r\n"
                "User-Agent: AwesomeLib/1.0\r\n"
                "Accept-Encoding: gzip\r\n\r\n";
        auto [status, err_msg] = hdr.parse(req);
        require_eq(status, http::status::ok, loc);
      };
  auto set_post_request
    = [this](std::string path,
             detail::source_location loc = detail::source_location::current()) {
        req = "POST " + path + " HTTP/1.1\r\n"
              + "Host: localhost:8090\r\n"
                "User-Agent: AwesomeLib/1.0\r\n"
                "Accept-Encoding: gzip\r\n\r\n";
        auto [status, err_msg] = hdr.parse(req);
        require_eq(status, http::status::ok, loc);
      };
  GIVEN("a make_route call that has fewer arguments than the callback") {
    WHEN("evaluating the factory call") {
      THEN("the factory produces an error") {
        auto res1 = make_route("/", [](responder&, int) {});
        check_eq(res1, sec::invalid_argument);
        auto res2 = make_route("/<arg>", [](responder&, int, int) {});
        check_eq(res2, sec::invalid_argument);
      }
    }
  }
  GIVEN("a make_route call that has more arguments than the callback") {
    WHEN("evaluating the factory call") {
      THEN("the factory produces an error") {
        auto res1 = make_route("/<arg>/<arg>", [](responder&) {});
        check_eq(res1, sec::invalid_argument);
        auto res2 = make_route("/<arg>/<arg>", [](responder&, int) {});
        check_eq(res2, sec::invalid_argument);
      }
    }
  }
  GIVEN("a make_route call with the matching number of arguments") {
    WHEN("evaluating the factory call") {
      THEN("the factory produces a valid callback") {
        if (auto res = make_route("/", [](responder&) {});
            check(res.has_value())) {
          set_get_request("/");
          check((*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
        }
        if (auto res = make_route("/foo/bar", http::method::get,
                                  [](responder&) {});
            check(res.has_value())) {
          set_get_request("/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar/baz");
          check(!(*res)->exec(hdr, {}, &rt));
          set_post_request("/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          check((*res)->exec(hdr, {}, &rt));
        }
        if (auto res = make_route(
              "/<arg>", [this](responder&, int x) { args = make_args(x); });
            check(res.has_value())) {
          set_get_request("/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/42");
          if (check((*res)->exec(hdr, {}, &rt)))
            check_eq(args, make_args(42));
        }
        if (auto res
            = make_route("/foo/<arg>/bar",
                         [this](responder&, int x) { args = make_args(x); });
            check(res.has_value())) {
          set_get_request("/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/123/bar");
          if (check((*res)->exec(hdr, {}, &rt)))
            check_eq(args, make_args(123));
        }
        if (auto res = make_route("/foo/<arg>/bar",
                                  [this](responder&, std::string x) {
                                    args = make_args(x);
                                  });
            check(res.has_value())) {
          set_get_request("/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/my-arg/bar");
          if (check((*res)->exec(hdr, {}, &rt)))
            check_eq(args, make_args("my-arg"s));
        }
        if (auto res = make_route("/<arg>/<arg>/<arg>",
                                  [this](responder&, int x, bool y, int z) {
                                    args = make_args(x, y, z);
                                  });
            check(res.has_value())) {
          set_get_request("/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/1/true/3?foo=bar");
          if (check((*res)->exec(hdr, {}, &rt)))
            check_eq(args, make_args(1, true, 3));
        }
      }
    }
  }
}

} // WITH_FIXTURE(fixture)
