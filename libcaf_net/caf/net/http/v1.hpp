// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte_span.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/net/http/status.hpp"

#include <string_view>
#include <utility>

namespace caf::net::http::v1 {

using string_view_pair = std::pair<std::string_view, std::string_view>;

/// Tries splitting the given byte span into an HTTP header (`first`) and a
/// remainder (`second`). Returns an empty `string_view` as `first` for
/// incomplete HTTP headers.
CAF_NET_EXPORT std::pair<std::string_view, byte_span>
split_header(byte_span bytes);

/// Writes an HTTP header to @p buf.
CAF_NET_EXPORT void write_header(status code,
                                 span<const string_view_pair> fields,
                                 byte_buffer& buf);

/// Write the status code for an HTTP header to @p buf.
CAF_NET_EXPORT void begin_header(status code, byte_buffer& buf);

/// Write a header field to @p buf.
CAF_NET_EXPORT void add_header_field(std::string_view key, std::string_view val,
                                     byte_buffer& buf);

/// Write the status code for an HTTP header to @buf.
CAF_NET_EXPORT bool end_header(byte_buffer& buf);

/// Writes a complete HTTP response to @p buf. Automatically sets Content-Type
/// and Content-Length header fields.
CAF_NET_EXPORT void write_response(status code, std::string_view content_type,
                                   std::string_view content, byte_buffer& buf);

/// Writes a complete HTTP response to @p buf. Automatically sets Content-Type
/// and Content-Length header fields followed by the user-defined @p fields.
CAF_NET_EXPORT void write_response(status code, std::string_view content_type,
                                   std::string_view content,
                                   span<const string_view_pair> fields,
                                   byte_buffer& buf);

} // namespace caf::net::http::v1
