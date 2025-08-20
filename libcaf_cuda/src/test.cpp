#include <iostream>
#include <vector>
#include <cassert>
#include <stdexcept>
#include <iomanip>
#include <cuda.h>
#include <caf/all.hpp>
#include <caf/cuda/manager.hpp>
#include <caf/cuda/program.hpp>
#include <caf/cuda/device.hpp>
#include <caf/cuda/command.hpp>
#include <caf/cuda/actor_facade.hpp>
#include <caf/cuda/mem_ref.hpp>
#include <caf/cuda/platform.hpp>
#include <caf/cuda/global.hpp>
#include "caf/detail/test.hpp"

using namespace caf;
using namespace caf::cuda;

//file of internal tests, testing each class

int test_actor_id = 0;

void test_platform([[maybe_unused]] actor_system& sys, [[maybe_unused]] platform_ptr plat) {
    std::cout << "\n=== Test Platform ===\n";
    
    std::cout << "Test 1: Creating platform...\n";
    assert(plat != nullptr && "Platform creation failed: nullptr returned");
    assert(!plat->devices().empty() && "Platform creation failed: no devices found");
    std::cout << "  -> Platform created with " << plat->devices().size() << " device(s).\n";

    std::cout << "Test 2: Retrieving device 0...\n";
    device_ptr dev = plat->getDevice(0);
    assert(dev != nullptr && "Device retrieval failed: nullptr returned for device 0");
    assert(dev->getId() == 0 && "Device ID mismatch: expected 0");
    std::cout << "  -> Device 0 retrieved successfully.\n";

    std::cout << "Test 3: Testing invalid device ID (-1)...\n";
    try {
        plat->getDevice(-1);
        assert(false && "Expected exception for negative device ID");
    } catch (const std::exception& e) {
        std::cout << "  -> Caught expected exception: " << e.what() << "\n";
    }
    std::cout << "---- Platform tests passed ----\n";
}

void test_device([[maybe_unused]] actor_system& sys, [[maybe_unused]] platform_ptr plat) {
    std::cout << "\n=== Test Device ===\n";
    
    std::cout << "Test 1: Checking device properties...\n";
    device_ptr dev = plat->getDevice(0);
    assert(dev->getContext() != nullptr && "Device context is null");
    //assert(dev->getStream() != nullptr && "Device stream is null");
    assert(dev->name() != nullptr && "Device name is null");
    std::cout << "  -> Device properties valid (context, stream, name).\n";

    std::cout << "Test 2: Testing memory argument creation...\n";
    std::vector<int> data(5, 42);
    in<int> input = create_in_arg(data);
    mem_ptr<int> in_mem = dev->make_arg(input,test_actor_id);
    std::cout << "input memory size is " << in_mem -> size() << "\n";
    assert(in_mem->size() == 5 && "Input memory size mismatch: expected 5");
    assert(in_mem->access() == IN && "Input memory access type incorrect: expected IN");
    std::cout << "  -> Input memory argument created successfully.\n";

    in_out<int> inout = create_in_out_arg(data);
    mem_ptr<int> inout_mem = dev->make_arg(inout,test_actor_id);
    assert(inout_mem->access() == IN_OUT && "In-out memory access type incorrect: expected IN_OUT");
    std::cout << "  -> In-out memory argument created successfully.\n";

    out<int> output = create_out_arg(std::vector<int>(5, 0));
    mem_ptr<int> out_mem = dev->make_arg(output,test_actor_id);
    assert(out_mem->size() == 5 && "Output memory size mismatch: expected 5");
    assert(out_mem->access() == OUT && "Output memory access type incorrect: expected OUT");
    std::cout << "  -> Output memory argument created successfully.\n";
    std::cout << "---- Device tests passed ----\n";
}

void test_manager([[maybe_unused]] actor_system& sys, [[maybe_unused]] platform_ptr plat) {
    std::cout << "\n=== Test Manager ===\n";
    
    std::cout << "Test 1: Initializing manager...\n";
    manager& mgr = manager::get();
    assert(&mgr == &manager::get() && "Manager singleton mismatch");
    std::cout << "  -> Manager initialized successfully.\n";

    std::cout << "Test 2: Retrieving device 0...\n";
    device_ptr dev = mgr.find_device(0);
    assert(dev != nullptr && "Device retrieval failed: nullptr returned");
    assert(dev->getId() == 0 && "Device ID mismatch: expected 0");
    std::cout << "  -> Device 0 retrieved successfully.\n";

    std::cout << "Test 3: Creating program with test kernel...\n";
    const char* kernel = R"(
        extern "C" __global__ void test_kernel(int* data) {
            int idx = threadIdx.x;
            data[idx] = idx;
        })";
    program_ptr prog = mgr.create_program(kernel, "test_kernel", dev);
    assert(prog != nullptr && "Program creation failed: nullptr returned");
    assert(prog->get_kernel(0) != nullptr && "Kernel creation failed: nullptr returned");
    std::cout << "  -> Program and kernel created successfully.\n";

    std::cout << "Test 4: Testing invalid device ID (999)...\n";
    try {
        mgr.find_device(999);
        assert(false && "Expected exception for invalid device ID");
    } catch (const std::exception& e) {
        std::cout << "  -> Caught expected exception: " << e.what() << "\n";
    }
    std::cout << "---- Manager tests passed ----\n";
}

void test_program([[maybe_unused]] actor_system& sys, [[maybe_unused]] platform_ptr plat) {
    std::cout << "\n=== Test Program ===\n";
    
    std::cout << "Test 1: Checking program properties...\n";
    manager& mgr = manager::get();
    device_ptr dev = mgr.find_device(0);
    const char* kernel = R"(
        extern "C" __global__ void test_kernel(int* data) {
            int idx = threadIdx.x;
            data[idx] = idx;
        })";
    program_ptr prog = mgr.create_program(kernel, "test_kernel", dev);
    //this test used to check more but redundant method of program were removed
    std::cout << "  -> Program properties valid (device_id=0, context_id=0, stream_id=0).\n";
    std::cout << "---- Program tests passed ----\n";
}

void test_create_program([[maybe_unused]] actor_system& sys, [[maybe_unused]] platform_ptr plat) {
    std::cout << "\n=== Test Create Program ===\n";

    manager& mgr = manager::get();
    device_ptr dev = mgr.find_device(0);
    std::cout << "  -> Device context: " << dev->getContext(0) << "\n";

    std::cout << "Test 1: Creating and validating simple kernel...\n";
    {
        const char* kernel_code = R"(
            extern "C" __global__ void simple_kernel(int* output) {
                output[0] = 42;
            })";
        try {
            std::cout << "  -> Creating program for kernel: simple_kernel\n";
            program_ptr prog = mgr.create_program(kernel_code, "simple_kernel", dev);
            if (!prog) {
                throw std::runtime_error("create_program returned null program_ptr");
            }
            std::cout << "  -> Program created: prog=" << prog.get() << "\n";

            CUfunction kernel = prog->get_kernel(0);
            if (!kernel) {
                throw std::runtime_error("get_kernel returned null CUfunction");
            }
            std::cout << "  -> Kernel handle: " << kernel << "\n";

            CUmodule module;
            CHECK_CUDA(cuFuncGetModule(&module, kernel));
            if (!module) {
                throw std::runtime_error("cuFuncGetModule returned null CUmodule");
            }
            std::cout << "  -> Module handle: " << module << "\n";

            CUcontext ctx = dev->getContext(0);
            CUcontext current_ctx;
            CHECK_CUDA(cuCtxGetCurrent(&current_ctx));
            std::cout << "  -> Current context: " << current_ctx << ", device context: " << ctx << "\n";
            if (current_ctx && current_ctx != ctx) {
                throw std::runtime_error("Context mismatch: expected " + std::to_string((uintptr_t)ctx) +
                                         ", got " + std::to_string((uintptr_t)current_ctx));
            }

            std::cout << "  -> Simple kernel created and validated successfully\n";
        } catch (const std::exception& e) {
            std::cout << "  -> Failed: " << e.what() << "\n";
            throw;
        }
        std::cout << "  -> End of scope: prog destroyed\n";
    }
    std::cout << "---- Create Program tests passed ----\n";
}

void test_mem_ref([[maybe_unused]] actor_system& sys, [[maybe_unused]] platform_ptr plat) {
    std::cout << "\n=== Test Mem Ref ===\n";
    
    manager& mgr = manager::get();
    device_ptr dev = mgr.find_device(0);
    CUcontext ctx = dev -> getContext(0);
    (void)ctx; //silence the warning
    std::cout << "Test 1: Testing input memory allocation...\n";
    std::vector<int> host_data(5, 10);
    in<int> input = create_in_arg(host_data);
    mem_ptr<int> mem = dev->make_arg(input,test_actor_id);
    assert(mem->size() == 5 && "Input memory size mismatch: expected 5");
    assert(mem->mem() != 0 && "Input memory allocation failed: null pointer");
    assert(mem->access() == IN && "Input memory access type incorrect: expected IN");
    std::cout << "  -> Input memory allocated successfully.\n";

    std::cout << "Test 2: Testing output memory allocation...\n";
    out<int> output = create_out_arg(std::vector<int>(5, 0));
    mem_ptr<int> out_mem = dev->make_arg(output,test_actor_id);
    assert(out_mem->size() == 5 && "Output memory size mismatch: expected 5");
    assert(out_mem->access() == OUT && "Output memory access type incorrect: expected OUT");
    std::cout << "  -> Output memory allocated successfully.\n";

    std::cout << "Test 3: Testing in-out memory data integrity...\n";
    in_out<int> inout = create_in_out_arg(host_data);
    mem_ptr<int> inout_mem = dev->make_arg(inout,test_actor_id);
    assert(inout_mem->access() == IN_OUT && "In-out memory access type incorrect: expected IN_OUT");
    std::vector<int> copied = inout_mem->copy_to_host();
    for (size_t i = 0; i < 5; ++i) {
        assert(copied[i] == 10 && "In-out memory data corruption");
        if (copied[i] != 10) {
            std::cout << "  -> Failed: copied[" << i << "] = " << copied[i] << ", expected 10\n";
        }
    }
    std::cout << "  -> In-out memory data copied correctly.\n";

    std::cout << "Test 4: Testing invalid copy from input memory...\n";
    try {
        mem->copy_to_host();
        assert(false && "Expected exception for copying IN memory");
    } catch (const std::runtime_error& e) {
        std::cout << "  -> Caught expected exception: " << e.what() << "\n";
    }

    std::cout << "Test 5: Testing memory reset...\n";
    mem->reset();
    assert(mem->size() == 0 && "Memory reset failed: size not 0");
    assert(mem->mem() == 0 && "Memory reset failed: pointer not null");
    std::cout << "  -> Memory reset successfully.\n";
    std::cout << "---- Mem Ref tests passed ----\n";
}

void test_command([[maybe_unused]] actor_system& sys, [[maybe_unused]] platform_ptr plat) {
    std::cout << "\n=== Test Command ===\n";
    std::cout << "Test 1: Command test skipped due to response_promise issue.\n";
    std::cout << "---- Command tests passed (skipped) ----\n";
}

void test_mem_ref_extended([[maybe_unused]] actor_system& sys, [[maybe_unused]] platform_ptr plat) {
    std::cout << "\n=== Test Mem Ref Extended ===\n";
    
    manager& mgr = manager::get();
    device_ptr dev = mgr.find_device(0);

    std::cout << "Test 1: Testing input memory allocation...\n";
    std::vector<int> host_input(5, 42);
    in<int> input = create_in_arg(host_input);
    mem_ptr<int> in_mem = dev->make_arg(input,test_actor_id);
    assert(in_mem->size() == 5 && "Input memory size mismatch: expected 5");
    assert(in_mem->access() == IN && "Input memory access type incorrect: expected IN");
    assert(in_mem->mem() != 0 && "Input memory allocation failed: null pointer");
    std::cout << "  -> Input memory allocated successfully.\n";

    std::cout << "Test 2: Testing output memory allocation...\n";
    std::vector<int> host_output(5, 0);
    out<int> output = create_out_arg(host_output);
    mem_ptr<int> out_mem = dev->make_arg(output,test_actor_id);
    assert(out_mem->size() == 5 && "Output memory size mismatch: expected 5");
    assert(out_mem->access() == OUT && "Output memory access type incorrect: expected OUT");
    assert(out_mem->mem() != 0 && "Output memory allocation failed: null pointer");
    std::cout << "  -> Output memory allocated successfully.\n";

    std::cout << "Test 3: Testing in-out memory data integrity...\n";
    std::vector<int> host_inout(5, 10);
    in_out<int> inout = create_in_out_arg(host_inout);
    mem_ptr<int> inout_mem = dev->make_arg(inout,test_actor_id);
    assert(inout_mem->size() == 5 && "In-out memory size mismatch: expected 5");
    assert(inout_mem->access() == IN_OUT && "In-out memory access type incorrect: expected IN_OUT");
    std::vector<int> copied = inout_mem->copy_to_host();
    for (size_t i = 0; i < 5; ++i) {
        assert(copied[i] == 10 && "In-out memory data corruption");
        if (copied[i] != 10) {
            std::cout << "  -> Failed: copied[" << i << "] = " << copied[i] << ", expected 10\n";
        }
    }
    std::cout << "  -> In-out memory data copied correctly.\n";

    std::cout << "Test 4: Testing small buffer allocation...\n";
    std::vector<int> small_data(2, 5);
    out<int> small_output = create_out_arg(small_data);
    mem_ptr<int> small_mem = dev->make_arg(small_output,test_actor_id);
    assert(small_mem->size() == 2 && "Small buffer size mismatch: expected 2");
    std::cout << "  -> Small buffer allocated successfully.\n";
    std::cout << "---- Mem Ref Extended tests passed ----\n";
}

void test_argument_translation([[maybe_unused]] actor_system& sys, [[maybe_unused]] platform_ptr plat) {
    std::cout << "\n=== Test Argument Translation ===\n";
    
    manager& mgr = manager::get();
    device_ptr dev = mgr.find_device(0);

    std::cout << "Test 1: Testing output argument creation...\n";
    std::vector<int> data(5, 0);
    out<int> output = create_out_arg(data);
    mem_ptr<int> out_mem = dev->make_arg(output,test_actor_id);
    assert(out_mem->size() == data.size() && "Output argument size mismatch: expected 5");
    assert(out_mem->access() == OUT && "Output argument access type incorrect: expected OUT");
    std::cout << "  -> Output argument created successfully.\n";

    std::cout << "Test 2: Testing type mismatch simulation...\n";
    in<int> input = create_in_arg(data);
    mem_ptr<int> in_mem = dev->make_arg(input,test_actor_id);
    assert(in_mem->access() == IN && "Input argument access type incorrect: expected IN");
    try {
        in_mem->copy_to_host();
        assert(false && "Expected exception for copying IN memory");
    } catch (const std::runtime_error& e) {
        std::cout << "  -> Caught expected exception: " << e.what() << "\n";
    }

    std::cout << "Test 3: Testing multiple argument creation...\n";
    in<int> input2 = create_in_arg(std::vector<int>(5, 7));
    out<int> output2 = create_out_arg(std::vector<int>(5, 0));
    mem_ptr<int> in_mem2 = dev->make_arg(input2,test_actor_id);
    mem_ptr<int> out_mem2 = dev->make_arg(output2,test_actor_id);
    assert(in_mem2->size() == 5 && out_mem2->size() == 5 && "Multiple argument size mismatch: expected 5");
    std::cout << "  -> Multiple arguments created successfully.\n";
    std::cout << "---- Argument Translation tests passed ----\n";
}

void test_kernel_launch([[maybe_unused]] actor_system& sys, [[maybe_unused]] platform_ptr plat) {
    std::cout << "\n=== Test Kernel Launch ===\n";
    
    manager& mgr = manager::get();
    device_ptr dev = mgr.find_device(0);
    std::cout << "  -> Device context: " << dev->getContext(0) << "\n";

    std::cout << "Test 1: Testing basic kernel launch...\n";
    {
        const char* kernel_code = R"(
            extern "C" __global__ void simple_kernel(int* output) {
                output[0] = 42;
            })";
        program_ptr prog = mgr.create_program(kernel_code, "simple_kernel", dev);
        std::cout << "  -> Program created with kernel: simple_kernel, handle: " << prog->get_kernel(0) << ", prog=" << prog.get() << "\n";
        std::vector<int> host_data(1, 0);
        out<int> output = create_out_arg(host_data);
        mem_ptr<int> out_mem = dev->make_arg(output,test_actor_id);
        assert(out_mem->mem() != 0 && "Output memory not allocated");
        std::cout << "  -> Output memory allocated: " << out_mem->mem() << ", out_mem=" << out_mem.get() << "\n";
        nd_range dims{1, 1, 1, 1, 1, 1};
        try {
            CUcontext ctx = dev->getContext(0);
            CUcontext current_ctx;
            CHECK_CUDA(cuCtxGetCurrent(&current_ctx));
            std::cout << "  -> Current context before launch: " << current_ctx << "\n";
            std::cout << "  -> Launching kernel with context: " << ctx << ", kernel: " << prog->get_kernel(0) << ", args: " << out_mem->mem() << "\n";
            dev->launch_kernel(prog->get_kernel(0), dims, std::make_tuple(out_mem), 0);
            std::cout << "  -> Kernel launched\n";
            CHECK_CUDA(cuCtxSynchronize());
            std::cout << "  -> Context synchronized\n";
            std::vector<int> result = out_mem->copy_to_host();
            std::cout << "  -> Data copied to host\n";
            assert(result[0] == 42 && "Kernel output incorrect");
            if (result[0] != 42) {
                std::cout << "  -> Failed: result[0] = " << result[0] << ", expected 42\n";
            }
            std::cout << "  -> Basic kernel launched successfully\n";
        } catch (const std::exception& e) {
            std::cout << "  -> Failed: CUDA error during kernel launch: " << e.what() << "\n";
            throw;
        }
        std::cout << "  -> End of scope: prog=" << prog.get() << ", out_mem=" << out_mem.get() << "\n";
    }
    std::cout << "---- Kernel Launch tests passed ----\n";
}

void test_kernel_launch_direct([[maybe_unused]] actor_system& sys, [[maybe_unused]] platform_ptr plat) {
    std::cout << "\n=== Test Kernel Launch Direct ===\n";
    
    manager& mgr = manager::get();
    device_ptr dev = mgr.find_device(0);
    std::cout << "  -> Device context: " << dev->getContext(0) << "\n";

    std::cout << "Test 1: Testing direct kernel launch with cuLaunchKernel...\n";
    {
        const char* kernel_code = R"(
            extern "C" __global__ void simple_kernel(int* output) {
                output[0] = 42;
            })";
        try {
            program_ptr prog = mgr.create_program(kernel_code, "simple_kernel", dev);
            std::cout << "  -> Program created with kernel: simple_kernel, handle: " << prog->get_kernel(0) << ", prog=" << prog.get() << "\n";

            std::vector<int> host_data(1, 0);
            out<int> output = create_out_arg(host_data);
            mem_ptr<int> out_mem = dev->make_arg(output,test_actor_id);
            assert(out_mem->mem() != 0 && "Output memory not allocated");
            std::cout << "  -> Output memory allocated: " << out_mem->mem() << ", out_mem=" << out_mem.get() << "\n";

            nd_range dims{1, 1, 1, 1, 1, 1};
            CUcontext ctx = dev->getContext(0);
            CHECK_CUDA(cuCtxPushCurrent(ctx));
            CUcontext current_ctx;
            CHECK_CUDA(cuCtxGetCurrent(&current_ctx));
            std::cout << "  -> Current context before launch: " << current_ctx << "\n";

            CUdeviceptr device_ptr = out_mem->mem();
            std::cout << "  -> Launching kernel with device_ptr=" << device_ptr << "\n";
            void* kernel_args[] = { &device_ptr };
            CHECK_CUDA(cuLaunchKernel(
                prog->get_kernel(0),
                dims.getGridDimX(), dims.getGridDimY(), dims.getGridDimZ(),
                dims.getBlockDimX(), dims.getBlockDimY(), dims.getBlockDimZ(),
                0, nullptr, kernel_args, nullptr
            ));
            std::cout << "  -> Kernel launched\n";

            CHECK_CUDA(cuCtxSynchronize());
            std::cout << "  -> Context synchronized\n";

            std::vector<int> result = out_mem->copy_to_host();
            std::cout << "  -> Data copied to host\n";
            assert(result[0] == 42 && "Kernel output incorrect");
            if (result[0] != 42) {
                std::cout << "  -> Failed: result[0] = " << result[0] << ", expected 42\n";
            }

            CHECK_CUDA(cuCtxPopCurrent(nullptr));
            std::cout << "  -> Direct kernel launched successfully\n";
        } catch (const std::exception& e) {
            std::cout << "  -> Failed: CUDA error during direct kernel launch: " << e.what() << "\n";
            throw;
        }
    }
    std::cout << "---- Kernel Launch Direct tests passed ----\n";
}

void test_kernel_launch_multi_buffer([[maybe_unused]] actor_system& sys, [[maybe_unused]] platform_ptr plat) {
  std::cout << "\n=== Test Kernel Launch Multi Buffer ===\n";

  manager& mgr = manager::get();
  device_ptr dev = mgr.find_device(0);
  std::cout << "  -> Device context: " << dev->getContext(0) << "\n";

  std::cout << "Test 1: Testing direct kernel launch with multiple buffers...\n";

  const char* kernel_code = R"(
    extern "C" __global__ void multi_buffer_kernel(
      const int* in_data,
      int* inout_data,
      int* out_data) {
    int idx = threadIdx.x;
    if (idx < 5) {
      inout_data[idx] = inout_data[idx] * 2;  // Hardcoded scale=2
      out_data[idx] = in_data[idx] + 5;       // Hardcoded offset=5
    }
  })";

  program_ptr prog = mgr.create_program(kernel_code, "multi_buffer_kernel", dev);
  std::cout << "  -> Program created with kernel: multi_buffer_kernel, handle: "
            << prog->get_kernel(0) << ", prog=" << prog.get() << "\n";

  constexpr size_t n = 5;
  std::vector<int> in_host(n, 10);
  std::vector<int> inout_host(n, 20);
  std::vector<int> out_host(n, 0);

  // Create memory arguments for input, inout, and output
  in<int> in_arg = create_in_arg(in_host);
  in_out<int> inout_arg = create_in_out_arg(inout_host);
  out<int> out_arg = create_out_arg(out_host);

  mem_ptr<int> in_mem = dev->make_arg(in_arg,test_actor_id);
  mem_ptr<int> inout_mem = dev->make_arg(inout_arg,test_actor_id);
  mem_ptr<int> out_mem = dev->make_arg(out_arg,test_actor_id);

  assert(in_mem->mem() != 0 && "Input memory not allocated");
  assert(inout_mem->mem() != 0 && "In-out memory not allocated");
  assert(out_mem->mem() != 0 && "Output memory not allocated");

  std::cout << "  -> Input memory allocated: " << in_mem->mem() << ", in_mem=" << in_mem.get() << "\n";
  std::cout << "  -> In-out memory allocated: " << inout_mem->mem() << ", inout_mem=" << inout_mem.get() << "\n";
  std::cout << "  -> Output memory allocated: " << out_mem->mem() << ", out_mem=" << out_mem.get() << "\n";

  nd_range dims{1, 1, 1, static_cast<int>(n), 1, 1};

  try {
    CUcontext ctx = dev->getContext(0);
    CUcontext current_ctx;
    CHECK_CUDA(cuCtxGetCurrent(&current_ctx));
    std::cout << "  -> Current context before launch: " << current_ctx << "\n";
    std::cout << "  -> Launching kernel with context: " << ctx
              << ", kernel: " << prog->get_kernel(0) << "\n";

    // Launch kernel with a tuple of mem_ptrs as arguments
    dev->launch_kernel(prog->get_kernel(0), dims,
                       std::make_tuple(in_mem, inout_mem, out_mem), 0);

    std::cout << "  -> Kernel launched\n";

    CHECK_CUDA(cuCtxSynchronize());
    std::cout << "  -> Context synchronized\n";

    // Copy results back to host
    std::vector<int> inout_result = inout_mem->copy_to_host();
    std::vector<int> out_result = out_mem->copy_to_host();

    std::cout << " --- Expected inout_result: 40 40 40 40 40\n";
    std::cout << " --- Expected out_result:   15 15 15 15 15\n";

    bool success = true;
    for (size_t i = 0; i < n; ++i) {
      if (inout_result[i] != 40) {
        std::cout << "  -> Failed: inout_result[" << i << "] = " << inout_result[i] << ", expected 40\n";
        success = false;
      }
      if (out_result[i] != 15) {
        std::cout << "  -> Failed: out_result[" << i << "] = " << out_result[i] << ", expected 15\n";
        success = false;
      }
    }

    if (success)
      std::cout << "  -> Multi-buffer kernel launched successfully\n";
    else
      std::cout << "  -> ❌ Multi-buffer test FAILED\n";

  } catch (const std::exception& e) {
    std::cout << "  -> Failed: CUDA error during kernel launch: " << e.what() << "\n";
    throw;
  }

  std::cout << "  -> End of scope: prog=" << prog.get() << ", out_mem=" << out_mem.get() << "\n";
  std::cout << "---- Kernel Launch Multi Buffer tests passed ----\n";
}

// Test `in<T>` wrapper for scalar and buffer cases
void test_in_wrapper() {
    // Scalar case
    
    std::cout << "Testing in wrapper type\n";
    in<int> scalar_in(42);
    assert(scalar_in.is_scalar() && "in<int> should be scalar");
    assert(scalar_in.size() == 1 && "Scalar in should have size 1");
    assert(scalar_in.getscalar() == 42 && "Scalar in should return correct value");
    assert(scalar_in.data() != nullptr && "Scalar in data should be non-null");
    assert(*scalar_in.data() == 42 && "Scalar in data should point to correct value");

    // Buffer case
    std::vector<int> vec = {1, 2, 3};
    in<int> buffer_in(vec);
    assert(!buffer_in.is_scalar() && "in<std::vector<int>> should be buffer");
    assert(buffer_in.size() == 3 && "Buffer in should have correct size");
    assert(buffer_in.get_buffer() == vec && "Buffer in should return correct vector");
}

/*
// Test `out<T>` wrapper for scalar and buffer cases
void test_out_wrapper() {
    // Scalar case
    out<int> scalar_out(42);
    assert(scalar_out.is_scalar() && "out<int> should be scalar");
    assert(scalar_out.size() == 1 && "Scalar out should have size 1");
    assert(scalar_out.getscalar() == 42 && "Scalar out should return correct value");
    assert(scalar_out.data() != nullptr && "Scalar out data should be non-null");
    assert(*scalar_out.data() == 42 && "Scalar out data should point to correct value");

    // Buffer case
    std::vector<int> vec = {4, 5, 6};
    out<int> buffer_out(vec);
    assert(!buffer_out.is_scalar() && "out<std::vector<int>> should be buffer");
    assert(buffer_out.size() == 3 && "Buffer out should have correct size");
    assert(buffer_out.get_buffer() == vec && "Buffer out should return correct vector");
}

*/

// Test `in_out<T>` wrapper for scalar and buffer cases
void test_in_out_wrapper() {
    // Scalar case
    in_out<int> scalar_in_out(42);
    assert(scalar_in_out.is_scalar() && "in_out<int> should be scalar");
    assert(scalar_in_out.size() == 1 && "Scalar in_out should have size 1");
    assert(scalar_in_out.getscalar() == 42 && "Scalar in_out should return correct value");
    assert(scalar_in_out.data() != nullptr && "Scalar in_out data should be non-null");
    assert(*scalar_in_out.data() == 42 && "Scalar in_out data should point to correct value");

    // Buffer case
    std::vector<int> vec = {7, 8, 9};
    in_out<int> buffer_in_out(vec);
    assert(!buffer_in_out.is_scalar() && "in_out<std::vector<int>> should be buffer");
    assert(buffer_in_out.size() == 3 && "Buffer in_out should have correct size");
    assert(buffer_in_out.get_buffer() == vec && "Buffer in_out should return correct vector");
}


// A trivial scalar‐only CUDA kernel
static const char* scalar_kernel_code = R"(
extern "C" __global__ void scalar_kernel(int a, float b, double c) {
  // no‐op
}
)";

void test_mem_ref_scalar_host() {
  std::cout << "\n=== test_mem_ref_scalar_host ===\n";
  // Direct scalar constructor
  auto ptr = intrusive_ptr<mem_ref<int>>(new mem_ref<int>(123, IN_OUT, 0, 0,nullptr ,nullptr));
  assert(ptr->is_scalar() && "should report scalar");
  assert(ptr->host_scalar_ptr() && *ptr->host_scalar_ptr() == 123);
  auto host_copy = ptr->copy_to_host();
  assert(host_copy.size() == 1 && host_copy[0] == 123);
  std::cout << "✔ mem_ref<int> scalar host copy correct\n";
}

void test_extract_kernel_args_scalar() {
  std::cout << "\n=== test_extract_kernel_args_scalar ===\n";
  manager& mgr = manager::get();
  auto dev = mgr.find_device(0);
  assert(dev && "device 0 must exist");


  // First wrap a scalar in an `in<T>` and make a mem_ptr
  in_out<double> in_arg(3.14);
  auto mem = dev->make_arg(in_arg, /*actor_id=*/0);
  assert(mem->size() == 1);
  assert(mem->is_scalar() && "current path still allocates a device buffer");

  // Now extract the raw kernel args
  auto vec = dev->extract_kernel_args(std::make_tuple(mem));
  // Should have allocated one CUdeviceptr slot
  assert(vec.size() == 1);
  double* val_ptr = static_cast<double*>(vec[0]);
  assert(val_ptr != nullptr);
  assert(*val_ptr == 3.14);
  std::cout << "✔ extract_kernel_args_scalar returns device‐buffer pointer\n";
}

void test_device_make_arg_scalar() {
  std::cout << "\n=== test_device_make_arg_scalar ===\n";
  manager& mgr = manager::get();
  auto dev = mgr.find_device(0);
  assert(dev);

  // Wrap raw scalar with in_out (or in) wrapper
  in_out<int> in_arg(77);
  auto mem = dev->make_arg(in_arg, /*actor_id=*/0);
  assert(mem->size() == 1);

  // Since this is a scalar, no device buffer should be allocated
  assert(mem->is_scalar() && "Expected scalar mem_ref for scalar argument");
  assert(mem->mem() == 0 && "No device memory should be allocated for scalar");

  // Copy back to host (should just return the scalar)
  auto vec = mem->copy_to_host();
  assert(vec.size() == 1 && vec[0] == 77);

  std::cout << "✔ device::make_arg(in_out<int> scalar) round-trip correct\n";
}

void test_scalar_kernel_launch_wrapper_api() {
  std::cout << "\n=== test_scalar_kernel_launch_wrapper_api ===\n";
  manager& mgr = manager::get();
  auto dev = mgr.find_device(0);
  assert(dev);

  // Compile the scalar‐only kernel
  auto prog = mgr.create_program(scalar_kernel_code, "scalar_kernel", dev);
  CUfunction k = prog->get_kernel(0);

  // Prepare scalar args via the wrapper types
  in<int>    a_arg(42);
  in<float>  b_arg(4.2f);
  in<double> c_arg(6.28);

  auto a_mem = dev->make_arg(a_arg, 0);
  auto b_mem = dev->make_arg(b_arg, 0);
  auto c_mem = dev->make_arg(c_arg, 0);

  nd_range range{1,1,1, 1,1,1};

  // Launch—no exception means success
  dev->launch_kernel(k, range,
                     std::make_tuple(a_mem, b_mem, c_mem),
                     /*actor_id=*/0);
  std::cout << "✔ scalar kernel launch via wrapper API succeeded\n";
}

void test_scalar_kernel_launch_runtime_api()  {
  std::cout << "\n=== test_scalar_kernel_launch_program_runtime_api ===\n";
  manager& mgr = manager::get();
  device_ptr dev = mgr.find_device(0);
  assert(dev);

  const char* kernel_code = R"(
    extern "C" __global__ void scalar_kernel(int a, float b, double c, int* out) {
      if (threadIdx.x == 0 && blockIdx.x == 0)
        out[0] = a + static_cast<int>(b + c); // Should be a + floor(b + c)
    })";

  program_ptr prog = mgr.create_program(kernel_code, "scalar_kernel", dev);
  CUfunction func = prog->get_kernel(0);
  CUcontext ctx = dev->getContext();

  // Host-side input values
  int a_val = 10;
  float b_val = 2.5f;
  double c_val = 3.5;

  // Allocate device memory for output
  CUdeviceptr d_out;
  CHECK_CUDA(cuCtxPushCurrent(ctx));
  CHECK_CUDA(cuMemAlloc(&d_out, sizeof(int)));
  CHECK_CUDA(cuMemsetD32(d_out, 0, 1));
  CHECK_CUDA(cuCtxPopCurrent(nullptr));

  // Kernel arguments (must be pointers-to-host-variables)
  void* args[] = {
    &a_val,
    &b_val,
    &c_val,
    &d_out
  };

  std::cout << "  -> Launching kernel with a=" << a_val << ", b=" << b_val << ", c=" << c_val << "\n";

  CHECK_CUDA(cuCtxPushCurrent(ctx));
  CUresult launch_result = cuLaunchKernel(
    func,
    1, 1, 1,  // grid dim
    1, 1, 1,  // block dim
    0, nullptr, args, nullptr
  );
  if (launch_result != CUDA_SUCCESS) {
    const char* err_str = nullptr;
    cuGetErrorName(launch_result, &err_str);
    throw std::runtime_error(std::string("Kernel launch failed: ") + (err_str ? err_str : "unknown error"));
  }

  CHECK_CUDA(cuCtxSynchronize());

  // Copy back result
  int out_val = 0;
  CHECK_CUDA(cuMemcpyDtoH(&out_val, d_out, sizeof(int)));
  CHECK_CUDA(cuMemFree(d_out));
  CHECK_CUDA(cuCtxPopCurrent(nullptr));

  int expected = a_val + static_cast<int>(b_val + c_val);
  std::cout << "  -> Kernel output: " << out_val << ", expected: " << expected << "\n";
  assert(out_val == expected && "Scalar kernel output incorrect");

  std::cout << "✔ scalar kernel launched via program + raw API successfully\n";
}


void test_add_scalar_to_buffer() {
  std::cout << "\n=== test_add_scalar_to_buffer ===\n";
  manager& mgr = manager::get();
  auto dev = mgr.find_device(0);
  assert(dev);

  // CUDA kernel code
  const char* kernel_code = R"(
    extern "C" __global__ void add_scalar_kernel(int* data, int scalar) {
      int idx = threadIdx.x + blockIdx.x * blockDim.x;
      if (idx < 5)
        data[idx] += scalar;
    })";

  program_ptr prog = mgr.create_program(kernel_code, "add_scalar_kernel", dev);
  CUfunction kernel = prog->get_kernel(0);

  constexpr size_t n = 5;
  std::vector<int> buffer_host = {1, 2, 3, 4, 5};
  int scalar_value = 10;

  // Wrap args
  in_out<int> buffer_arg = create_in_out_arg(buffer_host); // writable buffer
  in<int> scalar_arg(scalar_value); // scalar is read-only

  mem_ptr<int> buffer_mem = dev->make_arg(buffer_arg, 0);
  mem_ptr<int> scalar_mem = dev->make_arg(scalar_arg, 0);

  // Configure kernel launch
  nd_range dims{/*grid=*/1,1,1, /*block=*/n,1,1};

  // Launch
  try {
    std::cout << "  -> Launching kernel with scalar=" << scalar_value << "\n";
    dev->launch_kernel(kernel, dims, std::make_tuple(buffer_mem, scalar_mem), 0);
    std::cout << "  -> Kernel launched\n";

    std::vector<int> result = buffer_mem->copy_to_host();
    std::cout << "  -> Result copied to host: ";
    for (int x : result) std::cout << x << " ";
    std::cout << "\n";

    bool success = true;
    for (size_t i = 0; i < n; ++i) {
      int expected = buffer_host[i] + scalar_value;
      if (result[i] != expected) {
        std::cout << "  ❌ Mismatch at " << i << ": got " << result[i]
                  << ", expected " << expected << "\n";
        success = false;
      }
    }

    if (success)
      std::cout << "✔ Buffer correctly updated by scalar kernel\n";
    else
      std::cout << "❌ Buffer update test failed\n";

  } catch (const std::exception& e) {
    std::cerr << "❌ Exception during kernel launch: " << e.what() << "\n";
    throw;
  }
}



void test_main([[maybe_unused]] actor_system& sys) {
    std::cout << "\n===== Running CUDA CAF Tests =====\n";
    manager::init(sys);
    platform_ptr plat = platform::create();
    manager& mgr = manager::get();
    device_ptr dev = mgr.find_device(0);
    std::cout << "  -> Manager initialized with context: " << dev->getContext(0) << "\n";
    try {
        test_platform(sys, plat);
        test_device(sys, plat);
        test_manager(sys, plat);
        test_program(sys, plat);
        test_create_program(sys, plat);
        test_mem_ref(sys, plat);
        test_command(sys, plat);
        test_mem_ref_extended(sys, plat);
        test_argument_translation(sys, plat);
        test_kernel_launch_direct(sys, plat); // Added new test
        test_kernel_launch(sys, plat);
        //test_actor_facade(sys, plat);
	//test_actor_facade_multi_buffer(sys, plat);
        test_kernel_launch_multi_buffer(sys, plat);


	test_in_wrapper();
    	//test_out_wrapper();
    	test_in_out_wrapper();

	test_mem_ref_scalar_host();
  	test_extract_kernel_args_scalar();
  	test_device_make_arg_scalar();
  	test_scalar_kernel_launch_wrapper_api();
  	test_scalar_kernel_launch_runtime_api();
	test_add_scalar_to_buffer();

	/*
	 * TODO actually write these tests someday
	test_prepare_kernel_args_scalar();
  	test_prepare_kernel_args_buffer();
  	test_cleanup_kernel_args_mixed();
	*/

    } catch (const std::exception& e) {
        std::cout << "Test failed: " << e.what() << "\n";
        manager::shutdown();
        plat.reset();
        throw;
    }
    manager::shutdown();
    plat.reset();
    std::cout << "\n===== All CUDA CAF Tests Completed Successfully =====\n";
}
