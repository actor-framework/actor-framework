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


#ifndef CPPA_OPENCL_HPP
#define CPPA_OPENCL_HPP

#include "cppa/option.hpp"
#include "cppa/cow_tuple.hpp"

#include "cppa/util/call.hpp"
#include "cppa/util/limited_vector.hpp"

#include "cppa/opencl/global.hpp"
#include "cppa/opencl/command_dispatcher.hpp"

namespace cppa {

namespace detail {

template<typename Signature, typename SecondSignature = void>
struct cl_spawn_helper;

template<typename R, typename... Ts>
struct cl_spawn_helper<R (Ts...), void> {
    template<typename... Us>
    actor_ptr operator()(const opencl::program& p, const char* fname, Us&&... args) {
        auto cd = opencl::get_command_dispatcher();
        return cd->spawn<R, Ts...>(p, fname, std::forward<Us>(args)...);
    }
};

template<typename R, typename... Ts>
struct cl_spawn_helper<std::function<option<cow_tuple<Ts...>> (any_tuple)>,
                       std::function<any_tuple (R&)>>
: cl_spawn_helper<R (Ts...)> { };

} // namespace detail

/**
 * @brief Creates a new actor facade for an OpenCL kernel that invokes
 *        the function named @p fname from @p prog.
 * @throws std::runtime_error if more than three dimensions are set,
 *                            <tt>dims.empty()</tt>, or @p clCreateKernel
 *                            failed.
 */
template<typename Signature, typename... Ts>
inline actor_ptr spawn_cl(const opencl::program& prog,
                          const char* fname,
                          const opencl::dim_vec& dims,
                          const opencl::dim_vec& offset = {},
                          const opencl::dim_vec& local_dims = {}) {
    detail::cl_spawn_helper<Signature> f;
    return f(prog, fname, dims, offset, local_dims);
}

/**
 * @brief Compiles @p source and creates a new actor facade for an OpenCL kernel
 *        that invokes the function named @p fname.
 * @throws std::runtime_error if more than three dimensions are set,
 *                            <tt>dims.empty()</tt>, a compilation error
 *                            occured, or @p clCreateKernel failed.
 */
template<typename Signature, typename... Ts>
inline actor_ptr spawn_cl(const char* source,
                          const char* fname,
                          const opencl::dim_vec& dims,
                          const opencl::dim_vec& offset = {},
                          const opencl::dim_vec& local_dims = {}) {
    auto prog = opencl::program::create(source);
    detail::cl_spawn_helper<Signature> f;
    return f(prog, fname, dims, offset, local_dims);
}

/**
 * @brief Creates a new actor facade for an OpenCL kernel that invokes
 *        the function named @p fname from @p prog, using @p map_args
 *        to extract the function arguments from incoming messages and
 *        @p map_result to transform the result before sending it as response.
 * @throws std::runtime_error if more than three dimensions are set,
 *                            <tt>dims.empty()</tt>, or @p clCreateKernel
 *                            failed.
 */
template<typename MapArgs, typename MapResult>
inline actor_ptr spawn_cl(const opencl::program& prog,
                          const char* fname,
                          MapArgs map_args,
                          MapResult map_result,
                          const opencl::dim_vec& dims,
                          const opencl::dim_vec& offset = {},
                          const opencl::dim_vec& local_dims = {}) {
    typedef typename util::get_callable_trait<MapArgs>::fun_type f0;
    typedef typename util::get_callable_trait<MapResult>::fun_type f1;
    detail::cl_spawn_helper<f0, f1> f;
    return f(prog, fname, dims, offset, local_dims,
             f0{map_args}, f1{map_result});
}

/**
 * @brief Compiles @p source and creates a new actor facade for an OpenCL kernel
 *        that invokes the function named @p fname, using @p map_args
 *        to extract the function arguments from incoming messages and
 *        @p map_result to transform the result before sending it as response.
 * @throws std::runtime_error if more than three dimensions are set,
 *                            <tt>dims.empty()</tt>, a compilation error
 *                            occured, or @p clCreateKernel failed.
 */
template<typename MapArgs, typename MapResult>
inline actor_ptr spawn_cl(const char* source,
                          const char* fun_name,
                          MapArgs map_args,
                          MapResult map_result,
                          const opencl::dim_vec& dims,
                          const opencl::dim_vec& offset = {},
                          const opencl::dim_vec& local_dims = {}) {
    using std::move;
    return spawn_cl(opencl::program::create(source), fun_name,
                    move(map_args), move(map_result),
                    dims, offset, local_dims);
}

} // namespace cppa

#endif // CPPA_OPENCL_HPP
