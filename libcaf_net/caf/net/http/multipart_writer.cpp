// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/multipart_writer.hpp"

#include <span>

namespace caf::net::http {

namespace {

/// The default boundary string used by the multipart writer. No particular
/// reason for this string, just using the example string from rfc2046.
constexpr std::string_view default_boundary = "gc0p4Jq0M2Yt08j34c0p";

// Helper function to write a string to a byte buffer
void write_string(byte_buffer& buf, std::string_view str) {
  auto bytes = as_bytes(std::span{str});
  buf.insert(buf.end(), bytes.begin(), bytes.end());
}

} // namespace

multipart_writer::header_builder&
multipart_writer::header_builder::add(std::string_view key,
                                      std::string_view value) {
  auto& buf = writer_->buf_;
  write_string(buf, key);
  write_string(buf, ": ");
  write_string(buf, value);
  write_string(buf, "\r\n");
  return *this;
}

multipart_writer::multipart_writer() : boundary_(default_boundary) {
  // nop
}

multipart_writer::multipart_writer(std::string boundary)
  : boundary_(std::move(boundary)) {
  // nop
}

void multipart_writer::reset() {
  buf_.clear();
}

void multipart_writer::reset(std::string boundary) {
  boundary_ = std::move(boundary);
  buf_.clear();
}

void multipart_writer::append(const_byte_span payload) {
  append(payload, [](header_builder&) {});
}

void multipart_writer::append(const_byte_span payload, std::string_view key,
                              std::string_view value) {
  auto fn = [key, value](header_builder& builder) { builder.add(key, value); };
  append(payload, fn);
}

const_byte_span multipart_writer::finalize() {
  write_string(buf_, "--");
  write_string(buf_, boundary_);
  write_string(buf_, "--\r\n");
  return std::span{buf_};
}

std::string_view multipart_writer::boundary() const noexcept {
  return boundary_;
}

void multipart_writer::do_append(const_byte_span payload, add_headers_fn& fn) {
  header_builder builder{this};
  write_string(buf_, "--");
  write_string(buf_, boundary_);
  write_string(buf_, "\r\n");
  fn(builder);
  write_string(buf_, "\r\n");
  buf_.insert(buf_.end(), payload.begin(), payload.end());
  write_string(buf_, "\r\n");
}

} // namespace caf::net::http
