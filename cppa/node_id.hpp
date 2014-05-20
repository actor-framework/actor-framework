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


#ifndef CPPA_NODE_ID_HPP
#define CPPA_NODE_ID_HPP

#include <array>
#include <string>
#include <cstdint>

#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/util/comparable.hpp"

namespace cppa {

class serializer;

/**
 * @brief Identifies a process.
 */
class node_id : public ref_counted, util::comparable<node_id> {

    typedef ref_counted super;

 public:

    ~node_id();

    /**
     * @brief @c libcppa uses 160 bit hashes (20 bytes).
     */
    static constexpr size_t host_id_size = 20;

    /**
     * @brief Represents a 160 bit hash.
     */
    typedef std::array<std::uint8_t, host_id_size> host_id_type;

    /**
     * @brief Copy constructor.
     */
    node_id(const node_id&);

    /**
     * @brief Creates @c this from @p process_id and @p hash.
     * @param process_id System-wide unique process identifier.
     * @param hash Unique node id as hexadecimal string representation.
     */
    node_id(std::uint32_t process_id, const std::string& hash);

    /**
     * @brief Creates @c this from @p process_id and @p hash.
     * @param process_id System-wide unique process identifier.
     * @param node_id Unique node id.
     */
    node_id(std::uint32_t process_id, const host_id_type& node_id);

    /**
     * @brief Identifies the running process.
     * @returns A system-wide unique process identifier.
     */
    inline std::uint32_t process_id() const { return m_process_id; }

    /**
     * @brief Identifies the host system.
     * @returns A hash build from the MAC address of the first network device
     *          and the UUID of the root partition (mounted in "/" or "C:").
     */
    inline const host_id_type& host_id() const { return m_host_id; }

    /** @cond PRIVATE */

    // "inherited" from comparable<node_id>
    int compare(const node_id& other) const;

    static void serialize_invalid(serializer*);

    /** @endcond */

 private:

    std::uint32_t m_process_id;
    host_id_type m_host_id;

};

void host_id_from_string(const std::string& hash,
                         node_id::host_id_type& node_id);

bool equal(const std::string& hash,
           const node_id::host_id_type& node_id);

inline bool equal(const node_id::host_id_type& node_id,
                  const std::string& hash) {
    return equal(hash, node_id);
}

/**
 * @brief A smart pointer type that manages instances of
 *        {@link node_id}.
 * @relates node_id
 */
typedef intrusive_ptr<node_id> node_id_ptr;

/**
 * @relates node_id
 */
std::string to_string(const node_id& what);

/**
 * @relates node_id
 */
std::string to_string(const node_id_ptr& what);

/**
 * @brief Converts a {@link node_id::host_id_type node_id}
 *        to a hexadecimal string.
 * @param node_id A unique node identifier.
 * @returns A hexadecimal representation of @p node_id.
 * @relates node_id
 */
std::string to_string(const node_id::host_id_type& node_id);

} // namespace cppa

#endif // CPPA_NODE_ID_HPP
