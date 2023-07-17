#include "caf/net/http/status.hpp"

#include <string_view>

namespace caf::net::http {

std::string_view phrase(status code) {
  using namespace std::literals;
  switch (code) {
    case status::continue_request:
      return "Continue"sv;
    case status::switching_protocols:
      return "Switching Protocols"sv;
    case status::ok:
      return "OK"sv;
    case status::created:
      return "Created"sv;
    case status::accepted:
      return "Accepted"sv;
    case status::non_authoritative_information:
      return "Non-Authoritative Information"sv;
    case status::no_content:
      return "No Content"sv;
    case status::reset_content:
      return "Reset Content"sv;
    case status::partial_content:
      return "Partial Content"sv;
    case status::multiple_choices:
      return "Multiple Choices"sv;
    case status::moved_permanently:
      return "Moved Permanently"sv;
    case status::found:
      return "Found"sv;
    case status::see_other:
      return "See Other"sv;
    case status::not_modified:
      return "Not Modified"sv;
    case status::use_proxy:
      return "Use Proxy"sv;
    case status::temporary_redirect:
      return "Temporary Redirect"sv;
    case status::bad_request:
      return "Bad Request"sv;
    case status::unauthorized:
      return "Unauthorized"sv;
    case status::payment_required:
      return "Payment Required"sv;
    case status::forbidden:
      return "Forbidden"sv;
    case status::not_found:
      return "Not Found"sv;
    case status::method_not_allowed:
      return "Method Not Allowed"sv;
    case status::not_acceptable:
      return "Not Acceptable"sv;
    case status::proxy_authentication_required:
      return "Proxy Authentication Required"sv;
    case status::request_timeout:
      return "Request Timeout"sv;
    case status::conflict:
      return "Conflict"sv;
    case status::gone:
      return "Gone"sv;
    case status::length_required:
      return "Length Required"sv;
    case status::precondition_failed:
      return "Precondition Failed"sv;
    case status::payload_too_large:
      return "Payload Too Large"sv;
    case status::uri_too_long:
      return "URI Too Long"sv;
    case status::unsupported_media_type:
      return "Unsupported Media Type"sv;
    case status::range_not_satisfiable:
      return "Range Not Satisfiable"sv;
    case status::expectation_failed:
      return "Expectation Failed"sv;
    case status::upgrade_required:
      return "Upgrade Required"sv;
    case status::precondition_required:
      return "Precondition Required"sv;
    case status::too_many_requests:
      return "Too Many Requests"sv;
    case status::request_header_fields_too_large:
      return "Request Header Fields Too Large"sv;
    case status::internal_server_error:
      return "Internal Server Error"sv;
    case status::not_implemented:
      return "Not Implemented"sv;
    case status::bad_gateway:
      return "Bad Gateway"sv;
    case status::service_unavailable:
      return "Service Unavailable"sv;
    case status::gateway_timeout:
      return "Gateway Timeout"sv;
    case status::http_version_not_supported:
      return "HTTP Version Not Supported"sv;
    case status::network_authentication_required:
      return "Network Authentication Required"sv;
    default:
      return "Unrecognized"sv;
  }
}

} // namespace caf::net::http
