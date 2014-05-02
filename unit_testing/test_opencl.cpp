#include <vector>
#include <iomanip>
#include <cassert>
#include <iostream>
#include <algorithm>

#include "test.hpp"

#include "cppa/cppa.hpp"
#include "cppa/opencl.hpp"

using namespace std;
using namespace cppa;
using namespace cppa::opencl;

namespace {

using ivec = vector<int>;
using fvec = vector<float>;

constexpr size_t matrix_size = 4;
constexpr size_t array_size = 32;

constexpr int magic_number = 23;

constexpr const char* kernel_name = "matrix_square";
constexpr const char* kernel_name_compiler_flag = "compiler_flag";
constexpr const char* kernel_name_reduce = "reduce";
constexpr const char* kernel_name_const = "const_mod";

constexpr const char* compiler_flag = "-D OPENCL_CPPA_TEST_FLAG";

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
#ifdef OPENCL_CPPA_TEST_FLAG
        output[x] = input[x];
#else
        output[x] = 0;
#endif
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

}

template<size_t Size>
class square_matrix {

 public:

    static constexpr size_t num_elements = Size * Size;

    static void announce() {
        cppa::announce<square_matrix>(&square_matrix::m_data);
    }

    square_matrix(square_matrix&&) = default;
    square_matrix(const square_matrix&) = default;
    square_matrix& operator=(square_matrix&&) = default;
    square_matrix& operator=(const square_matrix&) = default;

    square_matrix() : m_data(num_elements) { }

    explicit square_matrix(ivec d) : m_data(move(d)) {
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

    typedef typename ivec::const_iterator const_iterator;

    const_iterator begin() const { return m_data.begin(); }

    const_iterator end() const { return m_data.end(); }

    ivec& data() { return m_data; }

    const ivec& data() const { return m_data; }

    void data(ivec new_data) { m_data = std::move(new_data); }

 private:

    ivec m_data;

};

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

size_t get_max_workgroup_size(size_t device_id, size_t dimension) {
    size_t max_size = 512;
    auto devices = get_opencl_metainfo()->get_devices()[device_id];
    size_t dimsize = devices.get_max_work_items_per_dim()[dimension];
    return max_size < dimsize ? max_size : dimsize;
}

void test_opencl() {

    scoped_actor self;

    const ivec expected1{  56,  62,  68,  74
                        , 152, 174, 196, 218
                        , 248, 286, 324, 362
                        , 344, 398, 452, 506};

    auto worker1 = spawn_cl<ivec(ivec&)>(program::create(kernel_source),
                                         kernel_name,
                                         {matrix_size, matrix_size});
    ivec m1(matrix_size * matrix_size);
    iota(begin(m1), end(m1), 0);
    self->send(worker1, move(m1));
    self->receive (
        on_arg_match >> [&] (const ivec& result) {
            CPPA_CHECK(equal(begin(expected1), end(expected1), begin(result)));
        }
    );

    auto worker2 = spawn_cl<ivec(ivec&)>(kernel_source,
                                         kernel_name,
                                         {matrix_size, matrix_size});
    ivec m2(matrix_size * matrix_size);
    iota(begin(m2), end(m2), 0);
    self->send(worker2, move(m2));
    self->receive (
        on_arg_match >> [&] (const ivec& result) {
            CPPA_CHECK(equal(begin(expected1), end(expected1), begin(result)));
        }
    );


    const matrix_type expected2(move(expected1));

    auto map_args = [] (any_tuple msg) -> optional<cow_tuple<ivec>> {
        auto opt = tuple_cast<matrix_type>(msg);
        if (opt) {
            return make_cow_tuple(move(get_ref<0>(*opt).data()));
        }
        return none;
    };

    auto map_results = [] (ivec& result) -> any_tuple {
        return make_any_tuple(matrix_type{move(result)});
    };

    matrix_type m3;
    m3.iota_fill();
    auto worker3 = spawn_cl(program::create(kernel_source), kernel_name,
                            map_args, map_results,
                            {matrix_size, matrix_size});
    self->send(worker3, move(m3));
    self->receive (
        on_arg_match >> [&] (const matrix_type& result) {
            CPPA_CHECK(expected2 == result);
        }
    );

    matrix_type m4;
    m4.iota_fill();
    auto worker4 = spawn_cl(kernel_source, kernel_name,
                            map_args, map_results,
                            {matrix_size, matrix_size}
    );
    self->send(worker4, move(m4));
    self->receive (
        on_arg_match >> [&] (const matrix_type& result) {
            CPPA_CHECK(expected2 == result);
        }
    );

    try {
        program create_error = program::create(kernel_source_error);
    }
    catch (const exception& exc) {
        cout << exc.what() << endl;
        CPPA_CHECK_EQUAL("clBuildProgram: CL_BUILD_PROGRAM_FAILURE", exc.what());
    }

    // test for opencl compiler flags
    ivec arr5(array_size);
    iota(begin(arr5), end(arr5), 0);
    auto prog5 = program::create(kernel_source_compiler_flag, compiler_flag);
    auto worker5 = spawn_cl<ivec(ivec&)>(prog5, kernel_name_compiler_flag, {array_size});
    self->send(worker5, move(arr5));

    ivec expected3(array_size);
    iota(begin(expected3), end(expected3), 0);
    self->receive (
        on_arg_match >> [&] (const ivec& result) {
            CPPA_CHECK(equal(begin(expected3), end(expected3), begin(result)));
        }
    );

    // test for manuel return size selection
    const int max_workgroup_size = static_cast<int>(get_max_workgroup_size(0,1)); // max workgroup size (1d)
    const size_t reduce_buffer_size = max_workgroup_size * 8;
    const size_t reduce_local_size  = max_workgroup_size;
    const size_t reduce_work_groups = reduce_buffer_size / reduce_local_size;
    const size_t reduce_global_size = reduce_buffer_size;
    const size_t reduce_result_size = reduce_work_groups;

    ivec arr6(reduce_buffer_size);
    int n{static_cast<int>(arr6.capacity())};
    generate(begin(arr6), end(arr6), [&]{ return --n; });
    auto worker6 = spawn_cl<ivec(ivec&)>(kernel_source_reduce,
                                         kernel_name_reduce,
                                         {reduce_global_size},
                                         {},
                                         {reduce_local_size},
                                         reduce_result_size);
    self->send(worker6, move(arr6));
    const ivec expected4{ max_workgroup_size * 7, max_workgroup_size * 6,
                          max_workgroup_size * 5, max_workgroup_size * 4,
                          max_workgroup_size * 3, max_workgroup_size * 2,
                          max_workgroup_size, 0 };
    self->receive (
        on_arg_match >> [&] (const ivec& result) {
            CPPA_CHECK(equal(begin(expected4), end(expected4), begin(result)));
        }
    );

    // constant memory arguments
    const ivec arr7{magic_number};
    auto worker7 = spawn_cl<ivec(ivec&)>(kernel_source_const,
                                         kernel_name_const,
                                         {magic_number});
    self->send(worker7, move(arr7));
    ivec expected5(magic_number);
    fill(begin(expected5), end(expected5), magic_number);
    self->receive(
        on_arg_match >> [&] (const ivec& result) {
            CPPA_CHECK(equal(begin(expected5), end(expected5), begin(result)));
        }
    );

}

int main() {
    CPPA_TEST(tkest_opencl);

    announce<ivec>();
    matrix_type::announce();

    test_opencl();
    await_all_actors_done();
    shutdown();

    return CPPA_TEST_RESULT();
}
