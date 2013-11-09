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


#include <cstdio>
#include <cstring>
#include <sstream>

#include <unistd.h>
#include <sys/types.h>

#include "cppa/config.hpp"
#include "cppa/node_id.hpp"
#include "cppa/serializer.hpp"
#include "cppa/primitive_variant.hpp"

#include "cppa/util/algorithm.hpp"
#include "cppa/util/ripemd_160.hpp"
#include "cppa/util/get_root_uuid.hpp"
#include "cppa/util/get_mac_addresses.hpp"

namespace {

cppa::node_id* compute_proc_info() {
    using namespace cppa::util;
    auto macs = get_mac_addresses();
    auto hd_serial_and_mac_addr = join(macs.begin(), macs.end())
                                + get_root_uuid();
    cppa::node_id::host_id_type node_id;
    ripemd_160(node_id, hd_serial_and_mac_addr);
    return new cppa::node_id(getpid(), node_id);
}

cppa::node_id_ptr s_pinfo;

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

void host_id_from_string(const std::string& hash,
                         node_id::host_id_type& node_id) {
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
           const node_id::host_id_type& node_id) {
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

node_id::node_id(const node_id& other)
: super(), m_process_id(other.process_id()), m_host_id(other.host_id()) { }

node_id::node_id(std::uint32_t a, const std::string& b)
: m_process_id(a) {
    host_id_from_string(b, m_host_id);
}

node_id::node_id(std::uint32_t a, const host_id_type& b)
: m_process_id(a), m_host_id(b) { }

std::string to_string(const node_id::host_id_type& node_id) {
    std::ostringstream oss;
    oss << std::hex;
    oss.fill('0');
    for (size_t i = 0; i < node_id::host_id_size; ++i) {
        oss.width(2);
        oss << static_cast<std::uint32_t>(node_id[i]);
    }
    return oss.str();
}

const intrusive_ptr<node_id>& node_id::get() {
    return s_pinfo;
}

int node_id::compare(const node_id& other) const {
    int tmp = strncmp(reinterpret_cast<const char*>(host_id().data()),
                      reinterpret_cast<const char*>(other.host_id().data()),
                      host_id_size);
    if (tmp == 0) {
        if (m_process_id < other.process_id()) return -1;
        else if (m_process_id == other.process_id()) return 0;
        return 1;
    }
    return tmp;
}

void node_id::serialize_invalid(serializer* sink) {
    sink->write_value(static_cast<uint32_t>(0));
    node_id::host_id_type zero;
    std::fill(zero.begin(), zero.end(), 0);
    sink->write_raw(node_id::host_id_size, zero.data());
}

std::string to_string(const node_id& what) {
    std::ostringstream oss;
    oss << what.process_id() << "@" << to_string(what.host_id());
    return oss.str();
}

std::string to_string(const node_id_ptr& what) {
    std::ostringstream oss;
    oss << "@process_info(";
    if (!what) oss << "null";
    else oss << to_string(*what);
    oss << ")";
    return oss.str();
}

} // namespace cppa
