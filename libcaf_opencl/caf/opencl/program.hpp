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

#include <map>
#include <memory>

#include "caf/ref_counted.hpp"

#include "caf/detail/raw_ptr.hpp"

#include "caf/opencl/device.hpp"
#include "caf/opencl/global.hpp"

namespace caf {
namespace opencl {

class program;
using program_ptr = intrusive_ptr<program>;

/// @brief A wrapper for OpenCL's cl_program.
class program : public ref_counted {
public:
  friend class manager;
  template <bool PassConfig, class... Ts>
  friend class actor_facade;
  template <class T, class... Ts>
  friend intrusive_ptr<T> caf::make_counted(Ts&&...);

private:
  program(detail::raw_context_ptr context, detail::raw_command_queue_ptr queue,
          detail::raw_program_ptr prog,
          std::map<std::string, detail::raw_kernel_ptr> available_kernels);

  ~program();

  detail::raw_context_ptr context_;
  detail::raw_program_ptr program_;
  detail::raw_command_queue_ptr queue_;
  std::map<std::string, detail::raw_kernel_ptr> available_kernels_;
};

} // namespace opencl
} // namespace caf

