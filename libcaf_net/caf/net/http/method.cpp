#include "caf/net/http/method.hpp"

namespace caf::net::http {

std::string to_rfc_string(method x) {
  using namespace std::literals;
  switch (x) {
    case method::get:
      return "GET"s;
    case method::head:
      return "HEAD"s;
    case method::post:
      return "POST"s;
    case method::put:
      return "PUT"s;
    case method::del:
      return "DELETE"s;
    case method::connect:
      return "CONNECT"s;
    case method::options:
      return "OPTIONS"s;
    case method::trace:
      return "TRACE"s;
    default:
      return "INVALID"s;
  }
}

} // namespace caf::net::http
