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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 * Raphael Hiesgen <raphael.hiesgen@haw-hamburg.de>                           *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
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


#ifndef CPPA_OPENCL_COMMAND_HPP
#define CPPA_OPENCL_COMMAND_HPP

#include <vector>
#include <functional>

#include "cppa/actor.hpp"
#include "cppa/logging.hpp"
#include "cppa/opencl/global.hpp"
#include "cppa/response_handle.hpp"
#include "cppa/opencl/smart_ptr.hpp"
#include "cppa/util/scope_guard.hpp"

namespace cppa { namespace opencl {

class command : public ref_counted {

 public:

    command* next;

    virtual void enqueue(command_queue_ptr queue) = 0;

};

class command_dummy : public command {

 public:

    void enqueue(command_queue_ptr) { }
};

template<typename T>
class command_impl : public command {

 public:

    command_impl(response_handle handle,
                 kernel_ptr kernel,
                 std::vector<mem_ptr> arguments,
                 std::vector<size_t> global_dimensions,
                 std::vector<size_t> local_dimensions,
                 std::function<any_tuple(T&)> map_result)
        : m_number_of_values(1)
        , m_handle(handle)
        , m_kernel(kernel)
        , m_arguments(arguments)
        , m_global_dimensions(global_dimensions)
        , m_local_dimensions(local_dimensions)
        , m_map_result(map_result)
    {
        m_kernel_event.adopt(cl_event());
        for (size_t s : m_global_dimensions) {
            m_number_of_values *= s;
        }
    }

    void enqueue (command_queue_ptr queue) {
        CPPA_LOG_TRACE("command::enqueue()");
        this->ref();
        cl_int err{0};
        m_queue = queue;

        auto ptr = m_kernel_event.get();

        /* enqueue kernel */
        err = clEnqueueNDRangeKernel(m_queue.get(),
                                     m_kernel.get(),
                                     3,
                                     NULL,
                                     m_global_dimensions.data(),
                                     m_local_dimensions.data(),
                                     0,
                                     nullptr,
                                     &ptr);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("[!!!] clEnqueueNDRangeKernel: '"
                                     + get_opencl_error(err)
                                     + "'.");
        }
        err = clSetEventCallback(ptr,
                                 CL_COMPLETE,
                                 [](cl_event, cl_int, void* data) {
                                     CPPA_LOGC_TRACE("command_impl",
                                                    "enqueue",
                                                    "command::enqueue()::callback()");
                                     auto cmd = reinterpret_cast<command_impl*>(data);
                                     cmd->handle_results();
                                     cmd->deref();
                                 },
                                 this);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("[!!!] clSetEventCallback: '"
                                     + get_opencl_error(err)
                                     + "'.");
        }
    }

 private:

    int  m_number_of_values;
    response_handle m_handle;
    kernel_ptr      m_kernel;
    event_ptr       m_kernel_event;
    command_queue_ptr m_queue;
    std::vector<mem_ptr> m_arguments;
    std::vector<size_t>  m_global_dimensions;
    std::vector<size_t>  m_local_dimensions;
    std::function<any_tuple (T&)> m_map_result;

    void handle_results () {
        CPPA_LOG_TRACE("command::handle_results()");
        /* get results from gpu */
        cl_int err{0};
        cl_event read_event;
        T results(m_number_of_values);
        err = clEnqueueReadBuffer(m_queue.get(),
                                  m_arguments[0].get(),
                                  CL_TRUE,
                                  0,
                                  sizeof(typename T::value_type) * m_number_of_values,
                                  results.data(),
                                  0,
                                  NULL,
                                  &read_event);
        clReleaseEvent(read_event);
        if (err != CL_SUCCESS) {
           throw std::runtime_error("[!!!] clEnqueueReadBuffer: '"
                                    + get_opencl_error(err)
                                    + "'.");
        }
        auto mapped_result = m_map_result(results);
        reply_tuple_to(m_handle, mapped_result);
        //reply_to(m_handle, results);
    }
};

typedef intrusive_ptr<command> command_ptr;

} } // namespace cppa::opencl

#endif // CPPA_OPENCL_COMMAND_HPP
