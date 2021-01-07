// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/middleman_backend.hpp"

namespace caf::net {

middleman_backend::middleman_backend(std::string id) : id_(std::move(id)) {
  // nop
}

middleman_backend::~middleman_backend() {
  // nop
}

} // namespace caf::net
