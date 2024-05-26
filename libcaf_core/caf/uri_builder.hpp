// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/uri.hpp"

#include <cstdint>
#include <string>

namespace caf {

class CAF_CORE_EXPORT uri_builder {
public:
  // -- member types -----------------------------------------------------------

  /// Pointer to implementation.
  using impl_ptr = intrusive_ptr<uri::impl_type>;

  // -- constructors, destructors, and assignment operators --------------------

  uri_builder();

  uri_builder(uri_builder&&) = default;

  uri_builder& operator=(uri_builder&&) = default;

  // -- setter -----------------------------------------------------------------

  uri_builder& scheme(std::string str);

  uri_builder& userinfo(std::string str);

  uri_builder& userinfo(std::string str, std::string password);

  uri_builder& host(std::string str);

  uri_builder& host(ip_address addr);

  uri_builder& port(uint16_t value);

  uri_builder& path(std::string str);

  uri_builder& query(uri::query_map map);

  uri_builder& fragment(std::string str);

  // -- factory functions ------------------------------------------------------

  uri make();

private:
  // -- member variables -------------------------------------------------------

  impl_ptr impl_;
};

} // namespace caf
