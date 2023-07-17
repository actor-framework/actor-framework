// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/response.hpp"

namespace caf::net::http {

response::response(status code, fields_map fm, std::vector<std::byte> body) {
  pimpl_ = std::make_shared<impl>(impl{code, std::move(fm), std::move(body)});
}

} // namespace caf::net::http
