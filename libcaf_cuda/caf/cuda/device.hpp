#pragma once

#include <string>
#include <cstddef>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <iostream>
#include <unordered_map>
#include <shared_mutex>
#include <tuple>
#include <mutex>

#include <caf/intrusive_ptr.hpp>
#include <caf/ref_counted.hpp>

#include <cuda.h>

#include "caf/cuda/global.hpp"
#include "caf/cuda/types.hpp"
#include "caf/cuda/streampool.hpp"
#include "caf/cuda/mem_ref.hpp"

namespace caf::cuda {

class device : public caf::ref_counted {
public:
  using device_ptr = caf::intrusive_ptr<device>;

  device(CUdevice device, CUcontext context, const char* name, int id, size_t stream_pool_size = 32)
    : device_(device),
      context_(context),
      id_(id),
      name_(name),
      stream_table_(context, stream_pool_size) {
  }

  ~device() {
    check(cuCtxDestroy(context_), "cuCtxDestroy");
  }

  device(const device&) = delete;
  device& operator=(const device&) = delete;
  device(device&&) noexcept = default;
  device& operator=(device&&) noexcept = default;

  const char* name() const { return name_; }
  CUdevice getDevice() const { return device_; }
  CUcontext getContext() const { return context_; }
  int getId() const { return id_; }
  int getContextId() {return 0;}
  int getStreamId() {return 0;}

  CUcontext getContext(int) { return context_; }

  //returns the CUStream associated with the actor id 
  CUstream get_stream_for_actor(int actor_id) {
    return stream_table_.get_stream(actor_id);
  }

  //releases the CUStream associated with the actor id 
  void release_stream_for_actor(int actor_id) {
    stream_table_.release_stream(actor_id);
  }


  // Overloads for make_arg using actor_id
  template <typename T>
  mem_ptr<T> make_arg(in<T> arg, int actor_id) {
    return global_argument(arg, actor_id, IN);
  }

  template <typename T>
  mem_ptr<T> make_arg(in_out<T> arg, int actor_id) {
    return global_argument(arg, actor_id, IN_OUT);
  }

  template <typename T>
  mem_ptr<T> make_arg(out<T> arg, int actor_id) {
    return scratch_argument(arg, actor_id, OUT);
  }


  // Overloads for make_arg using CUstream directly

  template <typename T>
  mem_ptr<T> make_arg(in<T> arg, CUstream stream) {
     return global_argument(arg, stream, IN);
   }


  template <typename T>
  mem_ptr<T> make_arg(in_out<T> arg, CUstream stream) {
    return global_argument(arg, stream, IN_OUT);
   }

  template <typename T>
  mem_ptr<T> make_arg(out<T> arg, CUstream stream) {
  return scratch_argument(arg, stream, OUT);
  }



  //handling the case that a mem_ref is passed in
  //should I force synchronization onto the same stream always?
  template <typename T>
  mem_ptr<T> make_arg(mem_ptr<T> arg, CUstream stream) {
  
	  if (arg -> deviceID() != id_) {
	  
	throw std::runtime_error("Error memory on device " + std::to_string(arg->deviceID()) +
                         " attempted to be used on a different device, device id was " + std::to_string(id_) + "\n");

	  }
	  //just return the arg back
	  return arg; 
  }




  //given a tuple of mem_ptrs
  //will copy their data back to host and place them in an output buffer
  template <typename... Ts>
  std::vector<output_buffer>  collect_output_buffers_helper(const std::tuple<Ts...>& args) {
    std::vector<output_buffer> result;
    std::apply([&](auto&&... mem) {
      (([&] {
        if (mem && (mem->access() == OUT || mem->access() == IN_OUT)) {
          //using T = typename std::decay_t<decltype(*mem)>::value_type;
          result.emplace_back(output_buffer{buffer_variant{mem->copy_to_host()}});
        }
      })(), ...);
    }, args);
    return result;
  }




  //launches a kernel using wrapper types, in, in_out and out as arguments
  //and returns a tuple of mem ref's that hold device memory  
  	  template <typename... Args>
	  std::tuple<mem_ptr<raw_t<Args>>...>
	  launch_kernel_mem_ref(CUfunction kernel,
                      const nd_range& range,
                      std::tuple<Args...> args,
                      int actor_id,
		      int shared_mem = 0 //in bytes
		      ) {

  // Step 1: Allocate mem_ref<T> for each wrapper type 
   CUstream stream = get_stream_for_actor(actor_id);
   auto mem_refs = std::apply([&](auto&&... arg) {
    return std::make_tuple(make_arg(std::forward<decltype(arg)>(arg), stream)...);
  }, args);

  // Step 2: Prepare kernel argument pointers
  auto kernel_args = prepare_kernel_args(mem_refs);

  // Step 3: Launch kernel
  CHECK_CUDA(cuCtxPushCurrent(getContext()));
  launch_kernel_internal(kernel, range, stream, kernel_args.ptrs.data(),shared_mem);
  CHECK_CUDA(cuCtxPopCurrent(nullptr));

  // Step 4: Clean up kernel argument pointers
  cleanup_kernel_args(kernel_args);

  // Step 5: Return tuple of mem_ref<T>...
  return mem_refs;
}



  // Launch kernel with args that have already been allocated 
  // on the device via mem_ref<T>
  template <typename... Ts>
  std::vector<output_buffer> launch_kernel(CUfunction kernel,
                                           const nd_range& range,
                                           std::tuple<Ts...> args,
                                           int actor_id) {
    CUstream stream = get_stream_for_actor(actor_id);
    CHECK_CUDA(cuCtxPushCurrent(getContext()));

    auto kernel_args = prepare_kernel_args(args);
    launch_kernel_internal(kernel, range, stream, kernel_args.ptrs.data());

    //CHECK_CUDA(cuStreamSynchronize(stream));
    CHECK_CUDA(cuCtxPopCurrent(nullptr));

    auto outputs = collect_output_buffers(args);
    cleanup_kernel_args(kernel_args);

    return outputs;
  }

  // For testing: scalar/buffer detection and cleanup
  struct kernel_arg_pack {
    std::vector<void*> ptrs;
    std::vector<CUdeviceptr*> allocated_device_ptrs; // Buffers only
  };

 

   //given a tuple of mem_refs, turns them into  CUDeviceptrs that 
   //can be used to launch kernels
  template <typename... Ts>
  kernel_arg_pack prepare_kernel_args(const std::tuple<Ts...>& args) {
    kernel_arg_pack pack;
    std::apply([&](auto&&... mem) {
      (([&] {
        if (mem->is_scalar()) {
          pack.ptrs.push_back(const_cast<void*>(
            static_cast<const void*>(mem->host_scalar_ptr())));
        } else {
          CUdeviceptr* dev_ptr = new CUdeviceptr(mem->mem());
          pack.ptrs.push_back(static_cast<void*>(dev_ptr));
          pack.allocated_device_ptrs.push_back(dev_ptr);
        }
      })(), ...);
    }, args);
    return pack;
  }

  //cleans up the cuDevicePtrs that are no longer needed
  void cleanup_kernel_args(kernel_arg_pack& pack) {
    for (auto* ptr : pack.allocated_device_ptrs)
      delete ptr;
    pack.ptrs.clear();
    pack.allocated_device_ptrs.clear();
  }


  //given a tuple of mem_ptrs, collects their data on the gpu and 
  //returns an std::vector<output_buffer>
  template <typename... Ts>
  std::vector<output_buffer> collect_output_buffers(const std::tuple<Ts...>& args) {
   return collect_output_buffers_helper(args);
  }



  // === Old method for legacy tests ===
  template <typename... Ts>
  std::vector<void*> extract_kernel_args(const std::tuple<Ts...>& t) {
    return extract_kernel_args_impl(t, std::index_sequence_for<Ts...>{});
  }

private:
  CUdevice device_;
  CUcontext context_;
  int id_;
  const char* name_;
  DeviceStreamTable stream_table_;
  std::mutex stream_mutex_;

  // === Memory handling ===
  
  //----------------------------------------------
// Helpers for actor_id version
//----------------------------------------------

// allocate a readonly input buffer on the GPU
template <typename T>
mem_ptr<T> global_argument(const in<T>& arg, int actor_id, int access) {
  CUstream stream = get_stream_for_actor(actor_id);
  return global_argument(arg, stream, access);
}

// allocate a read/write input buffer on the GPU
template <typename T>
mem_ptr<T> global_argument(const in_out<T>& arg, int actor_id, int access) {
  CUstream stream = get_stream_for_actor(actor_id);
  return global_argument(arg, stream, access);
}

// allocate an output buffer on the GPU
template <typename T>
mem_ptr<T> scratch_argument(const out<T>& arg, int actor_id, int access) {
  CUstream stream = get_stream_for_actor(actor_id);
  return scratch_argument(arg, stream, access);
}

//----------------------------------------------
// Helpers for CUstream version
//----------------------------------------------

// allocate a readonly input buffer on the GPU
template <typename T>
mem_ptr<T> global_argument(const in<T>& arg, CUstream stream, int access) {
  if (arg.is_scalar()) {
    return caf::intrusive_ptr<mem_ref<T>>(
      new mem_ref<T>(arg.getscalar(), access, id_, 0, getContext(), stream));
  }
  size_t bytes = arg.size() * sizeof(T);
  CUdeviceptr dev_ptr;
  CHECK_CUDA(cuCtxPushCurrent(getContext()));
  CHECK_CUDA(cuMemAlloc(&dev_ptr, bytes));
  CHECK_CUDA(cuMemcpyHtoDAsync(dev_ptr, arg.data(), bytes, stream));
  CHECK_CUDA(cuCtxPopCurrent(nullptr));
  return caf::intrusive_ptr<mem_ref<T>>(
    new mem_ref<T>(arg.size(), dev_ptr, access, id_, 0, getContext(), stream));
}

// allocate a read/write input buffer on the GPU
template <typename T>
mem_ptr<T> global_argument(const in_out<T>& arg, CUstream stream, int access) {
  if (arg.is_scalar()) {
    return caf::intrusive_ptr<mem_ref<T>>(
      new mem_ref<T>(arg.getscalar(), access, id_, 0, getContext(), stream));
  }
  size_t bytes = arg.size() * sizeof(T);
  CUdeviceptr dev_ptr;
  CHECK_CUDA(cuCtxPushCurrent(getContext()));
  CHECK_CUDA(cuMemAlloc(&dev_ptr, bytes));
  CHECK_CUDA(cuMemcpyHtoDAsync(dev_ptr, arg.data(), bytes, stream));
  CHECK_CUDA(cuCtxPopCurrent(nullptr));
  return caf::intrusive_ptr<mem_ref<T>>(
    new mem_ref<T>(arg.size(), dev_ptr, access, id_, 0, getContext(), stream));
}

// allocate an output buffer on the GPU
template <typename T>
mem_ptr<T> scratch_argument(const out<T>& arg, CUstream stream, int access) {
  size_t size =  arg.size();
  CUdeviceptr dev_ptr;
  CHECK_CUDA(cuCtxPushCurrent(getContext()));
  CHECK_CUDA(cuMemAlloc(&dev_ptr, size * sizeof(T)));
  CHECK_CUDA(cuCtxPopCurrent(nullptr));
  return caf::intrusive_ptr<mem_ref<T>>(
    new mem_ref<T>(size, dev_ptr, access, id_, 0, getContext(), stream));
}  

// === Kernel launch core ===
  void launch_kernel_internal(CUfunction kernel,
                              const nd_range& range,
                              CUstream stream,
                              void** args,
			      int shared_mem = 0) {
    CUresult result = cuLaunchKernel(kernel,
                                     range.getGridDimX(), range.getGridDimY(), range.getGridDimZ(),
                                     range.getBlockDimX(), range.getBlockDimY(), range.getBlockDimZ(),
                                     shared_mem, stream, args, nullptr);
    if (result != CUDA_SUCCESS) {
      const char* err_name = nullptr;
      cuGetErrorName(result, &err_name);
      throw std::runtime_error(std::string("cuLaunchKernel failed: ") +
                               (err_name ? err_name : "unknown error"));
    }
  }
  // === Legacy helper ===
  template <typename Tuple, std::size_t... Is>
  std::vector<void*> extract_kernel_args_impl(const Tuple& t,
                                              std::index_sequence<Is...>) {
    std::vector<void*> args(sizeof...(Is));
    size_t i = 0;
    (([&] {
      auto ptr = std::get<Is>(t);
      if (ptr->is_scalar()) {
        args[i++] = const_cast<void*>(static_cast<const void*>(ptr->host_scalar_ptr()));
      } else {
        CUdeviceptr* slot = new CUdeviceptr(ptr->mem());
        args[i++] = slot;
      }
    }()), ...);
    return args;
  }
};

} // namespace caf::cuda

