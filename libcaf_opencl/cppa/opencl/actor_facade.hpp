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
#include <stdexcept>

#include "cppa/cppa.hpp"

#include "cppa/channel.hpp"
#include "cppa/to_string.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/util/int_list.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/scope_guard.hpp"

#include "cppa/opencl/global.hpp"
#include "cppa/opencl/command.hpp"
#include "cppa/opencl/program.hpp"
#include "cppa/opencl/smart_ptr.hpp"

#include "cppa/detail/scheduled_actor_dummy.hpp"

namespace cppa { namespace opencl {

class command_dispatcher;

void enqueue_to_dispatcher(command_dispatcher*, command_ptr);

template<typename Signature>
class actor_facade;

template<typename Ret, typename... Args>
class actor_facade<Ret(Args...)> : public actor {

 public:

    actor_facade(command_dispatcher* dispatcher,
                 const program& prog,
                 const char* kernel_name)
        : m_program(prog.m_program)
        , m_context(prog.m_context)
        , m_dispatcher(dispatcher)
    {
        CPPA_LOG_TRACE("new actor facde with ID " << this->id());
        init_kernel(m_program.get(), kernel_name);
    }

    actor_facade(command_dispatcher* dispatcher,
                 const program& prog,
                 const char* kernel_name,
                 std::vector<size_t>& global_dimensions,
                 std::vector<size_t>& local_dimensions)
        : m_program(prog.m_program)
        , m_context(prog.m_context)
        , m_dispatcher(dispatcher)
        , m_global_dimensions(global_dimensions)
        , m_local_dimensions(local_dimensions)
    {
        CPPA_LOG_TRACE("new actor facde with ID " << this->id());
        if(m_local_dimensions .size() > 3 ||
           m_global_dimensions.size() > 3) {
            throw std::runtime_error("[!!!] Creating actor facade:"
                                     " a maximum of 3 dimensions allowed");
        }
        init_kernel(m_program.get(), kernel_name);
    }

    void enqueue(const actor_ptr& sender, any_tuple msg) {
        CPPA_LOG_TRACE("actor_facade::enqueue()");
        typename util::il_indices<util::type_list<Args...>>::type indices;
        enqueue_impl(sender, msg, message_id{}, indices);
    }

    void sync_enqueue(const actor_ptr& sender, message_id id, any_tuple msg) {
        CPPA_LOG_TRACE("actor_facade::sync_enqueue()");
        typename util::il_indices<util::type_list<Args...>>::type indices;
        enqueue_impl(sender, msg, id, indices);
    }

 private:

    void init_kernel(cl_program program, const char* kernel_name) {
        cl_int err{0};
        m_kernel.adopt(clCreateKernel(program,
                                      kernel_name,
                                      &err));
        if (err != CL_SUCCESS) {
            throw std::runtime_error("[!!!] clCreateKernel: '"
                                     + get_opencl_error(err)
                                     + "'.");
        }
    }

    template<long... Is>
    void enqueue_impl(const actor_ptr& sender, any_tuple msg, message_id id, util::int_list<Is...>) {
        auto opt = tuple_cast<Args...>(msg);
        if (opt) {
            response_handle handle{this, sender, id};
            size_t number_of_values = 1;
            if (!m_global_dimensions.empty()) {
                for (auto s : m_global_dimensions) {
                    number_of_values *= s;
                }
//                for (auto s : m_local_dimensions) {
//                    if (s > 0) {
//                        number_of_values *= s;
//                    }
//                }
            }
            else {
                number_of_values = get<0>(*opt).size();
                m_global_dimensions.push_back(number_of_values);
                m_global_dimensions.push_back(1);
                m_global_dimensions.push_back(1);
            }
            if (m_global_dimensions.empty() || number_of_values <= 0) {
                throw std::runtime_error("[!!!] enqueue: can't handle dimension sizes!");
            }
//            for (auto s : m_global_dimensions) std::cout << "[global] " << s << std::endl;
//            for (auto s : m_local_dimensions ) std::cout << "[local ] " << s << std::endl;
//            std::cout << "number stuff " << number_of_values << std::endl;
            Ret result_buf(number_of_values);
            std::vector<mem_ptr> arguments;
            add_arguments_to_kernel(arguments,
                                    m_context.get(),
                                    m_kernel.get(),
                                    result_buf,
                                    get_ref<Is>(*opt)...);
            CPPA_LOG_TRACE("enqueue to dispatcher");
            enqueue_to_dispatcher(m_dispatcher,
                                  make_counted<command_impl<Ret>>(handle,
                                                                  m_kernel,
                                                                  arguments,
                                                                  m_global_dimensions,
                                                                  m_local_dimensions));
        }
        else {
            aout << "*** warning: tuple_cast failed!\n";
            // slap caller around with a large fish
        }
    }

    typedef std::vector<mem_ptr> args_vec;

    program_ptr m_program;
    kernel_ptr m_kernel;
    context_ptr m_context;
    command_dispatcher* m_dispatcher;
    std::vector<size_t> m_global_dimensions;
    std::vector<size_t> m_local_dimensions;

    void add_arguments_to_kernel_rec(args_vec& arguments,
                                            cl_context,
                                            cl_kernel kernel) {
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
    }

    template<typename T0, typename... Ts>
    void add_arguments_to_kernel_rec(args_vec& arguments,
                                            cl_context context,
                                            cl_kernel kernel,
                                            T0& arg0,
                                            Ts&... args) {
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
                                               args...);
        }
    }

    template<typename R, typename... Ts>
    void add_arguments_to_kernel(args_vec& arguments,
                                        cl_context context,
                                        cl_kernel kernel,
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
                                               std::forward<Ts>(args)...);
        }
    }

};

} } // namespace cppa::opencl

#endif // CPPA_OPENCL_ACTOR_FACADE_HPP
