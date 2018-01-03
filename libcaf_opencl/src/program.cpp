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

#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>

#include "caf/opencl/manager.hpp"
#include "caf/opencl/program.hpp"
#include "caf/opencl/opencl_err.hpp"

using namespace std;

namespace caf {
namespace opencl {

program::program(detail::raw_context_ptr context,
                 detail::raw_command_queue_ptr queue,
                 detail::raw_program_ptr prog,
                 map<string, detail::raw_kernel_ptr> available_kernels)
    : context_(move(context)),
      program_(move(prog)),
      queue_(move(queue)),
      available_kernels_(move(available_kernels)) {
  // nop
}

program::~program() {
  // nop
}

} // namespace opencl
} // namespace caf
