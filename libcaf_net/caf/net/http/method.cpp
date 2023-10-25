#include "caf/net/http/method.hpp"

namespace caf::net::http {

std::string_view to_rfc_string(method x) {
  using namespace std::literals;
  switch (x) {
    case method::get:
      return "GET"sv;
    case method::head:
      return "HEAD"sv;
    case method::post:
      return "POST"sv;
    case method::put:
      return "PUT"sv;
    case method::del:
      return "DELETE"sv;
    case method::connect:
      return "CONNECT"sv;
    case method::options:
      return "OPTIONS"sv;
    case method::trace:
      return "TRACE"sv;
    default:
      return "INVALID"sv;
  }
}

} // namespace caf::net::http
