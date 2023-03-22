// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/print.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/expected.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/net/http/arg_parser.hpp"
#include "caf/net/http/lower_layer.hpp"
#include "caf/net/http/responder.hpp"
#include "caf/net/http/upper_layer.hpp"
#include "caf/ref_counted.hpp"

#include <algorithm>
#include <cassert>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace caf::net::http {

/// Sits on top of a @ref server and dispatches incoming requests to
/// user-defined handlers.
class CAF_NET_EXPORT router : public upper_layer {
public:
  // -- member types -----------------------------------------------------------

  class route : public ref_counted {
  public:
    virtual ~route();

    /// Returns `true` if the route accepted the request, `false` otherwise.
    virtual bool
    exec(const request_header& hdr, const_byte_span body, router* parent)
      = 0;

    /// Counts how many `<arg>` entries are in `path`.
    static size_t args_in_path(std::string_view path);

    /// Splits `str` in the first component of a path and its remainder.
    static std::pair<std::string_view, std::string_view>
    next_component(std::string_view str);

    /// Matches a two paths by splitting both inputs at '/' and then checking
    /// that `predicate` holds for each resulting pair.
    template <class F>
    bool match_path(std::string_view lhs, std::string_view rhs, F&& predicate) {
      std::string_view head1;
      std::string_view tail1;
      std::string_view head2;
      std::string_view tail2;
      std::tie(head1, tail1) = next_component(lhs);
      std::tie(head2, tail2) = next_component(rhs);
      if (!predicate(head1, head2))
        return false;
      while (!tail1.empty()) {
        if (tail2.empty())
          return false;
        std::tie(head1, tail1) = next_component(tail1);
        std::tie(head2, tail2) = next_component(tail2);
        if (!predicate(head1, head2))
          return false;
      }
      return tail2.empty();
    }
  };

  using route_ptr = intrusive_ptr<route>;

  // -- constructors and destructors -------------------------------------------

  router() = default;

  explicit router(std::vector<route_ptr> routes) : routes_(std::move(routes)) {
    // nop
  }

  ~router() override;

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<router> make(std::vector<route_ptr> routes);

  /// Tries to create a new HTTP route.
  /// @param path The path on this server for the new route.
  /// @param f The function object for handling requests on the new route.
  /// @return the @ref route object on success, an @ref error otherwise.
  template <class F>
  static expected<route_ptr> make_route(std::string path, F f) {
    return make_route_dis(path, std::nullopt, f);
  }

  /// Tries to create a new HTTP route.
  /// @param path The path on this server for the new route.
  /// @param method The allowed HTTP method on the new route.
  /// @param f The function object for handling requests on the new route.
  /// @return the @ref route object on success, an @ref error otherwise.
  template <class F>
  static expected<route_ptr>
  make_route(std::string path, http::method method, F f) {
    return make_route_dis(path, method, f);
  }

  /// Create a new HTTP default "catch all" route.
  /// @param f The function object for handling the requests.
  /// @return the @ref route object.
  template <class F>
  static route_ptr make_route(F f) {
    return make_counted<default_route_impl<F>>(std::move(f));
  }

  // -- properties -------------------------------------------------------------

  lower_layer* down() {
    return down_;
  }

  // -- API for the responders -------------------------------------------------

  /// Lifts a @ref responder to an @ref request object that allows asynchronous
  /// processing of the HTTP request.
  request lift(responder&& res);

  void shutdown(const error& err);

  // -- http::upper_layer implementation ---------------------------------------

  error start(lower_layer* down) override;

  ptrdiff_t consume(const request_header& hdr,
                    const_byte_span payload) override;

  void prepare_send() override;

  bool done_sending() override;

  void abort(const error& reason) override;

private:
  template <class F, class... Ts>
  class route_impl : public route {
  public:
    explicit route_impl(std::string&& path, std::optional<http::method> method,
                        F&& f)
      : path_(std::move(path)), method_(method), f_(std::move(f)) {
      // nop
    }

    bool exec(const request_header& hdr, const_byte_span body,
              router* parent) override {
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
    bool exec_dis(const request_header& hdr, const_byte_span body,
                  router* parent, std::index_sequence<Is...>,
                  std::string_view* arr) {
      return exec_impl(hdr, body, parent,
                       std::get<Is>(parsers_).parse(arr[Is])...);
    }

    template <class... Is>
    bool exec_impl(const request_header& hdr, const_byte_span body,
                   router* parent, std::optional<Ts>&&... args) {
      if ((args.has_value() && ...)) {
        responder rp{&hdr, body, parent};
        f_(rp, std::move(*args)...);
        return true;
      }
      return false;
    }

  private:
    std::string path_;
    std::optional<http::method> method_;
    F f_;
    std::tuple<arg_parser_t<Ts>...> parsers_;
  };

  template <class F>
  class trivial_route_impl : public route {
  public:
    explicit trivial_route_impl(std::string&& path,
                                std::optional<http::method> method, F&& f)
      : path_(std::move(path)), method_(method), f_(std::move(f)) {
      // nop
    }

    bool exec(const request_header& hdr, const_byte_span body,
              router* parent) override {
      if (method_ && *method_ != hdr.method())
        return false;
      if (hdr.path() == path_) {
        responder rp{&hdr, body, parent};
        f_(rp);
        return true;
      }
      return false;
    }

  private:
    std::string path_;
    std::optional<http::method> method_;
    F f_;
  };

  template <class F>
  class default_route_impl : public route {
  public:
    explicit default_route_impl(F&& f) : f_(std::move(f)) {
      // nop
    }

    bool exec(const request_header& hdr, const_byte_span body,
              router* parent) override {
      responder rp{&hdr, body, parent};
      f_(rp);
      return true;
    }

  private:
    F f_;
  };

  // Dispatches to make_route_impl after sanity checking.
  template <class F>
  static expected<route_ptr>
  make_route_dis(std::string& path, std::optional<http::method> method, F& f) {
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
    // The path must has as many <arg> entries as F takes extra arguments.
    auto num_args = route::args_in_path(path);
    if (num_args != f_trait::num_args - 1) {
      auto msg = path;
      msg += " defines ";
      detail::print(msg, num_args);
      msg += " arguments, but F accepts ";
      detail::print(msg, f_trait::num_args - 1);
      return make_error(sec::invalid_argument, std::move(msg));
    }
    // Dispatch to the actual factory.
    return make_route_impl(path, method, f, f_args{});
  }

  template <class F, class... Args>
  static expected<route_ptr>
  make_route_impl(std::string& path, std::optional<http::method> method, F& f,
                  detail::type_list<responder&, Args...>) {
    if constexpr (sizeof...(Args) == 0) {
      return make_counted<trivial_route_impl<F>>(std::move(path), method,
                                                 std::move(f));
    } else {
      return make_counted<route_impl<F, Args...>>(std::move(path), method,
                                                  std::move(f));
    }
  }

  lower_layer* down_ = nullptr;
  std::vector<route_ptr> routes_;
  size_t request_id_ = 0;
  std::unordered_map<size_t, disposable> pending_;
};

} // namespace caf::net::http
