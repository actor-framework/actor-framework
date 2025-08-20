
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


extern "C" __global__
void generate_random_matrix_float(float* matrix, int total_elements, int seed, float max_val) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total_elements) return;

    curandState state;
    curand_init((unsigned long long)seed, idx, 0, &state);

    unsigned int r = curand(&state);
    matrix[idx] = (float)(r % (unsigned int)max_val);
}


extern "C" __global__
void generate_random_matrix_double(double* matrix, int total_elements, int seed, double max_val) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total_elements) return;

    curandState state;
    curand_init((unsigned long long)seed, idx, 0, &state);

    unsigned int r = curand(&state);
    matrix[idx] = (double)(r % (unsigned int)max_val);
}



