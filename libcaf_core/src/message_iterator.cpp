/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/message_iterator.hpp"

#include "caf/detail/message_data.hpp"

namespace caf {
namespace detail {

message_iterator::message_iterator(const_pointer dataptr, size_t pos)
    : m_pos(pos),
      m_data(dataptr) {
  // nop
}

const void* message_iterator::value() const {
  return m_data->at(m_pos);
}

bool message_iterator::match_element(uint16_t typenr,
                                     const std::type_info* rtti) const {
  return m_data->match_element(m_pos, typenr, rtti);
}

} // namespace detail
} // namespace caf
