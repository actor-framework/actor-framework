#include <vector>

#include "test.hpp"

#include "cppa/cppa.hpp"
#include "cppa/opencl/program.hpp"
#include "cppa/opencl/actor_facade.hpp"
#include "cppa/opencl/command_dispatcher.hpp"

using namespace std;
using namespace cppa;
using namespace cppa::opencl;

#define STRINGIFY(A) #A
std::string kernel_source = STRINGIFY(
    float function_example(float a, float b) {
        return a + b;
    }

    __kernel void add(__global float* a, __global float* b, __global float* c) {
        unsigned int i = get_global_id(0);
        c[i] = function_example(a[i], b[i]);
    }

    __kernel void square(__global float* input, __global float* output)
    {
        unsigned int i = get_global_id(0);
        output[i] = input[i] * input[i];
    }
);

int main() {
    CPPA_TEST(test_opencl);

    program prog{kernel_source};

    command_dispatcher* disp =
            cppa::detail::singleton_manager::get_command_dispatcher();

    {

        unsigned size{1024};

        vector<float> a;
        vector<float> b;

        a.reserve(size);
        b.reserve(size);

        for (unsigned i{0}; i < size; ++i) {
            a.push_back(i*1.0);
            b.push_back(i*1.0);
        }

        auto a_facade = disp->spawn<vector<float>,
                                    vector<float>,
                                    vector<float>>(prog, "add");
        send(a_facade, a, b);
        vector<int> wrong_results;
        receive (
            on_arg_match >> [&](const vector<float>& result) {
                copy(begin(result),
                     end(result),
                     std::ostream_iterator<float>(cout, "\n"));
            },
            others() >> CPPA_UNEXPECTED_MSG_CB()
        );

        auto b_facade = disp->spawn<vector<float>,
                                    vector<float>>(prog, "square");
        cout << "square" << endl;
        send(b_facade, a);
        receive (
            on_arg_match >> [&](const vector<float>& result) {
                copy(begin(result),
                     end(result),
                     std::ostream_iterator<float>(cout, "\n"));
            },
            others() >> CPPA_UNEXPECTED_MSG_CB()
        );

        cppa::await_all_others_done();
        cppa::shutdown();
    }

    return CPPA_TEST_RESULT();
}
