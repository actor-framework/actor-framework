#include "caf/cuda/program.hpp"

namespace caf::cuda {

program::program(std::string name, std::vector<char> binary, bool is_fatbin)
    : name_(std::move(name)), binary_(std::move(binary)) {
  load_kernels(is_fatbin);
}

void program::load_kernels(bool is_fatbin) {
  // Create a platform instance to enumerate devices
  auto plat = platform::create();

  // Load kernel on all available devices
  for (const auto& dev : plat->devices()) {
    CUcontext ctx = dev->getContext();
    CHECK_CUDA(cuCtxPushCurrent(ctx));

    CUmodule module;
    if (is_fatbin) {
      // Load fatbinary directly
      CUresult res = cuModuleLoadFatBinary(&module, binary_.data());
      if (res != CUDA_SUCCESS) {
        throw std::runtime_error("Failed to load fatbinary for device " +
                                 std::to_string(dev->getId()));
      }
    } else {
      // Load PTX (driver will JIT-compile for the device)
      CHECK_CUDA(cuModuleLoadData(&module, binary_.data()));
    }

    CUfunction kernel;
    CHECK_CUDA(cuModuleGetFunction(&kernel, module, name_.c_str()));

    CHECK_CUDA(cuCtxPopCurrent(nullptr));

    // Store the function handle per device
    kernels_[dev->getId()] = kernel;
  }
}

CUfunction program::get_kernel(int device_id) {
  auto it = kernels_.find(device_id);
  if (it == kernels_.end()) {
    throw std::runtime_error("Kernel not found for device ID: " +
                             std::to_string(device_id));
  }
  return it->second;
}

} // namespace caf::cuda

