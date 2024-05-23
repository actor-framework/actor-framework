// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/request.hpp"

#include <utility>

using namespace std::literals;

// TODO: reduce number of memory allocations for producing the response.

namespace caf::net::http {

class request::impl : public ref_counted {
public:
  impl(request_header hdr, std::vector<std::byte> body,
       async::promise<response> prom)
    : hdr(std::move(hdr)), body(std::move(body)), prom(std::move(prom)) {
    // nop
  }

  request_header hdr;
  std::vector<std::byte> body;
  async::promise<response> prom;
};

request::request(request&& other) noexcept : impl_(other.impl_) {
  if (impl_) {
    other.impl_ = nullptr;
  }
}

request::request(const request& other) noexcept {
  if (other.impl_ != nullptr) {
    impl_ = other.impl_;
    impl_->ref();
  }
}

request::request(request_header hdr, std::vector<std::byte> body,
                 async::promise<response> prom) {
  impl_ = new impl(std::move(hdr), std::move(body), std::move(prom));
}

request& request::operator=(request&& other) noexcept {
  std::swap(impl_, other.impl_);
  return *this;
}

request& request::operator=(const request& other) noexcept {
  request tmp{other};
  std::swap(impl_, tmp.impl_);
  return *this;
}

request::~request() {
  if (impl_ != nullptr) {
    impl_->deref();
  }
}

const request_header& request::header() const {
  return impl_->hdr;
}

const_byte_span request::body() const {
  return make_span(impl_->body);
}

const_byte_span request::payload() const {
  return make_span(impl_->body);
}

void request::respond(status code, std::string_view content_type,
                      const_byte_span content) const {
  auto content_size = std::to_string(content.size());
  unordered_flat_map<std::string, std::string> fields;
  fields.emplace("Content-Type"sv, content_type);
  fields.emplace("Content-Length"sv, content_size);
  auto body = std::vector<std::byte>{content.begin(), content.end()};
  impl_->prom.set_value(response{code, std::move(fields), std::move(body)});
}

} // namespace caf::net::http
