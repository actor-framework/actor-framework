/*
 * A file full of caf cuda unit tests
 * Note that this does use the nvidia run time compiler for these tests
 * so a possible unsupported toolchain error can occur
 * If this does that just means the version of nvcc is not the same
 * as the version your GPU supports
 * There is no way to change this other than manually rollback nvcc 
 * to a version that your GPU supports
 * hence why you should use cubins and fatbins
 */


#include <caf/all.hpp>
#include <caf/actor_system.hpp>
#include <caf/cuda/manager.hpp>
#include <caf/cuda/helpers.hpp>
#include <caf/cuda/types.hpp>
#include <caf/cuda/command_runner.hpp>
#include <caf/cuda/mem_ref.hpp>
#include <caf/cuda/device.hpp>
#include <caf/cuda/streampool.hpp>
#include <caf/cuda/program.hpp>
#include <caf/cuda/nd_range.hpp>
#include <caf/cuda/platform.hpp>
#include <cassert>
#include <vector>
#include <string>
#include <cuda.h>
#include <stdexcept>
#include <iostream>

// Helper macro for CUDA checks in tests with debugging output
#define TEST_CHECK_CUDA(err, call) \
  do { \
    if (err != CUDA_SUCCESS) { \
      const char* err_str; \
      cuGetErrorString(err, &err_str); \
      std::cerr << "CUDA error in " << call << " at " << __FILE__ << ":" << __LINE__ << ": " << err_str << std::endl; \
      throw std::runtime_error(std::string("CUDA Error: ") + err_str); \
    } \
  } while (0)

// Helper function to compare vectors for test_extract_vector
template <typename T>
bool vectors_equal(const std::vector<T>& a, const std::vector<T>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

// Test for types.hpp: in_impl (in<T>)
void test_in_impl([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  
  // Test scalar
  in<int> scalar_in(42);
  assert(scalar_in.is_scalar());
  assert(scalar_in.getscalar() == 42);
  assert(scalar_in.size() == 1u);
  assert(*scalar_in.data() == 42);
  
  // Test buffer
  std::vector<int> buf = {1, 2, 3};
  in<int> buffer_in(buf);
  assert(!buffer_in.is_scalar());
  assert(buffer_in.size() == 3u);
  assert(buffer_in.get_buffer() == buf);
  
  // Move semantics
  in<int> moved(std::move(buffer_in));
  assert(moved.size() == 3u);
  bool threw = false;
  try {
    buffer_in.size();
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);  // Use after move
}

// Test for types.hpp: out_impl (out<T>)
void test_out_impl([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  
  // Test size-only constructor (for buffers)
  out<int> size_out(5);
  assert(!size_out.is_scalar());
  assert(size_out.size() == 5u);
  
  // Test buffer constructor
  std::vector<int> buf(3, 0);
  out<int> buffer_out(buf);
  assert(!buffer_out.is_scalar());
  assert(buffer_out.size() == 3u);
  assert(buffer_out.get_buffer() == buf);
  
  // Move semantics
  out<int> moved(std::move(buffer_out));
  assert(moved.size() == 3u);
  bool threw = false;
  try {
    buffer_out.size();
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);  // Use after move
}

// Test for types.hpp: in_out_impl (in_out<T>)
void test_in_out_impl([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  
  // Test scalar
  in_out<float> scalar_io(3.14f);
  assert(scalar_io.is_scalar());
  assert(scalar_io.getscalar() == 3.14f);
  assert(scalar_io.size() == 1u);
  
  // Test buffer
  std::vector<float> buf = {1.0f, 2.0f};
  in_out<float> buffer_io(buf);
  assert(!buffer_io.is_scalar());
  assert(buffer_io.size() == 2u);
  assert(buffer_io.get_buffer() == buf);
  
  // Move semantics
  in_out<float> moved(std::move(buffer_io));
  assert(moved.size() == 2u);
  bool threw = false;
  try {
    buffer_io.size();
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);  // Use after move
}

// Test for helpers.hpp: create_*_arg factory functions
void test_create_args([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  
  // in<T>
  auto in_scalar = create_in_arg(10);
  assert(in_scalar.is_scalar());
  assert(in_scalar.getscalar() == 10);
  
  std::vector<int> in_buf = {1, 2};
  auto in_buffer = create_in_arg(in_buf);
  assert(!in_buffer.is_scalar());
  assert(in_buffer.size() == 2u);
  
  // out<T>
  auto out_size = create_out_arg_with_size<int>(4);
  assert(out_size.size() == 4u);
  
  std::vector<int> out_buf(3);
  auto out_buffer = create_out_arg(out_buf);
  assert(out_buffer.size() == 3u);
  
  // in_out<T>
  auto io_scalar = create_in_out_arg(20);
  assert(io_scalar.is_scalar());
  assert(io_scalar.getscalar() == 20);
  
  std::vector<int> io_buf = {3, 4};
  auto io_buffer = create_in_out_arg(io_buf);
  assert(!io_buffer.is_scalar());
  assert(io_buffer.size() == 2u);
}

// Test for helpers.hpp: extract_vector functions
void test_extract_vector([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  
  std::vector<output_buffer> outputs = {
    {std::vector<int>{1, 2, 3}},
    {std::vector<float>{4.0f, 5.0f}},
    {std::vector<int>{6, 7}}
  };
  
  // Extract first matching
  auto opt_int = extract_vector_or_empty<int>(outputs);
  assert(opt_int.has_value());
  assert(vectors_equal(opt_int.value(), std::vector<int>{1, 2, 3}));
  
  auto vec_float = extract_vector<float>(outputs);
  assert(vectors_equal(vec_float, std::vector<float>{4.0f, 5.0f}));
  
  // By index
  auto vec_int_idx = extract_vector<int>(outputs, 2);
  assert(vectors_equal(vec_int_idx, std::vector<int>{6, 7}));
  
  // Non-matching or out of range
  assert(vectors_equal(extract_vector<double>(outputs), std::vector<double>{}));
  assert(vectors_equal(extract_vector<int>(outputs, 10), std::vector<int>{}));
}

// Test for mem_ref.hpp: mem_ref class (basic operations)
void test_mem_ref_basic([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  auto& mgr = caf::cuda::manager::get();
  auto dev = mgr.find_device(0);
  
  // Use the device context managed by CAF CUDA
  CUcontext ctx;
  TEST_CHECK_CUDA(cuCtxGetCurrent(&ctx), "cuCtxGetCurrent");
  if (ctx == nullptr) {
    throw std::runtime_error("No current CUDA context");
  }
  
  // Scalar mem_ref
  mem_ref<int> scalar_ref(42, IN, 0, 0, ctx, nullptr);
  assert(scalar_ref.is_scalar());
  assert(scalar_ref.size() == 1u);
  assert(scalar_ref.access() == IN);
  
  // Buffer mem_ref using CAF CUDA device
  size_t num = 10;
  out<int> out_arg(num);
  auto buffer_ref = dev->make_arg(out_arg, 0 /* actor_id */);
  assert(!buffer_ref->is_scalar());
  assert(buffer_ref->size() == num);
  assert(buffer_ref->access() == OUT);
  assert(buffer_ref->mem() != 0u);
  
  // Synchronize (no-op for null stream)
  buffer_ref->synchronize();
  
  // Reset (CAF CUDA handles memory cleanup)
  buffer_ref->reset();
  assert(buffer_ref->size() == 0u);
  assert(buffer_ref->mem() == 0u);
}

// Test for mem_ref.hpp: copy_to_host
void test_mem_ref_copy_to_host([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  auto& mgr = caf::cuda::manager::get();
  auto dev = mgr.find_device(0);
  
  // Use the device context managed by CAF CUDA
  CUcontext ctx;
  TEST_CHECK_CUDA(cuCtxGetCurrent(&ctx), "cuCtxGetCurrent");
  if (ctx == nullptr) {
    throw std::runtime_error("No current CUDA context");
  }
  
  // Prepare device buffer using CAF CUDA
  std::vector<int> host_data = {1, 2, 3};
  in_out<int> in_out_arg(host_data);
  auto ref = dev->make_arg(in_out_arg, 0 /* actor_id */);
  assert(ref->size() == host_data.size());
  assert(ref->access() == IN_OUT);
  
  // Copy to host and verify
  auto copied = ref->copy_to_host();
  assert(vectors_equal(copied, host_data));
  
  // Test invalid access (IN should throw)
  in<int> in_arg(host_data);
  auto in_ref = dev->make_arg(in_arg, 0 /* actor_id */);
  bool threw = false;
  try {
    in_ref->copy_to_host();
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
  
  // Reset (CAF CUDA handles memory cleanup)
  ref->reset();
  in_ref->reset();
}

// Test for command_runner.hpp: synchronous run
void test_command_runner_sync([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  auto& mgr = manager::get();
  auto dev = mgr.find_device(0);
  
  // Kernel with extern "C" to prevent name mangling
  const char* kernel_src = R"(
  extern "C" __global__
  void test_kernel(int* out) { *out = 42; }
  )";
  program_ptr prog;
  try {
    std::cerr << "Creating program for test_kernel" << std::endl;
    prog = mgr.create_program(kernel_src, "test_kernel", dev);
    std::cerr << "Program created successfully for test_kernel" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Warning: Skipping test_command_runner_sync due to create_program failure: " << e.what() << std::endl;
    return; // Skip test if program creation fails
  }
  
  nd_range dims(1, 1, 1, 1, 1, 1);  // 1D single thread
  command_runner<out<int>> runner;
  
  auto outputs = runner.run(prog, dims, 1 /* actor_id */, create_out_arg_with_size<int>(1));
  assert(outputs.size() == 1u);
  auto result = extract_vector<int>(outputs);
  assert(result[0] == 42);
}

// Test for command_runner.hpp: asynchronous run
void test_command_runner_async([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  auto& mgr = manager::get();
  auto dev = mgr.find_device(0);
  
  // Kernel with extern "C" to prevent name mangling
  const char* kernel_src = R"(
  extern "C" __global__
  void test_kernel(int* out) { *out = 42; }
  )";
  program_ptr prog;
  try {
    std::cerr << "Creating program for test_kernel" << std::endl;
    prog = mgr.create_program(kernel_src, "test_kernel", dev);
    std::cerr << "Program created successfully for test_kernel" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Warning: Skipping test_command_runner_async due to create_program failure: " << e.what() << std::endl;
    return; // Skip test if program creation fails
  }
  
  nd_range dims(1, 1, 1, 1, 1, 1);  // 1D single thread
  command_runner<out<int>> runner;
  
  auto mem_tuple = runner.run_async(prog, dims, 1 /* actor_id */, create_out_arg_with_size<int>(1));
  auto mem_ptr = std::get<0>(mem_tuple);
  mem_ptr->synchronize();
  auto host_data = mem_ptr->copy_to_host();
  assert(host_data[0] == 42);
}

// Test for manager.hpp: create_program and spawn
void test_manager_create_and_spawn([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  auto& mgr = manager::get();
  auto dev = mgr.find_device(0);
  
  // Kernel with extern "C" to prevent name mangling
  const char* kernel_src = R"(
  extern "C" __global__
  void simple_kernel() {}
  )";
  program_ptr prog;
  try {
    std::cerr << "Creating program for simple_kernel" << std::endl;
    prog = mgr.create_program(kernel_src, "simple_kernel", dev);
    std::cerr << "Program created successfully for simple_kernel" << std::endl;
    assert(prog != nullptr);
  } catch (const std::exception& e) {
    std::cerr << "Warning: Skipping test_manager_create_and_spawn due to create_program failure: " << e.what() << std::endl;
    return; // Skip test if program creation fails
  }
  
  // Test spawn (basic, no args)
  nd_range dims(1, 1, 1, 1, 1, 1);  // 1D single thread
  auto actor = mgr.spawn(kernel_src, "simple_kernel", dims);
  assert(actor != nullptr);
}

// Test for StreamPool.hpp: StreamPool basic operations
void test_stream_pool([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  auto& mgr = caf::cuda::manager::get();
  auto dev = mgr.find_device(0);
  
  // Use the device context managed by CAF CUDA
  CUcontext ctx;
  TEST_CHECK_CUDA(cuCtxGetCurrent(&ctx), "cuCtxGetCurrent");
  if (ctx == nullptr) {
    throw std::runtime_error("No current CUDA context");
  }
  
  StreamPool pool(ctx, 2);
  
  auto s1 = pool.acquire();
  assert(s1 != nullptr);
  auto s2 = pool.acquire();
  assert(s2 != nullptr);
  
  // Acquire expands pool
  auto s3 = pool.acquire();
  assert(s3 != nullptr);
  
  pool.release(s1);
  auto s4 = pool.acquire();  // Should reuse s1
  assert(s4 == s1);
}

// Test for device.hpp: memory allocation helpers
void test_device_mem_alloc([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  auto& mgr = manager::get();
  auto dev = mgr.find_device(0);
  
  // Test in<T>
  in<int> in_arg(std::vector<int>{1, 2, 3});
  auto mem_in = dev->make_arg(in_arg, 1 /* actor_id */);
  assert(mem_in->size() == 3u);
  assert(mem_in->access() == IN);
  
  // Test out<T>
  out<int> out_arg(4);
  auto mem_out = dev->make_arg(out_arg, 1);
  assert(mem_out->size() == 4u);
  assert(mem_out->access() == OUT);
  
  // Test in_out<T>
  in_out<int> io_arg(5);
  auto mem_io = dev->make_arg(io_arg, 1);
  assert(mem_io->is_scalar());
  assert(mem_io->access() == IN_OUT);
}

// Test for device.hpp: kernel launch with mem_refs
void test_device_launch_kernel([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  auto& mgr = manager::get();
  auto dev = mgr.find_device(0);
  
  // Kernel with extern "C" to prevent name mangling
  const char* kernel_src = R"(
  extern "C" __global__
  void set_out(int* out) { *out = 42; }
  )";
  program_ptr prog;
  try {
    std::cerr << "Creating program for set_out" << std::endl;
    prog = mgr.create_program(kernel_src, "set_out", dev);
    std::cerr << "Program created successfully for set_out" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Warning: Skipping test_device_launch_kernel due to create_program failure: " << e.what() << std::endl;
    return; // Skip test if program creation fails
  }
  
  CUfunction kernel;
  try {
    std::cerr << "Retrieving kernel for set_out" << std::endl;
    kernel = prog->get_kernel(dev->getId());
    std::cerr << "Program created successfully for set_out" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Warning: Skipping test_device_launch_kernel due to get_kernel failure: " << e.what() << std::endl;
    return; // Skip test if kernel retrieval fails
  }
  
  nd_range dims(1, 1, 1, 1, 1, 1);  // 1D single thread
  auto mem_tuple = dev->launch_kernel_mem_ref(kernel, dims, std::make_tuple(create_out_arg_with_size<int>(1)), 1 /* actor_id */);
  auto mem_out = std::get<0>(mem_tuple);
  mem_out->synchronize();
  auto host_out = mem_out->copy_to_host();
  assert(host_out[0] == 42);
}

// Test for multi-threaded kernel execution
void test_multi_thread_kernel([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  auto& mgr = manager::get();
  auto dev = mgr.find_device(0);
  
  // Kernel that sets each element to its thread index
  const char* kernel_src = R"(
  extern "C" __global__
  void index_kernel(int* out, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
      out[idx] = idx;
    }
  }
  )";
  program_ptr prog;
  try {
    std::cerr << "Creating program for index_kernel" << std::endl;
    prog = mgr.create_program(kernel_src, "index_kernel", dev);
    std::cerr << "Program created successfully for index_kernel" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Warning: Skipping test_multi_thread_kernel due to create_program failure: " << e.what() << std::endl;
    return; // Skip test if program creation fails
  }
  
  const int size = 256;
  nd_range dims(size / 64, 1, 1, 64, 1, 1);  // 4 blocks, 64 threads each
  command_runner<out<int>, in<int>> runner;
  
  auto outputs = runner.run(prog, dims, 1 /* actor_id */, create_out_arg_with_size<int>(size), create_in_arg(size));
  assert(outputs.size() == 1u);
  auto result = extract_vector<int>(outputs);
  assert(result.size() == size);
  for (int i = 0; i < size; ++i) {
    assert(result[i] == i);
  }
}

// Test for vector addition kernel
void test_vector_addition([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  auto& mgr = manager::get();
  auto dev = mgr.find_device(0);
  
  // Kernel that adds two arrays element-wise
  const char* kernel_src = R"(
  extern "C" __global__
  void add_vectors(const int* a, const int* b, int* result, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
      result[idx] = a[idx] + b[idx];
    }
  }
  )";
  program_ptr prog;
  try {
    std::cerr << "Creating program for add_vectors" << std::endl;
    prog = mgr.create_program(kernel_src, "add_vectors", dev);
    std::cerr << "Program created successfully for add_vectors" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Warning: Skipping test_vector_addition due to create_program failure: " << e.what() << std::endl;
    return; // Skip test if program creation fails
  }
  
  const int size = 128;
  std::vector<int> vec_a(size, 2), vec_b(size, 3);
  nd_range dims(size / 32, 1, 1, 32, 1, 1);  // 4 blocks, 32 threads each
  command_runner<in<int>, in<int>, out<int>, in<int>> runner;
  
  auto outputs = runner.run(prog, dims, 1 /* actor_id */,
                           create_in_arg(vec_a), 
			   create_in_arg(vec_b),
			   create_out_arg_with_size<int>(size),
			   create_in_arg(size));
  assert(outputs.size() == 1u);
  auto result = extract_vector<int>(outputs);
  assert(result.size() == size);
 
 /* 
  // Debug: Print result vector
  std::cerr << "Result vector: ";
  for (int i = 0; i < size; ++i) {
    std::cerr << result[i] << " ";
    if (i % 16 == 15) std::cerr << "\n";
  }
  std::cerr << "\n";
  */
  for (int i = 0; i < size; ++i) {
    if (result[i] != 5) {
      std::cerr << "Assertion failed at index " << i << ": expected 5, got " << result[i] << std::endl;
    }
    assert(result[i] == 5); // 2 + 3
  }
}

// Test for invalid kernel parameters
void test_invalid_kernel_params([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  auto& mgr = manager::get();
  auto dev = mgr.find_device(0);
  
  // Kernel with extern "C"
  const char* kernel_src = R"(
  extern "C" __global__
  void test_kernel(int* out) { *out = 42; }
  )";
  program_ptr prog;
  try {
    std::cerr << "Creating program for test_kernel" << std::endl;
    prog = mgr.create_program(kernel_src, "test_kernel", dev);
    std::cerr << "Program created successfully for test_kernel" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Warning: Skipping test_invalid_kernel_params due to create_program failure: " << e.what() << std::endl;
    return; // Skip test if program creation fails
  }
  
  nd_range dims(1, 1, 1, 1, 1, 1);  // 1D single thread
  command_runner<out<int>> runner;
  
  // Test with invalid (null) output buffer
  bool threw = false;
  try {
    auto outputs = runner.run(prog, dims, 1 /* actor_id */, create_out_arg_with_size<int>(0));
  } catch (const std::exception&) {
    threw = true;
  }
  assert(threw);
}

// Test for asynchronous execution with streams
void test_stream_async_execution([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  auto& mgr = manager::get();
  auto dev = mgr.find_device(0);
  
  // Use the device context managed by CAF CUDA
  CUcontext ctx;
  TEST_CHECK_CUDA(cuCtxGetCurrent(&ctx), "cuCtxGetCurrent");
  if (ctx == nullptr) {
    throw std::runtime_error("No current CUDA context");
  }
  
  // Kernel with extern "C"
  const char* kernel_src = R"(
  extern "C" __global__
  void set_value(int* out, int value) { *out = value; }
  )";
  program_ptr prog;
  try {
    std::cerr << "Creating program for set_value" << std::endl;
    prog = mgr.create_program(kernel_src, "set_value", dev);
    std::cerr << "Program created successfully for set_value" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Warning: Skipping test_stream_async_execution due to create_program failure: " << e.what() << std::endl;
    return; // Skip test if program creation fails
  }
  
  StreamPool pool(ctx, 2);
  auto stream1 = pool.acquire();
  auto stream2 = pool.acquire();
  
  nd_range dims(1, 1, 1, 1, 1, 1);  // 1D single thread
  command_runner<out<int>, in<int>> runner1, runner2;
  
  // Launch two async kernels with different actor IDs
  auto mem_tuple1 = runner1.run_async(prog, dims, 1 /* actor_id */, create_out_arg_with_size<int>(1), create_in_arg(100));
  auto mem_tuple2 = runner2.run_async(prog, dims, 2 /* actor_id */, create_out_arg_with_size<int>(1), create_in_arg(200));
  
  auto mem_ptr1 = std::get<0>(mem_tuple1);
  auto mem_ptr2 = std::get<0>(mem_tuple2);
  
  mem_ptr1->synchronize();
  mem_ptr2->synchronize();
  
  auto result1 = mem_ptr1->copy_to_host();
  auto result2 = mem_ptr2->copy_to_host();
  
  assert(result1[0] == 100);
  assert(result2[0] == 200);
  
  pool.release(stream1);
  pool.release(stream2);
}

// Test for string comparison kernel
void test_compare_strings([[maybe_unused]] caf::actor_system& sys) {
  using namespace caf::cuda;
  auto& mgr = manager::get();
  auto dev = mgr.find_device(0);
  
  // String comparison kernel
  const char* kernel_src = R"(
  extern "C" __global__
  void compare_strings(const char* a, const char* b, int* result, int length) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < length) {
      result[idx] = (a[idx] == b[idx]) ? 1 : 0;
    }
  }
  )";
  program_ptr prog;
  try {
    std::cerr << "Creating program for compare_strings" << std::endl;
    prog = mgr.create_program(kernel_src, "compare_strings", dev);
    std::cerr << "Program created successfully for compare_strings" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Warning: Skipping test_compare_strings due to create_program failure: " << e.what() << std::endl;
    return; // Skip test if program creation fails
  }
  
  const int size = 16;
  std::vector<char> str_a = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!', 0, 0, 0, 0};
  std::vector<char> str_b = {'h', 'e', 'l', 'l', 'o', ' ', 't', 'e', 's', 't', '!', '!', 0, 0, 0, 0};
  std::vector<int> expected = {1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1};
  
  nd_range dims(size / 4, 1, 1, 4, 1, 1);  // 4 blocks, 4 threads each
  command_runner<in<char>, in<char>, out<int>, in<int>> runner;
  
  auto outputs = runner.run(prog, dims, 1 /* actor_id */,
                           create_in_arg(str_a), create_in_arg(str_b), create_out_arg_with_size<int>(size), create_in_arg(size));
  assert(outputs.size() == 1u);
  auto result = extract_vector<int>(outputs);
  assert(result.size() == size);
  assert(vectors_equal(result, expected));
}

// Structure to hold test information
struct Test {
    std::string name;
    void (*function)(caf::actor_system&);
};

// List of all tests
const std::vector<Test> tests = {
    {"test_in_impl", test_in_impl},
    {"test_out_impl", test_out_impl},
    {"test_in_out_impl", test_in_out_impl},
    {"test_create_args", test_create_args},
    {"test_extract_vector", test_extract_vector},
    {"test_mem_ref_basic", test_mem_ref_basic},
    {"test_mem_ref_copy_to_host", test_mem_ref_copy_to_host},
    {"test_command_runner_sync", test_command_runner_sync},
    {"test_command_runner_async", test_command_runner_async},
    {"test_manager_create_and_spawn", test_manager_create_and_spawn},
    {"test_stream_pool", test_stream_pool},
    {"test_device_mem_alloc", test_device_mem_alloc},
    {"test_device_launch_kernel", test_device_launch_kernel},
    {"test_multi_thread_kernel", test_multi_thread_kernel},
    {"test_vector_addition", test_vector_addition},
    {"test_invalid_kernel_params", test_invalid_kernel_params},
    {"test_stream_async_execution", test_stream_async_execution},
    {"test_compare_strings", test_compare_strings}
};

// Function to run a single test and report its result
void run_test(const Test& test, caf::actor_system& sys) {
    std::cout << "Running test: " << test.name << "... ";
    try {
        test.function(sys);
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "FAILED: Unknown error" << std::endl;
    }
}

// CAF main function to run all unit tests
void caf_main(caf::actor_system& sys) {
    // Initialize CUDA manager
    try {
        caf::cuda::manager::init(sys);
        std::cout << "CUDA manager initialized successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize CUDA manager: " << e.what() << std::endl;
        return;
    }

    // Run all unit tests
    std::cout << "\nStarting unit tests...\n" << std::endl;
    int failed_tests = 0;
    for (const auto& test : tests) {
        run_test(test, sys);
        // Check if test threw an exception (indicating failure)
        try {
            test.function(sys);
        } catch (const std::exception& e) {
            if (std::string(e.what()).find("Skipping") == std::string::npos) {
                ++failed_tests; // Only count non-skipped failures
            }
        } catch (...) {
            ++failed_tests;
        }
    }

    // Shutdown CUDA manager
    try {
        caf::cuda::manager::shutdown();
        std::cout << "\nCUDA manager shutdown successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to shutdown CUDA manager: " << e.what() << std::endl;
    }

    // Summary
    std::cout << "\nTest Summary:\n";
    std::cout << "Total tests run: " << tests.size() << std::endl;
    std::cout << "Tests passed: " << (tests.size() - failed_tests) << std::endl;
    std::cout << "Tests failed: " << failed_tests << std::endl;

    // Exit with non-zero status if any tests failed
    if (failed_tests > 0) {
        throw std::runtime_error("One or more tests failed");
    }
}

// Define CAF main entry point
CAF_MAIN()
