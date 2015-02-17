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

#ifndef CAF_SPAWN_CL_HPP
#define CAF_SPAWN_CL_HPP

#include <algorithm>
#include <functional>

#include "caf/actor_cast.hpp"
#include "caf/optional.hpp"

#include "caf/detail/limited_vector.hpp"

#include "caf/opencl/global.hpp"
#include "caf/opencl/actor_facade.hpp"
#include "caf/opencl/opencl_metainfo.hpp"

namespace caf {

namespace detail {

// converts C arrays, i.e., pointers, to vectors
template <typename T>
struct carr_to_vec {
  using type = T;
};

template <typename T>
struct carr_to_vec<T*> {
  using type = std::vector<T>;
};

template <class Signature>
struct cl_spawn_helper;

template <typename R, typename... Ts>
struct cl_spawn_helper<R(Ts...)> {
  using res_t = typename carr_to_vec<R>::type;
  using impl = opencl::actor_facade<res_t (typename carr_to_vec<Ts>::type...)>;
  using map_arg_fun = std::function<optional<message> (message&)>;
  using map_res_fun = typename impl::result_mapping;

  template <typename... Us>
  actor operator()(const opencl::program& p, const char* fn, Us&&... vs) const {
    return actor_cast<actor>(impl::create(p, fn, std::forward<Us>(vs)...));
  }
};

} // namespace detail

/**
 * Creates a new actor facade for an OpenCL kernel that invokes
 * the function named `fname` from `prog`.
 * @throws std::runtime_error if more than three dimensions are set,
 *                            `dims.empty()`, or `clCreateKernel` failed.
 */
template <class Signature>
actor spawn_cl(const opencl::program& prog,
               const char* fname,
               const opencl::dim_vec& dims,
               const opencl::dim_vec& offset = {},
               const opencl::dim_vec& local_dims = {},
               size_t result_size = 0) {
  detail::cl_spawn_helper<Signature> f;
  return f(prog, fname, dims, offset, local_dims, result_size);
}

/**
 * Compiles `source` and creates a new actor facade for an OpenCL kernel
 * that invokes the function named `fname`.
 * @throws std::runtime_error if more than three dimensions are set,
 *                            <tt>dims.empty()</tt>, a compilation error
 *                            occured, or @p clCreateKernel failed.
 */
template <class Signature>
actor spawn_cl(const char* source,
               const char* fname,
               const opencl::dim_vec& dims,
               const opencl::dim_vec& offset = {},
               const opencl::dim_vec& local_dims = {},
               size_t result_size = 0) {
  return spawn_cl<Signature>(opencl::program::create(source), fname,
                             dims, offset, local_dims, result_size);
}

/**
 * Creates a new actor facade for an OpenCL kernel that invokes
 * the function named `fname` from `prog`.
 * @throws std::runtime_error if more than three dimensions are set,
 *                            `dims.empty()`, or `clCreateKernel` failed.
 */
template <class Signature, class Fun>
actor spawn_cl(const opencl::program& prog,
               const char* fname,
               std::function<optional<message> (message&)> map_args,
               Fun map_result,
               const opencl::dim_vec& dims,
               const opencl::dim_vec& offset = {},
               const opencl::dim_vec& local_dims = {},
               size_t result_size = 0) {
  detail::cl_spawn_helper<Signature> f;
  return f(prog, fname, dims, offset, local_dims, result_size,
           std::move(map_args), std::move(map_result));
}

/**
 * Compiles `source` and creates a new actor facade for an OpenCL kernel
 * that invokes the function named `fname`.
 * @throws std::runtime_error if more than three dimensions are set,
 *                            <tt>dims.empty()</tt>, a compilation error
 *                            occured, or @p clCreateKernel failed.
 */
template <class Signature, class Fun>
actor spawn_cl(const char* source,
               const char* fname,
               std::function<optional<message> (message&)> map_args,
               Fun map_result,
               const opencl::dim_vec& dims,
               const opencl::dim_vec& offset = {},
               const opencl::dim_vec& local_dims = {},
               size_t result_size = 0) {
  detail::cl_spawn_helper<Signature> f;
  return f(opencl::program::create(source), fname, dims, offset, local_dims,
           result_size, std::move(map_args), std::move(map_result));
}

} // namespace caf

#endif // CAF_SPAWN_CL_HPP
