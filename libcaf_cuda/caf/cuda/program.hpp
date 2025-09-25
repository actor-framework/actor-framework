#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include <caf/ref_counted.hpp>
#include "caf/cuda/global.hpp"
#include "caf/cuda/platform.hpp"
#include "caf/cuda/device.hpp"

namespace caf::cuda {

// Class that represents a CUDA kernel
class CAF_CUDA_EXPORT program : public caf::ref_counted {
public:
  /// Construct a program from binary data or PTX.
  /// Loads the kernel on all devices.
  /// @param name Name of the kernel function.
  /// @param binary Binary data (fatbin or PTX).
  /// @param is_fatbin Whether the binary is a fatbinary (default: false).
  program(std::string name, std::vector<char> binary, bool is_fatbin = false);

  /// Returns the CUfunction for a given device.
  /// @throws std::runtime_error if the kernel was not loaded for the device.
  CUfunction get_kernel(int device_id);

private:
  /// Internal helper to load the kernel modules on all devices.
  void load_kernels(bool is_fatbin);

  std::string name_;                       ///< Name of the kernel
  std::vector<char> binary_;               ///< The binary or PTX of the program
  std::unordered_map<int, CUfunction> kernels_; ///< Device ID -> CUfunction mapping
};

/// Alias for an intrusive pointer to a program
using program_ptr = caf::intrusive_ptr<program>;

} // namespace caf::cuda

