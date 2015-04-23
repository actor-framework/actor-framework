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

#ifndef CAF_DETAIL_RUN_PROGRAM_HPP
#define CAF_DETAIL_RUN_PROGRAM_HPP

#include "caf/send.hpp"
#include "caf/actor.hpp"
#include "caf/message.hpp"
#include "caf/string_algorithms.hpp"

#include <thread>
#include <vector>
#include <string>

namespace caf {

namespace detail {

std::thread run_program_impl(caf::actor, const char*, std::vector<std::string>);

template <class... Ts>
std::thread run_program(caf::actor listener, const char* path, Ts&&... args) {
  std::vector<std::string> vec{convert_to_str(std::forward<Ts>(args))...};
  return run_program_impl(listener, path, std::move(vec));
}

} // namespace detail

} // namespace caf

#endif // CAF_DETAIL_RUN_PROGRAM_HPP
