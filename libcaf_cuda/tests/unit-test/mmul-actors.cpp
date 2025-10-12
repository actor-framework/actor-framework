//file not in use since I am not wasting time messing around with cmake


#include "main.test.hpp"

const char* matrixMulKernel = R"(
extern "C" __global__
void matrixMul(const int* a, const int* b, int* c, int N) {
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



// Check result on the CPU
void verify_result(vector<int> &a, vector<int> &b, vector<int> &c, int N) {
  // For every row...
  for (int i = 0; i < N; i++) {
    // For every column...
    for (int j = 0; j < N; j++) {
      // For every element in the row-column pair
      int tmp = 0;
      for (int k = 0; k < N; k++) {
        // Accumulate the partial results
        tmp += a[i * N + k] * b[k * N + j];
      }

      // Check against the CPU result
      assert(tmp == c[i * N + j]);
    }
  }
}
void test_mmul(caf::actor_system& sys) {
  caf::cuda::manager& mgr = caf::cuda::manager::get();

  // Matrix dimension (N x N)
  int N = 1024;
  int THREADS = 32;
  int BLOCKS = N / THREADS;

  // Setup kernel launch configuration
  caf::cuda::nd_range dim(BLOCKS, BLOCKS, 1, THREADS, THREADS, 1);

  // Spawn CUDA actor for matrix multiplication kernel
  auto gpuActor = mgr.spawn(matrixMulKernel, "matrixMul", dim,
                            in<int>{}, in<int>{}, out<int>{}, in<int>{});

  // Allocate and initialize host matrices
  std::vector<int> h_a(N * N);
  std::vector<int> h_b(N * N);
  std::vector<int> h_c(N * N, 0);  // initialized to 0
  std::vector<int> h_n(1, N);

  std::generate(h_a.begin(), h_a.end(), []() { return rand() % 10; });
  std::generate(h_b.begin(), h_b.end(), []() { return rand() % 10; });

  // Compose device arguments
  auto arg1 = caf::cuda::create_in_arg(h_a);
  auto arg2 = caf::cuda::create_in_arg(h_b);
  auto arg3 = caf::cuda::create_out_arg(h_c);
  auto arg4 = caf::cuda::create_in_arg(h_n);

  // Spawn an actor to send the message and receive the result
  sys.spawn([=](caf::event_based_actor* self_actor) {
    self_actor->mail(gpuActor, arg1, arg2, arg3, arg4)
      .request(gpuActor, 30s).then(
        [=](const std::vector<output_buffer>& outputs) {
          std::vector<int> result;
          bool got_output = false;

          // Extract the result from outputs
          for (const auto& out : outputs) {
            std::visit([&](const auto& vec) {
              if constexpr (std::is_same_v<std::decay_t<decltype(vec)>, std::vector<int>>) {
                result = vec;
                got_output = true;
              }
            }, out.data);
          }

          if (!got_output) {
            aout(self_actor) << "No output data received!\n";
          } else {
            aout(self_actor) << "Verifying result..." << std::endl;

            // Verify GPU result against CPU computation
            std::vector<int> expected(N * N);
            for (int i = 0; i < N; ++i) {
              for (int j = 0; j < N; ++j) {
                int tmp = 0;
                for (int k = 0; k < N; ++k) {
                  tmp += h_a[i * N + k] * h_b[k * N + j];
                }
                expected[i * N + j] = tmp;
              }
            }

            bool success = std::equal(result.begin(), result.end(), expected.begin());
            if (success) {
              aout(self_actor) << "Matrix multiplication result verified successfully!\n";
            } else {
              aout(self_actor) << "Mismatch found in matrix multiplication results!\n";
            }
          }

          self_actor->quit();
        }
      );
  });

  std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait for actor to complete
}




