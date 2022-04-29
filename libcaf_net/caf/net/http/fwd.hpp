// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"

#include <string_view>

namespace caf::net::http {

class header;
class lower_layer;
class server;
class upper_layer;

using header_fields_map
  = unordered_flat_map<std::string_view, std::string_view>;

enum class method : uint8_t;

enum class status : uint16_t;

} // namespace caf::net::http
