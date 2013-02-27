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


#ifndef CPPA_OPENCL_ACTOR_FACADE_HPP
#define CPPA_OPENCL_ACTOR_FACADE_HPP

#include <iostream>
#include "cppa/to_string.hpp"

#include "cppa/channel.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/opencl/global.hpp"
#include "cppa/opencl/command.hpp"
#include "cppa/opencl/program.hpp"
#include "cppa/util/apply_args.hpp"
#include "cppa/opencl/smart_ptr.hpp"

#include "cppa/detail/scheduled_actor_dummy.hpp"

namespace cppa { namespace opencl {

class command_dispatcher;

void enqueue_to_dispatcher(command_dispatcher*, command*);

template<typename Signature>
class actor_facade;

template<typename Ret, typename... Args>
class actor_facade<Ret(Args...)> : public cppa::detail::scheduled_actor_dummy {

 public:

    actor_facade(command_dispatcher* dispatcher,
                 const program& prog,
                 const std::string& kernel_name)
        : m_program(prog.m_program)
        , m_kernel()
        , m_context(prog.m_context)
        , m_kernel_name(kernel_name)
        , m_dispatcher(dispatcher){

    }

    void enqueue(actor* sender, any_tuple msg) {
        auto opt = tuple_cast<Args...>(msg);
        if (opt) {
            response_handle handle{this, sender, message_id_t{}};
            auto fun = [=](cl_command_queue arg) {
                auto indices = util::get_indices(*opt);
                return util::apply_args_prefixed(*this,
                                                 *opt,
                                                 indices,
                                                 arg,
                                                 handle,
                                                 1024,
                                                 1,
                                                 1,
                                                 0,
                                                 1,
                                                 1);
            };
            enqueue_to_dispatcher(m_dispatcher, new command(fun, sender));
        }
        else {
            // slap caller around with a large fish
        }
    }

    /*
     * todo:
     * local_work_group_size currently unused, passing nullptr instead
     */
    void operator()(cl_command_queue queue,
                    response_handle handle,
                    size_t global_work_size_dim1,
                    size_t global_work_size_dim2,
                    size_t global_work_size_dim3,
                    size_t local_work_size_dim1,
                    size_t local_work_size_dim2,
                    size_t local_work_size_dim3,
                    Args... args) {
        cl_int err{0};

        if (!m_kernel.get()) {
            std::cout << "no kernel found" << std::endl;
            m_kernel.adopt(clCreateKernel(m_program.get(),
                                          m_kernel_name.c_str(),
                                          &err));
            if (err != CL_SUCCESS) {
                throw std::runtime_error("[!!!] clCreateKernel: '"
                                         + get_opencl_error(err)
                                         + "'.");
            }
        }
        else {
            std::cout << "i found a kernel" << std::endl;
        }
        cl_event event;
        int number_of_values = global_work_size_dim1
                             * global_work_size_dim2
                             * global_work_size_dim3;

        /* add arguments */
        Ret result_buf(number_of_values);
        std::vector<mem_ptr> arguments;
        add_arguments_to_kernel(arguments,
                                m_context.get(),
                                m_kernel.get(),
                                queue,
                                result_buf,
                                std::forward<Args>(args)...);

        /* enqueue kernel */
        size_t global_work_group_size[] = {global_work_size_dim1,
                                           global_work_size_dim2,
                                           global_work_size_dim3};
        if (local_work_size_dim1 == 0) {
            err = clEnqueueNDRangeKernel(queue,
                                         m_kernel.get(),
                                         3,
                                         NULL,
                                         global_work_group_size,
                                         nullptr,
                                         0,
                                         nullptr,
                                         &event);
        }
        else {
            size_t local_work_group_size[] = {local_work_size_dim1,
                                              local_work_size_dim2,
                                              local_work_size_dim3};
            err = clEnqueueNDRangeKernel(queue,
                                         m_kernel.get(),
                                         3,
                                         NULL,
                                         global_work_group_size,
                                         local_work_group_size,
                                         0,
                                         nullptr,
                                         &event);
        }
        clReleaseEvent(event);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("[!!!] clEnqueueNDRangeKernel: '"
                                     + get_opencl_error(err)
                                     + "'.");
        }
        clFinish(queue);

        /* get results from gpu */
        Ret results(number_of_values);
        err = clEnqueueReadBuffer(queue,
                                  arguments[0].get(),
                                  CL_TRUE,
                                  0,
                                  sizeof(typename Ret::value_type) * number_of_values,
                                  results.data(),
                                  0,
                                  NULL,
                                  &event);
        clReleaseEvent(event);
        if (err != CL_SUCCESS) {
           throw std::runtime_error("[!!!] clEnqueueReadBuffer: '"
                                    + get_opencl_error(err)
                                    + "'.");
        }
        reply_to(handle, results);
        //return results;
    }

 private:

    typedef std::vector<mem_ptr> args_vec;

    program_ptr m_program;
    kernel_ptr m_kernel;
    context_ptr m_context;
    std::string m_kernel_name;
    command_dispatcher* m_dispatcher;

    static void add_arguments_to_kernel_rec(args_vec& arguments,
                                            cl_context,
                                            cl_kernel kernel,
                                            cl_command_queue queue) {
        cl_int err{0};
        for(unsigned long i{1}; i < arguments.size(); ++i) {
            err = clSetKernelArg(kernel,
                                 (i-1),
                                 sizeof(cl_mem),
                                 static_cast<void*>(&arguments[i]));
            if (err != CL_SUCCESS) {
                throw std::runtime_error("[!!!] clSetKernelArg: '"
                                         + get_opencl_error(err)
                                         + "'.");
            }
        }
        err = clSetKernelArg(kernel,
                             arguments.size()-1,
                             sizeof(cl_mem),
                             static_cast<void*>(&arguments[0]));
        if (err != CL_SUCCESS) {
            throw std::runtime_error("[!!!] clSetKernelArg: '"
                                     + get_opencl_error(err)
                                     + "'.");
        }
        clFinish(queue);
    }

    template<typename T0, typename... Ts>
    static void add_arguments_to_kernel_rec(args_vec& arguments,
                                            cl_context context,
                                            cl_kernel kernel,
                                            cl_command_queue queue,
                                            T0&& arg0,
                                            Ts&&... args) {
        cl_int err{0};
        auto buf = clCreateBuffer(context,
                                  CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                  sizeof(typename T0::value_type)*arg0.size(),
                                  arg0.data(),
                                  &err);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("[!!!] clCreateBuffer: '"
                                     + get_opencl_error(err)
                                     + "'.");
        }
        else {
            mem_ptr tmp;
            tmp.adopt(std::move(buf));
            arguments.push_back(tmp);
            return add_arguments_to_kernel_rec(arguments,
                                               context,
                                               kernel,
                                               queue,
                                               std::forward<Ts>(args)...);
        }
    }

    template<typename R, typename... Ts>
    static void add_arguments_to_kernel(args_vec& arguments,
                                        cl_context context,
                                        cl_kernel kernel,
                                        cl_command_queue queue,
                                        R& ret,
                                        Ts&&... args) {
        arguments.clear();
        cl_int err{0};
        auto buf = clCreateBuffer(context,
                                  CL_MEM_WRITE_ONLY,
                                  sizeof(typename R::value_type)*ret.size(),
                                  nullptr,
                                  &err);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("[!!!] clCreateBuffer: '"
                                     + get_opencl_error(err)
                                     + "'.");
        }
        else {
            mem_ptr tmp;
            tmp.adopt(std::move(buf));
            arguments.push_back(tmp);
            return add_arguments_to_kernel_rec(arguments,
                                               context,
                                               kernel,
                                               queue,
                                               std::forward<Ts>(args)...);
        }
    }

};

} } // namespace cppa::opencl

#endif // CPPA_OPENCL_ACTOR_FACADE_HPP
