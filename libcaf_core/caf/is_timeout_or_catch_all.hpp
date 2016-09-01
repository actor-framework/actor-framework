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

#ifndef CAF_IS_TIMEOUT_OR_CATCH_ALL_HPP
#define CAF_IS_TIMEOUT_OR_CATCH_ALL_HPP

#include "caf/catch_all.hpp"
#include "caf/timeout_definition.hpp"

namespace caf {

template <class T>
struct is_timeout_or_catch_all : std::false_type {};

template <class T>
struct is_timeout_or_catch_all<catch_all<T>> : std::true_type {};

template <class T>
struct is_timeout_or_catch_all<timeout_definition<T>> : std::true_type {};

} // namespace caf

#endif // CAF_IS_TIMEOUT_OR_CATCH_ALL_HPP
