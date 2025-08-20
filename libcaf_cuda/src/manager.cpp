#include "caf/cuda/manager.hpp"
#include <stdexcept>
#include <mutex>
#include <map>


namespace caf::cuda {

	manager* manager::instance_ = nullptr;
	std::mutex manager::mutex_;




device_ptr manager::find_device(std::size_t) const {
  throw std::runtime_error("OpenCL support disabled: manager::find_device");
}

//finds a device given its id
device_ptr manager::find_device(int id) {

	return platform_ -> getDevice(id);

}

//creates a program ptr given a kernel and a string 
program_ptr manager::create_program(const char * kernel,
                                    const std::string& name,
                                    device_ptr device) {
  

	CUdevice current_device = device -> getDevice();;

	//the compiled program can be accessed via ptx.data() afterwards
	std::vector<char> ptx;
        if (!compile_nvrtc_program(kernel,current_device,ptx)) {
	
		throw std::runtime_error("Program failed to compile\n");
	
	}
	program_ptr prog = make_counted<program>(name, ptx);
	return prog;
}
//this actually doesnt even work do not use 
program_ptr manager::create_program_from_ptx(const std::string& filename,
                                             const char* kernel_name,
                                             [[maybe_unused]] device_ptr device) {
  static std::mutex global_ptx_mutex_map_guard;
  static std::map<std::string, std::shared_ptr<std::mutex>> ptx_mutex_map;

  // Get per-file mutex
  std::shared_ptr<std::mutex> file_mutex;
  {
    std::lock_guard<std::mutex> lock(global_ptx_mutex_map_guard);
    auto& mtx = ptx_mutex_map[filename];
    if (!mtx)
      mtx = std::make_shared<std::mutex>();
    file_mutex = mtx;
  }

  std::vector<char> ptx;
  {
    std::lock_guard<std::mutex> guard(*file_mutex);

    std::ifstream in(filename, std::ios::binary);
    if (!in) {
      throw std::runtime_error("Failed to open PTX file: " + filename);
    }

    ptx.assign(std::istreambuf_iterator<char>(in),
               std::istreambuf_iterator<char>());
  }

   // 🔒 Guard the actual JIT as well — this is the critical part!
  std::lock_guard<std::mutex> guard(*file_mutex);
  program_ptr prog = make_counted<program>(kernel_name, ptx);
  return prog;
}


//creates a program given a path to a cubin file and the kernels name
program_ptr manager::create_program_from_cubin(const std::string& filename,
                                               const char* kernel_name,
                                               [[maybe_unused]] device_ptr device) {
  // Open the cubin file in binary mode
  std::ifstream in(filename, std::ios::binary);
  if (!in)
    throw std::runtime_error("Failed to open CUBIN file: " + filename);

  // Read file contents into memory
  std::vector<char> cubin((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());

   // Reuse the same constructor as PTX (program class doesn't care)
  program_ptr prog = make_counted<program>(kernel_name, std::move(cubin));
  return prog;
}


//creates a program given a path to a cubin file and the kernels name
program_ptr manager::create_program_from_cubin(const std::string& filename,
                                               const char* kernel_name) {
  // Open the cubin file in binary mode
  std::ifstream in(filename, std::ios::binary);
  if (!in)
    throw std::runtime_error("Failed to open CUBIN file: " + filename);

  // Read file contents into memory
  std::vector<char> cubin((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());

   // Reuse the same constructor as PTX (program class doesn't care)
  program_ptr prog = make_counted<program>(kernel_name, std::move(cubin));
  return prog;
}



//creates a program given a path to a fatbin file and the kernels name
program_ptr manager::create_program_from_fatbin(const std::string& filename,
                                               const char* kernel_name) {
  // Open the fatbin file in binary mode
  std::ifstream in(filename, std::ios::binary);
  if (!in)
    throw std::runtime_error("Failed to open CUBIN file: " + filename);

  // Read file contents into memory
  std::vector<char> cubin((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());

   // Reuse the same constructor as PTX (program class doesn't care)
  program_ptr prog = make_counted<program>(kernel_name, std::move(cubin),true);
  return prog;
}





// Helper: Compile CUDA source to PTX for a specific device
// Returns true on success; on failure prints log and returns false
bool manager::compile_nvrtc_program(const char* source, CUdevice device, std::vector<char>& ptx_out) {

	return caf::cuda::compile_nvrtc_program(source,device,ptx_out);
}


 

} // namespace caf::cuda
