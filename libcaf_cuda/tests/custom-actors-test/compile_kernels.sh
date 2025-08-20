#!/bin/bash
set -e

# Detect first GPU compute capability
ARCH=$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader,nounits | head -n1)
SM_ARCH="sm_${ARCH/./}"
echo "Using NVCC arch flag: $SM_ARCH"

# Compile mmul.cu to cubin in current directory
nvcc -arch=$SM_ARCH -cubin mmul.cu -o mmul.cubin
echo "Generated mmul.cubin"

# Compile genMatrix.cu to fatbin in current directory
nvcc -arch=$SM_ARCH --fatbin genMatrix.cu -o generate_random_matrix.fatbin -lcudadevrt -lcurand
echo "Generated generate_random_matrix.fatbin"

echo "All kernels compiled successfully!"

