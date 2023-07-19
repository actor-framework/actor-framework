#include "caf/net/http/route.hpp"

#include "caf/net/http/request.hpp"
#include "caf/net/http/responder.hpp"
#include "caf/net/multiplexer.hpp"

#include "caf/async/future.hpp"
#include "caf/disposable.hpp"

namespace caf::net::http {

route::~route() {
  // nop
}

void route::init() {
  // nop
}

} // namespace caf::net::http

namespace caf::detail {

size_t args_in_path(std::string_view str) {
  size_t count = 0;
  size_t start = 0;
  size_t end = 0;
  while (end != std::string_view::npos) {
    end = str.find('/', start);
    auto component
      = str.substr(start, end == std::string_view::npos ? end : end - start);
    if (component == "<arg>")
      ++count;
    start = end + 1;
  }
  return count;
}

std::pair<std::string_view, std::string_view>
next_path_component(const std::string_view str) {
  if (str.empty() || str.front() != '/') {
    return {std::string_view{}, std::string_view{}};
  }
  size_t start = 1;
  size_t end = str.find('/', start);
  auto component
    = str.substr(start, end == std::string_view::npos ? end : end - start);
  auto remaining = end == std::string_view::npos ? std::string_view{}
                                                 : str.substr(end);
  return {component, remaining};
}

bool http_simple_route_base::exec(const net::http::request_header& hdr,
                                  const_byte_span body,
                                  net::http::router* parent) {
  if (method_ && *method_ != hdr.method())
    return false;
  if (hdr.path() == path_) {
    net::http::responder rp{&hdr, body, parent};
    do_apply(rp);
    return true;
  }
  return false;
}

} // namespace caf::detail
