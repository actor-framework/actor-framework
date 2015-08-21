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

#include "caf/optional.hpp"
#include "caf/actor_cast.hpp"

#include "caf/detail/limited_vector.hpp"

#include "caf/opencl/global.hpp"
#include "caf/opencl/metainfo.hpp"
#include "caf/opencl/arguments.hpp"
#include "caf/opencl/actor_facade.hpp"
#include "caf/opencl/spawn_config.hpp"

namespace caf {

namespace detail {

struct tuple_construct { };

template <class... Ts>
struct cl_spawn_helper {
  using impl = opencl::actor_facade<Ts...>;
  using map_in_fun = std::function<optional<message> (message&)>;
  using map_out_fun = typename impl::output_mapping;

  actor operator()(const opencl::program& p, const char* fn,
                   const opencl::spawn_config& cfg, Ts&&... xs) const {
    return actor_cast<actor>(impl::create(p, fn, cfg,
                                          map_in_fun{}, map_out_fun{},
                                          std::move(xs)...));
  }
  actor operator()(const opencl::program& p, const char* fn,
                   const opencl::spawn_config& cfg,
                   map_in_fun map_input, map_out_fun map_output,
                   Ts&&... xs) const {
    return actor_cast<actor>(impl::create(p, fn, cfg, std::move(map_input),
                                          std::move(map_output),
                                          std::move(xs)...));
  }
  // used by the deprecated spawn_helper
  template <class Tuple, long... Is>
  actor operator()(tuple_construct,
                   const opencl::program& p, const char* fn,
                   const opencl::spawn_config& cfg,
                   Tuple&& xs,
                   detail::int_list<Is...>) const {
    return actor_cast<actor>(impl::create(p, fn, cfg,
                                          map_in_fun{}, map_out_fun{},
                                          std::move(std::get<Is>(xs))...));
  }
  template <class Tuple, long... Is>
  actor operator()(tuple_construct,
                   const opencl::program& p, const char* fn,
                   const opencl::spawn_config& cfg,
                   map_in_fun map_input, map_out_fun map_output,
                   Tuple&& xs,
                   detail::int_list<Is...>) const {
    return actor_cast<actor>(impl::create(p, fn, cfg,std::move(map_input),
                                          std::move(map_output),
                                          std::move(std::get<Is>(xs))...));
  }
};

template <class Signature>
struct cl_spawn_helper_deprecated;

template <class R, class... Ts>
struct cl_spawn_helper_deprecated<R(Ts...)> {
  using input_types
    = detail::type_list<typename opencl::carr_to_vec<Ts>::type...>;
  using wrapped_input_types
    = typename detail::tl_map<input_types,opencl::to_input_arg>::type;
  using wrapped_output_type
    = opencl::out<typename opencl::carr_to_vec<R>::type>;
  using all_types =
    typename detail::tl_push_back<
      wrapped_input_types,
      wrapped_output_type
    >::type;
  using values = typename detail::tl_apply<all_types, std::tuple>::type;
  using helper_type =
    typename detail::tl_apply<all_types, cl_spawn_helper>::type;
  actor operator()(const opencl::program& p, const char* fn,
                   const opencl::spawn_config& conf,
                   size_t result_size) const {
    values xs;
    std::get<tl_size<all_types>::value - 1>(xs) =
      wrapped_output_type{[result_size](Ts&...){ return result_size; }};
    helper_type f;
    return f(tuple_construct{}, p, fn, conf, std::move(xs),
             detail::get_indices(xs));
  }
  actor operator()(const opencl::program& p, const char* fn,
                   const opencl::spawn_config& conf,
                   size_t result_size,
                   typename helper_type::map_in_fun map_args,
                   typename helper_type::map_out_fun map_results) const {
    values xs;
    std::get<tl_size<all_types>::value - 1>(xs) =
      wrapped_output_type{[result_size](Ts&...){ return result_size; }};
    auto indices = detail::get_indices(xs);
    helper_type f;
    return f(tuple_construct{}, p, fn, conf,
             std::move(map_args), std::move(map_results),
             std::move(xs), std::move(indices));
  }
};

} // namespace detail

/// Creates a new actor facade for an OpenCL kernel that invokes
/// the function named `fname` from `prog`.
/// @throws std::runtime_error if more than three dimensions are set,
///                            `dims.empty()`, or `clCreateKernel` failed.
template <class T, class... Ts>
typename std::enable_if<
  opencl::is_opencl_arg<T>::value,
  actor
>::type
spawn_cl(const opencl::program& prog,
               const char* fname,
               const opencl::spawn_config& config,
               T x,
               Ts... xs) {
  detail::cl_spawn_helper<T, Ts...> f;
  return f(prog, fname, config, std::move(x), std::move(xs)...);
}

/// Compiles `source` and creates a new actor facade for an OpenCL kernel
/// that invokes the function named `fname`.
/// @throws std::runtime_error if more than three dimensions are set,
///                            <tt>dims.empty()</tt>, a compilation error
///                            occured, or @p clCreateKernel failed.
template <class T, class... Ts>
typename std::enable_if<
  opencl::is_opencl_arg<T>::value,
  actor
>::type
spawn_cl(const char* source,
               const char* fname,
               const opencl::spawn_config& config,
               T x,
               Ts... xs) {
  detail::cl_spawn_helper<T, Ts...> f;
  return f(opencl::program::create(source), fname, config,
           std::move(x), std::move(xs)...);
}

/// Creates a new actor facade for an OpenCL kernel that invokes
/// the function named `fname` from `prog`.
/// @throws std::runtime_error if more than three dimensions are set,
///                            `dims.empty()`, or `clCreateKernel` failed.
template <class Fun, class... Ts>
actor spawn_cl(const opencl::program& prog,
               const char* fname,
               const opencl::spawn_config& config,
               std::function<optional<message> (message&)> map_args,
               Fun map_result,
               Ts... xs) {
  detail::cl_spawn_helper<Ts...> f;
  return f(prog, fname, config, std::move(map_args), std::move(map_result),
           std::forward<Ts>(xs)...);
}

/// Compiles `source` and creates a new actor facade for an OpenCL kernel
/// that invokes the function named `fname`.
/// @throws std::runtime_error if more than three dimensions are set,
///                            <tt>dims.empty()</tt>, a compilation error
///                            occured, or @p clCreateKernel failed.
template <class Fun, class... Ts>
actor spawn_cl(const char* source,
               const char* fname,
               const opencl::spawn_config& config,
               std::function<optional<message> (message&)> map_args,
               Fun map_result,
               Ts... xs) {
  detail::cl_spawn_helper<Ts...> f;
  return f(opencl::program::create(source), fname, config,
           std::move(map_args), std::move(map_result),
           std::forward<Ts>(xs)...);
}


// !!! Below are the deprecated spawn_cl functions !!!

// Signature first to mark them deprcated, gcc will complain otherwise

template <class Signature>
actor spawn_cl(const opencl::program& prog,
               const char* fname,
               const opencl::dim_vec& dims,
               const opencl::dim_vec& offset = {},
               const opencl::dim_vec& local_dims = {},
               size_t result_size = 0) CAF_DEPRECATED;

template <class Signature>
actor spawn_cl(const char* source,
               const char* fname,
               const opencl::dim_vec& dims,
               const opencl::dim_vec& offset = {},
               const opencl::dim_vec& local_dims = {},
               size_t result_size = 0) CAF_DEPRECATED;

template <class Signature, class Fun>
actor spawn_cl(const opencl::program& prog,
               const char* fname,
               std::function<optional<message> (message&)> map_args,
               Fun map_result,
               const opencl::dim_vec& dims,
               const opencl::dim_vec& offset = {},
               const opencl::dim_vec& local_dims = {},
               size_t result_size = 0) CAF_DEPRECATED;

template <class Signature, class Fun>
actor spawn_cl(const char* source,
               const char* fname,
               std::function<optional<message> (message&)> map_args,
               Fun map_result,
               const opencl::dim_vec& dims,
               const opencl::dim_vec& offset = {},
               const opencl::dim_vec& local_dims = {},
               size_t result_size = 0) CAF_DEPRECATED;

// now the implementations

/// Creates a new actor facade for an OpenCL kernel that invokes
/// the function named `fname` from `prog`.
/// @throws std::runtime_error if more than three dimensions are set,
///                            `dims.empty()`, or `clCreateKernel` failed.
template <class Signature>
actor spawn_cl(const opencl::program& prog,
               const char* fname,
               const opencl::dim_vec& dims,
               const opencl::dim_vec& offset,
               const opencl::dim_vec& local_dims,
               size_t result_size) {
  detail::cl_spawn_helper_deprecated<Signature> f;
  return f(prog, fname, opencl::spawn_config{dims, offset, local_dims},
           result_size);
}

/// Compiles `source` and creates a new actor facade for an OpenCL kernel
/// that invokes the function named `fname`.
/// @throws std::runtime_error if more than three dimensions are set,
///                            <tt>dims.empty()</tt>, a compilation error
///                            occured, or @p clCreateKernel failed.
template <class Signature>
actor spawn_cl(const char* source,
               const char* fname,
               const opencl::dim_vec& dims,
               const opencl::dim_vec& offset,
               const opencl::dim_vec& local_dims,
               size_t result_size) {
  detail::cl_spawn_helper_deprecated<Signature> f;
  return f(opencl::program::create(source), fname,
           opencl::spawn_config{dims, offset, local_dims}, result_size);
}

/// Creates a new actor facade for an OpenCL kernel that invokes
/// the function named `fname` from `prog`.
/// @throws std::runtime_error if more than three dimensions are set,
///                            `dims.empty()`, or `clCreateKernel` failed.
template <class Signature, class Fun>
actor spawn_cl(const opencl::program& prog,
               const char* fname,
               std::function<optional<message> (message&)> map_args,
               Fun map_result,
               const opencl::dim_vec& dims,
               const opencl::dim_vec& offset,
               const opencl::dim_vec& local_dims,
               size_t result_size) {
  detail::cl_spawn_helper_deprecated<Signature> f;
  return f(prog, fname, opencl::spawn_config{dims, offset, local_dims},
           result_size, std::move(map_args), std::move(map_result));
}

/// Compiles `source` and creates a new actor facade for an OpenCL kernel
/// that invokes the function named `fname`.
/// @throws std::runtime_error if more than three dimensions are set,
///                            <tt>dims.empty()</tt>, a compilation error
///                            occured, or @p clCreateKernel failed.
template <class Signature, class Fun>
actor spawn_cl(const char* source,
               const char* fname,
               std::function<optional<message> (message&)> map_args,
               Fun map_result,
               const opencl::dim_vec& dims,
               const opencl::dim_vec& offset,
               const opencl::dim_vec& local_dims,
               size_t result_size) {
  detail::cl_spawn_helper_deprecated<Signature> f;
  return f(opencl::program::create(source), fname,
           opencl::spawn_config{dims, offset, local_dims},
           result_size, std::move(map_args), std::move(map_result));
}

} // namespace caf

#endif // CAF_SPAWN_CL_HPP
