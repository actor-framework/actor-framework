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


#ifndef CPPA_OPENCL_COMMAND_HPP
#define CPPA_OPENCL_COMMAND_HPP

#include <vector>
#include <numeric>
#include <algorithm>
#include <functional>

#include "cppa/logging.hpp"
#include "cppa/opencl/global.hpp"
#include "cppa/abstract_actor.hpp"
#include "cppa/response_promise.hpp"
#include "cppa/opencl/smart_ptr.hpp"
#include "cppa/util/scope_guard.hpp"

namespace cppa {
namespace opencl {

template<typename T, typename R>
class command : public ref_counted {

 public:

    command(response_promise handle,
            intrusive_ptr<T> actor_facade,
            std::vector<cl_event> events,
            std::vector<mem_ptr> arguments,
            size_t result_size,
            any_tuple msg)
        : m_result_size(result_size)
        , m_handle(handle)
        , m_actor_facade(actor_facade)
        , m_queue(actor_facade->m_queue)
        , m_events(std::move(events))
        , m_arguments(std::move(arguments))
        , m_result(m_result_size)
        , m_msg(msg) { }

    ~command() {
        cl_int err{0};
        for(auto& e : m_events) {
            err = clReleaseEvent(e);
            if (err != CL_SUCCESS) {
                CPPA_LOGMF(CPPA_ERROR, "clReleaseEvent: "
                                       << get_opencl_error(err));
            }
        }
    }

    void enqueue () {
        CPPA_LOG_TRACE("command::enqueue()");
        this->ref(); // reference held by the OpenCL comand queue
        cl_int err{0};
        cl_event event_k;
        auto data_or_nullptr = [](const dim_vec& vec) {
            return vec.empty() ? nullptr : vec.data();
        };
        err = clEnqueueNDRangeKernel(m_queue.get(),
                                     m_actor_facade->m_kernel.get(),
                                     m_actor_facade->m_global_dimensions.size(),
                                     data_or_nullptr(m_actor_facade->m_global_offsets),
                                     data_or_nullptr(m_actor_facade->m_global_dimensions),
                                     data_or_nullptr(m_actor_facade->m_local_dimensions),
                                     m_events.size(),
                                     (m_events.empty() ? nullptr : m_events.data()),
                                     &event_k);
        if (err != CL_SUCCESS) {
            CPPA_LOGMF(CPPA_ERROR, "clEnqueueNDRangeKernel: "
                                   << get_opencl_error(err));
            this->deref(); // or can anything actually happen?
            return;
        }
        else {
            cl_event event_r;
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
                return;
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
                CPPA_LOGMF(CPPA_ERROR, "clSetEventCallback: "
                                       << get_opencl_error(err));
                this->deref(); // callback is not set
                return;
            }

            err = clFlush(m_queue.get());
            if (err != CL_SUCCESS) {
                CPPA_LOGMF(CPPA_ERROR, "clFlush: " << get_opencl_error(err));
            }
            m_events.push_back(std::move(event_k));
            m_events.push_back(std::move(event_r));
        }
    }

 private:

    int m_result_size;
    response_promise m_handle;
    intrusive_ptr<T> m_actor_facade;
    command_queue_ptr m_queue;
    std::vector<cl_event> m_events;
    std::vector<mem_ptr> m_arguments;
    R m_result;
    any_tuple m_msg; // required to keep the argument buffers alive (for async copy)

    void handle_results () {
        m_handle.deliver(m_actor_facade->m_map_result(m_result));
    }
};

} // namespace opencl
} // namespace cppa

#endif // CPPA_OPENCL_COMMAND_HPP
