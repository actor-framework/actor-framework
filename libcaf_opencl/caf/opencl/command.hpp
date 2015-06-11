/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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
#include "caf/opencl/opencl_err.hpp"
#include "caf/detail/scope_guard.hpp"

namespace caf {
namespace opencl {

template <typename T, typename R>
class command : public ref_counted {
public:
  command(response_promise handle, intrusive_ptr<T> actor_facade,
          std::vector<cl_event> events, std::vector<mem_ptr> arguments,
          size_t result_size, message msg)
      : result_size_(result_size),
        handle_(handle),
        actor_facade_(actor_facade),
        queue_(actor_facade->queue_),
        events_(std::move(events)),
        arguments_(std::move(arguments)),
        result_(result_size),
        msg_(msg) {
    // nop
  }

  ~command() {
    for (auto& e : events_) {
      v1callcl(CAF_CLF(clReleaseEvent),e);
    }
  }

  void enqueue() {
    // Errors in this function can not be handled by opencl_err.hpp
    // because they require non-standard error handling
    CAF_LOG_TRACE("command::enqueue()");
    this->ref(); // reference held by the OpenCL comand queue
    cl_event event_k;
    auto data_or_nullptr = [](const dim_vec& vec) {
      return vec.empty() ? nullptr : vec.data();
    };
    // OpenCL expects cl_uint (unsigned int), hence the cast
    cl_int err = clEnqueueNDRangeKernel(
      queue_.get(), actor_facade_->kernel_.get(),
      static_cast<cl_uint>(actor_facade_->global_dimensions_.size()),
      data_or_nullptr(actor_facade_->global_offsets_),
      data_or_nullptr(actor_facade_->global_dimensions_),
      data_or_nullptr(actor_facade_->local_dimensions_),
      static_cast<cl_uint>(events_.size()),
      (events_.empty() ? nullptr : events_.data()), &event_k);
    if (err != CL_SUCCESS) {
      CAF_LOGMF(CAF_ERROR, "clEnqueueNDRangeKernel: " << get_opencl_error(err));
      this->deref(); // or can anything actually happen?
      return;
    } else {
      cl_event event_r;
      err =
        clEnqueueReadBuffer(queue_.get(), arguments_.back().get(), CL_FALSE,
                            0, sizeof(typename R::value_type) * result_size_,
                            result_.data(), 1, &event_k, &event_r);
      if (err != CL_SUCCESS) {
        this->deref(); // failed to enqueue command
        throw std::runtime_error("clEnqueueReadBuffer: " +
                                 get_opencl_error(err));
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

      err = clFlush(queue_.get());
      if (err != CL_SUCCESS) {
        CAF_LOGMF(CAF_ERROR, "clFlush: " << get_opencl_error(err));
      }
      events_.push_back(std::move(event_k));
      events_.push_back(std::move(event_r));
    }
  }

private:
  size_t result_size_;
  response_promise handle_;
  intrusive_ptr<T> actor_facade_;
  command_queue_ptr queue_;
  std::vector<cl_event> events_;
  std::vector<mem_ptr> arguments_;
  R result_;
  message msg_; // required to keep the argument buffers alive (async copy)

  void handle_results() {
    auto& map_fun = actor_facade_->map_result_;
    auto msg = map_fun ? map_fun(result_) : make_message(std::move(result_));
    handle_.deliver(std::move(msg));
  }
};

} // namespace opencl
} // namespace caf

#endif // CAF_OPENCL_COMMAND_HPP
