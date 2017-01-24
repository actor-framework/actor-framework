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

#ifndef CAF_OPENCL_SMART_PTR_HPP
#define CAF_OPENCL_SMART_PTR_HPP

#include <memory>
#include <algorithm>
#include <type_traits>

#include "caf/intrusive_ptr.hpp"

#include "caf/opencl/global.hpp"

#define CAF_OPENCL_PTR_ALIAS(aliasname, cltype, claddref, clrelease)           \
  inline void intrusive_ptr_add_ref(cltype ptr) { claddref(ptr); }             \
  inline void intrusive_ptr_release(cltype ptr) { clrelease(ptr); }            \
  namespace caf {                                                              \
  namespace opencl {                                                           \
  using aliasname = intrusive_ptr<std::remove_pointer<cltype>::type>;          \
  } /* namespace opencl */                                                     \
  } // namespace caf

CAF_OPENCL_PTR_ALIAS(cl_mem_ptr, cl_mem, clRetainMemObject, clReleaseMemObject)

CAF_OPENCL_PTR_ALIAS(cl_event_ptr, cl_event, clRetainEvent, clReleaseEvent)

CAF_OPENCL_PTR_ALIAS(cl_kernel_ptr, cl_kernel, clRetainKernel, clReleaseKernel)

CAF_OPENCL_PTR_ALIAS(cl_context_ptr, cl_context, clRetainContext, clReleaseContext)

CAF_OPENCL_PTR_ALIAS(cl_program_ptr, cl_program, clRetainProgram, clReleaseProgram)

CAF_OPENCL_PTR_ALIAS(cl_device_ptr, cl_device_id,
                     clRetainDeviceDummy, clReleaseDeviceDummy)

CAF_OPENCL_PTR_ALIAS(cl_command_queue_ptr, cl_command_queue,
                     clRetainCommandQueue, clReleaseCommandQueue)

#endif // CAF_OPENCL_SMART_PTR_HPP
