#include "caf/cuda/helpers.hpp"

namespace caf::cuda {
//gets a random number
CAF_CUDA_EXPORT int random_number() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> distrib(INT_MIN, INT_MAX);
    return distrib(gen);  
}


//helper function designed to read in a cubin from file
std::vector<char> load_cubin(const std::string& filename) {
  std::ifstream in(filename, std::ios::binary);
  if (!in)
    throw std::runtime_error("Failed to open CUBIN file: " + filename);

  return std::vector<char>(
      std::istreambuf_iterator<char>(in),
      std::istreambuf_iterator<char>());
}



std::string get_computer_architecture_string(CUdevice device) {
    int major = 0, minor = 0;

    CUresult res1 = cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device);
    CUresult res2 = cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device);

//std::cout << nvrtcVersion(&major,&minor) << "\n";
    if (res1 != CUDA_SUCCESS || res2 != CUDA_SUCCESS) {
        std::cerr << "Failed to get compute capability for device\n";
        return "";
    }

    return "--gpu-architecture=compute_" + std::to_string(major) + std::to_string(minor);
}


// Helper: Compile CUDA source to PTX for a specific device
// Returns true on success; on failure prints log and returns false
bool compile_nvrtc_program(const char* source, CUdevice device, std::vector<char>& ptx_out) {
    // 1. Create NVRTC program
    nvrtcProgram prog;
    nvrtcResult res = nvrtcCreateProgram(&prog, source, "kernel.cu", 0, nullptr, nullptr);
    if (res != NVRTC_SUCCESS) {
        std::cerr << "nvrtcCreateProgram failed: " << nvrtcGetErrorString(res) << "\n";
        return false;
    }

    // 2. Get architecture string for device
    std::string arch = get_computer_architecture_string(device);
    if (arch.empty()) {
        nvrtcDestroyProgram(&prog);
        return false;
    }

    const char* options[] = {
    arch.c_str(),           // e.g., "--gpu-architecture=compute_70"
    "--std=c++11",          // Avoids too-modern C++ features
    "--fmad=false",         // Optional: disables fused multiply-add
    "--device-as-default-execution-space" // Optional compatibility
};


    // 3. Compile program
    res = nvrtcCompileProgram(prog, 1, options);

    // 4. Print compile log regardless of success/failure
    size_t logSize;
    nvrtcGetProgramLogSize(prog, &logSize);
    if (logSize > 1) {
        std::vector<char> log(logSize);
        nvrtcGetProgramLog(prog, log.data());
        std::cout << "NVRTC Compile Log:\n" << log.data() << "\n";
    }

    if (res != NVRTC_SUCCESS) {
        std::cerr << "nvrtcCompileProgram failed: " << nvrtcGetErrorString(res) << "\n";
        nvrtcDestroyProgram(&prog);
        return false;
    }

    // 5. Get PTX size
    size_t ptxSize;
    res = nvrtcGetPTXSize(prog, &ptxSize);
    if (res != NVRTC_SUCCESS) {
        std::cerr << "nvrtcGetPTXSize failed: " << nvrtcGetErrorString(res) << "\n";
        nvrtcDestroyProgram(&prog);
        return false;
    }

    // 6. Extract PTX
    ptx_out.resize(ptxSize);
    res = nvrtcGetPTX(prog, ptx_out.data());
    if (res != NVRTC_SUCCESS) {
        std::cerr << "nvrtcGetPTX failed: " << nvrtcGetErrorString(res) << "\n";
        nvrtcDestroyProgram(&prog);
        return false;
    }

    // 7. Clean up
    nvrtcDestroyProgram(&prog);
    return true;
}

}
