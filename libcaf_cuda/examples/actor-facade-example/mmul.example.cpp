/* An example file demonstrating how the actor facade works
 * by showing how to launch a matrix multiplication kernel using the actor facade 
 * we use managers spawnFromCubin and is recommended that you do too or use spawnFromFatbin
 * to spawn an actor facade 
 * since using the spawn method will likely result in an unsupported toolchain error
 * be sure to run compile_kernels.sh
 */


#include <caf/all.hpp>  // Includes most CAF essentials

#include "caf/cuda/actor_facade.hpp"
#include "caf/cuda/manager.hpp"
#include "caf/cuda/nd_range.hpp"
#include "caf/cuda/all.hpp"
#include <caf/type_id.hpp>
#include "caf/detail/test.hpp"
#include <cassert>

#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <numeric>
#include "caf/actor_registry.hpp"



using namespace caf;
using namespace std::chrono_literals;

//verification of matrix multiplication on the gpu
void serial_matrix_multiply(const std::vector<int>& a,
                            const std::vector<int>& b,
                            std::vector<int>& c,
                            int N) {
  

 for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      int sum = 0;
      for (int k = 0; k < N; ++k) {
        sum += a[i * N + k] * b[k * N + j];
      }
      c[i * N + j] = sum;
    }
  }
}


void test_mmul_from_cubin(caf::actor_system& sys, int N) {
  std::cout << "[TEST] Starting test_mmul_from_cubin\n";

  caf::cuda::manager& mgr = caf::cuda::manager::get();

  int THREADS = 32;
  int BLOCKS = (N + THREADS - 1) / THREADS;

  caf::cuda::nd_range dim(
		  BLOCKS, //grid X dimension
		  BLOCKS, //grid Y dimension
		  1, //grid Z dimension
		  THREADS, // block X dimension 
		  THREADS, // block Y dimension
		  1 // block Z dimension
		  );


  // Spawn actor from precompiled cubin file
  auto gpuActor = mgr.spawnFromCUBIN(
		  "../mmul.cubin", //kernel file location
		  "matrixMul", //kernel name 
		  dim, //kernel dimensions             
		  in<int>{}, in<int>{}, out<int>{}, in<int>{} //kernel arg tags 
							      //in order they appear in kernel
		  );




  //generate random matrices
  std::vector<int> h_a(N * N);
  std::vector<int> h_b(N * N);
  std::vector<int> h_c(N * N, 0);
  std::vector<int> h_ref(N * N, 0);
  std::vector<int> h_n(1, N);
  std::generate(h_a.begin(), h_a.end(), []() { return rand() % 10; });
  std::generate(h_b.begin(), h_b.end(), []() { return rand() % 10; });



  //tag the arguments 
  auto arg1 = caf::cuda::create_in_arg(h_a); //matrix A readonly buffer
  auto arg2 = caf::cuda::create_in_arg(h_b); //matrix B readonly buffer
  auto arg3 = caf::cuda::create_out_arg_with_size<int>(N*N); //matrix size Writeonly buffer
  auto arg4 = caf::cuda::create_in_arg(N); // int size, readonly scalar

  serial_matrix_multiply(h_a, h_b, h_ref, N);

  sys.spawn([=](caf::event_based_actor* self_actor) {
    auto start = std::chrono::high_resolution_clock::now();

    //when mailing the gpu actor, the message is in the form of the kernel arguments 
    //and must be in the order they appear in the kernel parameters 
    //it will deliever a response promise with the results of that kernel launch
    self_actor->mail(arg1, arg2, arg3, arg4)
      .request(gpuActor, std::chrono::seconds(10))
      .then([=](const std::vector<output_buffer>& outputs) {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;

	
        std::vector<int> result = caf::cuda::extract_vector<int>(outputs); //collect the result buffer from output


        // Compare result with reference
        bool match = (result == h_ref);
        std::cout << "[INFO] Kernel round-trip time: " << elapsed.count() << " seconds\n";
        std::cout << (match ? "[PASS] GPU result matches reference\n" : "[FAIL] Mismatch in GPU result\n");

        self_actor->send_exit(gpuActor, caf::exit_reason::user_shutdown);
        self_actor->quit();
      });
  });

  sys.await_all_actors_done();
}




void caf_main(caf::actor_system& sys) {
  caf::cuda::manager::init(sys);  //be sure to initialize the manager
				  //as certain things need to be startup 
   test_mmul_from_cubin(sys,100);

   //test_mmul_from_cubin(sys,50);
   //test_mmul_from_cubin(sys,1024);

}




CAF_MAIN()
