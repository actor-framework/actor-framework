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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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
#include "cppa/util/get_root_uuid.hpp"

#ifdef CPPA_MACOS

namespace {

inline void erase_trailing_newline(std::string& str) {
    while (!str.empty() && (*str.rbegin()) == '\n') {
        str.resize(str.size() - 1);
    }
}

constexpr const char* s_get_uuid = "/usr/sbin/diskutil info / | "
                                   "/usr/bin/awk '$0 ~ /UUID/ { print $3 }'";

} // namespace <anonymous>

namespace cppa { namespace util {

std::string get_root_uuid() {
    char cbuf[100];
    // fetch hd serial
    std::string uuid;
    FILE* get_uuid_cmd = popen(s_get_uuid, "r");
    while (fgets(cbuf, 100, get_uuid_cmd) != 0) {
        uuid += cbuf;
    }
    pclose(get_uuid_cmd);
    erase_trailing_newline(uuid);
    return uuid;
}

} } // namespace cppa::util

#elif defined(CPPA_LINUX)

#include <vector>
#include <string>
#include <cctype>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <cstring>
#include <unistd.h>
#include <iostream>

using namespace std;

struct columns_iterator : iterator<forward_iterator_tag, vector<string>> {

    columns_iterator(ifstream* s = nullptr) : fs(s) { }

    vector<string>& operator*() { return cols; }

    columns_iterator& operator++() {
        string line;
        if (!getline(*fs, line)) fs = nullptr;
        else cols = split(line);
        return *this;
    }

    ifstream* fs;
    vector<string> cols;

};

bool operator==(const columns_iterator& lhs, const columns_iterator& rhs) {
    return lhs.fs == rhs.fs;
}

bool operator!=(const columns_iterator& lhs, const columns_iterator& rhs) {
    return !(lhs == rhs);
}

namespace cppa { namespace util {

std::string get_root_uuid() {
    char          buf[1024] = {0};
    struct ifconf ifc = {0};
    struct ifreq *ifr = NULL;
    int           sck = 0;
    int           nInterfaces = 0;

    // Get a socket handle.
    sck = socket(AF_INET, SOCK_DGRAM, 0);
    if(sck < 0) {
        perror("socket");
        return 1;
    }

    // Query available interfaces.
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if(ioctl(sck, SIOCGIFCONF, &ifc) < 0) {
        perror("ioctl(SIOCGIFCONF)");
        return 1;
    }

    vector<string> hw_addresses;
    auto ctoi = [](char c) -> unsigned {
        return static_cast<unsigned char>(c);
    };
    // Iterate through the list of interfaces.
    ifr = ifc.ifc_req;
    nInterfaces = ifc.ifc_len / sizeof(struct ifreq);
    for(int i = 0; i < nInterfaces; i++) {
        struct ifreq *item = &ifr[i];
        // Get the MAC address
        if(ioctl(sck, SIOCGIFHWADDR, item) < 0) {
            perror("ioctl(SIOCGIFHWADDR)");
            return 1;
        }
        std::ostringstream oss;
        oss << hex;
        oss.width(2);
        oss << ctoi(item->ifr_hwaddr.sa_data[0]);
        for (size_t i = 1; i < 6; ++i) {
            oss << ":";
            oss.width(2);
            oss << ctoi(item->ifr_hwaddr.sa_data[i]);
        }
        auto addr = oss.str();
        if (addr != "00:00:00:00:00:00") {
            hw_addresses.push_back(std::move(addr));
        }
    }
    string uuid;
    ifstream fs;
    fs.open("/etc/fstab", ios_base::in);
    columns_iterator end;
    auto i = find_if(columns_iterator{&fs}, end, [](const vector<string>& cols) {
        return cols.size() == 6 && cols[1] == "/";
    });
    if (i != end) {
        uuid = move((*i)[0]);
        const char cstr[] = { "UUID=" };
        auto slen = sizeof(cstr) - 1;
        if (uuid.compare(0, slen, cstr) == 0) uuid.erase(0, slen);
        // UUIDs are formatted as 8-4-4-4-12 hex digits groups
        auto cpy = uuid;
        replace_if(cpy.begin(), cpy.end(), ::isxdigit, 'F');
        // discard invalid UUID
        if (cpy != "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF") uuid.clear();
    }
    for (auto& addr : hw_addresses) {
        cout << addr << endl;
    }

    cout << uuid << endl;
}

} } // namespace cppa::util

#endif // CPPA_LINUX
