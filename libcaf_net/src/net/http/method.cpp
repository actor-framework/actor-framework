#include "caf/net/http/method.hpp"

namespace caf::net::http {

std::string to_rfc_string(method x) {
  using namespace caf::literals;
  switch (x) {
    case method::get:
      return "GET";
    case method::head:
      return "HEAD";
    case method::post:
      return "POST";
    case method::put:
      return "PUT";
    case method::del:
      return "DELETE";
    case method::connect:
      return "CONNECT";
    case method::options:
      return "OPTIONS";
    case method::trace:
      return "TRACE";
    default:
      return "INVALID";
  }
}

} // namespace caf::net::http
