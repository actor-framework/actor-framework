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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#ifndef CAF_OPENCL_HPP
#define CAF_OPENCL_HPP

#include <algorithm>
#include <functional>

#include "caf/optional.hpp"
#include "caf/cow_tuple.hpp"

#include "caf/util/call.hpp"
#include "caf/util/limited_vector.hpp"

#include "caf/opencl/global.hpp"
#include "caf/opencl/actor_facade.hpp"
#include "caf/opencl/opencl_metainfo.hpp"

namespace caf {

namespace detail {

// converts C arrays, i.e., pointers, to vectors
template<typename T>
struct carr_to_vec {
    using type = T;

};

template<typename T>
struct carr_to_vec<T*> {
    using type = std::vector<T>;

};

template<typename Signature, typename SecondSignature = void>
struct cl_spawn_helper;

template<typename R, typename... Ts>
struct cl_spawn_helper<R(Ts...), void> {

    using result_type = typename carr_to_vec<R>::type;

    using impl = opencl::actor_facade<result_type(
        typename carr_to_vec<typename carr_to_vec<Ts>::type>::type...)>;
    using map_arg_fun = typename impl::arg_mapping;
    using map_res_fun = typename impl::result_mapping;

    template<typename... Us>
    actor operator()(map_arg_fun f0, map_res_fun f1, const opencl::program& p,
                     const char* fname, Us&&... args) const {
        using std::move;
        using std::forward;
        return impl::create(p, fname, move(f0), move(f1), forward<Us>(args)...);
    }

    template<typename... Us>
    actor operator()(const opencl::program& p, const char* fname,
                     Us&&... args) const {
        using std::move;
        using std::forward;
        map_arg_fun f0 = [](message msg) {
            return tuple_cast<typename util::rm_const_and_ref<
                typename carr_to_vec<Ts>::type>::type...>(msg);

        };
        map_res_fun f1 = [](result_type& result) {
            return make_message(move(result));

        };
        return impl::create(p, fname, move(f0), move(f1), forward<Us>(args)...);
    }

};

template<typename R, typename... Ts>
struct cl_spawn_helper<
    std::function<optional<cow_tuple<Ts...>>(message)>,
    std::function<message(R&)>> : cl_spawn_helper<R(Ts...)> {};

} // namespace detail

/**
 * @brief Creates a new actor facade for an OpenCL kernel that invokes
 *        the function named @p fname from @p prog.
 * @throws std::runtime_error if more than three dimensions are set,
 *                            <tt>dims.empty()</tt>, or @p clCreateKernel
 *                            failed.
 */
template<typename Signature, typename... Ts>
inline actor
spawn_cl(const opencl::program& prog, const char* fname,
         const opencl::dim_vec& dims, const opencl::dim_vec& offset = {},
         const opencl::dim_vec& local_dims = {}, size_t result_size = 0) {
    using std::move;
    detail::cl_spawn_helper<Signature> f;
    return f(prog, fname, dims, offset, local_dims, result_size);
}

/**
 * @brief Compiles @p source and creates a new actor facade for an OpenCL kernel
 *        that invokes the function named @p fname.
 * @throws std::runtime_error if more than three dimensions are set,
 *                            <tt>dims.empty()</tt>, a compilation error
 *                            occured, or @p clCreateKernel failed.
 */
template<typename Signature, typename... Ts>
inline actor
spawn_cl(const char* source, const char* fname, const opencl::dim_vec& dims,
         const opencl::dim_vec& offset = {},
         const opencl::dim_vec& local_dims = {}, size_t result_size = 0) {
    using std::move;
    return spawn_cl<Signature, Ts...>(opencl::program::create(source), fname,
                                      dims, offset, local_dims, result_size);
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
inline actor
spawn_cl(const opencl::program& prog, const char* fname, MapArgs map_args,
         MapResult map_result, const opencl::dim_vec& dims,
         const opencl::dim_vec& offset = {},
         const opencl::dim_vec& local_dims = {}, size_t result_size = 0) {
    using std::move;
    using f0 = typename util::get_callable_trait<MapArgs>::fun_type;
    using f1 = typename util::get_callable_trait<MapResult>::fun_type;
    detail::cl_spawn_helper<f0, f1> f;
    return f(f0{move(map_args)}, f1{move(map_result)}, prog, fname, dims,
             offset, local_dims, result_size);
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
inline actor
spawn_cl(const char* source, const char* fun_name, MapArgs map_args,
         MapResult map_result, const opencl::dim_vec& dims,
         const opencl::dim_vec& offset = {},
         const opencl::dim_vec& local_dims = {}, size_t result_size = 0) {
    using std::move;
    return spawn_cl(opencl::program::create(source), fun_name, move(map_args),
                    move(map_result), dims, offset, local_dims, result_size);
}

} // namespace caf

#endif // CAF_OPENCL_HPP
