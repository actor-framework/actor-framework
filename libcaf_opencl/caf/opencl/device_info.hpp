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

#ifndef CAF_OPENCL_DEVICE_INFO_HPP
#define CAF_OPENCL_DEVICE_INFO_HPP

#include "caf/opencl/global.hpp"
#include "caf/opencl/program.hpp"
#include "caf/opencl/smart_ptr.hpp"

namespace caf {
namespace opencl {

class device_info {

  friend class program;

public:
  device_info(device_ptr device, command_queue_ptr queue,
              size_t work_group_size, cl_uint dimensons,
              const dim_vec& items_per_dimension)
      : max_work_group_size_(work_group_size),
        max_dimensions_(dimensons),
        max_work_items_per_dim_(items_per_dimension),
        device_(device),
        cmd_queue_(queue) {}

  inline size_t get_max_work_group_size();
  inline cl_uint get_max_dimensions();
  inline dim_vec get_max_work_items_per_dim();

private:
  size_t max_work_group_size_;
  cl_uint max_dimensions_;
  dim_vec max_work_items_per_dim_;
  device_ptr device_;
  command_queue_ptr cmd_queue_;
};

/******************************************************************************\
 *                 implementation of inline member functions                  *
\******************************************************************************/

inline size_t device_info::get_max_work_group_size() {
  return max_work_group_size_;
}

inline cl_uint device_info::get_max_dimensions() { return max_dimensions_; }

inline dim_vec device_info::get_max_work_items_per_dim() {
  return max_work_items_per_dim_;
}

} // namespace opencl
} // namespace caf

#endif // CAF_OPENCL_DEVICE_INFO_HPP
