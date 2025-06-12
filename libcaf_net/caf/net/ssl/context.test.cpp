// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/ssl/context.hpp"

#include "caf/test/test.hpp"

#include "caf/net/ssl/format.hpp"
#include "caf/net/ssl/tls.hpp"

namespace ssl = caf::net::ssl;
using namespace std::literals;

TEST("constructing and setting values in the context object") {
  auto maybe_ctx = ssl::context::make_client(ssl::tls::v1_0);
  require(maybe_ctx.has_value());
  auto ctx = std::move(*maybe_ctx);
  SECTION("default constructed context discards SNI") {
    check_eq(ctx.sni_hostname(), nullptr);
  }
  SECTION("using empty string discards SNI") {
    ctx.sni_hostname(""s);
    check_eq(ctx.sni_hostname(), nullptr);
  }
  SECTION("configuring a hostname for SNI") {
    auto host = "feodotracker.abuse.ch"s;
    ctx.sni_hostname(host);
    check_eq(ctx.sni_hostname(), host);
    SECTION("new connections use the selected SNI hostname") {
      auto fd_pair = caf::net::make_stream_socket_pair();
      require(fd_pair.has_value());
      auto conn = ctx.new_connection(fd_pair->first);
      auto g = caf::net::socket_guard{fd_pair->second};
      if (check_has_value(conn)) {
        check_eq(conn->sni_hostname(), host);
      }
    }
  }
}

TEST("invalid arguments to ..._if DSL functions leave the context unchanged") {
  SECTION("add_verify_path_if") {
    auto res = ssl::context::make_server(ssl::tls::v1_0)
                 .and_then(ssl::add_verify_path_if(nullptr));
    check_has_value(res);
  }
  SECTION("load_verify_file_if") {
    auto res = ssl::context::make_server(ssl::tls::v1_0)
                 .and_then(ssl::load_verify_file_if(nullptr));
    check_has_value(res);
  }
  SECTION("use_password_if") {
    auto res = ssl::context::make_server(ssl::tls::v1_0)
                 .and_then(ssl::use_password_if(nullptr));
    check_has_value(res);
  }
  SECTION("use_certificate_file_if") {
    SECTION("invalid path") {
      auto res
        = ssl::context::make_server(ssl::tls::v1_0)
            .and_then(ssl::use_certificate_file_if(nullptr, ssl::format::pem));
      check_has_value(res);
    }
    SECTION("invalid format") {
      auto res = ssl::context::make_server(ssl::tls::v1_0)
                   .and_then(ssl::use_certificate_file_if(
                     "/foo/bar", std::optional<ssl::format>{}));
      check_has_value(res);
    }
  }
  SECTION("use_certificate_chain_file_if") {
    auto res = ssl::context::make_server(ssl::tls::v1_0)
                 .and_then(ssl::use_certificate_chain_file_if(nullptr));
    check_has_value(res);
  }
  SECTION("use_private_key_file_if") {
    SECTION("invalid path") {
      auto res
        = ssl::context::make_server(ssl::tls::v1_0)
            .and_then(ssl::use_private_key_file_if(nullptr, ssl::format::pem));
      check_has_value(res);
    }
    SECTION("invalid format") {
      auto res = ssl::context::make_server(ssl::tls::v1_0)
                   .and_then(ssl::use_private_key_file_if(
                     "/foo/bar", std::optional<ssl::format>{}));
      check_has_value(res);
    }
  }
  SECTION("use_sni_hostname") {
    SECTION("string hostname") {
      auto host = "feodotracker.abuse.ch"s;
      auto res = ssl::context::make_client(ssl::tls::v1_0)
                   .and_then(ssl::use_sni_hostname(host));
      if (check_has_value(res))
        check_eq(res->sni_hostname(), host);
    }
    SECTION("URI with hostname") {
      auto uri = caf::make_uri("https://feodotracker.abuse.ch:443"s);
      require(uri.has_value());
      auto res = ssl::context::make_client(ssl::tls::v1_0)
                   .and_then(ssl::use_sni_hostname(*uri));
      if (check_has_value(res))
        check_eq(res->sni_hostname(), "feodotracker.abuse.ch"s);
    }
    SECTION("URI without a hostname") {
      auto uri = caf::make_uri("https://192.168.1.1"s);
      require(uri.has_value());
      auto res = ssl::context::make_client(ssl::tls::v1_0)
                   .and_then(ssl::use_sni_hostname(*uri));
      check(!res.has_value());
    }
  }
}
