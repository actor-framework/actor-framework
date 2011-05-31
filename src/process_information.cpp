#include <cstdio>
#include <cstring>
#include <sstream>

#include <unistd.h>
#include <sys/types.h>


#include "cppa/util/ripemd_160.hpp"
#include "cppa/process_information.hpp"

namespace {

void erase_trailing_newline(std::string& str)
{
    while (!str.empty() && (*str.rbegin()) == '\n')
    {
        str.resize(str.size() - 1);
    }
}

cppa::process_information compute_proc_info()
{
    cppa::process_information result;
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
    auto tmp = cppa::util::ripemd_160(first_mac_addr + hd_serial);
    memcpy(result.node_id, tmp.data(), 20);
    return result;
}

} // namespace <anonymous>

namespace cppa {

std::string process_information::node_id_as_string() const
{
    std::ostringstream oss;
    oss << std::hex;
    oss.fill('0');
    for (size_t i = 0; i < 20; ++i)
    {
        oss.width(2);
        oss << static_cast<std::uint32_t>(node_id[i]);
    }
    return oss.str();
}

const process_information& process_information::get()
{
    static auto s_proc_info = compute_proc_info();
    return s_proc_info;
}

int process_information::compare(const process_information& other) const
{
    int tmp = strncmp(reinterpret_cast<const char*>(node_id),
                      reinterpret_cast<const char*>(other.node_id), 20);
    if (tmp == 0)
    {
        if (process_id < other.process_id) return -1;
        else if (process_id == other.process_id) return 0;
        return 1;
    }
    return tmp;
}

} // namespace cppa
