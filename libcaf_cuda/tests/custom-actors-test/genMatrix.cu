
#include <curand_kernel.h>
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


