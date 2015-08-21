#define CAF_SUITE opencl
#include "caf/test/unit_test.hpp"

#include <vector>
#include <iomanip>
#include <cassert>
#include <iostream>
#include <algorithm>

#include "caf/all.hpp"
#include "caf/opencl/spawn_cl.hpp"

CAF_PUSH_NO_DEPRECATED_WARNING

using namespace caf;
using namespace caf::opencl;

using detail::limited_vector;

namespace {

using ivec = std::vector<int>;

constexpr size_t matrix_size = 4;
constexpr size_t array_size = 32;

constexpr int magic_number = 23;

constexpr const char* kernel_name = "matrix_square";
constexpr const char* kernel_name_compiler_flag = "compiler_flag";
constexpr const char* kernel_name_const = "const_mod";

constexpr const char* compiler_flag = "-D CAF_OPENCL_TEST_FLAG";

constexpr const char* kernel_source = R"__(
  __kernel void matrix_square(__global int* matrix,
                              __global int* output) {
    size_t size = get_global_size(0); // == get_global_size_(1);
    size_t x = get_global_id(0);
    size_t y = get_global_id(1);
    int result = 0;
    for (size_t idx = 0; idx < size; ++idx) {
      result += matrix[idx + y * size] * matrix[x + idx * size];
    }
    output[x + y * size] = result;
  }
)__";

constexpr const char* kernel_source_error = R"__(
  __kernel void missing(__global int*) {
    size_t semicolon
  }
)__";

constexpr const char* kernel_source_compiler_flag = R"__(
  __kernel void compiler_flag(__global int* input,
                              __global int* output) {
    size_t x = get_global_id(0);
#   ifdef CAF_OPENCL_TEST_FLAG
    output[x] = input[x];
#   else
    output[x] = 0;
#   endif
  }
)__";

constexpr const char* kernel_source_const = R"__(
  __kernel void const_mod(__constant int* input,
                          __global int* output) {
    size_t idx = get_global_id(0);
    output[idx] = input[0];
  }
)__";

} // namespace <anonymous>

template<size_t Size>
class square_matrix {
public:
  static constexpr size_t num_elements = Size * Size;

  static void announce() {
    caf::announce<square_matrix>("square_matrix", &square_matrix::data_);
  }

  square_matrix(square_matrix&&) = default;
  square_matrix(const square_matrix&) = default;
  square_matrix& operator=(square_matrix&&) = default;
  square_matrix& operator=(const square_matrix&) = default;

  square_matrix() : data_(num_elements) {
    // nop
  }

  explicit square_matrix(ivec d) : data_(move(d)) {
    assert(data_.size() == num_elements);
  }

  float& operator()(size_t column, size_t row) {
    return data_[column + row * Size];
  }

  const float& operator()(size_t column, size_t row) const {
    return data_[column + row * Size];
  }

  typedef typename ivec::const_iterator const_iterator;

  const_iterator begin() const {
    return data_.begin();
  }

  const_iterator end() const {
    return data_.end();
  }

  ivec& data() {
    return data_;
  }

  const ivec& data() const {
    return data_;
  }

  void data(ivec new_data) {
    data_ = std::move(new_data);
  }

private:
  ivec data_;
};


template <class T>
std::vector<T> make_iota_vector(size_t num_elements) {
  std::vector<T> result;
  result.resize(num_elements);
  std::iota(result.begin(), result.end(), T{0});
  return result;
}

template <size_t Size>
square_matrix<Size> make_iota_matrix() {
  square_matrix<Size> result;
  std::iota(result.data().begin(), result.data().end(), 0);
  return result;
}

template<size_t Size>
bool operator==(const square_matrix<Size>& lhs,
                const square_matrix<Size>& rhs) {
  return lhs.data() == rhs.data();
}

template<size_t Size>
bool operator!=(const square_matrix<Size>& lhs,
                const square_matrix<Size>& rhs) {
  return ! (lhs == rhs);
}

using matrix_type = square_matrix<matrix_size>;

template <class T>
void check_vector_results(const std::string& description,
                          const std::vector<T>& expected,
                          const std::vector<T>& result) {
  auto cond = (expected == result);
  CAF_CHECK(cond);
  if (! cond) {
    CAF_TEST_INFO(description << " failed.");
    std::cout << "Expected: " << std::endl;
    for (size_t i = 0; i < expected.size(); ++i) {
      std::cout << " " << expected[i];
    }
    std::cout << std::endl << "Received: " << std::endl;
    for (size_t i = 0; i < result.size(); ++i) {
      std::cout << " " << result[i];
    }
    std::cout << std::endl;
  }
}

void test_opencl_deprecated() {
  scoped_actor self;
  const ivec expected1{ 56,  62,  68,  74,
                       152, 174, 196, 218,
                       248, 286, 324, 362,
                       344, 398, 452, 506};
  auto w1 = spawn_cl<ivec (ivec)>(program::create(kernel_source),
                                  kernel_name,
                                  limited_vector<size_t, 3>{matrix_size,
                                                             matrix_size});
  self->send(w1, make_iota_vector<int>(matrix_size * matrix_size));
  self->receive (
    [&](const ivec& result) {
      check_vector_results("Simple matrix multiplication using vectors"
                           "(kernel wrapped in program)",result, expected1);
    }
  );
  auto w2 = spawn_cl<ivec (ivec)>(kernel_source, kernel_name,
                                  limited_vector<size_t, 3>{matrix_size,
                                                            matrix_size});
  self->send(w2, make_iota_vector<int>(matrix_size * matrix_size));
  self->receive (
    [&](const ivec& result) {
      check_vector_results("Simple matrix multiplication using vectors",
                           expected1, result);
    }
  );
  const matrix_type expected2(std::move(expected1));
  auto map_arg = [](message& msg) -> optional<message> {
    return msg.apply(
      [](matrix_type& mx) {
        return make_message(std::move(mx.data()));
      }
    );
  };
  auto map_res = [](ivec& result) -> message {
    return make_message(matrix_type{std::move(result)});
  };
  auto w3 = spawn_cl<ivec (ivec)>(program::create(kernel_source),
                                  kernel_name, map_arg, map_res,
                                  limited_vector<size_t, 3>{matrix_size,
                                                            matrix_size});
  self->send(w3, make_iota_matrix<matrix_size>());
  self->receive (
    [&](const matrix_type& result) {
      check_vector_results("Matrix multiplication with user defined type "
                           "(kernel wrapped in program)",
                           expected2.data(), result.data());
    }
  );
  auto w4 = spawn_cl<ivec (ivec)>(kernel_source, kernel_name,
                                  map_arg, map_res,
                                  limited_vector<size_t, 3>{matrix_size,
                                                            matrix_size});
  self->send(w4, make_iota_matrix<matrix_size>());
  self->receive (
    [&](const matrix_type& result) {
      check_vector_results("Matrix multiplication with user defined type",
                           expected2.data(), result.data());
    }
  );
  CAF_TEST_INFO("Expecting exception (compiling invalid kernel, "
                "semicolon is missing).");
  try {
    auto create_error = program::create(kernel_source_error);
  }
  catch (const std::exception& exc) {
    auto cond = (strcmp("clBuildProgram: CL_BUILD_PROGRAM_FAILURE",
                        exc.what()) == 0);
      CAF_CHECK(cond);
      if (! cond) {
        CAF_TEST_INFO("Wrong exception cought for program build failure.");
      }
  }
  // test for opencl compiler flags
  auto prog5 = program::create(kernel_source_compiler_flag, compiler_flag);
  auto w5 = spawn_cl<ivec (ivec)>(prog5, kernel_name_compiler_flag,
                                  limited_vector<size_t, 3>{array_size});
  self->send(w5, make_iota_vector<int>(array_size));
  auto expected3 = make_iota_vector<int>(array_size);
  self->receive (
    [&](const ivec& result) {
      check_vector_results("Passing compiler flags", expected3, result);
    }
  );
  // constant memory arguments
  const ivec arr7{magic_number};
  auto w7 = spawn_cl<ivec (ivec)>(kernel_source_const, kernel_name_const,
                                  limited_vector<size_t, 3>{magic_number});
  self->send(w7, move(arr7));
  ivec expected5(magic_number);
  fill(begin(expected5), end(expected5), magic_number);
  self->receive(
    [&](const ivec& result) {
      check_vector_results("Using const input argument", expected5, result);
    }
  );
}

CAF_TEST(test_opencl_deprecated) {
  std::cout << "Staring deprecated OpenCL test" << std::endl;
  announce<ivec>("ivec");
  matrix_type::announce();
  test_opencl_deprecated();
  await_all_actors_done();
  shutdown();
  std::cout << "Done with depreacted OpenCL test" << std::endl;
}

CAF_POP_WARNINGS
