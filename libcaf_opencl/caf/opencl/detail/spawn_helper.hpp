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

#ifndef CAF_OPENCL_DETAIL_SPAWN_HELPER_HPP
#define CAF_OPENCL_DETAIL_SPAWN_HELPER_HPP

#include "caf/opencl/actor_facade.hpp"

namespace caf {
namespace opencl {
namespace detail {

struct tuple_construct { };

template <class... Ts>
struct cl_spawn_helper {
  using impl = opencl::actor_facade<Ts...>;
  using map_in_fun = std::function<optional<message> (message&)>;
  using map_out_fun = typename impl::output_mapping;

  actor operator()(actor_config actor_cfg, const opencl::program& p,
                   const char* fn, const opencl::spawn_config& spawn_cfg,
                   Ts&&... xs) const {
    return actor_cast<actor>(impl::create(std::move(actor_cfg),
                                          p, fn, spawn_cfg,
                                          map_in_fun{}, map_out_fun{},
                                          std::move(xs)...));
  }
  actor operator()(actor_config actor_cfg, const opencl::program& p,
                   const char* fn, const opencl::spawn_config& spawn_cfg,
                   map_in_fun map_input, map_out_fun map_output,
                   Ts&&... xs) const {
    return actor_cast<actor>(impl::create(std::move(actor_cfg),
                                          p, fn, spawn_cfg,
                                          std::move(map_input),
                                          std::move(map_output),
                                          std::move(xs)...));
  }
};

} // namespace detail
} // namespace opencl
} // namespace caf

 #endif // CAF_OPENCL_DETAIL_SPAWN_HELPER_HPP
