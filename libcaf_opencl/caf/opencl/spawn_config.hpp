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

#ifndef CAF_OPENCL_SPAWN_CONFIG_HPP
#define CAF_OPENCL_SPAWN_CONFIG_HPP

#include "caf/opencl/global.hpp"

namespace caf {
namespace opencl {

class spawn_config {
public:
  spawn_config(const opencl::dim_vec& dims,
               const opencl::dim_vec& offset = {},
               const opencl::dim_vec& local_dims = {})
    : dims_{dims},
      offset_{offset},
      local_dims_{local_dims} {
    // nop
  }

  spawn_config(opencl::dim_vec&& dims,
               opencl::dim_vec&& offset = {},
               opencl::dim_vec&& local_dims = {})
    : dims_{std::move(dims)},
      offset_{std::move(offset)},
      local_dims_{std::move(local_dims)} {
    // nop
  }

  spawn_config(const spawn_config&) = default;
  spawn_config(spawn_config&&) = default;

  spawn_config& operator=(const spawn_config&) = default;
  spawn_config& operator=(spawn_config&&) = default;

  const opencl::dim_vec& dimensions() const {
    return dims_;
  }

  const opencl::dim_vec& offsets() const {
    return offset_;
  }

  const opencl::dim_vec& local_dimensions() const {
    return local_dims_;
  }

private:
  opencl::dim_vec dims_;
  opencl::dim_vec offset_;
  opencl::dim_vec local_dims_;
};

} // namespace opencl
} // namespace caf

#endif // CAF_OPENCL_SPAWN_CONFIG_HPP
