#include <vector>
#include <iomanip>

#include "test.hpp"

#include "cppa/cppa.hpp"
#include "cppa/opencl/program.hpp"
#include "cppa/opencl/actor_facade.hpp"
#include "cppa/opencl/command_dispatcher.hpp"

using namespace std;
using namespace cppa;
using namespace cppa::opencl;

namespace { constexpr const char* kernel_source = R"__(
    __kernel void matrix(__global float* matrix1,
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

    __kernel void dimensions(__global float* dummy, __global float* output) {
        int g_size = get_global_size(0); // == get_global_size_(1);
        int g_dim_0 = get_global_id(0);
        int g_dim_1 = get_global_id(1);
        int l_dim_0 = get_local_id(0);
        int l_dim_1 = get_local_id(1);
        int g_off_0 = get_global_offset(0);
        int g_off_1 = get_global_offset(1);
        output[(g_dim_0-g_off_0)+(g_dim_1-g_off_1)*g_size] =
            g_dim_0 * 10.0f + g_dim_1 * 1.0f + l_dim_0 * 0.1f + l_dim_1* 0.01f;
    }
)__"; }

int main() {
    CPPA_TEST(test_opencl);

    announce<vector<int>>();
    announce<vector<float>>();

    size_t size{6};
    auto prog = program::create(kernel_source);

    command_dispatcher* disp =
            cppa::detail::singleton_manager::get_command_dispatcher();
    auto matrix_global = disp->spawn<vector<float>,
                                     vector<float>,
                                     vector<float>>(prog, "matrix",
                                                    {size, size});

    vector<float> m1(size * size);
    vector<float> m2(size * size);

    iota(m1.begin(), m1.end(), 0.0);
    iota(m2.begin(), m2.end(), 0.0);

    send(matrix_global, m1, m2);

    receive (
        on_arg_match >> [&] (const vector<float>&) {
            cout << "done!" << endl;
//            cout << "results:" << endl;
//            for (unsigned y{0}; y < size; ++y) {
//                for (unsigned x{0}; x < size; ++x) {
//                    cout << fixed << setprecision(2) << setw(8) << result[x+y*size];
//                }
//                cout << endl;
//            }
        },
        others() >> [=]() {
            cout << "Unexpected message: '"
                 << to_string(self->last_dequeued()) << "'.\n";
        }
    );

//    cout << endl;
//    auto matrix_local = disp->spawn<vector<float>,
//                                    vector<float>>(prog, "dimensions",
//                                                   {size, size},
//                                                   {1,2},
//                                                   {(size/3), (size/2)});

//    m1.clear();
//    m1.push_back(0.0); // dummy

//    send(matrix_local, m1);

//    receive (
//        on_arg_match >> [&] (const vector<float>& result) {
//            cout << "dimenson example: " << endl;
//            for (unsigned y{0}; y < size; ++y) {
//                for (unsigned x{0}; x < size; ++x) {
//                    cout << fixed << setprecision(2) << setw(6) << result[x+y*size];
//                }
//                cout << endl;
//            }
//        },
//        others() >> [=]() {
//            cout << "Unexpected message: '"
//                 << to_string(self->last_dequeued()) << "'.\n";
//        }
//    );

    cppa::await_all_others_done();
    cppa::shutdown();

    return CPPA_TEST_RESULT();
}
