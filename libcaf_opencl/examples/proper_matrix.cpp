/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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
#include <cassert>
#include <iostream>

#include "caf/all.hpp"
#include "caf/detail/limited_vector.hpp"

#include "caf/opencl/all.hpp"

using namespace std;
using namespace caf;
using namespace caf::opencl;

using caf::detail::limited_vector;

namespace {

using fvec = vector<float>;

constexpr size_t matrix_size = 8;
constexpr const char* kernel_name = "matrix_mult";

// opencl kernel, multiplies matrix1 and matrix2
// last parameter is, by convention, the output parameter
constexpr const char* kernel_source = R"__(
  kernel void matrix_mult(global const float* matrix1,
                          global const float* matrix2,
                          global       float* output) {
    // we only use square matrices, hence: width == height
    size_t size = get_global_size(0); // == get_global_size_(1);
    size_t x = get_global_id(0);
    size_t y = get_global_id(1);
    float result = 0;
    for (size_t idx = 0; idx < size; ++idx)
      result += matrix1[idx + y * size] * matrix2[x + idx * size];
    output[x+y*size] = result;
  }
)__";

} // namespace <anonymous>

template<size_t Size>
class square_matrix {
public:
  using value_type = fvec::value_type;
  static constexpr size_t num_elements = Size * Size;

  // allows serialization
  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 square_matrix& m) {
    return f(meta::type_name("square_matrix"), m.data_);
  }

  square_matrix(square_matrix&&) = default;
  square_matrix(const square_matrix&) = default;
  square_matrix& operator=(square_matrix&&) = default;
  square_matrix& operator=(const square_matrix&) = default;

  square_matrix() : data_(num_elements) { }

  explicit square_matrix(fvec d) : data_(move(d)) {
    assert(data_.size() == num_elements);
  }

  inline float& operator()(size_t column, size_t row) {
    return data_[column + row * Size];
  }

  inline const float& operator()(size_t column, size_t row) const {
    return data_[column + row * Size];
  }

  inline void iota_fill() {
    iota(data_.begin(), data_.end(), 0);
  }

  using const_iterator = typename fvec::const_iterator;

  const_iterator begin() const { return data_.begin(); }

  const_iterator end() const { return data_.end(); }

  fvec& data() { return data_; }

  const fvec& data() const { return data_; }

private:
  fvec data_;
};

template<size_t Size>
string to_string(const square_matrix<Size>& m) {
  ostringstream oss;
  oss.fill(' ');
  for (size_t row = 0; row < Size; ++row) {
    for (size_t column = 0; column < Size; ++column)
      oss << fixed << setprecision(2) << setw(9) << m(column, row);
    oss << '\n';
  }
  return oss.str();
}

// to annouce the square_matrix to libcaf, we have to define these operators
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
  auto& mngr = self->system().opencl_manager();

  // create two matrices with ascending values
  matrix_type m1;
  m1.iota_fill();
  auto m2 = m1;

  // print "source" matrix
  cout << "calculating square of matrix:" << endl
       << to_string(m1) << endl;

  auto unbox_args = [](message& msg) -> optional<message> {
    return msg.apply([](matrix_type& lhs, matrix_type& rhs) {
      return make_message(std::move(lhs.data()), std::move(rhs.data()));
    });
  };

  auto box_res = [] (fvec& result) -> message {
    return make_message(matrix_type{move(result)});
  };

  // spawn an opencl actor
  // 1st arg: source code of one or more opencl kernels
  // 2nd arg: name of the kernel to use
  // 3rd arg: the config specifies how many dimensions the kernel uses and how
  //          many work items are created, creates matrix_size * matrix_size
  //          global work items in this case
  // 4th arg: the opencl function operates on vectors, this function converts
  //          a tuple of two matrices to a tuple of vectors; note that this
  //          function returns an option (an empty results causes the actor to
  //          ignore the message)
  // 5th arg: converts the ouptut vector back to a matrix that is then
  //          used as response message
  // from 6 : a description of the kernel signature using in/out/in_out classes
  //          with the argument type. Since the actor always expects input
  //          arguments for global memory to be contained in vectors,
  //          the vector is omitted here.
  auto worker = mngr.spawn(kernel_source, kernel_name,
                           nd_range{dim_vec{matrix_size, matrix_size}},
                           unbox_args, box_res,
                           in<float>{}, in<float>{}, out<float>{});

  // send both matrices to the actor and
  // wait for results in form of a matrix_type
  self->request(worker, chrono::seconds(5), move(m1), move(m2)).then(
    [](const matrix_type& result) {
      cout << "result:" << endl << to_string(result);
    }
  );
}

int main() {
  // matrix_type ist not a simple type,
  // it must be annouced to libcaf
  actor_system_config cfg;
  cfg.load<opencl::manager>()
    .add_message_type<fvec>("float_vector")
    .add_message_type<matrix_type>("square_matrix");
  actor_system system{cfg};
  system.spawn(multiplier);
  system.await_all_actors_done();
  return 0;
}
