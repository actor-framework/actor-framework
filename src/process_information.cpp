#include "cppa/config.hpp"

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

#ifdef CPPA_MACOS
const char* s_get_uuid =
    "/usr/sbin/diskutil info / | "
    "/usr/bin/awk '$0 ~ /UUID/ { print $3 }'";
const char* s_get_mac =
    "/usr/sbin/system_profiler SPNetworkDataType | "
    "/usr/bin/grep -Fw MAC | "
    "/usr/bin/grep -o '[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5}' | "
    "/usr/bin/head -n1";
#elif defined(CPPA_LINUX)
const char* s_get_uuid =
    "/bin/egrep -o 'UUID=(([0-9a-fA-F-]+)(-[0-9a-fA-F-]+){3})\\s+/\\s+' "
                  "/etc/fstab | "
    "/bin/egrep -o '([0-9a-fA-F-]+)(-[0-9a-fA-F-]+){3}'";
const char* s_get_mac =
    "/sbin/ifconfig | "
    "/bin/egrep -o '[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5}' | "
    "head -n1";
#endif

cppa::process_information compute_proc_info()
{
    cppa::process_information result;
    result.process_id = getpid();
    char cbuf[100];
    // fetch hd serial
    std::string hd_serial;
    FILE* get_uuid_cmd = popen(s_get_uuid, "r");
    while (fgets(cbuf, 100, get_uuid_cmd) != 0)
    {
        hd_serial += cbuf;
    }
    pclose(get_uuid_cmd);
    erase_trailing_newline(hd_serial);
    // fetch mac address of first network device
    std::string first_mac_addr;
    FILE* get_mac_cmd = popen(s_get_mac, "r");
    while (fgets(cbuf, 100, get_mac_cmd) != 0)
    {
        first_mac_addr += cbuf;
    }
    pclose(get_mac_cmd);
    erase_trailing_newline(first_mac_addr);
    result.node_id = cppa::util::ripemd_160(first_mac_addr + hd_serial);
    //memcpy(result.node_id, tmp.data(), cppa::process_information::node_id_size);
    return result;
}

} // namespace <anonymous>

namespace cppa {

process_information::process_information() : process_id(0)
{
    memset(node_id.data(), 0, node_id_size);
}

process_information::process_information(const process_information& other)
    : ref_counted(), process_id(other.process_id), node_id(other.node_id)
{
    //memcpy(node_id, other.node_id, node_id_size);
}

process_information::process_information(std::uint32_t a, const node_id_type& b)
    : ref_counted(), process_id(a), node_id(b)
{
}

process_information&
process_information::operator=(const process_information& other)
{
    // prevent self-assignment
    if (this != &other)
    {
        process_id = other.process_id;
        node_id = other.node_id;
        //memcpy(node_id, other.node_id, node_id_size);
    }
    return *this;
}


std::string process_information::node_id_as_string() const
{
    std::ostringstream oss;
    oss << std::hex;
    oss.fill('0');
    for (size_t i = 0; i < node_id_size; ++i)
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
    int tmp = strncmp(reinterpret_cast<const char*>(node_id.data()),
                      reinterpret_cast<const char*>(other.node_id.data()),
                      node_id_size);
    if (tmp == 0)
    {
        if (process_id < other.process_id) return -1;
        else if (process_id == other.process_id) return 0;
        return 1;
    }
    return tmp;
}

void process_information::node_id_from_string(const std::string& str,
                                      process_information::node_id_type& arr)
{
    if (str.size() != (arr.size() * 2))
    {
        throw std::logic_error("str is not a node id hash");
    }
    auto j = str.begin();
    for (size_t i = 0; i < arr.size(); ++i)
    {
        auto& val = arr[i];
        val = 0;
        for (int tmp = 0; tmp < 2; ++tmp)
        {
            char c = *j;
            ++j;
            if (isdigit(c))
            {
                val |= static_cast<std::uint8_t>(c - '0');
            }
            else if (isalpha(c))
            {
                if (c >= 'a' && c <= 'f')
                {
                    val |= static_cast<std::uint8_t>((c - 'a') + 10);
                }
                else if (c >= 'A' && c <= 'F')
                {
                    val |= static_cast<std::uint8_t>((c - 'A') + 10);
                }
                else
                {
                    throw std::logic_error(std::string("illegal character: ") + c);
                }
            }
            else
            {
                throw std::logic_error(std::string("illegal character: ") + c);
            }
            if (tmp == 0)
            {
                val <<= 4;
            }
        }
    }
}

std::string to_string(const process_information& what)
{
    std::ostringstream oss;
    oss << what.process_id << "@" << what.node_id_as_string();
    return oss.str();
}

} // namespace cppa
