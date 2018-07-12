/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <vector>

#include "caf/sec.hpp"

#include "caf/detail/raw_ptr.hpp"

#include "caf/opencl/global.hpp"
#include "caf/opencl/opencl_err.hpp"


namespace caf {
namespace opencl {

class program;
class manager;
template <class T> class mem_ref;

class device;
using device_ptr = intrusive_ptr<device>;

class device : public ref_counted {
public:
  friend class program;
  friend class manager;
  template <class T> friend class mem_ref;
  template <class T, class... Ts>
  friend intrusive_ptr<T> caf::make_counted(Ts&&...);

  ~device();

  /// Create an argument for an OpenCL kernel with data placed in global memory.
  template <class T>
  mem_ref<T> global_argument(const std::vector<T>& data,
                             cl_mem_flags flags = buffer_type::input_output,
                             optional<size_t> size = none,
                             cl_bool blocking = CL_FALSE) {
    size_t num_elements = size ? *size : data.size();
    size_t buffer_size = sizeof(T) * num_elements;
    auto buffer = v2get(CAF_CLF(clCreateBuffer), context_.get(), flags,
                        buffer_size, nullptr);
    detail::raw_event_ptr event{v1get<cl_event>(CAF_CLF(clEnqueueWriteBuffer),
                                                queue_.get(), buffer, blocking,
                                                cl_uint{0}, buffer_size,
                                                data.data()),
                                false};
    return mem_ref<T>{num_elements, queue_, std::move(buffer), flags,
                      std::move(event)};
  }

  /// Create an argument for an OpenCL kernel in global memory without data.
  template <class T>
  mem_ref<T> scratch_argument(size_t size,
                              cl_mem_flags flags = buffer_type::scratch_space) {
    auto buffer = v2get(CAF_CLF(clCreateBuffer), context_.get(), flags,
                        sizeof(T) * size, nullptr);
    return mem_ref<T>{size, queue_, std::move(buffer), flags, nullptr};
  }

  template <class T>
  expected<mem_ref<T>> copy(mem_ref<T>& mem) {
    if (!mem.get())
      return make_error(sec::runtime_error, "No memory assigned.");
    auto buffer_size = sizeof(T) * mem.size();
    cl_event event;
    auto buffer = v2get(CAF_CLF(clCreateBuffer), context_.get(), mem.access(),
                        buffer_size, nullptr);
    std::vector<cl_event> prev_events;
    cl_event e = mem.take_event();
    if (e)
      prev_events.push_back(e);
    auto err = clEnqueueCopyBuffer(queue_.get(), mem.get().get(), buffer,
                                   0, 0, // no offset for now
                                   buffer_size, prev_events.size(),
                                   prev_events.data(), &event);
    if (err != CL_SUCCESS)
      return make_error(sec::runtime_error, opencl_error(err));
    // callback to release the previous event 
    if (e) {
      err = clSetEventCallback(event, CL_COMPLETE,
                               [](cl_event, cl_int, void* data) {
                                 auto tmp = reinterpret_cast<cl_event>(data);
                                 if (tmp)
                                   clReleaseEvent(tmp);
                               },
                               e);
      if (err != CL_SUCCESS)
        return make_error(sec::runtime_error, opencl_error(err));
    }
    // decrements the previous event we used for waiting above
    return mem_ref<T>(mem.size(), queue_, std::move(buffer),
                      mem.access(), {event, false});
  }

  /// Initialize a new device in a context using a specific device_id
  static device_ptr create(const detail::raw_context_ptr& context,
                           const detail::raw_device_ptr& device_id,
                           unsigned id);
  /// Synchronizes all commands in its queue, waiting for them to finish.
  void synchronize();
  /// Get the id assigned by caf
  inline unsigned id() const;
  /// Returns device info on CL_DEVICE_ADDRESS_BITS
  inline cl_uint address_bits() const;
  /// Returns device info on CL_DEVICE_ENDIAN_LITTLE
  inline cl_bool little_endian() const;
  /// Returns device info on CL_DEVICE_GLOBAL_MEM_CACHE_SIZE
  inline cl_ulong global_mem_cache_size() const;
  /// Returns device info on CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE
  inline cl_uint global_mem_cacheline_size() const;
  /// Returns device info on CL_DEVICE_GLOBAL_MEM_SIZE
  inline cl_ulong global_mem_size() const;
  /// Returns device info on CL_DEVICE_HOST_UNIFIED_MEMORY
  inline cl_bool host_unified_memory() const;
  /// Returns device info on CL_DEVICE_LOCAL_MEM_SIZE
  inline cl_ulong local_mem_size() const;
  /// Returns device info on CL_DEVICE_LOCAL_MEM_TYPE
  inline cl_ulong local_mem_type() const;
  /// Returns device info on CL_DEVICE_MAX_CLOCK_FREQUENCY
  inline cl_uint max_clock_frequency() const;
  /// Returns device info on CL_DEVICE_MAX_COMPUTE_UNITS
  inline cl_uint max_compute_units() const;
  /// Returns device info on CL_DEVICE_MAX_CONSTANT_ARGS
  inline cl_uint max_constant_args() const;
  /// Returns device info on CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE
  inline cl_ulong max_constant_buffer_size() const;
  /// Returns device info on CL_DEVICE_MAX_MEM_ALLOC_SIZE
  inline cl_ulong max_mem_alloc_size() const;
  /// Returns device info on CL_DEVICE_MAX_PARAMETER_SIZE
  inline size_t max_parameter_size() const;
  /// Returns device info on CL_DEVICE_MAX_WORK_GROUP_SIZE
  inline size_t max_work_group_size() const;
  /// Returns device info on CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS
  inline cl_uint max_work_item_dimensions() const;
  /// Returns device info on CL_DEVICE_PROFILING_TIMER_RESOLUTION
  inline size_t profiling_timer_resolution() const;
  /// Returns device info on CL_DEVICE_MAX_WORK_ITEM_SIZES
  inline const dim_vec& max_work_item_sizes() const;
  /// Returns device info on CL_DEVICE_TYPE
  inline device_type type() const;
  /// Returns device info on CL_DEVICE_EXTENSIONS
  inline const std::vector<std::string>& extensions() const;
  /// Returns device info on CL_DEVICE_OPENCL_C_VERSION
  inline const std::string& opencl_c_version() const;
  /// Returns device info on CL_DEVICE_VENDOR
  inline const std::string& device_vendor() const;
  /// Returns device info on CL_DEVICE_VERSION
  inline const std::string& device_version() const;
  /// Returns device info on CL_DRIVER_VERSION
  inline const std::string& driver_version() const;
  /// Returns device info on CL_DEVICE_NAME
  inline const std::string& name() const;

private:
  device(detail::raw_device_ptr device_id, detail::raw_command_queue_ptr queue,
         detail::raw_context_ptr context, unsigned id);

  template <class T>
  static T info(const detail::raw_device_ptr& device_id, unsigned info_flag) {
    T value;
    clGetDeviceInfo(device_id.get(), info_flag, sizeof(T), &value, nullptr);
    return value;
  }

  static std::string info_string(const detail::raw_device_ptr& device_id,
                                 unsigned info_flag);
  detail::raw_device_ptr device_id_;
  detail::raw_command_queue_ptr queue_;
  detail::raw_context_ptr context_;
  unsigned id_;

  bool profiling_enabled_;              // CL_DEVICE_QUEUE_PROPERTIES
  bool out_of_order_execution_;         // CL_DEVICE_QUEUE_PROPERTIES

  cl_uint address_bits_;                // CL_DEVICE_ADDRESS_BITS
  cl_bool little_endian_;               // CL_DEVICE_ENDIAN_LITTLE
  cl_ulong global_mem_cache_size_;      // CL_DEVICE_GLOBAL_MEM_CACHE_SIZE
  cl_uint global_mem_cacheline_size_;   // CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE
  cl_ulong global_mem_size_;            // CL_DEVICE_GLOBAL_MEM_SIZE
  cl_bool host_unified_memory_;         // CL_DEVICE_HOST_UNIFIED_MEMORY
  cl_ulong local_mem_size_;             // CL_DEVICE_LOCAL_MEM_SIZE
  cl_uint local_mem_type_;              // CL_DEVICE_LOCAL_MEM_TYPE
  cl_uint max_clock_frequency_;         // CL_DEVICE_MAX_CLOCK_FREQUENCY
  cl_uint max_compute_units_;           // CL_DEVICE_MAX_COMPUTE_UNITS
  cl_uint max_constant_args_;           // CL_DEVICE_MAX_CONSTANT_ARGS
  cl_ulong max_constant_buffer_size_;   // CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE
  cl_ulong max_mem_alloc_size_;         // CL_DEVICE_MAX_MEM_ALLOC_SIZE
  size_t max_parameter_size_;           // CL_DEVICE_MAX_PARAMETER_SIZE
  size_t max_work_group_size_;          // CL_DEVICE_MAX_WORK_GROUP_SIZE
  cl_uint max_work_item_dimensions_;    // CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS
  size_t profiling_timer_resolution_;   // CL_DEVICE_PROFILING_TIMER_RESOLUTION
  dim_vec max_work_item_sizes_;         // CL_DEVICE_MAX_WORK_ITEM_SIZES
  device_type device_type_;             // CL_DEVICE_TYPE
  std::vector<std::string> extensions_; // CL_DEVICE_EXTENSIONS
  std::string opencl_c_version_;        // CL_DEVICE_OPENCL_C_VERSION
  std::string device_vendor_;           // CL_DEVICE_VENDOR
  std::string device_version_;          // CL_DEVICE_VERSION
  std::string driver_version_;          // CL_DRIVER_VERSION
  std::string name_;                    // CL_DEVICE_NAME
};

/******************************************************************************\
 *                 implementation of inline member functions                  *
\******************************************************************************/

inline unsigned device::id() const {
  return id_;
}

inline cl_uint device::address_bits() const {
  return address_bits_;
}

inline cl_bool device::little_endian() const {
  return little_endian_;
}

inline cl_ulong device::global_mem_cache_size() const {
  return global_mem_cache_size_;
}

inline cl_uint device::global_mem_cacheline_size() const {
  return global_mem_cacheline_size_;
}

inline cl_ulong device::global_mem_size() const {
  return global_mem_size_;
}

inline cl_bool device::host_unified_memory() const {
  return host_unified_memory_;
}

inline cl_ulong device::local_mem_size() const {
  return local_mem_size_;
}

inline cl_ulong device::local_mem_type() const {
  return local_mem_size_;
}

inline cl_uint device::max_clock_frequency() const {
  return max_clock_frequency_;
}

inline cl_uint device::max_compute_units() const {
  return max_compute_units_;
}

inline cl_uint device::max_constant_args() const {
  return max_constant_args_;
}

inline cl_ulong device::max_constant_buffer_size() const {
  return max_constant_buffer_size_;
}

inline cl_ulong device::max_mem_alloc_size() const {
  return max_mem_alloc_size_;
}

inline size_t device::max_parameter_size() const {
  return max_parameter_size_;
}

inline size_t device::max_work_group_size() const {
  return max_work_group_size_;
}

inline cl_uint device::max_work_item_dimensions() const {
  return max_work_item_dimensions_;
}

inline size_t device::profiling_timer_resolution() const {
  return profiling_timer_resolution_;
}

inline const dim_vec& device::max_work_item_sizes() const {
  return max_work_item_sizes_;
}

inline device_type device::type() const {
  return device_type_;
}

inline const std::vector<std::string>& device::extensions() const {
  return extensions_;
}

inline const std::string& device::opencl_c_version() const {
  return opencl_c_version_;
}

inline const std::string& device::device_vendor() const {
  return device_vendor_;
}

inline const std::string& device::device_version() const {
  return device_version_;
}

inline const std::string& device::driver_version() const {
  return driver_version_;
}

inline const std::string& device::name() const {
  return name_;
}

} // namespace opencl
} // namespace caf

