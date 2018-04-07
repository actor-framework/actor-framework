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

#include "caf/opencl/global.hpp"

namespace caf {
namespace opencl {

class nd_range {
public:
  nd_range(const opencl::dim_vec& dimensions,
           const opencl::dim_vec& offsets = {},
           const opencl::dim_vec& local_dimensions = {})
    : dims_{dimensions},
      offset_{offsets},
      local_dims_{local_dimensions} {
    // nop
  }

  nd_range(opencl::dim_vec&& dimensions,
           opencl::dim_vec&& offsets = {},
           opencl::dim_vec&& local_dimensions = {})
    : dims_{std::move(dimensions)},
      offset_{std::move(offsets)},
      local_dims_{std::move(local_dimensions)} {
    // nop
  }

  nd_range(const nd_range&) = default;
  nd_range(nd_range&&) = default;

  nd_range& operator=(const nd_range&) = default;
  nd_range& operator=(nd_range&&) = default;

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

