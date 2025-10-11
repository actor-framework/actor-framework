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

// Define a custom type ID block for custom actors
CAF_ADD_ATOM(cuda,shared_mem)



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

mmulCommand mmul;
matrixGenCommand randomMatrix;
mmulAsyncCommand mmulAsync;


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




// Stateful actor behavior
caf::behavior mmul_actor_fun(caf::stateful_actor<mmul_actor_state>* self) {
  return {
    // 1st handler: Just int N, and who to send the matrices to
    [=](int N, std::vector<caf::actor> receivers) {

	/*
	 * Unfortuanley libraries such as curand cannot be linked with cubins 
	 * making it incompatable with this software for right now
	 * its not really random, just a matrix filled with 5's
	 */
        caf::cuda::manager& mgr = caf::cuda::manager::get();
        //create the program and configure the dimesnions of the kernel
        auto program = mgr.create_program_from_fatbin("../generate_random_matrix.fatbin","generate_random_matrix");
	int THREADS = 256;
	int BLOCKS = (N*N + THREADS - 1) / THREADS;
  	caf::cuda::nd_range dim(BLOCKS,1, 1, THREADS,1, 1);

	//tag the arguments so that caf::cuda knows what to do with them	
         auto arg1 = caf::cuda::create_out_arg(N*N); //output buffer indicate its size, caf::cuda will handle the rest
          auto arg2 = caf::cuda::create_in_arg(N*N); //matrix size
          auto arg3 = caf::cuda::create_in_arg(1234); //seed
	  auto arg4 = caf::cuda::create_in_arg(9999); //max valux
	  


	  //launch kernels and collect their outputs
	  auto tempA = randomMatrix.run(program,dim, self -> state().id,arg1,arg2,arg3,arg4);
	  auto tempB = randomMatrix.run(program,dim, self -> state().id,arg1,arg2,arg3,arg4);
	  std::vector<int> matrixA =  caf::cuda::extract_vector<int>(tempA);
	  std::vector<int> matrixB = caf::cuda::extract_vector<int>(tempB);



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
    [=](const std::vector<int> matrixA,
        const std::vector<int> matrixB, int N) {
 

  caf::cuda::manager& mgr = caf::cuda::manager::get();

  //create program and dims   
  auto program = mgr.create_program_from_cubin("../mmul.cubin","matrixMul");
  const int THREADS = 32;
  const int BLOCKS = (N + THREADS - 1) / THREADS;
  caf::cuda::nd_range dims(BLOCKS, BLOCKS, 1, THREADS, THREADS, 1);

    //create args
    auto arg1 = caf::cuda::create_in_arg(matrixA);
    auto arg2 = caf::cuda::create_in_arg(matrixB);
    auto arg3 = caf::cuda::create_out_arg(N*N);
    auto arg4 = caf::cuda::create_in_arg(N);

    auto tempC = mmul.run(program,dims,self -> state().id,arg1,arg2,arg3,arg4);
    std::vector<int> matrixC = caf::cuda::extract_vector<int>(tempC);

    //verify its own result 
    self -> mail(matrixA,matrixB,matrixC,N).send(self);

    },

    // 3rd handler: CPU atom + matrices + N
    [=](const std::vector<int>& matrixA,
        const std::vector<int>& matrixB, const std::vector<int> matrixC, int N) {
       
	 std::vector<int> result(N*N);

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
caf::behavior mmul_async_actor_fun(caf::stateful_actor<mmul_actor_state>* self) {
  return {
    // 1st handler: Just int N, and who to send the matrices to
    [=](int N, std::vector<caf::actor> receivers) {

        caf::cuda::manager& mgr = caf::cuda::manager::get();
        //create the program and configure the dimesnions of the kernel
        auto program = mgr.create_program_from_fatbin("../generate_random_matrix.fatbin","generate_random_matrix");
	int THREADS = 256;
	int BLOCKS = (N*N + THREADS - 1) / THREADS;
  	caf::cuda::nd_range dim(BLOCKS,1, 1, THREADS,1, 1);

	//tag the arguments so that caf::cuda knows what to do with them	
         auto arg1 = caf::cuda::create_out_arg(N*N); //output buffer indicate its size, caf::cuda will handle the rest
          auto arg2 = caf::cuda::create_in_arg(N*N); //matrix size
          auto arg3 = caf::cuda::create_in_arg(rand()); //seed
	  auto arg4 = caf::cuda::create_in_arg(9999); //max valux
	  
          auto arg3B = caf::cuda::create_in_arg(rand()); //seed
	  int device_number= 74; //arbitary number to show that 
				 //can give illusion of selecting gpus that are
				 //not there


	  //launch kernels and collect their outputs
	  auto tempA = randomMatrix.run_async(program,dim, self -> state().id,0,device_number,arg1,arg2,arg3,arg4);
	  auto tempB = randomMatrix.run_async(program,dim, self -> state().id,0,device_number,arg1,arg2,arg3B,arg4);
	  caf::cuda::mem_ptr<int> matrixA =  std::get<0>(tempA);
	  caf::cuda::mem_ptr<int> matrixB = std::get<0>(tempB);

	  //ensure the data is actually done being worked on
	  matrixA -> synchronize();
	  matrixB -> synchronize();




	  //cpu code
	  //std::vector<int> matrixA(N*N);
	  //std::vector<int> matrixB(N*N);

	  // std::generate(matrixA.begin(), matrixA.end(), []() { return rand() % 10; });
	   //std::generate(matrixB.begin(), matrixB.end(), []() { return rand() % 10; });


	  std::cout << "Broadcasting\n";
	  //broadcast the result out to receviers.
	  for (auto actor: receivers) {
	  
		  self->mail(3,matrixA,matrixB,N,device_number).send(actor);
	  }

    },

    // 2nd handler: GPU atom + matrices + N, launches a kenrel and sends its result to itself for verification
    [=](const caf::cuda::mem_ptr<int> matrixA,
        const caf::cuda::mem_ptr<int> matrixB, int N,int device_number) {
 

  caf::cuda::manager& mgr = caf::cuda::manager::get();

  //create program and dims   
  auto program = mgr.create_program_from_cubin("../mmul.cubin","matrixMul");
  const int THREADS = 32;
  const int BLOCKS = (N + THREADS - 1) / THREADS;
  caf::cuda::nd_range dims(BLOCKS, BLOCKS, 1, THREADS, THREADS, 1);

    //create args
    auto arg1 = matrixA;
    auto arg2 = matrixB;
    auto arg3 = caf::cuda::create_out_arg(N*N);
    auto arg4 = caf::cuda::create_in_arg(N);


    auto tempC = mmulAsync.run(program,dims,self -> state().id,0,device_number,arg1,arg2,arg3,arg4);

    std::vector<int> matrix1 = matrixA -> copy_to_host();
    std::vector<int> matrix2 = matrixB -> copy_to_host();    
    std::vector<int> matrixC = caf::cuda::extract_vector<int>(tempC,2);

    //verify its own result 
    self -> mail(matrix1,matrix2,matrixC,N).send(self);

    },

 // 3nd handler: GPU atom + matrices + N, launches a kenrel using shared memory and sends its result to itself for verification
    [=](int x,const caf::cuda::mem_ptr<int> matrixA,
        const caf::cuda::mem_ptr<int> matrixB, int N,int device_number) {
 

  caf::cuda::manager& mgr = caf::cuda::manager::get();

  //create program and dims   
  auto program = mgr.create_program_from_cubin("../shared_mmul.cubin","matrixMul");
  const int THREADS = 32;
  const int BLOCKS = (N + THREADS - 1) / THREADS;
  caf::cuda::nd_range dims(BLOCKS, BLOCKS, 1, THREADS, THREADS, 1);

  int shared_mem = 8192; //we need 8KB of shared memory here
    //create args
    auto arg1 = matrixA;
    auto arg2 = matrixB;
    auto arg3 = caf::cuda::create_out_arg(N*N);
    auto arg4 = caf::cuda::create_in_arg(N);


    auto tempC = mmulAsync.run(program,dims,self -> state().id,shared_mem,device_number,arg1,arg2,arg3,arg4);

    std::vector<int> matrix1 = matrixA -> copy_to_host();
    std::vector<int> matrix2 = matrixB -> copy_to_host();    
    std::vector<int> matrixC = caf::cuda::extract_vector<int>(tempC,2);

    //verify its own result 
    self -> mail(matrix1,matrix2,matrixC,N).send(self);

    },



    // 3rd handler: CPU atom + matrices + N
    [=](const std::vector<int>& matrixA,
    const std::vector<int> &matrixB,
    const std::vector<int> &matrixC, int N) {

    std::vector<int> result(N * N);

    serial_matrix_multiply(matrixA, matrixB, result, N);

    if (result == matrixC) {
        std::cout << "actor with id " << self->state().id << " references match\n";
    }
    else {
        std::cout << "actor with id " << self->state().id << " references did not match\n";

    }


    /*
    auto print_matrix = [N](const std::vector<int>& mat, const std::string& name) {
            std::cout << name << ":\n";
            for (int i = 0; i < N; ++i) {
                for (int j = 0; j < N; ++j) {
                    std::cout << mat[i * N + j] << " ";
                }
                std::cout << "\n";
            }
            std::cout << std::endl;
        };

        print_matrix(matrixA, "Matrix A");
        print_matrix(matrixB, "Matrix B");
        print_matrix(result, "Result Matrix");
        print_matrix(matrixC, "GPU Result Matrix");
	*/
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
caf::behavior mmul_async_actor_fun_perf(caf::stateful_actor<mmul_actor_state>* self) {
  return {
    // 1) start: generate matrices and send them to self
    [=](int N) {
      // store start time in actor state (no locks)
      self->state().start_time = std::chrono::high_resolution_clock::now();
      self->state().last_N = N;

      caf::cuda::manager& mgr = caf::cuda::manager::get();
      // use the generator fatbin (as in your code)
      auto program = mgr.create_program_from_fatbin("../generate_random_matrix.fatbin",
                                                    "generate_random_matrix");

      int THREADS = 256;
      int BLOCKS = (N * N + THREADS - 1) / THREADS;
      caf::cuda::nd_range dim(BLOCKS, 1, 1, THREADS, 1, 1);

      // prepare args (same as your existing code)
      auto arg_out = caf::cuda::create_out_arg(N * N);
      auto arg_size = caf::cuda::create_in_arg(N * N);
      auto arg_seed = caf::cuda::create_in_arg(rand());
      auto arg_max = caf::cuda::create_in_arg(9999);

      int device_number = rand()%2;

      // launch generator(s) asynchronously and get mem_ptrs back
      // (we follow your earlier style: run_async returns tuple of mem_ptrs)
      auto tA = randomMatrix.run_async(program, dim, self->state().id,0,device_number,arg_out, arg_size, arg_seed, arg_max);
      auto tB = randomMatrix.run_async(program, dim, self->state().id,0,device_number, arg_out, arg_size, arg_seed, arg_max);

      // Extract the mem_ptrs (assume index 0 holds the buffer)
      auto matA_ptr = std::get<0>(tA);
      auto matB_ptr = std::get<0>(tB);

      // ensure kernels are done and data is ready
      //since we are sending to ourself no need to synchronize, since actors
      //get their own stream
      //if (matA_ptr) matA_ptr->synchronize();
      //if (matB_ptr) matB_ptr->synchronize();

      // send the mem_ptrs to ourselves to trigger the multiply step
      // (we send device buffers, N)
      for (int i =0;i < 20;i++)
      self->mail(matA_ptr, matB_ptr, N).send(self);
    },

    // 2) multiply: receive mem_ptrs, run the mmul kernel, measure time, print, quit
    [=](const caf::cuda::mem_ptr<int> matA,
        const caf::cuda::mem_ptr<int> matB,
        int N) {

      // prepare mmul program + dims (same as your code)
      caf::cuda::manager& mgr = caf::cuda::manager::get();
      auto program = mgr.create_program_from_cubin("../mmul.cubin", "matrixMul");
      const int THREADS = 32;
      const int BLOCKS = (N + THREADS - 1) / THREADS;
      caf::cuda::nd_range dims(BLOCKS, BLOCKS, 1, THREADS, THREADS, 1);

      // create arguments; use device pointers directly (your style)
      auto arg1 = matA;
      auto arg2 = matB;
      auto arg3 = caf::cuda::create_out_arg(N * N);
      auto arg4 = caf::cuda::create_in_arg(N);

      // Synchronous launch (blocks until kernel finishes and output is collected).
      // This represents "actor is done with its result".
      auto start = std::chrono::high_resolution_clock::now();
      auto out_bufs = mmulAsync.run(program, dims, self->state().id,0,matA ->deviceNumber(), arg1, arg2, arg3, arg4);
      auto end = std::chrono::high_resolution_clock::now();

      // compute per-actor latency from the generation start stored in state
      double actor_latency_ms =
        std::chrono::duration<double, std::milli>(end - self->state().start_time).count();

      // Print per-actor latency (actor id included)
      std::cout << "[PERF] Actor id=" << self->state().id
                << " N=" << N
                << " latency=" << actor_latency_ms << " ms\n";

      if (self -> state().times++ == 19) {
      // Done for this actor; exit
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



// ---------------------------
// Shared-memory perf actor
// ---------------------------
caf::behavior mmul_shared_async_actor_fun_perf(caf::stateful_actor<mmul_actor_state>* self) {
  return {
    // 1) start: generate matrices and send them to self
    [=](int N) {
      auto& st = self->state();
      st.start_time = std::chrono::high_resolution_clock::now();
      st.last_N = N;

      caf::cuda::manager& mgr = caf::cuda::manager::get();
      // generator (same as before)
      auto gen_prog = mgr.create_program_from_fatbin(
          "../generate_random_matrix.fatbin", "generate_random_matrix");

      const int GEN_THREADS = 256;
      const int GEN_BLOCKS = (N * N + GEN_THREADS - 1) / GEN_THREADS;
      caf::cuda::nd_range gen_dim(GEN_BLOCKS, 1, 1, GEN_THREADS, 1, 1);

      auto arg_out = caf::cuda::create_out_arg(N * N);
      auto arg_size = caf::cuda::create_in_arg(N * N);
      auto arg_seed = caf::cuda::create_in_arg(rand());
      auto arg_max  = caf::cuda::create_in_arg(9999);

      // choose device (keep consistent across generator and shared kernel)
      int device_number = rand() % 2; // or any device selection strategy

      // generate device buffers asynchronously (shared_mem for generator = 0)
      auto tA = randomMatrix.run_async(gen_prog, gen_dim, st.id, 0, device_number,
                                       arg_out, arg_size, arg_seed, arg_max);
      auto tB = randomMatrix.run_async(gen_prog, gen_dim, st.id, 0, device_number,
                                       arg_out, arg_size, arg_seed, arg_max);

      auto matA_ptr = std::get<0>(tA);
      auto matB_ptr = std::get<0>(tB);

      //since we send to ourselves we dont need to synchronize
      //each actor gets its own stream
      //if (matA_ptr) matA_ptr->synchronize();
      //if (matB_ptr) matB_ptr->synchronize();

      // send mem_ptrs + N + device_number to self for the shared-memory multiply
     for(int i = 0; i < 20;i++)
      self->mail(matA_ptr, matB_ptr, N, device_number).send(self);
    },

    // 2) multiply with shared memory: receive mem_ptrs, run shared kernel, measure, quit
    [=](const caf::cuda::mem_ptr<int> matA,
        const caf::cuda::mem_ptr<int> matB,
        int N,
        int device_number) {

      auto& st = self->state();

      caf::cuda::manager& mgr = caf::cuda::manager::get();
      // shared-memory kernel binary
      auto shared_prog = mgr.create_program_from_cubin("../shared_mmul.cubin", "matrixMul");

      const int THREADS = 32;
      const int BLOCKS = (N + THREADS - 1) / THREADS;
      caf::cuda::nd_range dims(BLOCKS, BLOCKS, 1, THREADS, THREADS, 1);

      // prepare args (use device pointers / wrapper args as your API expects)
      auto arg1 = matA;
      auto arg2 = matB;
      auto arg3 = caf::cuda::create_out_arg(N * N);
      auto arg4 = caf::cuda::create_in_arg(N);

      // choose shared memory amount for this launch (bytes)
      // adjust as needed for your kernel (here example: 8KB)
      const int shared_mem_bytes = 8 * 1024;

      // synchronous launch that returns when outputs are ready
      auto launch_start = std::chrono::high_resolution_clock::now();
      // NOTE: shared_mem comes BEFORE device_number in your API
      auto out_bufs = mmulAsync.run(shared_prog,
                                    dims,
                                    st.id,
                                    shared_mem_bytes,         // <-- shared memory
                                    device_number,            // <-- device number (same device)
                                    arg1, arg2, arg3, arg4);
      auto launch_end = std::chrono::high_resolution_clock::now();

      // per-actor latency measured from generation start stored in state
      double actor_latency_ms =
        std::chrono::duration<double, std::milli>(launch_end - st.start_time).count();

      std::cout << "[PERF][SHARED] Actor id=" << st.id
                << " N=" << N
                << " shared_mem=" << shared_mem_bytes
                << " latency=" << actor_latency_ms << " ms\n";

      
      if (self -> state().times++ == 19) {
      // Done for this actor; exit
      self->quit();
      }
     }
  };
}

// Driver for shared-memory perf test
void run_shared_mmul_perf_test(caf::actor_system& sys, int matrix_size, int num_actors) {
  if (num_actors < 1) {
    std::cerr << "[ERROR] Number of actors must be >= 1\n";
    return;
  }

  // spawn actors
  std::vector<caf::actor> actors;
  actors.reserve(num_actors);
  for (int i = 0; i < num_actors; ++i) {
    actors.push_back(sys.spawn(mmul_shared_async_actor_fun_perf));
  }

  // Total runtime start
  auto total_start = std::chrono::high_resolution_clock::now();

  // Tell every actor to generate a matrix and handle it with the shared kernel
  for (auto& a : actors) {
    caf::anon_mail(matrix_size).send(a);
  }

  // wait for all actors to finish
  sys.await_all_actors_done();

  // Total runtime end & print
  auto total_end = std::chrono::high_resolution_clock::now();
  double total_ms = std::chrono::duration<double, std::milli>(total_end - total_start).count();
  std::cout << "[PERF][SHARED] Total runtime for " << num_actors << " actors: "
            << total_ms << " ms\n";
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

// Benchmark driver for the "shared-memory" perf test
void benchmark_shared_perf_all(caf::actor_system& sys) {
  const std::vector<int> actor_counts = {1, 50, 200};
  const std::vector<int> matrix_sizes = {1024, 2048, 4096};

  std::cout << "=== Shared-memory benchmark ===\n";
  for (int size : matrix_sizes) {
    for (int num_actors : actor_counts) {
      std::cout << "[RUN] matrix_size=" << size
                << " actors=" << num_actors
                << "  -- starting\n" << std::flush;

      auto t0 = std::chrono::high_resolution_clock::now();
      // This function blocks until all actors finish and prints per-actor latencies.
      run_shared_mmul_perf_test(sys, size, num_actors);
      auto t1 = std::chrono::high_resolution_clock::now();

      double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
      std::cout << "[RESULT] shared matrix_size=" << size
                << " actors=" << num_actors
                << " total_time_ms=" << total_ms << "\n\n" << std::flush;
    }
  }
  std::cout << "=== Shared-memory benchmark complete ===\n\n";
}




void caf_main(caf::actor_system& sys) {
  caf::cuda::manager::init(sys);

  run_mmul_test(sys,100,4000);
  //run_async_mmul_test(sys,100,1);
  //run_async_mmul_perf_test(sys,1024,200);

  // run the async (no-shared) suite:
  //benchmark_async_perf_all(sys);

  // run the shared-memory suite:
  //benchmark_shared_perf_all(sys);
}




CAF_MAIN()
