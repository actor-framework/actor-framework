/* An example file where we demonstrate how to use cuda actors using
 * the entry point command runner, enabling the user to create their own custom gpu
 * actor
 * Be sure to run compile_kernels.sh
 * We will show how to create an actor that generates 2 random matrices 
 * then sends it to itself for matrix multiplication and then sends its result 
 * to itself for verification
 * Note that we will be using create_program_from_cubin and create_program_from_fatbin
 * methods these are the recommends and supported methods, as while you can
 * use create_program method, you are likely to run into unsupported toolchain 
 * errors
 */

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



#include <chrono>
#include <iostream>

struct mmul_actor_state {
  static inline const char* name = "my_actor";
  int id = rand(); // an actor id, each actor uses an id to request gpu resources
		   // if you want actors to share the same gpu resources (such as custreams)
		   // then they must share the same id
  
  //mostly here for benchmarking if needed 
  // per-actor timing start 
  std::chrono::high_resolution_clock::time_point start_time;
  int times = 0;
};


//commands classes used to launch kernels
//how they work is there templates is the sequence of wrapper types that are
//used by the gpu actors software to deduce what is to be done with the data
//arguments are expected to appear in the order that they would appear in the 
//kernel, for instance the out<int> is represented of the matrixC 
using mmulCommand = caf::cuda::command_runner<
      in<int>, //matrixA a readonly integer buffer
      in<int>, //matrixB a readonly integer buffer
      out<int>, //matrixC a writeonly integer buffer, you can just pass in integer for size and it will automatically allocate a buffer for you on the gpu 
      in<int> //int N, the size of the matrices, you can just pass in a single integer and it will recognize this 
      >;

using matrixGenCommand = caf::cuda::command_runner<
      out<int>, //writeonly matrix buffer 
      in<int>, //total elements 
      in<int>, //seed
      in<int> //max value
      >;

//mem_ptrs are references to memory on the gpu, same rules apply, it must be in the same order as it would appear in the kernel
using mmulAsyncCommand = caf::cuda::command_runner<caf::cuda::mem_ptr<int>,caf::cuda::mem_ptr<int>,out<int>,in<int>>;

mmulCommand mmul;
matrixGenCommand randomMatrix;
mmulAsyncCommand mmulAsync;



//a simple cpu matrix multiplication verification function
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
    // 1st handler: Just int N, matrix size, will generate an N*N matrix 
    [=](int N) {

	 caf::cuda::manager& mgr = caf::cuda::manager::get();


        //create the program and configure the dimesnions of the kernel
        auto program = mgr.create_program_from_fatbin("../generate_random_matrix.fatbin", //path to kernel file
			"generate_random_matrix" //kernel name 
			);
	
	int THREADS = 256;
	int BLOCKS = (N*N + THREADS - 1) / THREADS;
  	caf::cuda::nd_range dim(
			BLOCKS, //grid X dimension
			1, // grid Y dimension
		       	1, //grid Z dimension
			THREADS, //block X dimension
			1, //block Y dimension
		       	1 // block Z dimension
			);

	//tag the arguments so that caf::cuda knows what to do with them	
         auto arg1 = caf::cuda::create_out_arg_with_size<int>(N*N); //output buffer indicate its size, caf::cuda will handle the rest
          auto arg2 = caf::cuda::create_in_arg(N*N); //matrix size
          auto arg3 = caf::cuda::create_in_arg(1234); //seed
	  auto arg4 = caf::cuda::create_in_arg(9999); //max valux
	  


	  //launch kernels and collect their outputs
	  //the args tagged with type in<T>, will not show up in the result 
	  auto tempA = randomMatrix.run(
			  program,//kernel to launch
			  dim, //kernel dimensions
			  self -> state().id, //actor id 
			  arg1,arg2,arg3,arg4 //kernel args in order that they appear in the kernel  
			  );
	  auto tempB = randomMatrix.run(program,dim, self -> state().id,arg1,arg2,arg3,arg4);
	  std::vector<int> matrixA =  caf::cuda::extract_vector<int>(tempA);
	  std::vector<int> matrixB = caf::cuda::extract_vector<int>(tempB);
		  
	  //send the results to ourself	  	  
	  self->mail(matrixA,matrixB,N).send(self);

    },

    // 2nd handler: GPU atom + matrices + N, launches a kenrel and sends its result to itself for verification
    [=](const std::vector<int> matrixA,
        const std::vector<int> matrixB, int N) {
 

  caf::cuda::manager& mgr = caf::cuda::manager::get();

  //create program and dims   
  auto program = mgr.create_program_from_cubin("../mmul.cubin", //kernel file path 
		  "matrixMul" //kernel name
		  );

  const int THREADS = 32;
  const int BLOCKS = (N + THREADS - 1) / THREADS;
  caf::cuda::nd_range dims(
		  BLOCKS, //grid X dimension
		  BLOCKS, // grid Y dimension
		  1,  //grid Z dimension
		  THREADS, //block X dimension
		  THREADS, //block Y dimension
		  1 //block Z dimension
		  ); 

    //create args
    auto arg1 = caf::cuda::create_in_arg(matrixA); //matrix A
    auto arg2 = caf::cuda::create_in_arg(matrixB); //matrix B
    auto arg3 = caf::cuda::create_out_arg_with_size<int>(N*N); //matrix C (specify with size)
    auto arg4 = caf::cuda::create_in_arg(N); //size of the matrices

    //launch kernel and collect the output 
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

  //send a size to all actors 
  for (auto a : actors)
	  caf::anon_mail(matrix_size).send(a);

   sys.await_all_actors_done();
}



//demonstration of sending memory on the gpu to other actors 
//its worth mentioning that if you are sending gpu memory around
//you must ensure that it stays on the same device and stream by ensuring the 
//device number and actor id is the same per kernel launch 
caf::behavior mmul_async_actor_fun(caf::stateful_actor<mmul_actor_state>* self) {
  return {
    // 1st handler: Just int N, send the matrix to itself 
    [=](int N) {

        caf::cuda::manager& mgr = caf::cuda::manager::get();
        //create the program and configure the dimesnions of the kernel
        auto program = mgr.create_program_from_fatbin("../generate_random_matrix.fatbin","generate_random_matrix");
	int THREADS = 256;
	int BLOCKS = (N*N + THREADS - 1) / THREADS;
  	caf::cuda::nd_range dim(BLOCKS,1, 1, THREADS,1, 1);

	//tag the arguments so that caf::cuda knows what to do with them	
         auto arg1 = caf::cuda::create_out_arg_with_size<int>(N*N); //output buffer indicate its size, caf::cuda will handle the rest
          auto arg2 = caf::cuda::create_in_arg(N*N); //matrix size
          auto arg3 = caf::cuda::create_in_arg(rand()); //seed
	  auto arg4 = caf::cuda::create_in_arg(9999); //max valux
          auto arg3B = caf::cuda::create_in_arg(rand()); //seed
	  
	  int device_number= std::rand(); //any commmand that uses this number will 
					  //guarantee that it stays on the same device 
					  //as other commands and other gpu actors 
					  //other device numbers may or may not run on the same gpu
					  //depending on how many they are as it will just do device_number % num_devices
					  //to pick the device



	  //launch kernels and collect their outputs
	  auto tempA = randomMatrix.run_async(
			  program, //kernel to launch
			  dim, //kernel dimensions
			  self -> state().id, //actor id 
			  0, //shared memory size in bytes
			  device_number, //device number 
			  arg1,arg2,arg3,arg4 //kernel arguments
			  );
	  auto tempB = randomMatrix.run_async(program,dim, self -> state().id,0,device_number,arg1,arg2,arg3B,arg4);
	  caf::cuda::mem_ptr<int> matrixA =  std::get<0>(tempA);
	  caf::cuda::mem_ptr<int> matrixB = std::get<0>(tempB);

	  /*
	   * Optional synchronize, as there is no guarantee that the data is 
	   * actually done being worked on
	   * but since each actor will use its own stream and device number
	   * we dont need to 
	  matrixA -> synchronize();
	  matrixB -> synchronize();
	  */
	  
	  //send to itself for matrix multiplication
          self->mail(matrixA,matrixB,N,device_number).send(self);

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

    std::vector<int> matrix1 = matrixA -> copy_to_host(); //copy to host transfers memory back to the device
    std::vector<int> matrix2 = matrixB -> copy_to_host();    
    std::vector<int> matrixC = caf::cuda::extract_vector<int>(tempC,2); //the output buffer that we want is at position 2 
									//as the command runner will always return the result values 
									//of in_out and out types in order that they appear in the launch 
									//since matrixA and matrixB where of out type orginally, they will get returned as well
									//

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

  //send a size to all actors 
  for (auto a : actors)
	  caf::anon_mail(matrix_size).send(a);

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
  caf::cuda::manager::init(sys); //be sure to initialize the manager
				 //it needs to do some things before running

  //run_mmul_test(sys,100,4000);
  //run_async_mmul_test(sys,100,700);

  // run the async (no-shared) suite:
  //benchmark_async_perf_all(sys);
}




CAF_MAIN()
