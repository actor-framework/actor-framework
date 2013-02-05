#include <atomic>

#include "test.hpp"
#include "cppa/cppa.hpp"

using namespace cppa;

namespace { std::atomic<size_t> s_error_count{0}; }

void cppa_inc_error_count() {
    ++s_error_count;
}

size_t cppa_error_count() {
    return s_error_count;
}

void cppa_unexpected_message(const char* fname, size_t line_num) {
    std::cerr << "ERROR in file " << fname << " on line " << line_num
              << ": unexpected message: "
              << to_string(self->last_dequeued())
              << std::endl;
}

void cppa_unexpected_timeout(const char* fname, size_t line_num) {
    std::cerr << "ERROR in file " << fname << " on line " << line_num
              << ": unexpected timeout" << std::endl;
}
