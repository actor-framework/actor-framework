/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011-2014                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 * Raphael Hiesgen <raphael.hiesgen@haw-hamburg.de>                           *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef CPPA_OPENCL_SMART_PTR_HPP
#define CPPA_OPENCL_SMART_PTR_HPP

#include <memory>
#include <algorithm>
#include <type_traits>

namespace cppa {
namespace opencl {

template<typename T, cl_int (*ref)(T), cl_int (*deref)(T)>
class smart_ptr {

    typedef typename std::remove_pointer<T>::type element_type;

    typedef element_type*       pointer;
    typedef element_type&       reference;
    typedef const element_type* const_pointer;
    typedef const element_type& const_reference;


 public:

    smart_ptr(pointer ptr = nullptr) : m_ptr(ptr) {
        if (m_ptr) ref(m_ptr);
    }

    ~smart_ptr() { reset(); }

    smart_ptr(const smart_ptr& other) : m_ptr(other.m_ptr) {
        if (m_ptr) ref(m_ptr);
    }

    smart_ptr(smart_ptr&& other) : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    smart_ptr& operator=(pointer ptr) {
        reset(ptr);
        return *this;
    }

    smart_ptr& operator=(smart_ptr&& other) {
        std::swap(m_ptr, other.m_ptr);
        return *this;
    }

    smart_ptr& operator=(const smart_ptr& other) {
        smart_ptr tmp{other};
        std::swap(m_ptr, tmp.m_ptr);
        return *this;
    }

    inline void reset(pointer ptr = nullptr) {
        if (m_ptr) deref(m_ptr);
        m_ptr = ptr;
        if (ptr) ref(ptr);
    }

    // does not modify reference count of ptr
    inline void adopt(pointer ptr) {
        reset();
        m_ptr = ptr;
    }

    inline pointer get() const { return m_ptr; }

    inline pointer operator->() const { return m_ptr; }

    inline reference operator*() const { return *m_ptr; }

    inline bool operator!() const { return m_ptr == nullptr; }

    inline explicit operator bool() const { return m_ptr != nullptr; }

 private:

    pointer m_ptr;

};

typedef smart_ptr<cl_mem, clRetainMemObject, clReleaseMemObject> mem_ptr;
typedef smart_ptr<cl_event, clRetainEvent, clReleaseEvent>       event_ptr;
typedef smart_ptr<cl_kernel, clRetainKernel, clReleaseKernel>    kernel_ptr;
typedef smart_ptr<cl_context, clRetainContext, clReleaseContext> context_ptr;
typedef smart_ptr<cl_program, clRetainProgram, clReleaseProgram> program_ptr;
typedef smart_ptr<cl_device_id, clRetainDeviceDummy, clReleaseDeviceDummy>
        device_ptr;
typedef smart_ptr<cl_command_queue, clRetainCommandQueue, clReleaseCommandQueue>
        command_queue_ptr;

} // namespace opencl
} // namespace cppa

#endif // CPPA_OPENCL_SMART_PTR_HPP
