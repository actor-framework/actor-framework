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

template<typename T, typename R>
class command : public ref_counted {

 public:

    command(response_handle handle,
            intrusive_ptr<T> actor_facade,
            std::vector<mem_ptr> arguments,
            size_t result_size)
        : m_result_size(result_size)
        , m_handle(handle)
        , m_actor_facade(actor_facade)
        , m_queue(actor_facade->m_queue)
        , m_arguments(move(arguments))
        , m_result(m_result_size) { }

    void enqueue () {
        CPPA_LOG_TRACE("command::enqueue()");
        this->ref(); // reference held by the OpenCL comand queue
        cl_int err{0};
        auto event_k = m_kernel_event.get();
        auto data_or_nullptr = [](const dim_vec& vec) {
            return vec.empty() ? nullptr : vec.data();
        };

        err = clEnqueueNDRangeKernel(m_queue.get(),
                                     m_actor_facade->m_kernel.get(),
                                     m_actor_facade->m_global_dimensions.size(),
                                     data_or_nullptr(m_actor_facade->m_global_offsets),
                                     data_or_nullptr(m_actor_facade->m_global_dimensions),
                                     data_or_nullptr(m_actor_facade->m_local_dimensions),
                                     0,
                                     nullptr,
                                     &event_k);
        if (err != CL_SUCCESS) {
            CPPA_LOGMF(CPPA_ERROR, self, "clEnqueueNDRangeKernel: "
                                         << get_opencl_error(err));
            this->deref(); // or can anything actually happen?
        }
        else {
            auto event_r = m_read_event.get();
            err = clEnqueueReadBuffer(m_queue.get(),
                                      m_arguments.back().get(),
                                      CL_FALSE,
                                      0,
                                      sizeof(typename R::value_type) * m_result_size,
                                      m_result.data(),
                                      1,
                                      &event_k,
                                      &event_r);
            if (err != CL_SUCCESS) {
                throw std::runtime_error("clEnqueueReadBuffer: "
                                         + get_opencl_error(err));
                this->deref(); // failed to enqueue command
            }
            err = clSetEventCallback(event_r,
                                     CL_COMPLETE,
                                     [](cl_event, cl_int, void* data) {
                                         auto cmd = reinterpret_cast<command*>(data);
                                         cmd->handle_results();
                                         cmd->deref();
                                     },
                                     this);
            if (err != CL_SUCCESS) {
                CPPA_LOGMF(CPPA_ERROR, self, "clSetEventCallback: "
                                             << get_opencl_error(err));
                this->deref(); // callback is not set
            }

            err = clFlush(m_queue.get());
            if (err != CL_SUCCESS) {
                CPPA_LOGMF(CPPA_ERROR, self, "clFlush: " << get_opencl_error(err));
            }
        }
    }

 private:

    int m_result_size;
    response_handle m_handle;
    intrusive_ptr<T> m_actor_facade;
    event_ptr m_kernel_event;
    event_ptr m_read_event;
    command_queue_ptr m_queue;
    std::vector<mem_ptr> m_arguments;
    R m_result;

    void handle_results () {
        reply_tuple_to(m_handle, m_actor_facade->m_map_result(m_result));
    }
};

} } // namespace cppa::opencl

#endif // CPPA_OPENCL_COMMAND_HPP
