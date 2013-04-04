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


#ifndef CPPA_OPENCL_HPP
#define CPPA_OPENCL_HPP

#include "cppa/option.hpp"
#include "cppa/cow_tuple.hpp"

#include "cppa/opencl/global.hpp"
#include "cppa/opencl/command_dispatcher.hpp"

namespace cppa {

template<typename Signature>
struct cl_spawn_helper;

template<typename R, typename... Ts>
struct cl_spawn_helper<R (Ts...)> {
    template<typename... Us>
    actor_ptr operator()(const char* source, const char* fname, Us&&... args) {
        auto p = opencl::program::create(source);
        auto cd = opencl::get_command_dispatcher();
        return cd->spawn<R, Ts...>(p, fname, std::forward<Us>(args)...);
    }
};

template<typename MapArgs, typename MapResult>
struct get_cl_spawn_helper;

template<typename R, typename... Ts>
struct get_cl_spawn_helper<std::function<option<cow_tuple<Ts...>> (const any_tuple&)>,
                           std::function<any_tuple (R&)>> {
    typedef cl_spawn_helper<R (const Ts&...)> type;
};

template<typename Signature, typename... Ts>
actor_ptr spawn_cl(const char* source,
                   const char* fun_name,
                   std::vector<size_t> dimensions,
                   std::vector<size_t> offset = {},
                   std::vector<size_t> local_dims = {}) {
    using std::move;
    cl_spawn_helper<Signature> f;
    return f(source, fun_name, move(dimensions), move(offset), move(local_dims));
}

template<typename MapArgs, typename MapResult>
actor_ptr spawn_cl(const char* source,
                   const char* fun_name,
                   MapArgs map_args,
                   MapResult map_result,
                   std::vector<size_t> dimensions,
                   std::vector<size_t> offset = {},
                   std::vector<size_t> local_dims = {}) {
    using std::move;
    typedef typename util::get_callable_trait<MapArgs>::type t0;
    typedef typename util::get_callable_trait<MapResult>::type t1;
    typedef typename t0::fun_type f0;
    typedef typename t1::fun_type f1;
    typename get_cl_spawn_helper<f0,f1>::type f;
    f(source, fun_name, move(dimensions), move(offset), move(local_dims), f0{map_args}, f1{map_result});
}


} // namespace cppa

#endif // CPPA_OPENCL_HPP
