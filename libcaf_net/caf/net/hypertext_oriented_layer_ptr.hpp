// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte_span.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"

namespace caf::net {

/// Wraps a pointer to a hypertext-oriented layer with a pointer to its lower
/// layer. Both pointers are then used to implement the interface required for a
/// hypertext-oriented layer when calling into its upper layer.
template <class Layer, class LowerLayerPtr>
class hypertext_oriented_layer_ptr {
public:
  using context_type = typename Layer::context_type;

  using status_code_type = typename Layer::status_code_type;

  using header_fields_type = typename Layer::header_fields_type;

  class access {
  public:
    access(Layer* layer, LowerLayerPtr down) : lptr_(layer), llptr_(down) {
      // nop
    }

    /// Queries whether the underlying transport can send additional data.
    bool can_send_more() const noexcept {
      return lptr_->can_send_more(llptr_);
    }

    /// Asks the underlying transport to stop receiving additional data until
    /// resumed.
    void suspend_reading() {
      return lptr_->suspend_reading(llptr_);
    }

    /// Returns the socket handle.
    auto handle() const noexcept {
      return lptr_->handle(llptr_);
    }

    /// Sends a response header for answering the request identified by the
    /// context.
    /// @param context Identifies which request this response belongs to.
    /// @param code Indicates either success or failure to the client.
    /// @param fields Various informational fields for the client. When sending
    ///               a payload afterwards, the fields should at least include
    ///               the content length.
    bool send_header(context_type context, status_code_type code,
                     const header_fields_type& fields) {
      return lptr_->send_header(llptr_, context, code, fields);
    }

    /// Sends a payload to the client. Must follow a header.
    /// @param context Identifies which request this response belongs to.
    /// @param bytes Arbitrary data for the client.
    /// @pre `bytes.size() > 0`
    [[nodiscard]] bool send_payload(context_type context,
                                    const_byte_span bytes) {
      return lptr_->send_payload(llptr_, context, bytes);
    }

    /// Sends a single chunk of arbitrary data. The chunks must follow a header.
    /// @pre `bytes.size() > 0`
    [[nodiscard]] bool send_chunk(context_type context, const_byte_span bytes) {
      return lptr_->send_chunk(llptr_, context, bytes);
    }

    /// Informs the client that the transfer completed, i.e., that the server
    /// will not send additional chunks.
    [[nodiscard]] bool send_end_of_chunks(context_type context) {
      return lptr_->send_end_of_chunks(llptr_, context);
    }

    /// Convenience function for completing a request 'raw' (without adding
    /// additional header fields) in a single function call. Calls
    /// `send_header`, `send_payload` and `fin`.
    bool send_raw_response(context_type context, status_code_type code,
                           const header_fields_type& fields,
                           const_byte_span content) {
      if (lptr_->send_header(llptr_, context, code, fields)
          && (content.empty()
              || lptr_->send_payload(llptr_, context, content))) {
        lptr_->fin(llptr_, context);
        return true;
      } else {
        return false;
      }
    }

    /// Convenience function for completing a request in a single function call.
    /// Automatically sets the header fields 'Content-Type' and
    /// 'Content-Length'. Calls `send_header`, `send_payload` and `fin`.
    bool send_response(context_type context, status_code_type code,
                       header_fields_type fields, string_view content_type,
                       const_byte_span content) {
      std::string len;
      if (!content.empty()) {
        auto len = std::to_string(content.size());
        fields.emplace("Content-Length", len);
        fields.emplace("Content-Type", content_type);
      }
      return send_raw_response(context, code, fields, content);
    }

    /// Convenience function for completing a request in a single function call.
    /// Automatically sets the header fields 'Content-Type' and
    /// 'Content-Length'. Calls `send_header`, `send_payload` and `fin`.
    bool send_response(context_type context, status_code_type code,
                       string_view content_type, const_byte_span content) {
      std::string len;
      header_fields_type fields;
      if (!content.empty()) {
        len = std::to_string(content.size());
        fields.emplace("Content-Type", content_type);
        fields.emplace("Content-Length", len);
      }
      return send_raw_response(context, code, fields, content);
    }

    bool send_response(context_type context, status_code_type code,
                       header_fields_type fields, string_view content_type,
                       string_view content) {
      return send_response(context, code, content_type, std::move(fields),
                           as_bytes(make_span(content)));
    }

    bool send_response(context_type context, status_code_type code,
                       string_view content_type, string_view content) {
      return send_response(context, code, content_type,
                           as_bytes(make_span(content)));
    }

    void fin(context_type context) {
      lptr_->fin(llptr_, context);
    }

    /// Sets an abort reason on the transport.
    void abort_reason(error reason) {
      return lptr_->abort_reason(llptr_, std::move(reason));
    }

    /// Returns the current abort reason on the transport or a
    /// default-constructed error is no error occurred yet.
    const error& abort_reason() {
      return lptr_->abort_reason(llptr_);
    }

  private:
    Layer* lptr_;
    LowerLayerPtr llptr_;
  };

  hypertext_oriented_layer_ptr(Layer* layer, LowerLayerPtr down)
    : access_(layer, down) {
    // nop
  }

  hypertext_oriented_layer_ptr(const hypertext_oriented_layer_ptr&) = default;

  explicit operator bool() const noexcept {
    return true;
  }

  access* operator->() const noexcept {
    return &access_;
  }

  access& operator*() const noexcept {
    return access_;
  }

private:
  mutable access access_;
};

template <class Layer, class LowerLayerPtr>
auto make_hypertext_oriented_layer_ptr(Layer* this_layer, LowerLayerPtr down) {
  using result_t = hypertext_oriented_layer_ptr<Layer, LowerLayerPtr>;
  return result_t{this_layer, down};
}

} // namespace caf::net
