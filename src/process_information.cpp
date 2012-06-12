/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#include "cppa/config.hpp"

#include <cstdio>
#include <cstring>
#include <sstream>

#include <unistd.h>
#include <sys/types.h>


#include "cppa/util/ripemd_160.hpp"
#include "cppa/process_information.hpp"

namespace {

inline void erase_trailing_newline(std::string& str) {
    while (!str.empty() && (*str.rbegin()) == '\n') {
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

cppa::process_information* compute_proc_info() {
    char cbuf[100];
    // fetch hd serial
    std::string hd_serial_and_mac_addr;
    FILE* get_uuid_cmd = popen(s_get_uuid, "r");
    while (fgets(cbuf, 100, get_uuid_cmd) != 0) {
        hd_serial_and_mac_addr += cbuf;
    }
    pclose(get_uuid_cmd);
    erase_trailing_newline(hd_serial_and_mac_addr);
    // fetch mac address of first network device
    FILE* get_mac_cmd = popen(s_get_mac, "r");
    while (fgets(cbuf, 100, get_mac_cmd) != 0) {
        hd_serial_and_mac_addr += cbuf;
    }
    pclose(get_mac_cmd);
    erase_trailing_newline(hd_serial_and_mac_addr);
    cppa::process_information::node_id_type node_id;
    cppa::util::ripemd_160(node_id, hd_serial_and_mac_addr);
    return new cppa::process_information(getpid(), node_id);
}

cppa::process_information_ptr s_pinfo;

struct pinfo_manager {
    pinfo_manager() {
        if (!s_pinfo) {
            s_pinfo.reset(compute_proc_info());
        }
    }
}
s_pinfo_manager;

std::uint8_t hex_char_value(char c) {
    if (isdigit(c)) {
        return static_cast<std::uint8_t>(c - '0');
    }
    else if (isalpha(c)) {
        if (c >= 'a' && c <= 'f') {
            return static_cast<std::uint8_t>((c - 'a') + 10);
        }
        else if (c >= 'A' && c <= 'F') {
            return static_cast<std::uint8_t>((c - 'A') + 10);
        }
    }
    throw std::invalid_argument(std::string("illegal character: ") + c);
}

} // namespace <anonymous>

namespace cppa {

void node_id_from_string(const std::string& hash,
                         process_information::node_id_type& node_id) {
    if (hash.size() != (node_id.size() * 2)) {
        throw std::invalid_argument("string argument is not a node id hash");
    }
    auto j = hash.c_str();
    for (size_t i = 0; i < node_id.size(); ++i) {
        // read two characters, each representing 4 bytes
        auto& val = node_id[i];
        val  = hex_char_value(*j++) << 4;
        val |= hex_char_value(*j++);
    }
}

bool equal(const std::string& hash,
           const process_information::node_id_type& node_id) {
    if (hash.size() != (node_id.size() * 2)) {
        return false;
    }
    auto j = hash.c_str();
    try {
        for (size_t i = 0; i < node_id.size(); ++i) {
            // read two characters, each representing 4 bytes
            std::uint8_t val;
            val  = hex_char_value(*j++) << 4;
            val |= hex_char_value(*j++);
            if (val != node_id[i]) {
                return false;
            }
        }
    }
    catch (std::invalid_argument&) {
        return false;
    }
    return true;
}

process_information::process_information(const process_information& other)
    : super(), m_process_id(other.process_id()), m_node_id(other.node_id()) {
}

process_information::process_information(std::uint32_t a, const std::string& b)
    : m_process_id(a) {
    node_id_from_string(b, m_node_id);
}

process_information::process_information(std::uint32_t a, const node_id_type& b)
    : m_process_id(a), m_node_id(b) {
}

std::string to_string(const process_information::node_id_type& node_id) {
    std::ostringstream oss;
    oss << std::hex;
    oss.fill('0');
    for (size_t i = 0; i < process_information::node_id_size; ++i) {
        oss.width(2);
        oss << static_cast<std::uint32_t>(node_id[i]);
    }
    return oss.str();
}

const intrusive_ptr<process_information>& process_information::get() {
    return s_pinfo;
}

int process_information::compare(const process_information& other) const {
    int tmp = strncmp(reinterpret_cast<const char*>(node_id().data()),
                      reinterpret_cast<const char*>(other.node_id().data()),
                      node_id_size);
    if (tmp == 0) {
        if (m_process_id < other.process_id()) return -1;
        else if (m_process_id == other.process_id()) return 0;
        return 1;
    }
    return tmp;
}

std::string to_string(const process_information& what) {
    std::ostringstream oss;
    oss << what.process_id() << "@" << to_string(what.node_id());
    return oss.str();
}

} // namespace cppa
