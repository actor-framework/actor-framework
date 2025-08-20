#include <caf/all.hpp>
#include <caf/cuda/all.hpp>
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <numeric>
#include <random>
#include "caf/actor_registry.hpp"
//#include <caf/atoms.hpp>



using namespace caf;
using namespace std::chrono_literals;




const char* kernel_code = R"(
extern "C" __global__
void compare_strings(const char* a, const char* b, int* result, int * length) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < * length) {
        result[idx] = (a[idx] == b[idx]) ? 1 : 0;
    }
}
)";

const char* matrixMulKernel2 = R"(
extern "C" __global__
void matrixMul(const int* a, const int* b, int* c, int *N_val) {
    int N = *N_val;
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    if (row < N && col < N) {
        int temp = 0;
        for (int k = 0; k < N; ++k) {
            temp += a[row * N + k] * b[k * N + col];
        }
        c[row * N + col] = temp;
    }
}
)";


const char* matrixMulKernel = R"(
extern "C" __global__
void matrixMul(const int* a, const int* b, int* c, int N) {
    //printf("%d\n",N);
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    if (row < N && col < N) {
        int temp = 0;
        for (int k = 0; k < N; ++k) {
            temp += a[row * N + k] * b[k * N + col];
        }
        c[row * N + col] = temp;
    }
}
)";




#include <chrono>
#include <iostream>

// Extend your actor state to keep the start time
struct mmul_actor_state {
  static inline const char* name = "my_actor";
  int last_N = 0; // example state variable
  int id = rand(); // an actor id
  // per-actor timing start
  std::chrono::high_resolution_clock::time_point start_time;
  int times = 0;
};




//commands classes used to launch kernels 
using mmulCommand = caf::cuda::command_runner<in<int>,in<int>,out<int>,in<int>>;
using matrixGenCommand = caf::cuda::command_runner<out<int>,in<int>,in<int>,in<int>>;

using mmulAsyncCommand = caf::cuda::command_runner<caf::cuda::mem_ptr<int>,caf::cuda::mem_ptr<int>,out<int>,in<int>>;
using mmulFloatCommand = caf::cuda::command_runner<in<float>,in<float>,out<float>,in<int>>;
using matrixGenFloatCommand = caf::cuda::command_runner<out<float>,in<int>,in<int>,in<int>>;

using mmulAsyncFloatCommand = caf::cuda::command_runner<caf::cuda::mem_ptr<float>,caf::cuda::mem_ptr<float>,out<float>,in<int>>;


//integer commands
mmulCommand mmul;
matrixGenCommand randomMatrix;
mmulAsyncCommand mmulAsync;

//floating point type commands 
mmulFloatCommand mmulFloat;
matrixGenFloatCommand randomFloatMatrix;
mmulAsyncFloatCommand mmulFloatAsync;



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


void serial_matrix_multiply(const std::vector<float>& a,
                            const std::vector<float>& b,
                            std::vector<float>& c,
                            int N) {

    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < N; ++k) {
                sum += a[i * N + k] * b[k * N + j];
            }
            c[i * N + j] = sum;
        }
    }
}



// Stateful actor behavior
caf::behavior mmul_actor_fun(caf::stateful_actor<mmul_actor_state>* self) {
  return {
    // 1st handler: Just int N, and who to send the matrices to
    [=](int N, std::vector<caf::actor> receivers) {

       caf::cuda::manager& mgr = caf::cuda::manager::get();
        //create the program and configure the dimesnions of the kernel
	
         auto program = mgr.create_program_from_fatbin("../generate_random_matrix.fatbin","generate_random_matrix_float");
        int THREADS = 256;
	int BLOCKS = (N*N + THREADS - 1) / THREADS;
  	caf::cuda::nd_range dim(BLOCKS,1, 1, THREADS,1, 1);

	//tag the arguments so that caf::cuda knows what to do with them	
         out<float> arg1 = caf::cuda::create_out_arg_with_size<float>(N*N); //output buffer indicate its size, caf::cuda will handle the rest
          in<int> arg2 = caf::cuda::create_in_arg(N*N); //matrix size
          in<int> arg3 = caf::cuda::create_in_arg(1234); //seed
	  in<int> arg4 = caf::cuda::create_in_arg(9999); //max valux
	  


	  //launch kernels and collect their outputs
	  auto tempA = randomFloatMatrix.run(program,dim, self -> state().id,arg1,arg2,arg3,arg4);
	  auto tempB = randomFloatMatrix.run(program,dim, self -> state().id,arg1,arg2,arg3,arg4);
	  std::vector<float> matrixA =  caf::cuda::extract_vector<float>(tempA);
	  std::vector<float> matrixB = caf::cuda::extract_vector<float>(tempB);



	  //cpu code
	  //std::vector<int> matrixA(N*N);
	  //std::vector<int> matrixB(N*N);

	  // std::generate(matrixA.begin(), matrixA.end(), []() { return rand() % 10; });
	   //std::generate(matrixB.begin(), matrixB.end(), []() { return rand() % 10; });


	  //broadcast the result out to receviers.
	  for (auto actor: receivers) {
	  
		  self->mail(matrixA,matrixB,N).send(actor);
	  }

    },

    // 2nd handler: GPU atom + matrices + N, launches a kenrel and sends its result to itself for verification
    [=](const std::vector<float> matrixA,
        const std::vector<float> matrixB, int N) {
 

  caf::cuda::manager& mgr = caf::cuda::manager::get();

  //create program and dims   
  auto program = mgr.create_program_from_cubin("../mmul.cubin","matrixMulFloat");
  const int THREADS = 32;
  const int BLOCKS = (N + THREADS - 1) / THREADS;
  caf::cuda::nd_range dims(BLOCKS, BLOCKS, 1, THREADS, THREADS, 1);

    //create args
    auto arg1 = caf::cuda::create_in_arg(matrixA);
    auto arg2 = caf::cuda::create_in_arg(matrixB);
    out<float> arg3 = caf::cuda::create_out_arg_with_size<float>(N*N);
    auto arg4 = caf::cuda::create_in_arg(N);

    auto tempC = mmulFloat.run(program,dims,self -> state().id,arg1,arg2,arg3,arg4);
    std::vector<float> matrixC = caf::cuda::extract_vector<float>(tempC);

    //verify its own result 
    self -> mail(matrixA,matrixB,matrixC,N).send(self);

    },

    // 3rd handler: CPU atom + matrices + N
    [=](const std::vector<float>& matrixA,
        const std::vector<float>& matrixB, const std::vector<float> matrixC, int N) {
       
	 std::vector<float> result(N*N);

	 serial_matrix_multiply(matrixA,matrixB,result,N);

	 if (result == matrixC) {
	 
		 std::cout << "actor with id " <<  self->state().id << " references match\n";
	 
	 }

	 else {
	    std::cout << "actor with id " <<  self->state().id << " references did not match\n";
	 }

	 self-> quit();

    }
  };
}



void run_mmul_test(caf::actor_system& sys, int matrix_size, int num_actors) {
  if (num_actors < 1) {
    std::cerr << "[ERROR] Number of actors must be >= 1\n";
    return;
  }

  // Spawn num_actors actors running the mmul behavior
  std::vector<caf::actor> actors;
  actors.reserve(num_actors);
  for (int i = 0; i < num_actors; ++i) {
    actors.push_back(sys.spawn(mmul_actor_fun));
  }

  // Actor 0 generates matrices and broadcasts to others 
  caf::anon_mail(matrix_size, actors).send(actors[0]);

   sys.await_all_actors_done();
}


// Stateful actor behavior
// Stateful actor behavior
caf::behavior mmul_async_actor_fun(caf::stateful_actor<mmul_actor_state>* self) {
  return {
    // 1st handler: Just int N, and who to send the matrices to
    [=](int N, std::vector<caf::actor> receivers) {

        caf::cuda::manager& mgr = caf::cuda::manager::get();
        // float generator
        auto program = mgr.create_program_from_fatbin("../generate_random_matrix.fatbin",
                                                      "generate_random_matrix_float");
        int THREADS = 256;
        int BLOCKS  = (N*N + THREADS - 1) / THREADS;
        caf::cuda::nd_range dim(BLOCKS, 1, 1, THREADS, 1, 1);

        // args (float out, int scalars)
        out<float> arg1 = caf::cuda::create_out_arg_with_size<float>(N*N);
        auto arg2 = caf::cuda::create_in_arg(N*N);
        auto arg3 = caf::cuda::create_in_arg(rand());
        auto arg4 = caf::cuda::create_in_arg(9999);

        auto arg3B = caf::cuda::create_in_arg(rand());
        int device_number = 74;

        // launch float generators (async)
        auto tempA = randomFloatMatrix.run_async(program, dim, self -> state().id,
                                                 0, device_number,
                                                 arg1, arg2, arg3, arg4);
        auto tempB = randomFloatMatrix.run_async(program, dim, self -> state().id,
                                                 0, device_number,
                                                 arg1, arg2, arg3B, arg4);

        caf::cuda::mem_ptr<float> matrixA = std::get<0>(tempA);
        caf::cuda::mem_ptr<float> matrixB = std::get<0>(tempB);

        // ensure completion (optional)
        matrixA -> synchronize();
        matrixB -> synchronize();

        // broadcast device buffers to receivers
        for (auto actor: receivers) {
          self->mail( matrixA, matrixB, N, device_number).send(actor);
        }
    },

    // 2nd handler: run float mmul on device buffers, send result to self for verification
    [=](const caf::cuda::mem_ptr<float> matrixA,
        const caf::cuda::mem_ptr<float> matrixB, int N, int device_number) {

      caf::cuda::manager& mgr = caf::cuda::manager::get();

      auto program = mgr.create_program_from_cubin("../mmul.cubin", "matrixMulFloat");
      const int THREADS = 32;
      const int BLOCKS  = (N + THREADS - 1) / THREADS;
      caf::cuda::nd_range dims(BLOCKS, BLOCKS, 1, THREADS, THREADS, 1);

      // args: mem_ptr<float>, mem_ptr<float>, out<float>, in<int>
      auto arg1 = matrixA;
      auto arg2 = matrixB;
      out<float> arg3 = caf::cuda::create_out_arg_with_size<float>(N*N);
      auto arg4 = caf::cuda::create_in_arg(N);

      auto tempC = mmulFloatAsync.run(program, dims, self -> state().id,
                                      0, device_number,
                                      arg1, arg2, arg3, arg4);

      std::vector<float> matrix1 = matrixA -> copy_to_host();
      std::vector<float> matrix2 = matrixB -> copy_to_host();
      std::vector<float> matrixC = caf::cuda::extract_vector<float>(tempC, 2);

      // verify on CPU
      self -> mail(matrix1, matrix2, matrixC, N).send(self);
    },

    // 3rd handler: shared-mem float mmul and verify
    [=](int x, const caf::cuda::mem_ptr<float> matrixA,
        const caf::cuda::mem_ptr<float> matrixB, int N, int device_number) {

      caf::cuda::manager& mgr = caf::cuda::manager::get();

      auto program = mgr.create_program_from_cubin("../shared_mmul.cubin", "matrixMulFloat");
      const int THREADS = 32;
      const int BLOCKS  = (N + THREADS - 1) / THREADS;
      caf::cuda::nd_range dims(BLOCKS, BLOCKS, 1, THREADS, THREADS, 1);

      int shared_mem = 8192;

      auto arg1 = matrixA;
      auto arg2 = matrixB;
      out<float> arg3 = caf::cuda::create_out_arg_with_size<float>(N*N);
      auto arg4 = caf::cuda::create_in_arg(N);

      auto tempC = mmulFloatAsync.run(program, dims, self -> state().id,
                                      shared_mem, device_number,
                                      arg1, arg2, arg3, arg4);

      std::vector<float> matrix1 = matrixA -> copy_to_host();
      std::vector<float> matrix2 = matrixB -> copy_to_host();
      std::vector<float> matrixC = caf::cuda::extract_vector<float>(tempC, 2);

      self -> mail(matrix1, matrix2, matrixC, N).send(self);
    },

    // 4th handler: CPU verify (float)
    [=](const std::vector<float>& matrixA,
        const std::vector<float>& matrixB,
        const std::vector<float>& matrixC, int N) {

      std::vector<float> result(N * N);
      serial_matrix_multiply(matrixA, matrixB, result, N);

      if (result == matrixC) {
        std::cout << "actor with id " << self->state().id << " references match\n";
      } else {
        std::cout << "actor with id " << self->state().id << " references did not match\n";
      }
      self->quit();
    }
  };
}



void run_async_mmul_test(caf::actor_system& sys, int matrix_size, int num_actors) {
  if (num_actors < 1) {
    std::cerr << "[ERROR] Number of actors must be >= 1\n";
    return;
  }

  // Spawn num_actors actors running the mmul behavior
  std::vector<caf::actor> actors;
  actors.reserve(num_actors);
  for (int i = 0; i < num_actors; ++i) {
    actors.push_back(sys.spawn(mmul_async_actor_fun));
  }

  // Actor 0 generates matrices and broadcasts to others 
  caf::anon_mail(matrix_size, actors).send(actors[0]);

   sys.await_all_actors_done();
}


//--------------------------------Perfomance tests 

// Perf-version of the actor: each actor generates a matrix and sends to itself
// // Perf-version (no-shared): float everywhere
caf::behavior mmul_async_actor_fun_perf(caf::stateful_actor<mmul_actor_state>* self) {
  return {
    // 1) start: generate matrices and send them to self
    [=](int N) {
      self->state().start_time = std::chrono::high_resolution_clock::now();
      self->state().last_N = N;

      caf::cuda::manager& mgr = caf::cuda::manager::get();
      auto program = mgr.create_program_from_fatbin("../generate_random_matrix.fatbin",
                                                    "generate_random_matrix_float");

      int THREADS = 256;
      int BLOCKS  = (N * N + THREADS - 1) / THREADS;
      caf::cuda::nd_range dim(BLOCKS, 1, 1, THREADS, 1, 1);

      out<float> arg_out = caf::cuda::create_out_arg_with_size<float>(N * N);
      auto arg_size = caf::cuda::create_in_arg(N * N);
      auto arg_seed = caf::cuda::create_in_arg(rand());
      auto arg_max  = caf::cuda::create_in_arg(9999);

      int device_number = rand() % 2;

      auto tA = randomFloatMatrix.run_async(program, dim, self->state().id,
                                            0, device_number,
                                            arg_out, arg_size, arg_seed, arg_max);
      auto tB = randomFloatMatrix.run_async(program, dim, self->state().id,
                                            0, device_number,
                                            arg_out, arg_size, arg_seed, arg_max);

      auto matA_ptr = std::get<0>(tA); // caf::cuda::mem_ptr<float>
      auto matB_ptr = std::get<0>(tB);

      for (int i = 0; i < 20; i++)
        self->mail(matA_ptr, matB_ptr, N).send(self);
    },

    // 2) multiply: float mmul
    [=](const caf::cuda::mem_ptr<float> matA,
        const caf::cuda::mem_ptr<float> matB,
        int N) {

      caf::cuda::manager& mgr = caf::cuda::manager::get();
      auto program = mgr.create_program_from_cubin("../mmul.cubin", "matrixMulFloat");
      const int THREADS = 32;
      const int BLOCKS  = (N + THREADS - 1) / THREADS;
      caf::cuda::nd_range dims(BLOCKS, BLOCKS, 1, THREADS, THREADS, 1);

      auto arg1 = matA;
      auto arg2 = matB;
      out<float> arg3 = caf::cuda::create_out_arg_with_size<float>(N * N);
      auto arg4 = caf::cuda::create_in_arg(N);

      auto start = std::chrono::high_resolution_clock::now();
      auto out_bufs = mmulFloatAsync.run(program, dims, self->state().id,
                                         0, matA -> deviceNumber(),
                                         arg1, arg2, arg3, arg4);
      auto end = std::chrono::high_resolution_clock::now();

      double actor_latency_ms =
        std::chrono::duration<double, std::milli>(end - self->state().start_time).count();

      std::cout << "[PERF] Actor id=" << self->state().id
                << " N=" << N
                << " latency=" << actor_latency_ms << " ms\n";

      if (self -> state().times++ == 19) {
        self->quit();
      }
    }
  };
}


// Driver: spawn actors, start timer, tell each actor to generate/send-to-self, wait, print total time
void run_async_mmul_perf_test(caf::actor_system& sys, int matrix_size, int num_actors) {
  if (num_actors < 1) {
    std::cerr << "[ERROR] Number of actors must be >= 1\n";
    return;
  }

  // spawn actors
  std::vector<caf::actor> actors;
  actors.reserve(num_actors);
  for (int i = 0; i < num_actors; ++i) {
    actors.push_back(sys.spawn(mmul_async_actor_fun_perf));
  }

  // Total runtime start
  auto total_start = std::chrono::high_resolution_clock::now();

  // Tell every actor to generate a matrix and route it to itself
  for (auto& a : actors) {
    // send N to the actor (actor will generate and self-send)
    caf::anon_mail(matrix_size).send(a);
  }

  // wait for all actors to finish
  sys.await_all_actors_done();

  // Total runtime end & print
  auto total_end = std::chrono::high_resolution_clock::now();
  double total_ms = std::chrono::duration<double, std::milli>(total_end - total_start).count();
  std::cout << "[PERF] Total runtime for " << num_actors << " actors: " << total_ms << " ms\n";
}

//-----------------------------------BenchMark Tests


// Benchmark driver for the "async (no-shared)" perf test
void benchmark_async_perf_all(caf::actor_system& sys) {
  const std::vector<int> actor_counts = {1, 50, 200};
  const std::vector<int> matrix_sizes = {1024, 2048, 4096};

  std::cout << "=== Async (no-shared) benchmark ===\n";
  for (int size : matrix_sizes) {
    for (int num_actors : actor_counts) {
      std::cout << "[RUN] matrix_size=" << size
                << " actors=" << num_actors
                << "  -- starting\n" << std::flush;

      auto t0 = std::chrono::high_resolution_clock::now();
      // This function blocks until all actors finish and prints per-actor latencies.
      run_async_mmul_perf_test(sys, size, num_actors);
      auto t1 = std::chrono::high_resolution_clock::now();

      double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
      std::cout << "[RESULT] async  matrix_size=" << size
                << " actors=" << num_actors
                << " total_time_ms=" << total_ms << "\n\n" << std::flush;
    }
  }
  std::cout << "=== Async (no-shared) benchmark complete ===\n\n";
}



void caf_main(caf::actor_system& sys) {
  caf::cuda::manager::init(sys);

  run_mmul_test(sys,100,50);
  //run_async_mmul_test(sys,100,30);
   //run_async_mmul_perf_test(sys,1024,1);

  // run the async (no-shared) suite:
  //benchmark_async_perf_all(sys);

  // run the shared-memory suite:
  //benchmark_shared_perf_all(sys);
}




CAF_MAIN()
