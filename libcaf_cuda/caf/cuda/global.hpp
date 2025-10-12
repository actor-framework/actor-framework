#pragma once

#include <string>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <caf/logger.hpp>
#include <cuda.h>
#include "caf/cuda/types.hpp" //be sure to include any caf-cuda headers after this one
#include "caf/cuda/nd_range.hpp"
#include "caf/cuda/helpers.hpp"
#include <nvrtc.h>
// CAF type ID registration
#include <caf/type_id.hpp>
#include <caf/anon_mail.hpp>

//a strange fix required in order to get the .so files to become viewable for binaries
//linking against them, if this is not defined with classes you want viewable then
//the linker will complain
#if defined(_MSC_VER)
  #define CAF_CUDA_EXPORT __declspec(dllexport)
#else
  #define CAF_CUDA_EXPORT __attribute__((visibility("default")))
#endif

//helper function to check errors
void inline check(CUresult result, const char* msg) {
    if (result != CUDA_SUCCESS) {
        const char* err_str;
        cuGetErrorString(result, &err_str);
        std::cerr << "CUDA Driver API Error (" << msg << "): " << err_str << "\n";
        exit(1);
    }
}

// Serialization support for in<T>, out<T>, and in_out<T>
template <class Inspector, typename T>
bool inspect(Inspector& f, in<T>& x) {
  auto is_scalar = x.is_scalar();
  if constexpr (Inspector::is_loading) {
    if (is_scalar) {
      T val;
      if (!f.object(x).fields(f.field("is_scalar", is_scalar), f.field("value", val))) {
        return false;
      }
      x = in<T>(val);
    } else {
      std::vector<T> buf;
      if (!f.object(x).fields(f.field("is_scalar", is_scalar), f.field("buffer", buf))) {
        return false;
      }
      x = in<T>(buf);
    }
  } else {
    if (is_scalar) {
      auto val = x.getscalar();
      return f.object(x).fields(f.field("is_scalar", is_scalar), f.field("value", val));
    } else {
      auto buf = x.get_buffer();
      return f.object(x).fields(f.field("is_scalar", is_scalar), f.field("buffer", buf));
    }
  }
  return true;
}
template <class Inspector, typename T>
bool inspect(Inspector& f, out<T>& x) {
    if constexpr (Inspector::is_loading) {
        std::vector<T> buf;
        int size = 0;
        if (!f.object(x).fields(f.field("buffer", buf), f.field("size", size))) {
            return false;
        }
        x = out<T>(buf); // reconstruct with loaded buffer
        return true;
    } else {
        auto buf = x.get_buffer(); // make a copy (non-const)
        auto size = x.size();
        return f.object(x).fields(f.field("buffer", buf), f.field("size", size));
    }
}



template <class Inspector, typename T>
bool inspect(Inspector& f, in_out<T>& x) {
  auto is_scalar = x.is_scalar();
  if constexpr (Inspector::is_loading) {
    if (is_scalar) {
      T val;
      if (!f.object(x).fields(f.field("is_scalar", is_scalar), f.field("value", val))) {
        return false;
      }
      x = in_out<T>(val);
    } else {
      std::vector<T> buf;
      if (!f.object(x).fields(f.field("is_scalar", is_scalar), f.field("buffer", buf))) {
        return false;
      }
      x = in_out<T>(buf);
    }
  } else {
    if (is_scalar) {
      auto val = x.getscalar();
      return f.object(x).fields(f.field("is_scalar", is_scalar), f.field("value", val));
    } else {
      auto buf = x.get_buffer();
      return f.object(x).fields(f.field("is_scalar", is_scalar), f.field("buffer", buf));
    }
  }
  return true;
}

// Serialization support for output_buffer (global namespace)
template <class Inspector>
bool inspect(Inspector& f, output_buffer& x) {
  return f.object(x).fields(f.field("data", x.data));
}

// Serialization support for std::vector<output_buffer> (global namespace)
template <class Inspector>
bool inspect(Inspector& f, std::vector<output_buffer>& x) {
  return f.object(x).fields(f.field("elements", x));
}

// Serialization support for raw vector types
template <class Inspector>
bool inspect(Inspector& f, std::vector<char>& x) {
  return f.object(x).fields(f.field("elements", x));
}

template <class Inspector>
bool inspect(Inspector& f, std::vector<int>& x) {
  return f.object(x).fields(f.field("elements", x));
}

template <class Inspector>
bool inspect(Inspector& f, std::vector<float>& x) {
  return f.object(x).fields(f.field("elements", x));
}

template <class Inspector>
bool inspect(Inspector& f, std::vector<double>& x) {
  return f.object(x).fields(f.field("elements", x));
}

// Serialization support for buffer_variant (global namespace)
template <class Inspector>
bool inspect(Inspector& f, buffer_variant& x) {
  return f.apply(x);
}

// Check CUDA errors macro
#define CHECK_CUDA(call) \
    do { \
        CUresult err = call; \
        if (err != CUDA_SUCCESS) { \
            const char* errStr; \
            cuGetErrorString(err, &errStr); \
            throw std::runtime_error(std::string("CUDA Error: ") + errStr); \
        } \
    } while(0)


// Check NVRTC errors macro
#define CHECK_NVRTC(call) \
    do { nvrtcResult res = call; if (res != NVRTC_SUCCESS) { \
        std::cerr << "NVRTC Error: " << nvrtcGetErrorString(res) << std::endl; exit(1); }} while(0)


// Define a custom type ID block for CUDA types
// TODO should this become a macro???
CAF_BEGIN_TYPE_ID_BLOCK(cuda, caf::first_custom_type_id)

  // Your type IDs
  CAF_ADD_TYPE_ID(cuda, (std::vector<char>))
  CAF_ADD_TYPE_ID(cuda, (std::vector<int>))
  CAF_ADD_TYPE_ID(cuda, (in<int>))
  CAF_ADD_TYPE_ID(cuda, (in<char>))
  CAF_ADD_TYPE_ID(cuda, (out<int>))
  CAF_ADD_TYPE_ID(cuda, (in_out<int>))
  CAF_ADD_TYPE_ID(cuda, (std::vector<float>))
  CAF_ADD_TYPE_ID(cuda, (std::vector<double>))
  CAF_ADD_TYPE_ID(cuda, (buffer_variant))
  CAF_ADD_TYPE_ID(cuda, (output_buffer))
  CAF_ADD_TYPE_ID(cuda, (std::vector<output_buffer>))
  CAF_ADD_TYPE_ID(cuda,(caf::cuda::mem_ptr<int>))  
  CAF_ADD_TYPE_ID(cuda,(caf::cuda::mem_ptr<float>))  
  CAF_ADD_TYPE_ID(cuda,(caf::cuda::mem_ptr<double>))  
  CAF_ADD_TYPE_ID(cuda,(caf::cuda::mem_ptr<char>))  
  
  //atoms 
  CAF_ADD_ATOM(cuda, kernel_done_atom)
  //CAF_ADD_ATOM(cuda, become)
  //CAF_ADD_ATOM(cuda, launch_behavior)
  //CAF_ADD_ATOM(cuda, update_behavior)

CAF_END_TYPE_ID_BLOCK(cuda)

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(caf::cuda::mem_ptr<int>)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(caf::cuda::mem_ptr<float>)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(caf::cuda::mem_ptr<double>)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(caf::cuda::mem_ptr<char>)

