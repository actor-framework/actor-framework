// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/byte_buffer.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/net_export.hpp"

#include <string_view>

namespace caf::net::http {

/// A utility class for creating multipart content for HTTP requests.
class CAF_NET_EXPORT multipart_writer {
public:
  class CAF_NET_EXPORT header_builder {
  public:
    explicit header_builder(multipart_writer* writer) : writer_(writer) {
      // nop
    }

    header_builder& add(std::string_view key, std::string_view value);

  private:
    multipart_writer* writer_;
  };

  friend class header_builder;

  /// Constructs a multipart_writer with a default boundary.
  multipart_writer();

  /// Constructs a multipart_writer with a custom boundary.
  explicit multipart_writer(std::string boundary);

  /// Clears the buffer but keeps the boundary string.
  void reset();

  /// Clears the buffer and sets a new boundary string.
  void reset(std::string boundary);

  /// Appends a payload with no headers.
  void append(const_byte_span payload);

  /// Appends a payload with no headers from a string_view.
  void append(std::string_view payload) {
    append(as_bytes(make_span(payload)));
  }

  /// Appends a payload with a single header field.
  void append(const_byte_span payload, std::string_view key,
              std::string_view value);

  /// Appends a payload with a single header field from a string_view.
  void append(std::string_view payload, std::string_view key,
              std::string_view value) {
    append(as_bytes(make_span(payload)), key, value);
  }

  /// Appends a payload with custom header configuration.
  /// @param payload The payload to append.
  /// @param add_headers A function object for writing the headers to this part.
  ///                    The function object must be invocable with a
  ///                    `header_builder&` argument.
  template <class AddHeadersFn>
  void append(const_byte_span payload, AddHeadersFn&& add_headers) {
    using add_headers_t = std::decay_t<AddHeadersFn>;
    auto fn = [](void* ctx, header_builder& builder) {
      (*static_cast<add_headers_t*>(ctx))(builder);
    };
    do_append(payload, fn, &add_headers);
  }

  /// Appends a payload with custom header configuration from a string_view.
  template <class AddHeadersFn>
  void append(std::string_view payload, AddHeadersFn&& add_headers) {
    append(as_bytes(make_span(payload)),
           std::forward<AddHeadersFn>(add_headers));
  }

  /// Finalizes the multipart content by adding the final boundary and returns
  /// the rendered multipart body as a const_byte_span.
  const_byte_span finalize();

  /// Returns the boundary used by this writer.
  std::string_view boundary() const noexcept;

private:
  using add_headers_fn = void (*)(void*, header_builder&);

  /// Appends a payload with custom header configuration.
  /// @param payload The payload to append.
  /// @param fn A function object for writing the headers to this part.
  /// @param obj Additional context for the function object.
  void do_append(const_byte_span payload, add_headers_fn fn, void* obj);

  /// The buffer containing the multipart content.
  byte_buffer buf_;

  /// The boundary string used to separate parts.
  std::string boundary_;
};

} // namespace caf::net::http
