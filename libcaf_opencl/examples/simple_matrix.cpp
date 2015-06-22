/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <vector>
#include <iomanip>
#include <numeric>
#include <iostream>

#include "caf/all.hpp"
#include "caf/opencl/spawn_cl.hpp"

using namespace std;
using namespace caf;
using namespace caf::opencl;

using detail::limited_vector;

namespace {

using fvec = std::vector<float>;

constexpr size_t matrix_size = 8;
constexpr const char* kernel_name = "matrix_mult";

// opencl kernel, multiplies matrix1 and matrix2
// last parameter is, by convention, the output parameter
constexpr const char* kernel_source = R"__(
  __kernel void matrix_mult(__global float* matrix1,
                __global float* matrix2,
                __global float* output) {
    // we only use square matrices, hence: width == height
    size_t size = get_global_size(0); // == get_global_size_(1);
    size_t x = get_global_id(0);
    size_t y = get_global_id(1);
    float result = 0;
    for (size_t idx = 0; idx < size; ++idx) {
      result += matrix1[idx + y * size] * matrix2[x + idx * size];
    }
    output[x+y*size] = result;
  }
)__";

} // namespace <anonymous>

void print_as_matrix(const fvec& matrix) {
  for (size_t column = 0; column < matrix_size; ++column) {
    for (size_t row = 0; row < matrix_size; ++row) {
      cout << fixed << setprecision(2) << setw(9)
         << matrix[row + column * matrix_size];
    }
    cout << endl;
  }
}

void multiplier(event_based_actor* self) {
  // the opencl actor only understands vectors
  // so these vectors represent the matrices
  fvec m1(matrix_size * matrix_size);
  fvec m2(matrix_size * matrix_size);

  // fill each with ascending values
  iota(m1.begin(), m1.end(), 0);
  iota(m2.begin(), m2.end(), 0);

  // print "source" matrix
  cout << "calculating square of matrix:" << endl;
  print_as_matrix(m1);
  cout << endl;

  // spawn an opencl actor
  // 1st arg: source code of one or more kernels
  // 2nd arg: name of the kernel to use
  // 3rd arg: a spawn configuration that includes:
  //          - the global dimension arguments for opencl's enqueue
  //            creates matrix_size * matrix_size global work items
  //          - offsets for global dimensions (optional)
  //          - local dimensions (optional)
  // 4th to Nth arg: the kernel signature described by in/out/in_out classes
  //          that contain the argument type in their template, requires vectors
  auto worker = spawn_cl(kernel_source, kernel_name,
                         spawn_config{dim_vec{matrix_size, matrix_size}},
                         in<fvec>{}, in<fvec>{}, out<fvec>{});
  // send both matrices to the actor and wait for a result
  self->sync_send(worker, move(m1), move(m2)).then(
    [](const fvec& result) {
      cout << "result: " << endl;
      print_as_matrix(result);
    }
  );
}

int main() {
  announce<fvec>("float_vector");
  spawn(multiplier);
  await_all_actors_done();
  shutdown();
  return 0;
}
