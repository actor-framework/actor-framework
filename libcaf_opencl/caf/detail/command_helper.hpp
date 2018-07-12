/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#include "caf/detail/type_list.hpp"

#include "caf/opencl/command.hpp"

namespace caf {
namespace detail {

// signature for the function that is applied to output arguments
template <class List>
struct output_function_sig;

template <class... Ts>
struct output_function_sig<detail::type_list<Ts...>> {
  using type = std::function<message (Ts&...)>;
};

// derive signature of the command that handles the kernel execution
template <class T, class List>
struct command_sig;

template <class T, class... Ts>
struct command_sig<T, detail::type_list<Ts...>> {
  using type = opencl::command<T, Ts...>;
};

// derive type for a tuple matching the arguments as mem_refs
template <class List>
struct tuple_type_of;

template <class... Ts>
struct tuple_type_of<detail::type_list<Ts...>> {
  using type = std::tuple<Ts...>;
};

} // namespace detail
} // namespace caf

