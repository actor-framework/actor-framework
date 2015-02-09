#include <vector>
#include <iomanip>
#include <cassert>
#include <iostream>
#include <algorithm>

#include "../../unit_testing/test.hpp"

#include "caf/all.hpp"
#include "caf/opencl/spawn_cl.hpp"

using namespace caf;
using namespace caf::opencl;

namespace {

using ivec = std::vector<int>;
using fvec = std::vector<float>;

constexpr size_t matrix_size = 4;
constexpr size_t array_size = 32;

constexpr int magic_number = 23;

constexpr const char* kernel_name = "matrix_square";
constexpr const char* kernel_name_compiler_flag = "compiler_flag";
constexpr const char* kernel_name_reduce = "reduce";
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

// http://developer.amd.com/resources/documentation-articles/articles-whitepapers/
// opencl-optimization-case-study-simple-reductions
constexpr const char* kernel_source_reduce = R"__(
  __kernel void reduce(__global int* buffer,
                       __global int* result) {
    __local int scratch[512];
    int local_index = get_local_id(0);
    scratch[local_index] = buffer[get_global_id(0)];
    barrier(CLK_LOCAL_MEM_FENCE);
    for(int offset = get_local_size(0) / 2; offset > 0; offset = offset / 2) {
      if (local_index < offset) {
        int other = scratch[local_index + offset];
        int mine = scratch[local_index];
        scratch[local_index] = (mine < other) ? mine : other;
      }
      barrier(CLK_LOCAL_MEM_FENCE);
    }
    if (local_index == 0) {
      result[get_group_id(0)] = scratch[0];
    }
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
    caf::announce<square_matrix>("square_matrix", &square_matrix::m_data);
  }

  square_matrix(square_matrix&&) = default;
  square_matrix(const square_matrix&) = default;
  square_matrix& operator=(square_matrix&&) = default;
  square_matrix& operator=(const square_matrix&) = default;

  square_matrix() : m_data(num_elements) {
    // nop
  }

  explicit square_matrix(ivec d) : m_data(move(d)) {
    assert(m_data.size() == num_elements);
  }

  float& operator()(size_t column, size_t row) {
    return m_data[column + row * Size];
  }

  const float& operator()(size_t column, size_t row) const {
    return m_data[column + row * Size];
  }

  typedef typename ivec::const_iterator const_iterator;

  const_iterator begin() const {
    return m_data.begin();
  }

  const_iterator end() const {
    return m_data.end();
  }

  ivec& data() {
    return m_data;
  }

  const ivec& data() const {
    return m_data;
  }

  void data(ivec new_data) {
    m_data = std::move(new_data);
  }

 private:
  ivec m_data;
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
  return !(lhs == rhs);
}

using matrix_type = square_matrix<matrix_size>;

size_t get_max_workgroup_size(size_t device_id, size_t dimension) {
  size_t max_size = 512;
  auto devices = opencl_metainfo::instance()->get_devices()[device_id];
  size_t dimsize = devices.get_max_work_items_per_dim()[dimension];
  return max_size < dimsize ? max_size : dimsize;
}

void test_opencl() {
  scoped_actor self;
  const ivec expected1{ 56,  62,  68,  74,
                       152, 174, 196, 218,
                       248, 286, 324, 362,
                       344, 398, 452, 506};
  auto w1 = spawn_cl<ivec (ivec&)>(program::create(kernel_source),
                                   kernel_name,
                                   {matrix_size, matrix_size});
  self->send(w1, make_iota_vector<int>(matrix_size * matrix_size));
  self->receive (
    [&](const ivec& result) {
      CAF_CHECK(result == expected1);
    }
  );
  auto w2 = spawn_cl<ivec (ivec&)>(kernel_source, kernel_name,
                                   {matrix_size, matrix_size});
  self->send(w2, make_iota_vector<int>(matrix_size * matrix_size));
  self->receive (
    [&](const ivec& result) {
      CAF_CHECK(result == expected1);
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
  auto w3 = spawn_cl<ivec (ivec&)>(program::create(kernel_source), kernel_name,
                                   map_arg, map_res, {matrix_size, matrix_size});
  self->send(w3, make_iota_matrix<matrix_size>());
  self->receive (
    [&](const matrix_type& result) {
      CAF_CHECK(expected2 == result);
    }
  );
  auto w4 = spawn_cl<ivec (ivec&)>(kernel_source, kernel_name,
                                   map_arg, map_res,
                                   {matrix_size, matrix_size});
  self->send(w4, make_iota_matrix<matrix_size>());
  self->receive (
    [&](const matrix_type& result) {
      CAF_CHECK(expected2 == result);
    }
  );
  try {
    auto create_error = program::create(kernel_source_error);
  }
  catch (const std::exception& exc) {
    CAF_PRINT(exc.what());
    CAF_CHECK_EQUAL("clBuildProgram: CL_BUILD_PROGRAM_FAILURE", exc.what());
  }
  // test for opencl compiler flags
  auto prog5 = program::create(kernel_source_compiler_flag, compiler_flag);
  auto w5 = spawn_cl<ivec (ivec&)>(prog5, kernel_name_compiler_flag, {array_size});
  self->send(w5, make_iota_vector<int>(array_size));
  auto expected3 = make_iota_vector<int>(array_size);
  self->receive (
    [&](const ivec& result) {
      CAF_CHECK(result == expected3);
    }
  );

  // test for manuel return size selection (max workgroup size 1d)
  const int max_workgroup_size = static_cast<int>(get_max_workgroup_size(0,1));
  const int reduce_buffer_size = max_workgroup_size * 8;
  const int reduce_local_size  = max_workgroup_size;
  const int reduce_work_groups = reduce_buffer_size / reduce_local_size;
  const int reduce_global_size = reduce_buffer_size;
  const int reduce_result_size = reduce_work_groups;
  ivec arr6(static_cast<size_t>(reduce_buffer_size));
  int n = static_cast<int>(arr6.capacity());
  std::generate(arr6.begin(), arr6.end(), [&]{ return --n; });
  auto w6 = spawn_cl<ivec (ivec&)>(kernel_source_reduce, kernel_name_reduce,
                                   {static_cast<size_t>(reduce_global_size)},
                                   {},
                                   {static_cast<size_t>(reduce_local_size)},
                                   static_cast<size_t>(reduce_result_size));
  self->send(w6, move(arr6));
  ivec expected4{max_workgroup_size * 7, max_workgroup_size * 6,
                 max_workgroup_size * 5, max_workgroup_size * 4,
                 max_workgroup_size * 3, max_workgroup_size * 2,
                 max_workgroup_size,     0};
  self->receive(
    [&](const ivec& result) {
      CAF_CHECK(result == expected4);
    }
  );
  // constant memory arguments
  const ivec arr7{magic_number};
  auto w7 = spawn_cl<ivec (ivec&)>(kernel_source_const, kernel_name_const,
                                   {magic_number});
  self->send(w7, move(arr7));
  ivec expected5(magic_number);
  fill(begin(expected5), end(expected5), magic_number);
  self->receive(
    [&](const ivec& result) {
      CAF_CHECK(result == expected5);
    }
  );
}

int main() {
  CAF_TEST(test_opencl);
  test_opencl();
  await_all_actors_done();
  shutdown();
  return CAF_TEST_RESULT();
}
