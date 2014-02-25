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

constexpr size_t matrix_size = 4;
constexpr const char* kernel_name = "matrix_square";

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
    __kernel void matrix_square(__global int*) {
        size_t semicolon
    }
)__";

}

template<size_t Size>
class square_matrix {

 public:

    static constexpr size_t num_elements = Size * Size;

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

int main() {
    CPPA_TEST(test_opencl);

    announce<ivec>();
    announce<matrix_type>();

    const ivec expected1{ 56, 62, 68, 74
                        , 152, 174, 196, 218
                        , 248, 286, 324, 362
                        , 344, 398, 452, 506};

    auto worker1 = spawn_cl<ivec(ivec&)>(program::create(kernel_source),
                                         kernel_name,
                                         {matrix_size, matrix_size});
    ivec m1(matrix_size * matrix_size);
    iota(m1.begin(), m1.end(), 0);
    send(worker1, move(m1));
    receive (
        on_arg_match >> [&] (const ivec& result) {
            CPPA_CHECK(equal(begin(expected1), end(expected1), begin(result)));
        }
    );

    auto worker2 = spawn_cl<ivec(ivec&)>(kernel_source,
                                         kernel_name,
                                         {matrix_size, matrix_size});
    ivec m2(matrix_size * matrix_size);
    iota(m2.begin(), m2.end(), 0);
    send(worker2, move(m2));
    receive (
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
    send(worker3, move(m3));
    receive (
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
    send(worker4, move(m4));
    receive (
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

    cppa::await_all_others_done();
    cppa::shutdown();

    return CPPA_TEST_RESULT();
}
