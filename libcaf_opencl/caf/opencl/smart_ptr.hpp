/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

namespace caf {
namespace opencl {

template <class T, cl_int (*ref)(T), cl_int (*deref)(T)>
class smart_ptr {
 public:
  using element_type = typename std::remove_pointer<T>::type;
  using pointer = element_type*;
  using reference = element_type&;
  using const_pointer = const element_type*;
  using const_reference = const element_type&;

  smart_ptr(pointer ptr = nullptr, bool inc_ref_count = true) : m_ptr(nullptr) {
    reset(ptr, inc_ref_count);
  }

  ~smart_ptr() {
    reset();
  }

  smart_ptr(smart_ptr&& other) : m_ptr(nullptr) {
    swap(other);
  }

  smart_ptr(const smart_ptr& other) : m_ptr(nullptr) {
    reset(other.m_ptr);
  }

  smart_ptr& operator=(pointer ptr) {
    reset(ptr);
    return *this;
  }

  smart_ptr& operator=(smart_ptr&& other) {
    swap(other);
    return *this;
  }

  smart_ptr& operator=(const smart_ptr& other) {
    smart_ptr tmp{other};
    swap(tmp);
    return *this;
  }

  void swap(smart_ptr& other) {
    std::swap(m_ptr, other.m_ptr);
  }

  void reset(pointer ptr = nullptr, bool inc_ref_count = true) {
    if (m_ptr) {
      deref(m_ptr);
    }
    m_ptr = ptr;
    if (ptr && inc_ref_count) {
      ref(ptr);
    }
  }

  // does not modify reference count of ptr
  void adopt(pointer ptr) {
    reset(ptr, false);
  }

  pointer get() const {
    return m_ptr;
  }

  pointer operator->() const {
    return m_ptr;
  }

  reference operator*() const {
    return *m_ptr;
  }

  bool operator!() const {
    return m_ptr == nullptr;
  }

  explicit operator bool() const {
    return m_ptr != nullptr;
  }

 private:
  pointer m_ptr;
};

using mem_ptr = smart_ptr<cl_mem, clRetainMemObject, clReleaseMemObject>;
using event_ptr = smart_ptr<cl_event, clRetainEvent, clReleaseEvent>;
using kernel_ptr = smart_ptr<cl_kernel, clRetainKernel, clReleaseKernel>;
using context_ptr = smart_ptr<cl_context, clRetainContext, clReleaseContext>;
using program_ptr = smart_ptr<cl_program, clRetainProgram, clReleaseProgram>;
using device_ptr =
  smart_ptr<cl_device_id, clRetainDeviceDummy, clReleaseDeviceDummy>;
using command_queue_ptr =
  smart_ptr<cl_command_queue, clRetainCommandQueue, clReleaseCommandQueue>;

} // namespace opencl
} // namespace caf

#endif // CAF_OPENCL_SMART_PTR_HPP
