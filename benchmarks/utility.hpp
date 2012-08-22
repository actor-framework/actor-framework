#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <algorithm>

inline std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> result;
    std::stringstream strs{str};
    std::string tmp;
    while (std::getline(strs, tmp, delim)) result.push_back(tmp);
    return result;
}

inline std::string join(const std::vector<std::string>& vec,
                        const std::string& delim = "") {
    if (vec.empty()) return "";
    auto result = vec.front();
    for (auto i = vec.begin() + 1; i != vec.end(); ++i) {
        result += delim;
        result += *i;
    }
    return result;
}

template<typename T>
T rd(const char* cstr) {
    char* endptr = nullptr;
    T result = static_cast<T>(strtol(cstr, &endptr, 10));
    if (endptr == nullptr || *endptr != '\0') {
        std::string errstr;
        errstr += "\"";
        errstr += cstr;
        errstr += "\" is not an integer";
        throw std::invalid_argument(errstr);
    }
    return result;
}

int num_cores() {
    char cbuf[100];
    FILE* cmd = popen("/bin/cat /proc/cpuinfo | /bin/grep processor | /usr/bin/wc -l", "r");
    if (fgets(cbuf, 100, cmd) == 0) {
        throw std::runtime_error("cannot determine number of cores");
    }
    pclose(cmd);
    // erase trailing newline
    auto i = std::find(cbuf, cbuf + 100, '\n');
    *i = '\0';
    return rd<int>(cbuf);
}

std::vector<uint64_t> factorize(uint64_t n) {
    std::vector<uint64_t> result;
    if (n <= 3) {
        result.push_back(n);
        return std::move(result);
    }
    uint64_t d = 2;
    while(d < n) {
        if((n % d) == 0) {
            result.push_back(d);
            n /= d;
        }
        else {
            d = (d == 2) ? 3 : (d + 2);
        }
    }
    result.push_back(d);
    return std::move(result);
}

// some utility for cppa benchmarks only
#ifndef THERON_BENCHMARK

#include "cppa/option.hpp"
// a string => T projection
template<typename T>
cppa::option<T> spro(const std::string& str) {
    T value;
    if (std::istringstream(str) >> value) {
        return value;
    }
    return {};
}

#endif // THERON_BENCHMARK

#endif // UTILITY_HPP
