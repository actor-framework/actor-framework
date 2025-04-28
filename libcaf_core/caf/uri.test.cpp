// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/uri.hpp"

#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/uri_builder.hpp"

using namespace caf;

namespace {

struct authority_separator_t {
} authority_separator;

struct path_separator_t {
} path_separator;

struct uri_str_builder {
  std::string res;

  uri_str_builder() : res("http:") {
    // nop
  }

  uri_str_builder& add() {
    return *this;
  }

  template <class T, class... Ts>
  uri_str_builder& add(const T& x, const Ts&... xs) {
    res += x;
    return add(xs...);
  }

  template <class... Ts>
  uri_str_builder& add(const authority_separator_t&, const Ts&... xs) {
    if (res.back() == ':')
      return add("//", xs...);
    return add(xs...);
  }

  template <class... Ts>
  uri_str_builder& add(const path_separator_t&, const Ts&... xs) {
    if (res.back() != ':')
      return add("/", xs...);
    return add(xs...);
  }

  uri_str_builder& userinfo(std::string str) {
    return add(authority_separator, str, '@');
  }

  uri_str_builder& host(std::string str) {
    return add(authority_separator, str);
  }

  uri_str_builder& host(ip_address addr) {
    return add(authority_separator, '[', to_string(addr), ']');
  }

  uri_str_builder& port(uint16_t value) {
    return add(':', std::to_string(value));
  }

  uri_str_builder& path(std::string str) {
    return add(path_separator, str);
  }

  uri_str_builder& query(uri::query_map map) {
    if (map.empty())
      return *this;
    auto print_kvp = [&](const uri::query_map::value_type& kvp) {
      res += kvp.first;
      res += '=';
      res += kvp.second;
    };
    res += '?';
    auto i = map.begin();
    print_kvp(*i);
    for (++i; i != map.end(); ++i) {
      res += '&';
      print_kvp(*i);
    }
    return *this;
  }

  uri_str_builder& fragment(std::string str) {
    return add('#', str);
  }

  std::string operator*() {
    using std::swap;
    std::string str = "http:";
    swap(str, res);
    return str;
  }
};

struct fixture {
  // -- constructors, destructors, and assignment operators --------------------

  fixture() {
    http.scheme("http");
  }

  // -- member variables -------------------------------------------------------

  uri_builder http;

  uri_str_builder http_str;

  // -- utility functions ------------------------------------------------------

  byte_buffer serialize(uri x) {
    byte_buffer buf;
    binary_serializer sink{buf};
    if (!sink.apply(x))
      test::runnable::current().fail("unable to serialize {}: {}", x.str(),
                                     sink.get_error());
    return buf;
  }

  uri deserialize(byte_buffer buf) {
    uri result;
    binary_deserializer source{buf};
    if (!source.apply(result))
      test::runnable::current().fail("unable to deserialize from buffer: {}",
                                     source.get_error());
    return result;
  }
};

struct me_t {
} me;

template <class T>
T& operator<<(T& builder, me_t) {
  return builder.userinfo("me");
}

struct node_t {
} node;

template <class T>
T& operator<<(T& builder, node_t) {
  return builder.host("node");
}

struct port80_t {
} port80;

template <class T>
T& operator<<(T& builder, port80_t) {
  return builder.port(80);
}

struct file_t {
} file;

template <class T>
T& operator<<(T& builder, file_t) {
  return builder.path("file");
}

struct frag_t {
} frag;

template <class T>
T& operator<<(T& builder, frag_t) {
  return builder.fragment("42");
}

struct kvp_t {
} kvp;

template <class T>
T& operator<<(T& builder, kvp_t) {
  return builder.query(uri::query_map{{"a", "1"}, {"b", "2"}});
}

uri operator*(uri_builder& builder) {
  auto result = builder.make();
  builder = uri_builder();
  auto scheme = result.scheme();
  builder.scheme(std::string{scheme.begin(), scheme.end()});
  return result;
}

uri operator""_u(const char* cstr, size_t cstr_len) {
  uri result;
  std::string_view str{cstr, cstr_len};
  auto err = parse(str, result);
  if (err)
    test::runnable::current().fail("error while parsing {} : {}", str,
                                   to_string(err));
  return result;
}

bool operator""_i(const char* cstr, size_t cstr_len) {
  uri result;
  std::string_view str{cstr, cstr_len};
  if (uri::can_parse(str))
    test::runnable::current().fail("parse possible for invalid uri : {}", str);
  auto err = parse(str, result);
  return err != none;
}

} // namespace

WITH_FIXTURE(fixture) {

TEST("default URIs are empty") {
  uri x;
  check_eq(x.empty(), true);
  check_eq(x.str(), "");
}

TEST("URIs recognize IP addresses while parsing") {
  auto v6_localhost = "tcp://[::1]:8080"_u;
  check(holds_alternative<ip_address>(v6_localhost.authority().host));
  auto v4_localhost = "tcp://127.0.0.1:8080"_u;
  check(holds_alternative<ip_address>(v4_localhost.authority().host));
  auto str_localhost = "tcp://localhost:8080"_u;
  check(holds_alternative<std::string>(str_localhost.authority().host));
}

#define BUILD(components)                                                      \
  check_eq(*(http << components), *(http_str << components))

TEST("builder construction") {
  auto minimal = *(http << file);
  check_eq(minimal.empty(), false);
  check_eq(minimal, "http:file");
  // all combinations of components
  BUILD(file);
  BUILD(file << kvp);
  BUILD(file << frag);
  BUILD(file << kvp << frag);
  BUILD(node);
  BUILD(node << frag);
  BUILD(node << kvp);
  BUILD(node << kvp << frag);
  BUILD(node << port80);
  BUILD(node << port80 << frag);
  BUILD(node << port80 << kvp);
  BUILD(node << port80 << kvp << frag);
  BUILD(me << node);
  BUILD(me << node << kvp);
  BUILD(me << node << frag);
  BUILD(me << node << kvp << frag);
  BUILD(me << node << port80);
  BUILD(me << node << port80 << frag);
  BUILD(me << node << port80 << kvp);
  BUILD(me << node << port80 << kvp << frag);
  BUILD(node << file);
  BUILD(node << file << frag);
  BUILD(node << file << kvp);
  BUILD(node << file << kvp << frag);
  BUILD(node << port80 << file);
  BUILD(node << port80 << file << frag);
  BUILD(node << port80 << file << kvp);
  BUILD(node << port80 << file << kvp << frag);
  BUILD(me << node << file);
  BUILD(me << node << file << frag);
  BUILD(me << node << file << kvp);
  BUILD(me << node << file << kvp << frag);
  BUILD(me << node << port80 << file);
  BUILD(me << node << port80 << file << frag);
  BUILD(me << node << port80 << file << kvp);
  BUILD(me << node << port80 << file << kvp << frag);
  // percent encoding
  auto escaped = uri_builder{}
                   .scheme("hi there")
                   .userinfo("it's")
                   .host("me/")
                   .path("file 1")
                   .fragment("[42]")
                   .make();
  check_eq(escaped, "hi%20there://it%27s@me%2F/file%201#%5B42%5D");
}

#define ROUNDTRIP(str)                                                         \
  do {                                                                         \
    check(uri::can_parse(str));                                                \
    check_eq(str##_u, str);                                                    \
  } while (false)

TEST("from string") {
  // all combinations of components
  ROUNDTRIP("http:file");
  ROUNDTRIP("http:foo-bar");
  ROUNDTRIP("http:foo:bar");
  ROUNDTRIP("http:file?a=1&b=2");
  ROUNDTRIP("http:file#42");
  ROUNDTRIP("http:file?a=1&b=2#42");
  ROUNDTRIP("http://node");
  ROUNDTRIP("http://node?a=1&b=2");
  ROUNDTRIP("http://node#42");
  ROUNDTRIP("http://node?a=1&b=2#42");
  ROUNDTRIP("http://node:80");
  ROUNDTRIP("http://node:80?a=1&b=2");
  ROUNDTRIP("http://node:80#42");
  ROUNDTRIP("http://node:80?a=1&b=2#42");
  ROUNDTRIP("http://me@node");
  ROUNDTRIP("http://me@node?a=1&b=2");
  ROUNDTRIP("http://me@node#42");
  ROUNDTRIP("http://me@node?a=1&b=2#42");
  ROUNDTRIP("http://me@node:80");
  ROUNDTRIP("http://me@node:80?a=1&b=2");
  ROUNDTRIP("http://me@node:80#42");
  ROUNDTRIP("http://me@node:80?a=1&b=2#42");
  check(uri::can_parse("http://@node"));
  check_eq("http://@node"_u, "http://node");
  ROUNDTRIP("http://:@node");
  ROUNDTRIP("http://user:@node");
  ROUNDTRIP("http://user:p@node");
  ROUNDTRIP("http://user:pass@node");
  ROUNDTRIP("http://user:pass@node?a=1&b=2");
  ROUNDTRIP("http://user:pass@node#42");
  ROUNDTRIP("http://user:pass@node?a=1&b=2#42");
  ROUNDTRIP("http://user:@node:80");
  ROUNDTRIP("http://user:p@node:80");
  ROUNDTRIP("http://user:pass@node:8");
  ROUNDTRIP("http://user:pass@node:80");
  ROUNDTRIP("http://user:pass@node:80?a=1&b=2");
  ROUNDTRIP("http://user:pass@node:80#42");
  ROUNDTRIP("http://user:pass@node:80?a=1&b=2#42");
  ROUNDTRIP("http://node/file");
  ROUNDTRIP("http://node/file?a=1&b=2");
  ROUNDTRIP("http://node/file#42");
  ROUNDTRIP("http://node/file?a=1&b=2#42");
  ROUNDTRIP("http://node:80/file");
  ROUNDTRIP("http://node:80/file?a=1&b=2");
  ROUNDTRIP("http://node:80/file#42");
  ROUNDTRIP("http://node:80/file?a=1&b=2#42");
  ROUNDTRIP("http://me@node/file");
  ROUNDTRIP("http://me@node/file?a=1&b=2");
  ROUNDTRIP("http://me@node/file#42");
  ROUNDTRIP("http://me@node/file?a=1&b=2#42");
  ROUNDTRIP("http://me@node:80/file");
  ROUNDTRIP("http://me@node:80/file?a=1&b=2");
  ROUNDTRIP("http://me@node:80/file#42");
  ROUNDTRIP("http://me@node:80/file?a=1&b=2#42");
  ROUNDTRIP("http://user:@node/file");
  ROUNDTRIP("http://user:p@node/file");
  ROUNDTRIP("http://user:pass@node/file");
  ROUNDTRIP("http://user:pass@node/file?a=1&b=2");
  ROUNDTRIP("http://user:pass@node/file#42");
  ROUNDTRIP("http://user:pass@node/file?a=1&b=2#42");
  ROUNDTRIP("http://user:@node:80/file");
  ROUNDTRIP("http://user:p@node:80/file");
  ROUNDTRIP("http://user:pass@node:8/file");
  ROUNDTRIP("http://user:pass@node:80/file");
  ROUNDTRIP("http://user:pass@node:80/file?a=1&b=2");
  ROUNDTRIP("http://user:pass@node:80/file#42");
  ROUNDTRIP("http://user:pass@node:80/file?a=1&b=2#42");
  // all combinations of components with IPv6 host
  ROUNDTRIP("http://[::1]");
  ROUNDTRIP("http://[::1]?a=1&b=2");
  ROUNDTRIP("http://[::1]#42");
  ROUNDTRIP("http://[::1]?a=1&b=2#42");
  ROUNDTRIP("http://[::1]:80");
  ROUNDTRIP("http://[::1]:80?a=1&b=2");
  ROUNDTRIP("http://[::1]:80#42");
  ROUNDTRIP("http://[::1]:80?a=1&b=2#42");
  ROUNDTRIP("http://me@[::1]");
  ROUNDTRIP("http://me@[::1]?a=1&b=2");
  ROUNDTRIP("http://me@[::1]#42");
  ROUNDTRIP("http://me@[::1]?a=1&b=2#42");
  ROUNDTRIP("http://me@[::1]:80");
  ROUNDTRIP("http://me@[::1]:80?a=1&b=2");
  ROUNDTRIP("http://me@[::1]:80#42");
  ROUNDTRIP("http://me@[::1]:80?a=1&b=2#42");
  check(uri::can_parse("http://@[::1]"));
  check_eq("http://@[::1]"_u, "http://[::1]");
  ROUNDTRIP("http://:@[::1]");
  ROUNDTRIP("http://user:@[::1]");
  ROUNDTRIP("http://user:p@[::1]");
  ROUNDTRIP("http://user:pass@[::1]");
  ROUNDTRIP("http://user:pass@[::1]?a=1&b=2");
  ROUNDTRIP("http://user:pass@[::1]#42");
  ROUNDTRIP("http://user:pass@[::1]?a=1&b=2#42");
  ROUNDTRIP("http://user:pass@[::1]:80");
  ROUNDTRIP("http://user:pass@[::1]:80?a=1&b=2");
  ROUNDTRIP("http://user:pass@[::1]:80#42");
  ROUNDTRIP("http://user:pass@[::1]:80?a=1&b=2#42");
  ROUNDTRIP("http://[::1]/file");
  ROUNDTRIP("http://[::1]/file?a=1&b=2");
  ROUNDTRIP("http://[::1]/file#42");
  ROUNDTRIP("http://[::1]/file?a=1&b=2#42");
  ROUNDTRIP("http://[::1]:80/file");
  ROUNDTRIP("http://[::1]:80/file?a=1&b=2");
  ROUNDTRIP("http://[::1]:80/file#42");
  ROUNDTRIP("http://[::1]:80/file?a=1&b=2#42");
  ROUNDTRIP("http://me@[::1]/file");
  ROUNDTRIP("http://me@[::1]/file?a=1&b=2");
  ROUNDTRIP("http://me@[::1]/file#42");
  ROUNDTRIP("http://me@[::1]/file?a=1&b=2#42");
  ROUNDTRIP("http://me@[::1]:80/file");
  ROUNDTRIP("http://me@[::1]:80/file?a=1&b=2");
  ROUNDTRIP("http://me@[::1]:80/file#42");
  ROUNDTRIP("http://me@[::1]:80/file?a=1&b=2#42");
  ROUNDTRIP("http://user:@[::1]/file");
  ROUNDTRIP("http://user:p@[::1]/file");
  ROUNDTRIP("http://user:pass@[::1]/file");
  ROUNDTRIP("http://user:pass@[::1]/file?a=1&b=2");
  ROUNDTRIP("http://user:pass@[::1]/file#42");
  ROUNDTRIP("http://user:pass@[::1]/file?a=1&b=2#42");
  ROUNDTRIP("http://user:@[::1]:80/file");
  ROUNDTRIP("http://user:p@[::1]:80/file");
  ROUNDTRIP("http://user:pass@[::1]:8/file");
  ROUNDTRIP("http://user:pass@[::1]:80/file");
  ROUNDTRIP("http://user:pass@[::1]:80/file?a=1&b=2");
  ROUNDTRIP("http://user:pass@[::1]:80/file#42");
  ROUNDTRIP("http://user:pass@[::1]:80/file?a=1&b=2#42");
  // percent encoding
  ROUNDTRIP("hi%20there://it%27s@me%21/file%201#%5B42%5D");
  ROUNDTRIP("file://localhost/tmp/test/test.{:04d}.exr?q=%3A1");
}

#undef ROUNDTRIP

TEST("empty components") {
  check_eq("foo:/"_u, "foo:/");
  check_eq("foo:///"_u, "foo:/");
  check_eq("foo:/#"_u, "foo:/");
  check_eq("foo:/?"_u, "foo:/");
  check_eq("foo:/?#"_u, "foo:/");
  check_eq("foo:bar#"_u, "foo:bar");
  check_eq("foo:bar?"_u, "foo:bar");
  check_eq("foo:bar?#"_u, "foo:bar");
  check_eq("foo://bar#"_u, "foo://bar");
  check_eq("foo://bar?"_u, "foo://bar");
  check_eq("foo://bar?#"_u, "foo://bar");
}

TEST("invalid uris") {
  check("http"_i);
  check("http://"_i);
  check("http://foo:66000"_i);
}

#define SERIALIZATION_ROUNDTRIP(str)                                           \
  check_eq(deserialize(serialize(str##_u)), str)

TEST("serialization") {
  // all combinations of components
  SERIALIZATION_ROUNDTRIP("http:file");
  SERIALIZATION_ROUNDTRIP("http:file?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http:file#42");
  SERIALIZATION_ROUNDTRIP("http:file?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://node");
  SERIALIZATION_ROUNDTRIP("http://node?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://node#42");
  SERIALIZATION_ROUNDTRIP("http://node?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://node:80");
  SERIALIZATION_ROUNDTRIP("http://node:80?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://node:80#42");
  SERIALIZATION_ROUNDTRIP("http://node:80?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://me@node");
  SERIALIZATION_ROUNDTRIP("http://me@node?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://me@node#42");
  SERIALIZATION_ROUNDTRIP("http://me@node?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://me@node:80");
  SERIALIZATION_ROUNDTRIP("http://me@node:80?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://me@node:80#42");
  SERIALIZATION_ROUNDTRIP("http://me@node:80?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://node/file");
  SERIALIZATION_ROUNDTRIP("http://node/file?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://node/file#42");
  SERIALIZATION_ROUNDTRIP("http://node/file?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://node:80/file");
  SERIALIZATION_ROUNDTRIP("http://node:80/file?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://node:80/file#42");
  SERIALIZATION_ROUNDTRIP("http://node:80/file?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://me@node/file");
  SERIALIZATION_ROUNDTRIP("http://me@node/file?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://me@node/file#42");
  SERIALIZATION_ROUNDTRIP("http://me@node/file?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://me@node:80/file");
  SERIALIZATION_ROUNDTRIP("http://me@node:80/file?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://me@node:80/file#42");
  SERIALIZATION_ROUNDTRIP("http://me@node:80/file?a=1&b=2#42");
  // all combinations of with IPv6 host
  SERIALIZATION_ROUNDTRIP("http://[::1]");
  SERIALIZATION_ROUNDTRIP("http://[::1]?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://[::1]#42");
  SERIALIZATION_ROUNDTRIP("http://[::1]?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://[::1]:80");
  SERIALIZATION_ROUNDTRIP("http://[::1]:80?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://[::1]:80#42");
  SERIALIZATION_ROUNDTRIP("http://[::1]:80?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]#42");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]:80");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]:80?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]:80#42");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]:80?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://[::1]/file");
  SERIALIZATION_ROUNDTRIP("http://[::1]/file?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://[::1]/file#42");
  SERIALIZATION_ROUNDTRIP("http://[::1]/file?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://[::1]:80/file");
  SERIALIZATION_ROUNDTRIP("http://[::1]:80/file?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://[::1]:80/file#42");
  SERIALIZATION_ROUNDTRIP("http://[::1]:80/file?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]/file");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]/file?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]/file#42");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]/file?a=1&b=2#42");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]:80/file");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]:80/file?a=1&b=2");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]:80/file#42");
  SERIALIZATION_ROUNDTRIP("http://me@[::1]:80/file?a=1&b=2#42");
  // percent encoding
  SERIALIZATION_ROUNDTRIP("hi%20there://it%27s@me%21/file%201#%5B42%5D");
}

#undef SERIALIZATION_ROUNDTRIP

TEST("with_userinfo creates a copy with new userinfo") {
  SECTION("conversion fails for URIs without authority") {
    check_eq("http:file"_u.with_userinfo("me"), std::nullopt);
  }
  SECTION("setting the username only") {
    check_eq("http://node"_u.with_userinfo("me"), "http://me@node"_u);
    check_eq("http://node?a=1&b=2"_u.with_userinfo("me"),
             "http://me@node?a=1&b=2"_u);
    check_eq("http://node#42"_u.with_userinfo("me"), "http://me@node#42"_u);
    check_eq("http://node?a=1&b=2#42"_u.with_userinfo("me"),
             "http://me@node?a=1&b=2#42"_u);
    check_eq("http://node:80"_u.with_userinfo("me"), "http://me@node:80"_u);
    check_eq("http://node:80?a=1&b=2"_u.with_userinfo("me"),
             "http://me@node:80?a=1&b=2"_u);
    check_eq("http://node:80#42"_u.with_userinfo("me"),
             "http://me@node:80#42"_u);
    check_eq("http://node:80?a=1&b=2#42"_u.with_userinfo("me"),
             "http://me@node:80?a=1&b=2#42"_u);
    check_eq("http://node/file"_u.with_userinfo("me"), "http://me@node/file"_u);
    check_eq("http://node/file?a=1&b=2"_u.with_userinfo("me"),
             "http://me@node/file?a=1&b=2"_u);
    check_eq("http://node/file#42"_u.with_userinfo("me"),
             "http://me@node/file#42"_u);
    check_eq("http://node/file?a=1&b=2#42"_u.with_userinfo("me"),
             "http://me@node/file?a=1&b=2#42"_u);
    check_eq("http://node:80/file"_u.with_userinfo("me"),
             "http://me@node:80/file"_u);
    check_eq("http://node:80/file?a=1&b=2"_u.with_userinfo("me"),
             "http://me@node:80/file?a=1&b=2"_u);
    check_eq("http://node:80/file#42"_u.with_userinfo("me"),
             "http://me@node:80/file#42"_u);
    check_eq("http://node:80/file?a=1&b=2#42"_u.with_userinfo("me"),
             "http://me@node:80/file?a=1&b=2#42"_u);
  }
  SECTION("setting username and password") {
    check_eq("http://node"_u.with_userinfo("me", "foo"),
             "http://me:foo@node"_u);
    check_eq("http://node?a=1&b=2"_u.with_userinfo("me", "foo"),
             "http://me:foo@node?a=1&b=2"_u);
    check_eq("http://node#42"_u.with_userinfo("me", "foo"),
             "http://me:foo@node#42"_u);
    check_eq("http://node?a=1&b=2#42"_u.with_userinfo("me", "foo"),
             "http://me:foo@node?a=1&b=2#42"_u);
    check_eq("http://node:80"_u.with_userinfo("me", "foo"),
             "http://me:foo@node:80"_u);
    check_eq("http://node:80?a=1&b=2"_u.with_userinfo("me", "foo"),
             "http://me:foo@node:80?a=1&b=2"_u);
    check_eq("http://node:80#42"_u.with_userinfo("me", "foo"),
             "http://me:foo@node:80#42"_u);
    check_eq("http://node:80?a=1&b=2#42"_u.with_userinfo("me", "foo"),
             "http://me:foo@node:80?a=1&b=2#42"_u);
    check_eq("http://node/file"_u.with_userinfo("me", "foo"),
             "http://me:foo@node/file"_u);
    check_eq("http://node/file?a=1&b=2"_u.with_userinfo("me", "foo"),
             "http://me:foo@node/file?a=1&b=2"_u);
    check_eq("http://node/file#42"_u.with_userinfo("me", "foo"),
             "http://me:foo@node/file#42"_u);
    check_eq("http://node/file?a=1&b=2#42"_u.with_userinfo("me", "foo"),
             "http://me:foo@node/file?a=1&b=2#42"_u);
    check_eq("http://node:80/file"_u.with_userinfo("me", "foo"),
             "http://me:foo@node:80/file"_u);
    check_eq("http://node:80/file?a=1&b=2"_u.with_userinfo("me", "foo"),
             "http://me:foo@node:80/file?a=1&b=2"_u);
    check_eq("http://node:80/file#42"_u.with_userinfo("me", "foo"),
             "http://me:foo@node:80/file#42"_u);
    check_eq("http://node:80/file?a=1&b=2#42"_u.with_userinfo("me", "foo"),
             "http://me:foo@node:80/file?a=1&b=2#42"_u);
  }
}

} // WITH_FIXTURE(fixture)
