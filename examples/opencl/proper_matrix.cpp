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
 * Copyright (C) 2011-2014                                                    *
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
#include <cassert>
#include <iostream>

#include "cppa/cppa.hpp"
#include "cppa/opencl.hpp"

using namespace std;
using namespace cppa;

namespace {

using fvec = vector<float>;

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

template<size_t Size>
class square_matrix {

 public:

    static constexpr size_t num_elements = Size * Size;

    square_matrix(square_matrix&&) = default;
    square_matrix(const square_matrix&) = default;
    square_matrix& operator=(square_matrix&&) = default;
    square_matrix& operator=(const square_matrix&) = default;

    square_matrix() : m_data(num_elements) { }

    explicit square_matrix(fvec d) : m_data(move(d)) {
        assert(m_data.size() == num_elements);

    }

    inline float& operator()(size_t column, size_t row) {
        return m_data[column + row * Size];
    }

    inline const float& operator()(size_t column, size_t row) const {
        return m_data[column + row * Size];
    }

    inline void iota_fill() {
        iota(m_data.begin(), m_data.end(), 0);
    }

    typedef typename fvec::const_iterator const_iterator;

    const_iterator begin() const { return m_data.begin(); }

    const_iterator end() const { return m_data.end(); }

    fvec& data() { return m_data; }

    const fvec& data() const { return m_data; }

 private:

    fvec m_data;

};

template<size_t Size>
string to_string(const square_matrix<Size>& m) {
    ostringstream oss;
    oss.fill(' ');
    for (size_t row = 0; row < Size; ++row) {
        for (size_t column = 0; column < Size; ++column) {
            oss << fixed << setprecision(2) << setw(9) << m(column, row);
        }
        oss << '\n';
    }
    return oss.str();
}

// to annouce the square_matrix to libcppa, we have to define these operators
template<size_t Size>
inline bool operator==(const square_matrix<Size>& lhs,
                       const square_matrix<Size>& rhs) {
    return equal(lhs.begin(), lhs.end(), rhs.begin());
}

template<size_t Size>
inline bool operator!=(const square_matrix<Size>& lhs,
                       const square_matrix<Size>& rhs) {
    return !(lhs == rhs);
}

using matrix_type = square_matrix<matrix_size>;

void multiplier(event_based_actor* self) {

    // create two matrices with ascending values
    matrix_type m1;
    m1.iota_fill();
    auto m2 = m1;

    // print "source" matrix
    cout << "calculating square of matrix:" << endl
         << to_string(m1) << endl;

    // spawn an opencl actor
    // 1st arg: source code of one or more opencl kernels
    // 2nd arg: name of the kernel to use
    auto worker = spawn_cl(kernel_source, kernel_name,
        // 3rd arg: the opencl function operates on vectors,
        //          this function converts a tuple of two matrices
        //          to a tuple of vectors; note that this function returns
        //          an option (an empty results causes the actor to ignore
        //          the message)
        [] (any_tuple msg) -> optional<cow_tuple<fvec, fvec>> {
            auto opt = tuple_cast<matrix_type, matrix_type>(msg);
            if (opt) {
                return make_cow_tuple(move(get_ref<0>(*opt).data()),
                                      move(get_ref<1>(*opt).data()));
            }
            return none;
        },
        // 4th arg: converts the ouptut vector back to a matrix that is then
        //          used as response message
        [] (fvec& result) -> any_tuple {
            return make_any_tuple(matrix_type{move(result)});
        },
        // 5th arg: global dimension arguments for opencl's enqueue,
        //          creates matrix_size * matrix_size global work items
        {matrix_size, matrix_size}
    );

    // send both matrices to the actor and
    // wait for results in form of a matrix_type
    self->sync_send(worker, move(m1), move(m2)).then(
        [](const matrix_type& result) {
            cout << "result:" << endl << to_string(result);
        }
    );
}

int main() {
    // matrix_type ist not a simple type,
    // it must be annouced to libcppa
    announce<matrix_type>();
    spawn(multiplier);
    await_all_actors_done();
    shutdown();
    return 0;
}
