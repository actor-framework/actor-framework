/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_ILLEGAL_MESSAGE_ELEMENT_HPP
#define CAF_ILLEGAL_MESSAGE_ELEMENT_HPP

#include <type_traits>

namespace caf {

/// Marker class identifying classes in CAF that are not allowed
/// to be used as message element.
struct illegal_message_element {
  // no members (marker class)
};

template <class T>
struct is_illegal_message_element
  : std::is_base_of<illegal_message_element, T> { };

} // namespace caf

#endif // CAF_ILLEGAL_MESSAGE_ELEMENT_HPP
