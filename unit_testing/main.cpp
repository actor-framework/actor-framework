#include <map>
#include <cstdio>
#include <atomic>
#include <string>
#include <limits>
#include <vector>
#include <cstring>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <typeinfo>
#include <iostream>

#include <sys/types.h>
#include <unistd.h>

#include "test.hpp"
#include "hash_of.hpp"

#include "cppa/tuple.hpp"
#include "cppa/config.hpp"
#include "cppa/uniform_type_info.hpp"

#define RUN_TEST(fun_name)                                                     \
std::cout << "run " << #fun_name << " ..." << std::endl;                       \
errors += fun_name ();                                                         \
std::cout << std::endl

using std::cout;
using std::cerr;
using std::endl;

/**
 * @brief Identifies a process.
 */
struct process_information
{

    /**
     * @brief Identifies the host system.
     *
     * A hash build from the MAC address of the first network device
     * and the serial number from the root HD (mounted in "/" or "C:").
     */
    std::uint8_t node_id[20];

    /**
     * @brief Identifies the running process.
     */
    std::uint32_t process_id;

    /**
     * @brief Converts {@link node_id} to an hexadecimal string.
     */
    std::string node_id_as_string() const
    {
        std::ostringstream oss;
        oss << std::hex;
        for (size_t i = 0; i < 20; ++i)
        {
            oss.width(2);
            oss.fill('0');
            oss << static_cast<std::uint32_t>(node_id[i]);
        }
        return oss.str();
    }

};

namespace {

void erase_trailing_newline(std::string& str)
{
    while (!str.empty() && (*str.rbegin()) == '\n')
    {
        str.resize(str.size() - 1);
    }
}

process_information compute_proc_info()
{
    process_information result;
    result.process_id = getpid();
    char cbuf[100];
    // fetch hd serial
    std::string hd_serial;
    FILE* cmd = popen("/usr/sbin/diskutil info / | "
                      "/usr/bin/awk '$0 ~ /UUID/ { print $3 }'",
                      "r");
    while (fgets(cbuf, 100, cmd) != 0)
    {
        hd_serial += cbuf;
    }
    pclose(cmd);
    erase_trailing_newline(hd_serial);
    // fetch mac address of first network device
    std::string first_mac_addr;
    cmd = popen("/usr/sbin/system_profiler SPNetworkDataType | "
                "/usr/bin/grep -Fw MAC | "
                "/usr/bin/grep -o \"..:..:..:..:..:..\" | "
                "/usr/bin/head -n1",
                "r");
    while (fgets(cbuf, 100, cmd) != 0)
    {
        first_mac_addr += cbuf;
    }
    pclose(cmd);
    erase_trailing_newline(first_mac_addr);
    hash_result_160bit tmp = ripemd_160(first_mac_addr + hd_serial);
    memcpy(result.node_id, tmp.data, 20);
    return result;
}

} // namespace <anonymous>

const process_information& get_process_information()
{
    static auto s_proc_info = compute_proc_info();
    return s_proc_info;
}

void print_node_id()
{
    const auto& pinfo = get_process_information();
    // lifetime scope of oss
    std::string node_id_hash = pinfo.node_id_as_string();
    cout << "node id: " << node_id_hash << endl;
    cout << "process id: " << pinfo.process_id << endl;
    cout << "actor id format: {process id}.{actor id}@{node id}" << endl;
    cout << "example actor id: " << pinfo.process_id
                                 << ".42@"
                                 << node_id_hash
                                 << endl;
}

int main(int argc, char** c_argv)
{
    print_node_id();
    return 0;

    std::vector<std::string> argv;
    for (int i = 1; i < argc; ++i)
    {
        argv.push_back(c_argv[i]);
    }
    if (!argv.empty())
    {
        if (argv.size() == 1 && argv.front() == "performance_test")
        {
            cout << endl << "run queue performance test ... " << endl;
            test__queue_performance();
            return 0;
        }
        else
        {
            cerr << "usage: test [performance_test]" << endl;
        }
    }
    else
    {
        std::cout << std::boolalpha;
        size_t errors = 0;
        RUN_TEST(test__ripemd_160);
        RUN_TEST(test__primitive_variant);
        RUN_TEST(test__uniform_type);
        RUN_TEST(test__intrusive_ptr);
        RUN_TEST(test__a_matches_b);
        RUN_TEST(test__type_list);
        RUN_TEST(test__tuple);
        RUN_TEST(test__serialization);
        RUN_TEST(test__spawn);
        RUN_TEST(test__local_group);
        RUN_TEST(test__atom);
        cout << endl
             << "error(s) in all tests: " << errors
             << endl;
    }
    return 0;
}
