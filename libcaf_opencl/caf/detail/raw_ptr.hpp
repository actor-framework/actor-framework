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

#include <memory>
#include <algorithm>
#include <type_traits>

#include "caf/intrusive_ptr.hpp"

#include "caf/opencl/global.hpp"

#define CAF_OPENCL_PTR_ALIAS(aliasname, cltype, claddref, clrelease)           \
  inline void intrusive_ptr_add_ref(cltype ptr) { claddref(ptr); }             \
  inline void intrusive_ptr_release(cltype ptr) { clrelease(ptr); }            \
  namespace caf {                                                              \
  namespace detail {                                                           \
  using aliasname = intrusive_ptr<std::remove_pointer<cltype>::type>;          \
  } /* namespace detail */                                                     \
  } // namespace caf


CAF_OPENCL_PTR_ALIAS(raw_mem_ptr, cl_mem, clRetainMemObject, clReleaseMemObject)

CAF_OPENCL_PTR_ALIAS(raw_event_ptr, cl_event, clRetainEvent, clReleaseEvent)

CAF_OPENCL_PTR_ALIAS(raw_kernel_ptr, cl_kernel, clRetainKernel, clReleaseKernel)

CAF_OPENCL_PTR_ALIAS(raw_context_ptr, cl_context,
                     clRetainContext, clReleaseContext)

CAF_OPENCL_PTR_ALIAS(raw_program_ptr, cl_program,
                     clRetainProgram, clReleaseProgram)

CAF_OPENCL_PTR_ALIAS(raw_device_ptr, cl_device_id,
                     clRetainDeviceDummy, clReleaseDeviceDummy)

CAF_OPENCL_PTR_ALIAS(raw_command_queue_ptr, cl_command_queue,
                     clRetainCommandQueue, clReleaseCommandQueue)

