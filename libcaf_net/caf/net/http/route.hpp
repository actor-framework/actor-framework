// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/http/arg_parser.hpp"
#include "caf/net/http/request.hpp"
#include "caf/net/http/request_header.hpp"
#include "caf/net/http/responder.hpp"

#include "caf/byte_span.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"

#include <string_view>
#include <tuple>

namespace caf::net::http {

/// Represents a single route for HTTP requests at a server.
class CAF_NET_EXPORT route : public ref_counted {
public:
  virtual ~route();

  /// Tries to match an HTTP request and processes the request on a match. The
  /// route may send errors to the client or call `shutdown` on the `parent` for
  /// severe errors.
  /// @param hdr The HTTP request header from the client.
  /// @param body The payload from the client.
  /// @param parent Pointer to the object that uses this route.
  /// @return `true` if the route matches the request, `false` otherwise.
  virtual bool
  exec(const request_header& hdr, const_byte_span body, router* parent)
    = 0;

  /// Called by the HTTP server when starting up. May be used to spin up workers
  /// that the path dispatches to. The default implementation does nothing.
  virtual void init();
};

} // namespace caf::net::http

namespace caf::detail {

/// Counts how many `<arg>` entries are in `path`.
size_t CAF_NET_EXPORT args_in_path(std::string_view path);

/// Splits `str` in the first component of a path and its remainder.
std::pair<std::string_view, std::string_view>
  CAF_NET_EXPORT next_path_component(std::string_view str);

/// Matches two paths by splitting both inputs at '/' and then checking that
/// `predicate` holds for each resulting pair.
template <class F>
bool match_path(std::string_view lhs, std::string_view rhs, F&& predicate) {
  std::string_view head1;
  std::string_view tail1;
  std::string_view head2;
  std::string_view tail2;
  std::tie(head1, tail1) = next_path_component(lhs);
  std::tie(head2, tail2) = next_path_component(rhs);
  if (!predicate(head1, head2))
    return false;
  while (!tail1.empty()) {
    if (tail2.empty())
      return false;
    std::tie(head1, tail1) = next_path_component(tail1);
    std::tie(head2, tail2) = next_path_component(tail2);
    if (!predicate(head1, head2))
      return false;
  }
  return tail2.empty();
}
template <class, class = void>
struct http_route_has_init : std::false_type {};

template <class T>
struct http_route_has_init<T, std::void_t<decltype(std::declval<T>().init())>>
  : std::true_type {};

template <class T>
constexpr bool http_route_has_init_v = http_route_has_init<T>::value;

/// Base type for HTTP routes that parse one or more arguments from the requests
/// and then forward them to a user-provided function object.
template <class... Ts>
class http_route_base : public net::http::route {
public:
  explicit http_route_base(std::string&& path,
                           std::optional<net::http::method> method)
    : path_(std::move(path)), method_(method) {
    // nop
  }

  bool exec(const net::http::request_header& hdr, const_byte_span body,
            net::http::router* parent) override {
    if (method_ && *method_ != hdr.method())
      return false;
    // Try to match the path to the expected path and extract args.
    std::string_view args[sizeof...(Ts)];
    auto ok = match_path(path_, hdr.path(),
                         [pos = args](std::string_view lhs,
                                      std::string_view rhs) mutable {
                           if (lhs == "<arg>") {
                             *pos++ = rhs;
                             return true;
                           } else {
                             return lhs == rhs;
                           }
                         });
    if (!ok)
      return false;
    // Try to parse the arguments.
    using iseq = std::make_index_sequence<sizeof...(Ts)>;
    return exec_dis(hdr, body, parent, iseq{}, args);
  }

  template <size_t... Is>
  bool exec_dis(const net::http::request_header& hdr, const_byte_span body,
                net::http::router* parent, std::index_sequence<Is...>,
                std::string_view* arr) {
    return exec_impl(hdr, body, parent,
                     std::get<Is>(parsers_).parse(arr[Is])...);
  }

  template <class... Is>
  bool exec_impl(const net::http::request_header& hdr, const_byte_span body,
                 net::http::router* parent, std::optional<Ts>&&... args) {
    if ((args.has_value() && ...)) {
      net::http::responder rp{&hdr, body, parent};
      do_apply(rp, std::move(*args)...);
      return true;
    }
    return false;
  }

private:
  virtual void do_apply(net::http::responder&, Ts&&...) = 0;

  std::string path_;
  std::optional<net::http::method> method_;
  std::tuple<net::http::arg_parser_t<Ts>...> parsers_;
};

template <class F, class... Ts>
class http_route_impl : public http_route_base<Ts...> {
public:
  using super = http_route_base<Ts...>;

  http_route_impl(std::string&& path, std::optional<net::http::method> method,
                  F&& f)
    : super(std::move(path), method), f_(std::move(f)) {
    // nop
  }

  void init() override {
    if constexpr (detail::http_route_has_init_v<F>) {
      f_.init();
    }
  }

private:
  void do_apply(net::http::responder& res, Ts&&... args) override {
    f_(res, std::move(args)...);
  }

  F f_;
};

/// A simple implementation for `http::route` that does not parse any arguments
/// from the requests and simply calls the user-provided function object.
class CAF_NET_EXPORT http_simple_route_base : public net::http::route {
public:
  http_simple_route_base(std::string&& path,
                         std::optional<net::http::method> method)
    : path_(std::move(path)), method_(method) {
    // nop
  }

  bool exec(const net::http::request_header& hdr, const_byte_span body,
            net::http::router* parent) override;

private:
  virtual void do_apply(net::http::responder&) = 0;

  std::string path_;
  std::optional<net::http::method> method_;
};

template <class F>
class http_simple_route_impl : public http_simple_route_base {
public:
  using super = http_simple_route_base;

  http_simple_route_impl(std::string&& path,
                         std::optional<net::http::method> method, F&& f)
    : super(std::move(path), method), f_(std::move(f)) {
    // nop
  }

  void init() override {
    if constexpr (detail::http_route_has_init<F>::value) {
      f_.init();
    }
  }

private:
  void do_apply(net::http::responder& res) override {
    f_(res);
  }

  F f_;
};

/// Represents an HTTP route that matches any path.
template <class F>
class http_catch_all_route_impl : public net::http::route {
public:
  explicit http_catch_all_route_impl(F&& f) : f_(std::move(f)) {
    // nop
  }

  bool exec(const net::http::request_header& hdr, const_byte_span body,
            net::http::router* parent) override {
    net::http::responder rp{&hdr, body, parent};
    f_(rp);
    return true;
  }

private:
  F f_;
};

/// Creates a route from a function object.
template <class F, class... Args>
net::http::route_ptr
make_http_route_impl(std::string& path, std::optional<net::http::method> method,
                     F& f, type_list<net::http::responder&, Args...>) {
  if constexpr (sizeof...(Args) == 0) {
    using impl_t = http_simple_route_impl<F>;
    return make_counted<impl_t>(std::move(path), method, std::move(f));
  } else {
    using impl_t = http_route_impl<F, Args...>;
    return make_counted<impl_t>(std::move(path), method, std::move(f));
  }
}

} // namespace caf::detail

namespace caf::net::http {

/// Creates a @ref route object from a function object.
/// @param path Description of the path, optionally with `<arg>` placeholders.
/// @param method The HTTP method for the path or `std::nullopt` for "any".
/// @param f The callback for the path.
/// @returns a @ref path on success, an error when failing to parse the path or
///          to match it to the signature of `f`.
template <class F>
expected<route_ptr>
make_route(std::string path, std::optional<http::method> method, F f) {
  // F must have signature void (responder&, ...).
  using f_trait = detail::get_callable_trait_t<F>;
  using f_args = typename f_trait::arg_types;
  static_assert(f_trait::num_args > 0, "F must take at least one argument");
  using arg_0 = detail::tl_at_t<f_args, 0>;
  static_assert(std::is_same_v<arg_0, responder&>,
                "F must take 'responder&' as first argument");
  // The path must be absolute.
  if (path.empty() || path.front() != '/') {
    return make_error(sec::invalid_argument,
                      "expected an absolute path, got: " + path);
  }
  // The path must have as many <arg> entries as F takes extra arguments.
  auto num_args = detail::args_in_path(path);
  if (num_args != f_trait::num_args - 1) {
    auto msg = path;
    msg += " defines ";
    detail::print(msg, num_args);
    msg += " arguments, but F accepts ";
    detail::print(msg, f_trait::num_args - 1);
    return make_error(sec::invalid_argument, std::move(msg));
  }
  // Create the object.
  return detail::make_http_route_impl(path, method, f, f_args{});
}

/// Convenience function for calling `make_route(path, std::nullopt, f)`.
template <class F>
expected<route_ptr> make_route(std::string path, F f) {
  return make_route(std::move(path), std::nullopt, std::move(f));
}

/// Creates a @ref route that matches all paths.
template <class F>
expected<route_ptr> make_route(F f) {
  // F must have signature void (responder&).
  using f_trait = detail::get_callable_trait_t<F>;
  static_assert(std::is_same_v<typename f_trait::fun_sig, void(responder&)>);
  using impl_t = detail::http_catch_all_route_impl<F>;
  return make_counted<impl_t>(std::move(f));
}

} // namespace caf::net::http
