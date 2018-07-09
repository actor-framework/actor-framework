/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <cstdint>
#include <string>

#include "caf/fwd.hpp"
#include "caf/uri.hpp"

namespace caf {

class uri_builder {
public:
  // -- member types -----------------------------------------------------------

  /// Pointer to implementation.
  using impl_ptr = intrusive_ptr<detail::uri_impl>;

  // -- constructors, destructors, and assignment operators --------------------

  uri_builder();

  uri_builder(uri_builder&&) = default;

  uri_builder& operator=(uri_builder&&) = default;

  // -- setter -----------------------------------------------------------------

  uri_builder& scheme(std::string str);

  uri_builder& userinfo(std::string str);

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
