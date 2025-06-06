// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/http/responder.hpp"

#include "caf/byte_span.hpp"

#include <optional>
#include <string>
#include <vector>

namespace caf::net::http {

/// A utility class for parsing and accessing multipart content from an HTTP
/// request.
class CAF_NET_EXPORT multipart_reader {
public:
  /// Represents a single part in the multipart content.
  struct part {
    std::string_view headers;
    const_byte_span content;
  };

  /// Constructs a multipart_reader from a responder.
  explicit multipart_reader(const responder& res);

  /// Returns the MIME type of the multipart content.
  std::string_view mime_type() const noexcept {
    return mime_type_;
  }

  /// Parses the multipart content and returns the parts.
  /// Returns an empty optional if parsing fails.
  std::optional<std::vector<part>> parse();

  /// Parses the multipart content and calls the given function for each part.
  /// @param fn The function to call for each part. The function must be
  ///           callable as `void(std::string_view, const_byte_span)`.
  /// @returns `true` if parsing succeeded, `false` otherwise.
  template <class ConsumeFn>
  [[nodiscard]] bool for_each(ConsumeFn&& fn) {
    using fn_t = std::decay_t<ConsumeFn>;
    auto cb = [](void* fnptr, std::string_view headers,
                 const_byte_span content) {
      (*reinterpret_cast<fn_t*>(fnptr))(headers, content);
    };
    return do_parse(cb, &fn);
  }

private:
  using consume_fn = void (*)(void*, std::string_view, const_byte_span);

  bool do_parse(consume_fn fn, void* obj);

  const responder& res_;
  std::string_view mime_type_;
};

} // namespace caf::net::http
