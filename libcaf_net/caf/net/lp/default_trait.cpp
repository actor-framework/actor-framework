// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/lp/default_trait.hpp"

#include "caf/net/lp/frame.hpp"

#include "caf/error.hpp"
#include "caf/log/system.hpp"
#include "caf/sec.hpp"

namespace caf::net::lp {

bool default_trait::convert(const output_type& x, byte_buffer& bytes) {
  auto src = x.bytes();
  bytes.insert(bytes.end(), src.begin(), src.end());
  return true;
}

bool default_trait::convert(const_byte_span bytes, input_type& x) {
  x = frame{bytes};
  return true;
}

error default_trait::last_error() {
  log::system::error("lp::default_trait::last_error called");
  return {sec::logic_error};
}

} // namespace caf::net::lp
