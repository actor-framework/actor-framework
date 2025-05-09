// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/ssl/context.hpp"

#include "caf/test/test.hpp"

#include "caf/net/ssl/format.hpp"
#include "caf/net/ssl/tls.hpp"

namespace ssl = caf::net::ssl;

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
}
