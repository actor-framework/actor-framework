#include <curand_kernel.h>
// mmul.cu
extern "C" __global__
void matrixMul(const int* a, const int* b, int* c, int N) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    //printf("%d\n",N);
    if (row < N && col < N) {
        int temp = 0;
        for (int k = 0; k < N; ++k) {
            temp += a[row * N + k] * b[k * N + col];
        }
        c[row * N + col] = temp;
    }
}


//generate_random_matrix
extern "C" __global__
void generate_random_matrix(int* matrix, int total_elements, int seed, int max_val) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total_elements) return;

    curandState state;
    curand_init((unsigned long long)seed, idx, 0, &state);

    unsigned int r = curand(&state);
    matrix[idx] = r % max_val;
}


