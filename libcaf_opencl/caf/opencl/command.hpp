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
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_OPENCL_COMMAND_HPP
#define CAF_OPENCL_COMMAND_HPP

#include <vector>
#include <numeric>
#include <algorithm>
#include <functional>

#include "caf/detail/logging.hpp"
#include "caf/opencl/global.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/response_promise.hpp"
#include "caf/opencl/smart_ptr.hpp"
#include "caf/detail/scope_guard.hpp"

namespace caf {
namespace opencl {

template <typename T, typename R>
class command : public ref_counted {

 public:
  command(response_promise handle, intrusive_ptr<T> actor_facade,
          std::vector<cl_event> events, std::vector<mem_ptr> arguments,
          size_t result_size, message msg)
      : m_result_size(result_size),
        m_handle(handle),
        m_actor_facade(actor_facade),
        m_queue(actor_facade->m_queue),
        m_events(std::move(events)),
        m_arguments(std::move(arguments)),
        m_result(m_result_size),
        m_msg(msg) {}

  ~command() {
    cl_int err{0};
    for (auto& e : m_events) {
      err = clReleaseEvent(e);
      if (err != CL_SUCCESS) {
        CAF_LOGMF(CAF_ERROR, "clReleaseEvent: " << get_opencl_error(err));
      }
    }
  }

  void enqueue() {
    CAF_LOG_TRACE("command::enqueue()");
    this->ref(); // reference held by the OpenCL comand queue
    cl_int err{0};
    cl_event event_k;
    auto data_or_nullptr = [](const dim_vec& vec) {
      return vec.empty() ? nullptr : vec.data();
    };
    err = clEnqueueNDRangeKernel(
      m_queue.get(), m_actor_facade->m_kernel.get(),
      m_actor_facade->m_global_dimensions.size(),
      data_or_nullptr(m_actor_facade->m_global_offsets),
      data_or_nullptr(m_actor_facade->m_global_dimensions),
      data_or_nullptr(m_actor_facade->m_local_dimensions), m_events.size(),
      (m_events.empty() ? nullptr : m_events.data()), &event_k);
    if (err != CL_SUCCESS) {
      CAF_LOGMF(CAF_ERROR, "clEnqueueNDRangeKernel: " << get_opencl_error(err));
      this->deref(); // or can anything actually happen?
      return;
    } else {
      cl_event event_r;
      err =
        clEnqueueReadBuffer(m_queue.get(), m_arguments.back().get(), CL_FALSE,
                            0, sizeof(typename R::value_type) * m_result_size,
                            m_result.data(), 1, &event_k, &event_r);
      if (err != CL_SUCCESS) {
        throw std::runtime_error("clEnqueueReadBuffer: " +
                                 get_opencl_error(err));
        this->deref(); // failed to enqueue command
        return;
      }
      err = clSetEventCallback(event_r, CL_COMPLETE,
                               [](cl_event, cl_int, void* data) {
                                 auto cmd = reinterpret_cast<command*>(data);
                                 cmd->handle_results();
                                 cmd->deref();
                               },
                               this);
      if (err != CL_SUCCESS) {
        CAF_LOGMF(CAF_ERROR, "clSetEventCallback: " << get_opencl_error(err));
        this->deref(); // callback is not set
        return;
      }

      err = clFlush(m_queue.get());
      if (err != CL_SUCCESS) {
        CAF_LOGMF(CAF_ERROR, "clFlush: " << get_opencl_error(err));
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
  message m_msg; // required to keep the argument buffers alive (async copy)

  void handle_results() {
    m_handle.deliver(m_actor_facade->m_map_result(m_result));
  }
};

} // namespace opencl
} // namespace caf

#endif // CAF_OPENCL_COMMAND_HPP
