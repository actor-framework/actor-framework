// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/frame.hpp"

namespace caf::net::web_socket {

const_byte_span frame::as_binary() const noexcept {
  using res_t = const_byte_span;
  return data_ ? res_t{data_->storage(), data_->size()} : res_t{};
}

std::string_view frame::as_text() const noexcept {
  using res_t = std::string_view;
  return data_ ? res_t{reinterpret_cast<const char*>(data_->storage()),
                       data_->size()}
               : res_t{};
}

} // namespace caf::net::web_socket
