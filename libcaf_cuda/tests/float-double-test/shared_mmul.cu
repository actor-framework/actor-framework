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

        // Each thread loads one element into shared memory (with bounds checks)
        int aCol = i + threadIdx.x;
        int bRow = i + threadIdx.y;

        // s_a[y, x] = a[row, aCol] if in range, else 0
        if (row < N && aCol < N)
            s_a[threadIdx.y * TILE + threadIdx.x] = a[row * N + aCol];
        else
            s_a[threadIdx.y * TILE + threadIdx.x] = 0;

        // s_b[y, x] = b[bRow, col] if in range, else 0
        if (bRow < N && col < N)
            s_b[threadIdx.y * TILE + threadIdx.x] = b[bRow * N + col];
        else
            s_b[threadIdx.y * TILE + threadIdx.x] = 0;

        __syncthreads();

        // Compute partial dot product for this tile
        #pragma unroll
        for (int k = 0; k < TILE; ++k) {
            acc += s_a[threadIdx.y * TILE + k] *
                   s_b[k * TILE + threadIdx.x];
        }

        __syncthreads();
    }

    // Final write (guarded)
    if (row < N && col < N)
        c[row * N + col] = acc;
}

