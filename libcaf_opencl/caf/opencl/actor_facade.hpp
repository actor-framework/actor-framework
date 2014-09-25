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

#ifndef CAF_OPENCL_ACTOR_FACADE_HPP
#define CAF_OPENCL_ACTOR_FACADE_HPP

#include <ostream>
#include <iostream>
#include <algorithm>
#include <stdexcept>

#include "caf/all.hpp"

#include "caf/channel.hpp"
#include "caf/to_string.hpp"
#include "cppa/tuple_cast.hpp"
#include "caf/intrusive_ptr.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/limited_vector.hpp"

#include "caf/opencl/global.hpp"
#include "caf/opencl/command.hpp"
#include "caf/opencl/program.hpp"
#include "caf/opencl/smart_ptr.hpp"

namespace caf {
namespace opencl {

class opencl_metainfo;

template <typename Signature>
class actor_facade;

template <typename Ret, typename... Args>
class actor_facade<Ret(Args...)> : public abstract_actor {

  friend class command<actor_facade, Ret>;

 public:
  using args_tuple =
    cow_tuple<typename std::decay<Args>::type...>;

  using arg_mapping = std::function<optional<args_tuple>(message)>;
  using result_mapping = std::function<message(Ret&)>;

  static intrusive_ptr<actor_facade>
  create(const program& prog, const char* kernel_name, arg_mapping map_args,
         result_mapping map_result, const dim_vec& global_dims,
         const dim_vec& offsets, const dim_vec& local_dims,
         size_t result_size) {
    if (global_dims.empty()) {
      auto str = "OpenCL kernel needs at least 1 global dimension.";
      CAF_LOGM_ERROR(detail::demangle(typeid(actor_facade)).c_str(), str);
      throw std::runtime_error(str);
    }
    auto check_vec = [&](const dim_vec& vec, const char* name) {
      if (!vec.empty() && vec.size() != global_dims.size()) {
        std::ostringstream oss;
        oss << name << " vector is not empty, but "
            << "its size differs from global dimensions vector's size";
        CAF_LOGM_ERROR(detail::demangle<actor_facade>().c_str(), oss.str());
        throw std::runtime_error(oss.str());
      }
    };
    check_vec(offsets, "offsets");
    check_vec(local_dims, "local dimensions");
    cl_int err{0};
    kernel_ptr kernel;
    kernel.adopt(clCreateKernel(prog.m_program.get(), kernel_name, &err));
    if (err != CL_SUCCESS) {
      std::ostringstream oss;
      oss << "clCreateKernel: " << get_opencl_error(err);
      CAF_LOGM_ERROR(detail::demangle<actor_facade>().c_str(), oss.str());
      throw std::runtime_error(oss.str());
    }
    if (result_size == 0) {
      result_size = std::accumulate(global_dims.begin(), global_dims.end(), 1,
                                    std::multiplies<size_t>{});
    }
    return new actor_facade<Ret(Args...)>{
      prog,       kernel,              global_dims,           offsets,
      local_dims, std::move(map_args), std::move(map_result), result_size};
  }

  void enqueue(const actor_addr &sender, message_id mid, message content,
               execution_unit*) override {
    CAF_LOG_TRACE("");
    typename detail::il_indices<detail::type_list<Args...>>::type indices;
    enqueue_impl(sender, mid, std::move(content), indices);
  }

 private:
  using evnt_vec = std::vector<cl_event>;
  using args_vec = std::vector<mem_ptr>;

  actor_facade(const program& prog, kernel_ptr kernel,
               const dim_vec& global_dimensions, const dim_vec& global_offsets,
               const dim_vec& local_dimensions, arg_mapping map_args,
               result_mapping map_result, size_t result_size)
      : m_kernel(kernel),
        m_program(prog.m_program),
        m_context(prog.m_context),
        m_queue(prog.m_queue),
        m_global_dimensions(global_dimensions),
        m_global_offsets(global_offsets),
        m_local_dimensions(local_dimensions),
        m_map_args(std::move(map_args)),
        m_map_result(std::move(map_result)),
        m_result_size(result_size) {
    CAF_LOG_TRACE("id: " << this->id());
  }

  template <long... Is>
  void enqueue_impl(const actor_addr& sender, message_id mid, message msg,
                    detail::int_list<Is...>) {
    auto opt = m_map_args(std::move(msg));
    if (opt) {
      response_promise handle{this->address(), sender, mid.response_id()};
      evnt_vec events;
      args_vec arguments;
      add_arguments_to_kernel<Ret>(events, arguments, m_result_size,
                                   get_ref<Is>(*opt)...);
      auto cmd = detail::make_counted<command<actor_facade, Ret>>(
        handle, this, std::move(events), std::move(arguments), m_result_size,
        *opt);
      cmd->enqueue();
    } else {
      CAF_LOGMF(CAF_ERROR, "actor_facade::enqueue() tuple_cast failed.");
    }
  }

  kernel_ptr m_kernel;
  program_ptr m_program;
  context_ptr m_context;
  command_queue_ptr m_queue;
  dim_vec m_global_dimensions;
  dim_vec m_global_offsets;
  dim_vec m_local_dimensions;
  arg_mapping m_map_args;
  result_mapping m_map_result;
  size_t m_result_size;

  void add_arguments_to_kernel_rec(evnt_vec&, args_vec& arguments) {
    cl_int err{0};
    // rotate left (output buffer to the end)
    rotate(begin(arguments), begin(arguments) + 1, end(arguments));
    for (size_t i = 0; i < arguments.size(); ++i) {
      err = clSetKernelArg(m_kernel.get(), i, sizeof(cl_mem),
                           static_cast<void*>(&arguments[i]));
      CAF_LOG_ERROR_IF(err != CL_SUCCESS,
                       "clSetKernelArg: " << get_opencl_error(err));
    }
    clFlush(m_queue.get());
  }

  template <typename T0, typename... Ts>
  void add_arguments_to_kernel_rec(evnt_vec& events, args_vec& arguments,
                                   T0& arg0, Ts&... args) {
    cl_int err{0};
    size_t buffer_size = sizeof(typename T0::value_type) * arg0.size();
    auto buffer = clCreateBuffer(m_context.get(), CL_MEM_READ_ONLY, buffer_size,
                                 nullptr, &err);
    if (err != CL_SUCCESS) {
      CAF_LOGMF(CAF_ERROR, "clCreateBuffer: " << get_opencl_error(err));
      return;
    }
    cl_event event;
    err = clEnqueueWriteBuffer(m_queue.get(), buffer, CL_FALSE, 0, buffer_size,
                               arg0.data(), 0, nullptr, &event);
    if (err != CL_SUCCESS) {
      CAF_LOGMF(CAF_ERROR, "clEnqueueWriteBuffer: " << get_opencl_error(err));
      return;
    }
    events.push_back(std::move(event));
    mem_ptr tmp;
    tmp.adopt(std::move(buffer));
    arguments.push_back(tmp);
    add_arguments_to_kernel_rec(events, arguments, args...);
  }

  template <typename R, typename... Ts>
  void add_arguments_to_kernel(evnt_vec& events, args_vec& arguments,
                               size_t ret_size, Ts&&... args) {
    arguments.clear();
    cl_int err{0};
    auto buf =
      clCreateBuffer(m_context.get(), CL_MEM_WRITE_ONLY,
                     sizeof(typename R::value_type) * ret_size, nullptr, &err);
    if (err != CL_SUCCESS) {
      CAF_LOGMF(CAF_ERROR, "clCreateBuffer: " << get_opencl_error(err));
      return;
    }
    mem_ptr tmp;
    tmp.adopt(std::move(buf));
    arguments.push_back(tmp);
    add_arguments_to_kernel_rec(events, arguments, std::forward<Ts>(args)...);
  }
};

} // namespace opencl
} // namespace caf

#endif // CAF_OPENCL_ACTOR_FACADE_HPP
