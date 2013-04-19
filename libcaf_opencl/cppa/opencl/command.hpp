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


#ifndef CPPA_OPENCL_COMMAND_HPP
#define CPPA_OPENCL_COMMAND_HPP

#include <vector>
#include <numeric>
#include <algorithm>
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

    void enqueue(command_queue_ptr) override { }
};

template<typename T>
class command_impl : public command {

 public:

    command_impl(response_handle handle,
                 kernel_ptr kernel,
                 std::vector<mem_ptr> arguments,
                 const dim_vec& global_dims,
                 const dim_vec& offsets,
                 const dim_vec& local_dims,
                 const std::function<any_tuple(T&)>& map_result)
        : m_number_of_values(std::accumulate(global_dims.begin(),
                                             global_dims.end(),
                                             1, std::multiplies<size_t>{}))
        , m_handle(handle)
        , m_kernel(kernel)
        , m_arguments(arguments)
        , m_global_dims(global_dims)
        , m_offsets(offsets)
        , m_local_dims(local_dims)
        , m_map_result(map_result)
    {
    }

    void enqueue (command_queue_ptr queue) override {
        CPPA_LOG_TRACE("command::enqueue()");
        this->ref();
        cl_int err{0};
        m_queue = queue;
        auto ptr = m_kernel_event.get();
        auto data_or_nullptr = [](const dim_vec& vec) {
            return vec.empty() ? nullptr : vec.data();
        };

        /* enqueue kernel */
        err = clEnqueueNDRangeKernel(m_queue.get(),
                                     m_kernel.get(),
                                     m_global_dims.size(),
                                     data_or_nullptr(m_offsets),
                                     data_or_nullptr(m_global_dims),
                                     data_or_nullptr(m_local_dims),
                                     0,
                                     nullptr,
                                     &ptr);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("clEnqueueNDRangeKernel: "
                                     + get_opencl_error(err));
        }
        err = clSetEventCallback(ptr,
                                 CL_COMPLETE,
                                 [](cl_event, cl_int, void* data) {
                                     auto cmd = reinterpret_cast<command_impl*>(data);
                                     cmd->handle_results();
                                     cmd->deref();
                                 },
                                 this);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("clSetEventCallback: "
                                     + get_opencl_error(err));
        }
    }

 private:

    int m_number_of_values;
    response_handle m_handle;
    kernel_ptr      m_kernel;
    event_ptr       m_kernel_event;
    command_queue_ptr m_queue;
    std::vector<mem_ptr> m_arguments;
    dim_vec  m_global_dims;
    dim_vec  m_offsets;
    dim_vec  m_local_dims;
    std::function<any_tuple (T&)> m_map_result;

    void handle_results () {
        cl_int err{0};
        cl_event read_event;
        T result(m_number_of_values);
        err = clEnqueueReadBuffer(m_queue.get(),
                                  m_arguments[0].get(),
                                  CL_TRUE,
                                  0,
                                  sizeof(typename T::value_type) * m_number_of_values,
                                  result.data(),
                                  0,
                                  nullptr,
                                  &read_event);
        clReleaseEvent(read_event);
        if (err != CL_SUCCESS) {
           throw std::runtime_error("clEnqueueReadBuffer: "
                                    + get_opencl_error(err));
        }
        auto mapped_result = m_map_result(result);
        reply_tuple_to(m_handle, mapped_result);
    }
};

typedef intrusive_ptr<command> command_ptr;

} } // namespace cppa::opencl

#endif // CPPA_OPENCL_COMMAND_HPP
