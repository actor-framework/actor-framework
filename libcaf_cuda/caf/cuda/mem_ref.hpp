// mem_ref.hpp
#pragma once
#include <cuda.h>
#include <caf/intrusive_ptr.hpp>
#include <caf/ref_counted.hpp>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <iostream>
#include "caf/cuda/types.hpp"
//#include "caf/cuda/utility.hpp"
#include <cuda.h>
#include <atomic>

namespace caf::cuda {

//A class that is a handle to gpu memory
template <class T>
class mem_ref : public caf::ref_counted {
public:
  using value_type = T;

  //constructor with CUdeviceptr
  mem_ref(size_t num_elements,
          CUdeviceptr memory,
          int access,
          int device_id    = 0,
          int context_id   = 0,
	  CUcontext context = nullptr,
          CUstream stream  = nullptr)
    : num_elements_(num_elements),
      memory_(memory),
      access_(access),
      device_id(device_id),
      context_id(context_id),
      stream_(stream),
      ctx(context),
      is_scalar_(false)
  {
    if (memory_ == 0)
      std::abort(); // unchanged
  }

  // Scalar constructor (new!)
  mem_ref(const T& scalar_value,
          int access,
          int device_id    = 0,
          int context_id   = 0,
	  CUcontext context = nullptr,
          CUstream stream  = nullptr)
    : num_elements_(1),
      memory_(0),                    // no device buffer
      access_(access),
      device_id(device_id),
      context_id(context_id),
      stream_(stream),
      ctx(context),
      is_scalar_(true),
      host_scalar_(scalar_value)
  {
    // no cuMemAlloc, nothing to do
  }

  ~mem_ref() {
    reset();
  }

  mem_ref(mem_ref&&) noexcept = default;
  mem_ref& operator=(mem_ref&&) noexcept = default;
  mem_ref(const mem_ref&) = delete;
  mem_ref& operator=(const mem_ref&) = delete;

  // a bunch of getters
  bool is_scalar() const noexcept {return is_scalar_;}
  const T* host_scalar_ptr() const noexcept {return &host_scalar_;}
  size_t size()  const noexcept { return num_elements_; }
  CUdeviceptr mem()   const noexcept { return memory_; }
  int access()  const noexcept { return access_; }
  CUstream stream() const noexcept { return stream_; }
  int deviceID() const noexcept { return device_id;}
  int deviceNumber() const noexcept { return device_id;}

 //if it is ever needed, you can force synchronization on a mem_ptr
 //to ensure data on the device that the mem_ptr points to
 //is not in the middle of being operated on
 //mostly here to avoid race conditions that can occur between streams 
  void synchronize()  {
    CHECK_CUDA(cuCtxPushCurrent(ctx));
    CUstream s = stream_ ? stream_ : nullptr;
    if (s) CHECK_CUDA(cuStreamSynchronize(s));
    else  CHECK_CUDA(cuCtxSynchronize());
    CHECK_CUDA(cuCtxPopCurrent(nullptr)); 
  } 

  //Frees the memory on the gpu and 
  //sets all its attributes to null or -1
  void reset() {
    if (!is_scalar_ && memory_) {
      CHECK_CUDA(cuMemFree(memory_));
      memory_ = 0;
    }
    num_elements_ = 0;
    access_       = -1;
    stream_       = nullptr;
    ctx = nullptr;
  }

  //copies gpu memory back to cpu memory in the form of an std::vector
  std::vector<T> copy_to_host() const {
    if (access_ == IN)
    {
	    throw std::runtime_error("Cannt copy a read only buffer back to device\n");
    } 
    if (is_scalar_) {
      return std::vector<T>{host_scalar_};
    }
    std::vector<T> host_data(num_elements_);
    size_t bytes = num_elements_ * sizeof(T);
    CHECK_CUDA(cuCtxPushCurrent(ctx));
    CUstream s = stream_ ? stream_ : nullptr;
    CHECK_CUDA(cuMemcpyDtoHAsync(host_data.data(), memory_, bytes, s));
    if (s) CHECK_CUDA(cuStreamSynchronize(s));
    else  CHECK_CUDA(cuCtxSynchronize());
    CHECK_CUDA(cuCtxPopCurrent(nullptr));
    return host_data;
  }



    //reference counting for auto garabage collection
    friend void intrusive_ptr_add_ref(const mem_ref<T>* p) noexcept {
        p->ref_count_.fetch_add(1, std::memory_order_relaxed);
    }

    friend void intrusive_ptr_release(const mem_ref<T>* p) noexcept {
        if (p->ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1)
            delete p;
    }



private:
  size_t      num_elements_{0};
  CUdeviceptr memory_{0};
  int         access_{-1};
  int         device_id{0};
  int         context_id{0};
  CUstream    stream_{nullptr};
  CUcontext ctx;
  mutable std::atomic<size_t> ref_count_{0};

  bool is_scalar_{false};
  T    host_scalar_{};
};

template <class T>
using mem_ptr = caf::intrusive_ptr<mem_ref<T>>;

} // namespace caf::cuda

