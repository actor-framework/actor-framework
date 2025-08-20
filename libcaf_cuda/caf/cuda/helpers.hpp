#pragma once
#include "caf/cuda/global.hpp"
#include <random>
#include <climits>
#include <fstream>

//file of helper functions that can be called from anywhere at anytime 
//they are not class specific and is encouraged that users use these 
//functions rather than creating their own 
namespace caf::cuda {

	CAF_CUDA_EXPORT int random_number();
	CAF_CUDA_EXPORT std::vector<char> load_cubin(const std::string&);
	CAF_CUDA_EXPORT bool compile_nvrtc_program(const char* source, CUdevice device, std::vector<char>& ptx_out);

// Extract first matching vector<T> from outputs
template <typename T>
std::optional<std::vector<T>> extract_vector_or_empty(const std::vector<output_buffer>& outputs) {
    for (const auto& out : outputs) {
        if (auto ptr = std::get_if<std::vector<T>>(&out.data)) {
            return *ptr; // copy found vector<T>
        }
    }
    return std::nullopt; // none found
}

// Extract first matching vector<T> from outputs
template <typename T>
std::vector<T> extract_vector(const std::vector<output_buffer>& outputs) {
    for (const auto& out : outputs) {
        if (auto ptr = std::get_if<std::vector<T>>(&out.data)) {
            return *ptr; // copy and return matching vector<T>
        }
    }
    return {}; // no matching vector found, return empty vector
}

// Extract vector<T> from outputs[index], or return empty vector if not matching or out of range
template <typename T>
std::vector<T> extract_vector(const std::vector<output_buffer>& outputs, size_t index) {
    if (index >= outputs.size()) {
        return {}; // index out of bounds
    }
    if (auto ptr = std::get_if<std::vector<T>>(&outputs[index].data)) {
        return *ptr;  // copy and return the vector
    }
    return {}; // no matching type at this index
}


//factory methods that will create a tagged argument 
//required for launching kernels 

//creates a tag of a  readonly buffer on the gpu
template <typename T>
in<T> create_in_arg(const T& val) {
  return in<T>{val};
}

//creates a tag of a readonly buffer on the gpu
template <typename T>
in<T> create_in_arg(const std::vector<T>& buffer) {
  return in<T>{buffer};
}


//creates an output buffer/ write only buffer on the gpu

// Create `out<T>` from scalar or vector
template <typename T>
out<T> create_out_arg(const T& val) {
  return out<T>{val};
}

template <typename T>
out<T> create_out_arg(const std::vector<T>& buffer) {
  return out<T>{buffer};
}

// Create `out<T>` from scalar or vector
template <typename T>
out<T> create_out_arg_with_size(int size) {
  return out<T>{size};
}

//creates a read/write buffer on the gpu 

// Create `in_out<T>` from scalar or vector
template <typename T>
in_out<T> create_in_out_arg(const T& val) {
  return in_out<T>{val};
}

template <typename T>
in_out<T> create_in_out_arg(const std::vector<T>& buffer) {
  return in_out<T>{buffer};
}

} //namespace caf cuda
