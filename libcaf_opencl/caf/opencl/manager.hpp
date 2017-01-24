/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_OPENCL_MANAGER_HPP
#define CAF_OPENCL_MANAGER_HPP

#include <atomic>
#include <vector>
#include <algorithm>
#include <functional>

#include "caf/optional.hpp"
#include "caf/config.hpp"
#include "caf/actor_system.hpp"

#include "caf/opencl/device.hpp"
#include "caf/opencl/global.hpp"
#include "caf/opencl/program.hpp"
#include "caf/opencl/platform.hpp"
#include "caf/opencl/smart_ptr.hpp"
#include "caf/opencl/opencl_actor.hpp"

#include "caf/opencl/detail/spawn_helper.hpp"

namespace caf {
namespace opencl {

class manager : public actor_system::module {
public:
  friend class program;
  friend class actor_system;
  friend cl_command_queue_ptr get_command_queue(uint32_t id);
  manager(const manager&) = delete;
  manager& operator=(const manager&) = delete;
  /// Get the device with id, which is assigned sequientally.
  optional<device_ptr> get_device(size_t dev_id = 0) const;
  /// Get the first device that satisfies the predicate.
  /// The predicate should accept a `const device&` and return a bool;
  template <class UnaryPredicate>
  optional<device_ptr> get_device_if(UnaryPredicate p) const {
    for (auto& pl : platforms_) {
      for (auto& dev : pl->get_devices()) {
        if (p(dev))
          return dev;
      }
    }
    return none;
  }

  void start() override;
  void stop() override;
  void init(actor_system_config&) override;

  id_t id() const override;

  void* subtype_ptr() override;

  static actor_system::module* make(actor_system& sys,
                                    caf::detail::type_list<>);

  // OpenCL functionality

  /// @brief Factory method, that creates a caf::opencl::program
  ///        reading the source from given @p path.
  /// @returns A program object.
  program_ptr create_program_from_file(const char* path,
                                       const char* options = nullptr,
                                       uint32_t device_id = 0);

  /// @brief Factory method, that creates a caf::opencl::program
  ///        from a given @p kernel_source.
  /// @returns A program object.
  program_ptr create_program(const char* kernel_source,
                             const char* options = nullptr,
                             uint32_t device_id = 0);

  /// @brief Factory method, that creates a caf::opencl::program
  ///        reading the source from given @p path.
  /// @returns A program object.
  program_ptr create_program_from_file(const char* path,
                                       const char* options,
                                       const device_ptr dev);

  /// @brief Factory method, that creates a caf::opencl::program
  ///        from a given @p kernel_source.
  /// @returns A program object.
  program_ptr create_program(const char* kernel_source,
                             const char* options, const device_ptr dev);

  /// Creates a new actor facade for an OpenCL kernel that invokes
  /// the function named `fname` from `prog`.
  /// @throws std::runtime_error if more than three dimensions are set,
  ///                            `dims.empty()`, or `clCreateKernel` failed.
  template <class T, class... Ts>
  typename std::enable_if<
    opencl::is_opencl_arg<T>::value,
    actor
  >::type
  spawn(const opencl::program_ptr prog, const char* fname,
        const opencl::spawn_config& config, T x, Ts... xs) {
    detail::cl_spawn_helper<false, T, Ts...> f;
    return f(actor_config{system_.dummy_execution_unit()}, prog, fname, config,
             std::move(x), std::move(xs)...);
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
  spawn(const char* source, const char* fname,
        const opencl::spawn_config& config, T x, Ts... xs) {
    detail::cl_spawn_helper<false, T, Ts...> f;
    return f(actor_config{system_.dummy_execution_unit()},
             create_program(source), fname, config,
             std::move(x), std::move(xs)...);
  }

  /// Creates a new actor facade for an OpenCL kernel that invokes
  /// the function named `fname` from `prog`.
  /// @throws std::runtime_error if more than three dimensions are set,
  ///                            `dims.empty()`, or `clCreateKernel` failed.
  template <class Fun, class... Ts>
  actor spawn(const opencl::program_ptr prog, const char* fname,
              const opencl::spawn_config& config,
              std::function<optional<message> (message&)> map_args,
              Fun map_result, Ts... xs) {
    detail::cl_spawn_helper<false, Ts...> f;
    return f(actor_config{system_.dummy_execution_unit()}, prog, fname, config,
             std::move(map_args), std::move(map_result),
             std::forward<Ts>(xs)...);
  }

  /// Compiles `source` and creates a new actor facade for an OpenCL kernel
  /// that invokes the function named `fname`.
  /// @throws std::runtime_error if more than three dimensions are set,
  ///                            <tt>dims.empty()</tt>, a compilation error
  ///                            occured, or @p clCreateKernel failed.
  template <class Fun, class... Ts>
  actor spawn(const char* source, const char* fname,
              const opencl::spawn_config& config,
              std::function<optional<message> (message&)> map_args,
              Fun map_result, Ts... xs) {
    detail::cl_spawn_helper<false, Ts...> f;
    return f(actor_config{system_.dummy_execution_unit()},
             create_program(source), fname, config,
             std::move(map_args), std::move(map_result),
             std::forward<Ts>(xs)...);
  }

  // --- Only accept the input mapping function ---

  /// Creates a new actor facade for an OpenCL kernel that invokes
  /// the function named `fname` from `prog`.
  /// @throws std::runtime_error if more than three dimensions are set,
  ///                            `dims.empty()`, or `clCreateKernel` failed.
  template <class T, class... Ts>
  typename std::enable_if<
    opencl::is_opencl_arg<T>::value,
    actor
  >::type
  spawn(const opencl::program_ptr prog, const char* fname,
            const opencl::spawn_config& config,
            std::function<optional<message> (message&)> map_args,
            T x, Ts... xs) {
    detail::cl_spawn_helper<false, Ts...> f;
    return f(actor_config{system_.dummy_execution_unit()}, prog, fname, config,
             std::move(map_args), std::forward<T>(x), std::forward<Ts>(xs)...);
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
  spawn(const char* source, const char* fname,
            const opencl::spawn_config& config,
            std::function<optional<message> (message&)> map_args,
            T x, Ts... xs) {
    detail::cl_spawn_helper<false, Ts...> f;
    return f(actor_config{system_.dummy_execution_unit()},
             create_program(source), fname, config,
             std::move(map_args), std::forward<T>(x), std::forward<Ts>(xs)...);
  }


  // --- Input mapping function accepts config as well ---

  /// Creates a new actor facade for an OpenCL kernel that invokes
  /// the function named `fname` from `prog`.
  /// @throws std::runtime_error if more than three dimensions are set,
  ///                            `dims.empty()`, or `clCreateKernel` failed.
  template <class Fun, class... Ts>
  typename std::enable_if<
    !opencl::is_opencl_arg<Fun>::value,
    actor
  >::type
  spawn(const opencl::program_ptr prog, const char* fname,
        const opencl::spawn_config& config,
        std::function<optional<message> (spawn_config&, message&)> map_args,
        Fun map_result, Ts... xs) {
    detail::cl_spawn_helper<true, Ts...> f;
    return f(actor_config{system_.dummy_execution_unit()}, prog, fname, config,
             std::move(map_args), std::move(map_result),
             std::forward<Ts>(xs)...);
  }

  /// Compiles `source` and creates a new actor facade for an OpenCL kernel
  /// that invokes the function named `fname`.
  /// @throws std::runtime_error if more than three dimensions are set,
  ///                            <tt>dims.empty()</tt>, a compilation error
  ///                            occured, or @p clCreateKernel failed.
  template <class Fun, class... Ts>
  typename std::enable_if<
    !opencl::is_opencl_arg<Fun>::value,
    actor
  >::type
  spawn(const char* source, const char* fname,
        const opencl::spawn_config& config,
        std::function<optional<message> (spawn_config&, message&)> map_args,
        Fun map_result, Ts... xs) {
    detail::cl_spawn_helper<true, Ts...> f;
    return f(actor_config{system_.dummy_execution_unit()},
             create_program(source), fname, config,
             std::move(map_args), std::move(map_result),
             std::forward<Ts>(xs)...);
  }

  /// Creates a new actor facade for an OpenCL kernel that invokes
  /// the function named `fname` from `prog`.
  /// @throws std::runtime_error if more than three dimensions are set,
  ///                            `dims.empty()`, or `clCreateKernel` failed.
  template <class T, class... Ts>
  typename std::enable_if<
    opencl::is_opencl_arg<T>::value,
    actor
  >::type
  spawn(const opencl::program_ptr prog, const char* fname,
        const opencl::spawn_config& config,
        std::function<optional<message> (spawn_config&, message&)> map_args,
        T x, Ts... xs) {
    detail::cl_spawn_helper<true, T, Ts...> f;
    return f(actor_config{system_.dummy_execution_unit()}, prog, fname, config,
             std::move(map_args), std::forward<T>(x), std::forward<Ts>(xs)...);
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
  spawn(const char* source, const char* fname,
        const opencl::spawn_config& config,
        std::function<optional<message> (spawn_config&, message&)> map_args,
        T x, Ts... xs) {
    detail::cl_spawn_helper<true, T, Ts...> f;
    return f(actor_config{system_.dummy_execution_unit()},
             create_program(source), fname, config,
             std::move(map_args), std::forward<T>(x), std::forward<Ts>(xs)...);
  }

protected:
  manager(actor_system& sys);
  ~manager() override;

private:
  actor_system& system_;
  std::vector<platform_ptr> platforms_;
};

} // namespace opencl
} // namespace caf

#endif // CAF_OPENCL_MANAGER_HPP
