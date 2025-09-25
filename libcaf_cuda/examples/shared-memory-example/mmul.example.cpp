/* An example file where we demonstrate how to use cuda actors using
 * the entry point command runner, enabling the user to create their own custom gpu
 * actor and use shared memory on the gpu's
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
using mmulFloatCommand = caf::cuda::command_runner<
      in<float>, //matrixA a readonly float buffer
      in<float>, //matrixB a readonly float buffer
      out<float>, //matrixC a writeonly float buffer, you can just pass in integer for size and it will automatically allocate a buffer for you on the gpu 
      in<int> //int N, the size of the matrices, you can just pass in a single integer and it will recognize this 
      >;


using matrixGenFloatCommand = caf::cuda::command_runner<
      out<float>, //writeonly matrix buffer 
      in<int>, //total elements 
      in<int>, //seed
      in<int> //max value
      >;

using mmulAsyncFloatCommand = caf::cuda::command_runner<
      caf::cuda::mem_ptr<float>, //handle on the gpu that points to a float matrix
      caf::cuda::mem_ptr<float>, //handle on the gpu that points to a float matrix
      out<float>, // MatrixC a Writeonly float buffer
      in<int> //Size of the matrices
      >;


//floating point type commands 
mmulFloatCommand mmulFloat;
matrixGenFloatCommand randomFloatMatrix;
mmulAsyncFloatCommand mmulFloatAsync;




//used to verify the results
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



//A Custom GPU Actor that generates 2 random matrices and then sends it to itself 
//and will do shared memory matrix multiplication on it and send it to 
//itself to verify the result 
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
         auto arg1 = caf::cuda::create_out_arg_with_size<float>(N*N); //output buffer indicate its size, caf::cuda will handle the rest
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
          auto tempA = randomFloatMatrix.run_async(
                          program, //kernel to launch
                          dim, //kernel dimensions
                          self -> state().id, //actor id 
                          0, //shared memory size in bytes
                          device_number, //device number 
                          arg1,arg2,arg3,arg4 //kernel arguments
                          );
          auto tempB = randomFloatMatrix.run_async(program,dim, self -> state().id,0,device_number,arg1,arg2,arg3B,arg4);
          caf::cuda::mem_ptr<float> matrixA =  std::get<0>(tempA);
          caf::cuda::mem_ptr<float> matrixB = std::get<0>(tempB);

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

    // 2nd handler: shared-mem float mmul and verify
    [=](const caf::cuda::mem_ptr<float> matrixA,
        const caf::cuda::mem_ptr<float> matrixB, int N, int device_number) {

      caf::cuda::manager& mgr = caf::cuda::manager::get();

      auto program = mgr.create_program_from_cubin("../shared_mmul.cubin", "matrixMulFloat");
      const int THREADS = 32;
      const int BLOCKS  = (N + THREADS - 1) / THREADS;
      caf::cuda::nd_range dims(BLOCKS, BLOCKS, 1, THREADS, THREADS, 1);

      int shared_mem = 8192; //2 arrays of 32*32 elements = 8192 bytes 

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

    // 3th handler: CPU verify (float)
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

  for (auto a: actors)
	  caf::anon_mail(matrix_size).send(a);

   sys.await_all_actors_done();
}


void caf_main(caf::actor_system& sys) {
  caf::cuda::manager::init(sys);

  run_async_mmul_test(sys,100,30);
   //run_async_mmul_perf_test(sys,1024,1);

  // run the async (no-shared) suite:
  //benchmark_async_perf_all(sys);

  // run the shared-memory suite:
  //benchmark_shared_perf_all(sys);
}




CAF_MAIN()
