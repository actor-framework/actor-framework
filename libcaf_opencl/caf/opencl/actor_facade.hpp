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

#ifndef CAF_OPENCL_ACTOR_FACADE_HPP
#define CAF_OPENCL_ACTOR_FACADE_HPP

#include <ostream>
#include <iostream>
#include <algorithm>
#include <stdexcept>

#include "caf/all.hpp"

#include "caf/channel.hpp"
#include "caf/to_string.hpp"
#include "caf/intrusive_ptr.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/limited_vector.hpp"

#include "caf/opencl/global.hpp"
#include "caf/opencl/command.hpp"
#include "caf/opencl/program.hpp"
#include "caf/opencl/smart_ptr.hpp"
#include "caf/opencl/opencl_err.hpp"

namespace caf {
namespace opencl {

class opencl_metainfo;

template <class Signature>
class actor_facade;

template <class Ret, typename... Args>
class actor_facade<Ret(Args...)> : public abstract_actor {

  friend class command<actor_facade, Ret>;

 public:
  using arg_types = detail::type_list<typename std::decay<Args>::type...>;
  using arg_mapping = std::function<optional<message> (message&)>;
  using result_mapping = std::function<message(Ret&)>;
  using evnt_vec = std::vector<cl_event>;
  using args_vec = std::vector<mem_ptr>;
  using command_type = command<actor_facade, Ret>;

  static intrusive_ptr<actor_facade>
  create(const program& prog, const char* kernel_name,
         const dim_vec& global_dims, const dim_vec& offsets,
         const dim_vec& local_dims, size_t result_size,
         arg_mapping map_args = arg_mapping{},
         result_mapping map_result = result_mapping{}) {
    if (global_dims.empty()) {
      auto str = "OpenCL kernel needs at least 1 global dimension.";
      CAF_LOGF_ERROR(str);
      throw std::runtime_error(str);
    }
    auto check_vec = [&](const dim_vec& vec, const char* name) {
      if (!vec.empty() && vec.size() != global_dims.size()) {
        std::ostringstream oss;
        oss << name << " vector is not empty, but "
            << "its size differs from global dimensions vector's size";
        CAF_LOGF_ERROR(oss.str());
        throw std::runtime_error(oss.str());
      }
    };
    check_vec(offsets, "offsets");
    check_vec(local_dims, "local dimensions");
    kernel_ptr kernel;
    kernel.reset(v2get(CAF_CLF(clCreateKernel), prog.m_program.get(),
                       kernel_name),
                 false);
    if (result_size == 0) {
      result_size = std::accumulate(global_dims.begin(), global_dims.end(),
                                    size_t{1}, std::multiplies<size_t>{});
    }
    return new actor_facade(prog, kernel, global_dims, offsets, local_dims,
                            result_size, std::move(map_args),
                            std::move(map_result));
  }

  void enqueue(const actor_addr &sender, message_id mid, message content,
               execution_unit*) override {
    CAF_LOG_TRACE("");
    if (m_map_args) {
      auto mapped = m_map_args(content);
      if (!mapped) {
        return;
      }
      content = std::move(*mapped);
    }
    typename detail::il_indices<arg_types>::type indices;
    if (!content.match_elements(arg_types{})) {
      return;
    }
    response_promise handle{this->address(), sender, mid.response_id()};
    evnt_vec events;
    args_vec arguments;
    add_arguments_to_kernel<Ret>(events, arguments, m_result_size,
                                 content, indices);
    auto cmd = make_counted<command_type>(handle, this,
                                          std::move(events),
                                          std::move(arguments),
                                          m_result_size,
                                          std::move(content));
    cmd->enqueue();
  }

 private:
  actor_facade(const program& prog, kernel_ptr kernel,
               const dim_vec& global_dimensions, const dim_vec& global_offsets,
               const dim_vec& local_dimensions, size_t result_size,
               arg_mapping map_args, result_mapping map_result)
      : m_kernel(kernel),
        m_program(prog.m_program),
        m_context(prog.m_context),
        m_queue(prog.m_queue),
        m_global_dimensions(global_dimensions),
        m_global_offsets(global_offsets),
        m_local_dimensions(local_dimensions),
        m_result_size(result_size),
        m_map_args(std::move(map_args)),
        m_map_result(std::move(map_result)) {
    CAF_LOG_TRACE("id: " << this->id());
  }

  void add_arguments_to_kernel_rec(evnt_vec&, args_vec& arguments, message&,
                                   detail::int_list<>) {
    // rotate left (output buffer to the end)
    std::rotate(arguments.begin(), arguments.begin() + 1, arguments.end());
    for (cl_uint i = 0; i < arguments.size(); ++i) {
      v1callcl(CAF_CLF(clSetKernelArg), m_kernel.get(), i,
               sizeof(cl_mem), static_cast<void*>(&arguments[i]));
    }
    clFlush(m_queue.get());
  }

  template <long I, long... Is>
  void add_arguments_to_kernel_rec(evnt_vec& events, args_vec& arguments,
                                   message& msg, detail::int_list<I, Is...>) {
    using value_type = typename detail::tl_at<arg_types, I>::type;
    auto& arg = msg.get_as<value_type>(I);
    size_t buffer_size = sizeof(value_type) * arg.size();
    auto buffer = v2get(CAF_CLF(clCreateBuffer), m_context.get(),
                        cl_mem_flags{CL_MEM_READ_ONLY}, buffer_size, nullptr);
    cl_event event = v1get<cl_event>(CAF_CLF(clEnqueueWriteBuffer),
                                     m_queue.get(), buffer, cl_bool{CL_FALSE},
                                     cl_uint{0}, buffer_size, arg.data());
    events.push_back(std::move(event));
    mem_ptr tmp;
    tmp.reset(buffer, false);
    arguments.push_back(tmp);
    add_arguments_to_kernel_rec(events, arguments, msg,
                                detail::int_list<Is...>{});
  }

  template <class R, class Token>
  void add_arguments_to_kernel(evnt_vec& events, args_vec& arguments,
                               size_t ret_size, message& msg, Token tk) {
    arguments.clear();
    auto buf = v2get(CAF_CLF(clCreateBuffer), m_context.get(),
                     cl_mem_flags{CL_MEM_WRITE_ONLY},
                     sizeof(typename R::value_type) * ret_size, nullptr);
    mem_ptr tmp;
    tmp.reset(buf, false);
    arguments.push_back(tmp);
    add_arguments_to_kernel_rec(events, arguments, msg, tk);
  }

  kernel_ptr m_kernel;
  program_ptr m_program;
  context_ptr m_context;
  command_queue_ptr m_queue;
  dim_vec m_global_dimensions;
  dim_vec m_global_offsets;
  dim_vec m_local_dimensions;
  size_t m_result_size;
  arg_mapping m_map_args;
  result_mapping m_map_result;
};

} // namespace opencl
} // namespace caf

#endif // CAF_OPENCL_ACTOR_FACADE_HPP
