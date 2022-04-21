#include "caf/net/http/status.hpp"

namespace caf::net::http {

string_view phrase(status code) {
  using namespace caf::literals;
  switch (code) {
    case status::continue_request:
      return "Continue"_sv;
    case status::switching_protocols:
      return "Switching Protocols"_sv;
    case status::ok:
      return "OK"_sv;
    case status::created:
      return "Created"_sv;
    case status::accepted:
      return "Accepted"_sv;
    case status::non_authoritative_information:
      return "Non-Authoritative Information"_sv;
    case status::no_content:
      return "No Content"_sv;
    case status::reset_content:
      return "Reset Content"_sv;
    case status::partial_content:
      return "Partial Content"_sv;
    case status::multiple_choices:
      return "Multiple Choices"_sv;
    case status::moved_permanently:
      return "Moved Permanently"_sv;
    case status::found:
      return "Found"_sv;
    case status::see_other:
      return "See Other"_sv;
    case status::not_modified:
      return "Not Modified"_sv;
    case status::use_proxy:
      return "Use Proxy"_sv;
    case status::temporary_redirect:
      return "Temporary Redirect"_sv;
    case status::bad_request:
      return "Bad Request"_sv;
    case status::unauthorized:
      return "Unauthorized"_sv;
    case status::payment_required:
      return "Payment Required"_sv;
    case status::forbidden:
      return "Forbidden"_sv;
    case status::not_found:
      return "Not Found"_sv;
    case status::method_not_allowed:
      return "Method Not Allowed"_sv;
    case status::not_acceptable:
      return "Not Acceptable"_sv;
    case status::proxy_authentication_required:
      return "Proxy Authentication Required"_sv;
    case status::request_timeout:
      return "Request Timeout"_sv;
    case status::conflict:
      return "Conflict"_sv;
    case status::gone:
      return "Gone"_sv;
    case status::length_required:
      return "Length Required"_sv;
    case status::precondition_failed:
      return "Precondition Failed"_sv;
    case status::payload_too_large:
      return "Payload Too Large"_sv;
    case status::uri_too_long:
      return "URI Too Long"_sv;
    case status::unsupported_media_type:
      return "Unsupported Media Type"_sv;
    case status::range_not_satisfiable:
      return "Range Not Satisfiable"_sv;
    case status::expectation_failed:
      return "Expectation Failed"_sv;
    case status::upgrade_required:
      return "Upgrade Required"_sv;
    case status::precondition_required:
      return "Precondition Required"_sv;
    case status::too_many_requests:
      return "Too Many Requests"_sv;
    case status::request_header_fields_too_large:
      return "Request Header Fields Too Large"_sv;
    case status::internal_server_error:
      return "Internal Server Error"_sv;
    case status::not_implemented:
      return "Not Implemented"_sv;
    case status::bad_gateway:
      return "Bad Gateway"_sv;
    case status::service_unavailable:
      return "Service Unavailable"_sv;
    case status::gateway_timeout:
      return "Gateway Timeout"_sv;
    case status::http_version_not_supported:
      return "HTTP Version Not Supported"_sv;
    case status::network_authentication_required:
      return "Network Authentication Required"_sv;
    default:
      return "Unrecognized";
  }
}

} // namespace caf::net::http
