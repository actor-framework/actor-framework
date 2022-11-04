// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/string_view.hpp"

#include <cstdint>
#include <string>
#include <type_traits>

namespace caf::net::http {

/// Status codes as defined by RFC 7231 and RFC 6585.
enum class status : uint16_t {
  /// Indicates that the initial part of a request has been received and has not
  /// yet been rejected by the server. The server intends to send a final
  /// response after the request has been fully received and acted upon.
  continue_request = 100,
  /// Indicates that the server understands and is willing to comply with the
  /// client's request for a change in the application protocol being used on
  /// this connection.
  switching_protocols = 101,
  /// Indicates that the request has succeeded.
  ok = 200,
  /// Indicates that the request has been fulfilled and has resulted in one or
  /// more new resources being created.
  created = 201,
  /// Indicates that the request has been accepted for processing, but the
  /// processing has not been completed.
  accepted = 202,
  /// Indicates that the request was successful but the enclosed payload has
  /// been modified from that of the origin server's 200 (OK) response by a
  /// transforming proxy
  non_authoritative_information = 203,
  /// Indicates that the server has successfully fulfilled the request and that
  /// there is no additional content to send in the response payload body.
  no_content = 204,
  /// Indicates that the server has fulfilled the request and desires that the
  /// user agent reset the "document view".
  reset_content = 205,
  /// Indicates that the server is successfully fulfilling a range request for
  /// the target resource by transferring one or more parts of the selected
  /// representation that correspond to the satisfiable ranges found in the
  /// request's Range header field.
  partial_content = 206,
  /// Indicates that the target resource has more than one representation and
  /// information about the alternatives is being provided so that the user (or
  /// user agent) can select a preferred representation.
  multiple_choices = 300,
  /// Indicates that the target resource has been assigned a new permanent URI.
  moved_permanently = 301,
  /// Indicates that the target resource resides temporarily under a different
  /// URI.
  found = 302,
  /// Indicates that the server is redirecting the user agent to a different
  /// resource, as indicated by a URI in the Location header field, which is
  /// intended to provide an indirect response to the original request.
  see_other = 303,
  /// Indicates that a conditional GET or HEAD request has been received and
  /// would have resulted in a 200 (OK) response if it were not for the fact
  /// that the condition evaluated to false.
  not_modified = 304,
  /// Deprecated.
  use_proxy = 305,
  ///  No longer valid.
  temporary_redirect = 307,
  /// Indicates that the server cannot or will not process the request due to
  /// something that is perceived to be a client error (e.g., malformed request
  /// syntax, invalid request message framing, or deceptive request routing).
  bad_request = 400,
  /// Indicates that the request has not been applied because it lacks valid
  /// authentication credentials for the target resource.
  unauthorized = 401,
  /// Reserved for future use.
  payment_required = 402,
  /// Indicates that the server understood the request but refuses to authorize
  /// it.
  forbidden = 403,
  /// Indicates that the origin server did not find a current representation for
  /// the target resource or is not willing to disclose that one exists.
  not_found = 404,
  /// Indicates that the method received in the request-line is known by the
  /// origin server but not supported by the target resource.
  method_not_allowed = 405,
  /// Indicates that the target resource does not have a current representation
  /// that would be acceptable to the user agent, according to the proactive
  /// negotiation header fields received in the request (Section 5.3), and the
  /// server is unwilling to supply a default representation.
  not_acceptable = 406,
  /// Similar to 401 (Unauthorized), but it indicates that the client needs to
  /// authenticate itself in order to use a proxy.
  proxy_authentication_required = 407,
  /// Indicates that the server did not receive a complete request message
  /// within the time that it was prepared to wait.
  request_timeout = 408,
  /// Indicates that the request could not be completed due to a conflict with
  /// the current state of the target resource.  This code is used in situations
  /// where the user might be able to resolve the conflict and resubmit the
  /// request.
  conflict = 409,
  /// Indicates that access to the target resource is no longer available at the
  /// origin server and that this condition is likely to be permanent.
  gone = 410,
  /// Indicates that the server refuses to accept the request without a defined
  /// Content-Length.
  length_required = 411,
  /// Indicates that one or more conditions given in the request header fields
  /// evaluated to false when tested on the server.
  precondition_failed = 412,
  /// Indicates that the server is refusing to process a request because the
  /// request payload is larger than the server is willing or able to process.
  payload_too_large = 413,
  /// Indicates that the server is refusing to service the request because the
  /// request-target is longer than the server is willing to interpret.
  uri_too_long = 414,
  /// Indicates that the origin server is refusing to service the request
  /// because the payload is in a format not supported by this method on the
  /// target resource.
  unsupported_media_type = 415,
  /// Indicates that none of the ranges in the request's Range header field
  /// overlap the current extent of the selected resource or that the set of
  /// ranges requested has been rejected due to invalid ranges or an excessive
  /// request of small or overlapping ranges.
  range_not_satisfiable = 416,
  /// Indicates that the expectation given in the request's Expect header field
  /// could not be met by at least one of the inbound servers.
  expectation_failed = 417,
  /// Indicates that the server refuses to perform the request using the current
  /// protocol but might be willing to do so after the client upgrades to a
  /// different protocol.
  upgrade_required = 426,
  /// Indicates that the origin server requires the request to be conditional.
  precondition_required = 428,
  /// Indicates that the user has sent too many requests in a given amount of
  /// time ("rate limiting").
  too_many_requests = 429,
  /// Indicates that the server is unwilling to process the request because its
  /// header fields are too large.
  request_header_fields_too_large = 431,
  /// Indicates that the server encountered an unexpected condition that
  /// prevented it from fulfilling the request.
  internal_server_error = 500,
  /// Indicates that the server does not support the functionality required to
  /// fulfill the request.
  not_implemented = 501,
  /// Indicates that the server, while acting as a gateway or proxy, received an
  /// invalid response from an inbound server it accessed while attempting to
  /// fulfill the request.
  bad_gateway = 502,
  /// Indicates that the server is currently unable to handle the request due to
  /// a temporary overload or scheduled maintenance, which will likely be
  /// alleviated after some delay.
  service_unavailable = 503,
  /// Indicates that the server, while acting as a gateway or proxy, did not
  /// receive a timely response from an upstream server it needed to access in
  /// order to complete the request.
  gateway_timeout = 504,
  /// Indicates that the server does not support, or refuses to support, the
  /// major version of HTTP that was used in the request message.
  http_version_not_supported = 505,
  /// Indicates that the client needs to authenticate to gain network access.
  network_authentication_required = 511,
};

/// Returns the recommended response phrase to a status code.
/// @relates status
CAF_NET_EXPORT string_view phrase(status);

/// @relates status
CAF_NET_EXPORT std::string to_string(status);

/// @relates status
CAF_NET_EXPORT bool from_string(string_view, status&);

/// @relates status
CAF_NET_EXPORT bool from_integer(std::underlying_type_t<status>, status&);

/// @relates status
template <class Inspector>
bool inspect(Inspector& f, status& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::net::http
