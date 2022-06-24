// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/binary/frame.hpp"

#include <cstring>
#include <new>

namespace caf::net::binary {

frame::frame(const_byte_span data) {
  auto total_size = sizeof(web_socket::frame::data) + data.size();
  auto vptr = malloc(total_size);
  data_.reset(new (vptr) web_socket::frame::data(true, data.size()), false);
  memcpy(data_->storage(), data.data(), data.size());
}

} // namespace caf::net::binary
