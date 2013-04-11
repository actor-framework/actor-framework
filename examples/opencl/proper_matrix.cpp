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
        int x = get_global_id(0); // get position in matrix
        int y = get_global_id(1); // this work item should calculate
        int idx = 0;
        float result = 0;
        while (idx < size) { // calculate one element of the matrix
            float i = matrix1[idx+y*size];
            float j = matrix2[x+idx*size];
            float tmp = i*j;
            result = result + tmp;
            ++idx;
        }
        output[x+y*size] = result;
    }
)__"; }

// represensts a square matrix
template<size_t Size>
class square_matrix {

 public:

    static constexpr size_t num_elements = Size * Size;

    square_matrix(square_matrix&&) = default;
    square_matrix(const square_matrix&) = default;
    square_matrix& operator=(square_matrix&&) = default;
    square_matrix& operator=(const square_matrix&) = default;

    square_matrix() {
        m_data.resize(num_elements);
    }

    square_matrix(vector<float> d) : m_data(std::move(d)) { }

    square_matrix(const std::initializer_list<float>& args) : m_data(args) {
        m_data.resize(num_elements);
    }

    inline float& operator()(size_t row, size_t column) {
        return m_data[row + column * Size];
    }

    inline const float& operator()(size_t row, size_t column) const {
        return m_data[row + column * Size];
    }

    inline void zeroize() {
        std::fill(m_data.begin(), m_data.end(), 0);
    }

    inline void iota_fill() {
        std::iota(m_data.begin(), m_data.end(), 0);
    }

    typedef typename vector<float>::const_iterator const_iterator;

    const_iterator begin() const { return m_data.begin(); }

    const_iterator end() const { return m_data.end(); }

    vector<float>& data() { return m_data; }

    const vector<float>& data() const { return m_data; }

 private:

    vector<float> m_data;

};

// to annouce the square_matrix to lobccpa
// these operaters have to be implemented
template<size_t Size>
inline bool operator==(const square_matrix<Size>& lhs, const square_matrix<Size>& rhs) {
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template<size_t Size>
inline bool operator!=(const square_matrix<Size>& lhs, const square_matrix<Size>& rhs) {
    return !(lhs == rhs);
}

using matrix_type = square_matrix<matrix_size>;

void multiplier() {

    // create two matrix and fill them with values 0, 1, 2, 3, 4, ...
    matrix_type m1;
    matrix_type m2;
    m1.iota_fill();
    m2.iota_fill();

    // print "source" matrix
    cout << "calculating square of matrix:" << endl;
    for (size_t y = 0; y < matrix_size; ++y) {
        for (size_t x = 0; x < matrix_size; ++x) {
            cout << fixed << setprecision(2) << setw(6) << m1(x,y);
        }
        cout << endl;
    }
    cout << endl;

    // spawn an opencl actor
    // 1st arg: source code of one or more opencl kernels
    // 2nd arg: name of the kernel to use
    auto worker = spawn_cl(kernel_source, kernel_name,
        // 3rd arg: the opencl actor can only handle vectors,
        // this function creates vectors from a tuple of matrix_types
        [] (any_tuple msg) -> option<cow_tuple<vector<float>,vector<float>>> {
            auto opt = tuple_cast<matrix_type,matrix_type>(msg);
            if (opt) {
                return {move(get_ref<0>(*opt).data()),
                        move(get_ref<1>(*opt).data())};
            }
            return {};
        },
        // 4th arg: builds a matrix_type from a vector
        // allows the actor to pass a matrix_type back instead of a vector
        [] (vector<float>& result) -> any_tuple {
            return make_any_tuple<matrix_type>(move(result));
        },
        // 5th arg: global dimension arguments for opencl's enqueue
        // creates matrix_size * matrix_size global work items
        {matrix_size, matrix_size}
    );

    // send both matrices to the actor and
    // wait for results in form of a matrix_type
    sync_send(worker, move(m1), move(m2)).then(
        [](const matrix_type& result) {
            cout << "result:" << endl;
            for (size_t y = 0; y < matrix_size; ++y) {
                for (size_t x = 0; x < matrix_size; ++x) {
                    cout << fixed << setprecision(2) << setw(9) << result(x,y);
                }
                cout << endl;
            }
        }
    );
}

int main() {
    // matrix_type ist not a simple type,
    // it must be annouced to libcppa
    announce<matrix_type>();
    spawn(multiplier);
    await_all_others_done();
    shutdown();
    return 0;
}
