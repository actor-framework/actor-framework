/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 * Raphael Hiesgen <raphael.hiesgen (at) haw-hamburg.de>                      *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/chrono.hpp"
#include "caf/duration.hpp"

namespace caf {

time_point& time_point::operator+=(const duration& d) {
    switch (d.unit) {
      case time_unit::seconds:
        m_handle.seconds += static_cast<uint32_t>(d.count);
        break;
      case time_unit::microseconds:
        m_handle.microseconds += static_cast<uint32_t>(d.count);
        break;
      case time_unit::milliseconds:
        m_handle.microseconds += static_cast<uint32_t>(1000 * d.count);
        break;
      case time_unit::invalid:
        break;
    }
    adjust_overhead();
    return *this;
  }

} // namespace caf
