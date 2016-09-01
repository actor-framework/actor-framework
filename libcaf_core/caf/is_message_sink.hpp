/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_IS_MESSAGE_SINK_HPP
#define CAF_IS_MESSAGE_SINK_HPP

#include <type_traits>

#include "caf/fwd.hpp"

namespace caf {

template <class T>
struct is_message_sink : std::false_type { };

template <>
struct is_message_sink<actor> : std::true_type { };

template <>
struct is_message_sink<group> : std::true_type { };

template <class... Ts>
struct is_message_sink<typed_actor<Ts...>> : std::true_type { };

} // namespace caf

#endif // CAF_IS_MESSAGE_SINK_HPP
