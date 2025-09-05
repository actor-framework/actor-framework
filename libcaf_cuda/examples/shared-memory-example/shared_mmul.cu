extern "C" __global__
void matrixMul(const int* __restrict__ a,
               const int* __restrict__ b,
               int* __restrict__ c,
               int N)
{
    const int TILE = 32;
    int row = blockIdx.y * blockDim.y + threadIdx.y; // global row in C
    int col = blockIdx.x * blockDim.x + threadIdx.x; // global col in C

    __shared__ int s_a[TILE * TILE];
    __shared__ int s_b[TILE * TILE];

    int acc = 0;

    // Sweep tiles across the K dimension
    for (int i = 0; i < N; i += TILE) {
        int aCol = i + threadIdx.x;
        int bRow = i + threadIdx.y;

        if (row < N && aCol < N)
            s_a[threadIdx.y * TILE + threadIdx.x] = a[row * N + aCol];
        else
            s_a[threadIdx.y * TILE + threadIdx.x] = 0;

        if (bRow < N && col < N)
            s_b[threadIdx.y * TILE + threadIdx.x] = b[bRow * N + col];
        else
            s_b[threadIdx.y * TILE + threadIdx.x] = 0;

        __syncthreads();

        #pragma unroll
        for (int k = 0; k < TILE; ++k) {
            acc += s_a[threadIdx.y * TILE + k] *
                   s_b[k * TILE + threadIdx.x];
        }

        __syncthreads();
    }

    if (row < N && col < N)
        c[row * N + col] = acc;
}

extern "C" __global__
void matrixMulFloat(const float* __restrict__ a,
                    const float* __restrict__ b,
                    float* __restrict__ c,
                    int N)
{
    const int TILE = 32;
    int row = blockIdx.y * blockDim.y + threadIdx.y; // global row in C
    int col = blockIdx.x * blockDim.x + threadIdx.x; // global col in C

    __shared__ float s_a[TILE * TILE];
    __shared__ float s_b[TILE * TILE];

    float acc = 0.0f;

    // Sweep tiles across the K dimension
    for (int i = 0; i < N; i += TILE) {
        int aCol = i + threadIdx.x;
        int bRow = i + threadIdx.y;

        if (row < N && aCol < N)
            s_a[threadIdx.y * TILE + threadIdx.x] = a[row * N + aCol];
        else
            s_a[threadIdx.y * TILE + threadIdx.x] = 0.0f;

        if (bRow < N && col < N)
            s_b[threadIdx.y * TILE + threadIdx.x] = b[bRow * N + col];
        else
            s_b[threadIdx.y * TILE + threadIdx.x] = 0.0f;

        __syncthreads();

        #pragma unroll
        for (int k = 0; k < TILE; ++k) {
            acc += s_a[threadIdx.y * TILE + k] *
                   s_b[k * TILE + threadIdx.x];
        }

        __syncthreads();
    }

    if (row < N && col < N)
        c[row * N + col] = acc;
}

