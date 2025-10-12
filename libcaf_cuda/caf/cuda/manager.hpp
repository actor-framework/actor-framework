#pragma once

#include <string>
#include <cstddef>
#include <stdexcept>
#include <mutex>
#include <fstream>
#include <map>

#include <caf/actor_system.hpp>
#include <caf/actor.hpp>
#include <caf/intrusive_ptr.hpp>
#include <caf/detail/type_list.hpp>
#include <caf/init_global_meta_objects.hpp>

#include "caf/detail/spawn_helper.hpp"

#include "caf/cuda/global.hpp"
#include "caf/cuda/device.hpp"
#include "caf/cuda/program.hpp"
#include "caf/cuda/actor_facade.hpp"
#include "caf/cuda/platform.hpp"

//A class that just acts as a user interface
//and a system initialization for cuda 
namespace caf::cuda {

class device;
using device_ptr = caf::intrusive_ptr<device>;

class program;
using program_ptr = caf::intrusive_ptr<program>;

class platform;
using platform_ptr = caf::intrusive_ptr<platform>;

template <bool PassConfig, class... Ts>
class actor_facade;

class CAF_CUDA_EXPORT manager {
public:
  /// Initializes the singleton. Must be called exactly once before get().
  static void init(caf::actor_system& sys) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (instance_) {
      	//return;
      throw std::runtime_error("CUDA manager already initialized");
    }
    CHECK_CUDA(cuInit(0));
    CUcontext ctx = nullptr;
    cuCtxGetCurrent(&ctx);
    //std::cout << "Before cuCtxCreate, context: " << ctx << std::endl;
    instance_ = new manager(sys);
    //caf::core::init_global_meta_objects();
     caf::init_global_meta_objects<caf::id_block::cuda>();
  }

  /// Returns the singleton instance. Crashes if not yet initialized.
  static manager& get() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!instance_) {
      throw std::runtime_error("CUDA manager used before initialization\n  Please place caf::cuda::manager::init() at the top of CAF_MAIN\n");
    }
    return *instance_;
  }

  /// Deletes the singleton if needed (optional).
  static void shutdown() {
    std::lock_guard<std::mutex> guard(mutex_);
    delete instance_;
    instance_ = nullptr;
  }

  // Prevent copy/assignment
  manager(const manager&) = delete;
  manager& operator=(const manager&) = delete;

  //device_ptr getter
  device_ptr find_device(std::size_t id) const;


  //Creates a program ptr to be used to launch kernels
  //@param: kernel, A string representation of a kernel
  //@param:name, the function signature name of the kernel
  //@param dev, device pointer
  program_ptr create_program(const char* kernel,
                             const std::string& name,
                             device_ptr dev);


  program_ptr create_program_from_file(const std::string& filename,
                                       const char* options,
                                       device_ptr dev);


  //Currently not working DO NOT USE 
  program_ptr create_program_from_ptx(const std::string& filename,
                                    const char* kernel_name,
                                    device_ptr device);

  program_ptr create_program_from_cubin(const std::string& filename,
                                               const char* kernel_name,
                                               device_ptr device);



 //Creates a program ptr to be used to launch kernels
 //must read in a precompiled cubin file 
  //@param: filename, this is the path to the file that contains the kernel
  //@param: kernel_name, the function signature name of the kernel
  program_ptr create_program_from_cubin(const std::string& filename,
                                               const char* kernel_name);



 //Creates a program ptr to be used to launch kernels
 //must read in a precompiled fatbin file 
  //@param: filename, this is the path to the file that contains the kernel
  //@param: kernel_name, the function signature name of the kernel
  program_ptr create_program_from_fatbin(const std::string& filename,
                                               const char* kernel_name);



  //Spawns in an actor facade 
  //@param, kernel, string version of the kernel
  //@param name, name of the function that is the kernel 
  //@param dims: both block and grid dimensions of the kernel you want to launch
  //@returns a handle to the actor facade
  template <class... Ts>
  caf::actor spawn(const char* kernel,
                   const std::string& name,
		   nd_range dims,
                   Ts&&... xs) {
    caf::detail::cuda_spawn_helper<false, Ts...> f;
    caf::actor_config cfg;

    device_ptr device = find_device(0);
    program_ptr prog = create_program(kernel, name, device);

    return f(&system_, std::move(cfg), std::move(prog),dims,std::forward<Ts>(xs)...);
  }


  //Currently broken DO not use  
  template <class... Ts>
  caf::actor spawnFromPTX(
                   const std::string& fileName,
		   const char * kernelName,
		   nd_range dims,
                   Ts&&... xs) {
    caf::detail::cuda_spawn_helper<false, Ts...> f;
    caf::actor_config cfg;

    device_ptr device = find_device(0);
    program_ptr prog = create_program_from_ptx(fileName, kernelName, device);

    return f(&system_, std::move(cfg), std::move(prog),dims,std::forward<Ts>(xs)...);
  }



 
  //Spawns an actor in from a precompiled cubin
  //must read in a precompiled cubin file
  //@param filename, path to the file
  //@param kernelName: the name of the kernel
  //@param dims: both block and grid dimensions of the kernel you want to launch
  //@returns a handle to the actor facade
  template <class... Ts>
  caf::actor spawnFromCUBIN(
                   const std::string& fileName,
		   const char * kernelName,
		   nd_range dims,
                   Ts&&... xs) {
    caf::detail::cuda_spawn_helper<false, Ts...> f;
    caf::actor_config cfg;

    device_ptr device = find_device(0);
    program_ptr prog = create_program_from_cubin(fileName, kernelName, device);

    return f(&system_, std::move(cfg), std::move(prog),dims,std::forward<Ts>(xs)...);
  }


  caf::actor_system& system() { return system_; }

  device_ptr find_device(int id);

private:
  explicit manager(caf::actor_system& sys)
    : system_(sys), platform_(platform::create()) {
    // cuInit is done in init()
  }

  caf::actor_system& system_;
  platform_ptr platform_;

  //helper to compile a nvrtc program
  bool compile_nvrtc_program(const char* source, CUdevice device, std::vector<char>& ptx_out);


  static manager* instance_;
  static std::mutex mutex_;
};

} // namespace caf::cuda

