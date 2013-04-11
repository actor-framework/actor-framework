/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 * Raphael Hiesgen <raphael.hiesgen@haw-hamburg.de>                           *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/

#include <vector>
#include <iomanip>
#include <numeric>
#include <iostream>

#include "cppa/cppa.hpp"
#include "cppa/opencl.hpp"

using namespace std;
using namespace cppa;

static constexpr size_t matrix_size = 8;
static constexpr const char* kernel_name = "matrix_mult";

// opencl kernel, multiplies matrix1 and matrix2
namespace { constexpr const char* kernel_source = R"__(
    __kernel void matrix_mult(__global float* matrix1,
                              __global float* matrix2,
                              __global float* output) {
        int size = get_global_size(0); // == get_global_size_(1);
        int x = get_global_id(0);
        int y = get_global_id(1);
        int idx = 0;
        float result = 0;
        while (idx < size) {
            float i = matrix1[idx+y*size];
            float j = matrix2[x+idx*size];
            float tmp = i*j;
            result = result + tmp;
            ++idx;
        }
        output[x+y*size] = result;
    }
)__"; }

void multiplier() {
    // the opencl actor only understands vectors
    // so these vectors represent the matrices
    vector<float> m1(matrix_size * matrix_size);
    vector<float> m2(matrix_size * matrix_size);

    // fill each with values from 0 to matix_size * matrix_size - 1
    iota(m1.begin(), m1.end(), 0);
    iota(m2.begin(), m2.end(), 0);

    // print "source" matrix
    cout << "calculating square of matrix:" << endl;
    for (size_t y = 0; y < matrix_size; ++y) {
        for (size_t x = 0; x < matrix_size; ++x) {
            cout << fixed << setprecision(2) << setw(6) << m1[x+y*matrix_size];
        }
        cout << endl;
    }
    cout << endl;

    // spawn an opencl actor
    // 1st arg: sourec code of one or more kernels
    // 2nd arg: name of the kernel to use
    // 3rd arg: global dimension arguments for opencl's enqueue
    //          creates matrix_size * matrix_size global work items
    auto worker =
        spawn_cl<vector<float>(vector<float>&,vector<float>&)>(kernel_source,
                                                               kernel_name,
                                                               {matrix_size, matrix_size});
    // send both matrices to the actor and wait for a result
    sync_send(worker, move(m1), move(m2)).then(
        [](const vector<float>& result) {
            cout << "result:" << endl;
            for (size_t column = 0; column < matrix_size; ++column) {
                for (size_t row = 0; row < matrix_size; ++row) {
                    cout << fixed << setprecision(2) << setw(9)
                         << result[row+column*matrix_size];
                }
                cout << endl;
            }
        }

    );
}

int main() {
    announce<vector<float>>();
    spawn(multiplier);
    await_all_others_done();
    shutdown();
    return 0;
}
