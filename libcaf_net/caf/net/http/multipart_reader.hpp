// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/http/header.hpp"

#include "caf/byte_span.hpp"

#include <string_view>
#include <vector>

namespace caf::net::http {

/// A utility class for parsing and accessing multipart content from an HTTP
/// request.
class CAF_NET_EXPORT multipart_reader {
public:
  /// Represents a single part in the multipart content.
  struct part {
    http::header header;
    const_byte_span content;
  };

  /// Constructs a multipart_reader from a header and body.
  multipart_reader(const http::header& hdr, const_byte_span body);

  /// Constructs a multipart_reader from a responder.
  explicit multipart_reader(const responder& res);

  /// Returns the MIME type of the multipart content.
  std::string_view mime_type() const noexcept {
    return mime_type_;
  }

  /// Parses the multipart content and returns the parts.
  /// @param ok If not `nullptr`, set to `true` if parsing succeeded, `false`
  ///          otherwise. This allows to distinguish between parsing errors and
  ///          empty multipart content.
  /// @returns The parts of the multipart content or an empty vector if parsing
  ///          failed.
  std::vector<part> parse(bool* ok = nullptr);

  /// Parses the multipart content and calls the given function for each part.
  /// @param fn The function to call for each part. The function must be
  ///           callable as `void(http::header&&, const_byte_span)`.
  /// @returns `true` if parsing succeeded, `false` otherwise.
  template <class ConsumeFn>
  [[nodiscard]] bool for_each(ConsumeFn&& fn) {
    using fn_t = std::decay_t<ConsumeFn>;
    auto cb = [](void* fnptr, http::header&& header, const_byte_span content) {
      (*reinterpret_cast<fn_t*>(fnptr))(std::move(header), content);
    };
    return do_parse(cb, &fn);
  }

private:
  using consume_fn = void (*)(void*, http::header&&, const_byte_span);

  /// Parses multipart content and calls the given function for each part.
  /// @param fn Function to call for each part
  /// @param obj User data passed to the function
  /// @returns true if parsing succeeded, false on any error
  bool do_parse(consume_fn fn, void* obj);

  /// Provides access to the HTTP body.
  const_byte_span body_;

  /// The MIME type of the HTTP request.
  std::string_view mime_type_;
};

} // namespace caf::net::http
