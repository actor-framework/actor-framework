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


#ifndef CPPA_PROCESS_INFORMATION_HPP
#define CPPA_PROCESS_INFORMATION_HPP

#include <array>
#include <string>
#include <cstdint>

#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/util/comparable.hpp"

namespace cppa {

/**
 * @brief Identifies a process.
 */
class process_information : public ref_counted,
                            util::comparable<process_information> {

    typedef ref_counted super;

 public:

    /**
     * @brief @c libcppa uses 160 bit hashes (20 bytes).
     */
    static constexpr size_t node_id_size = 20;

    /**
     * @brief Represents a 160 bit hash.
     */
    typedef std::array<std::uint8_t, node_id_size> node_id_type;

    /**
     * @brief Copy constructor.
     */
    process_information(const process_information&);

    /**
     * @brief Creates @c this from @p process_id and @p hash.
     * @param process_id System-wide unique process identifier.
     * @param hash Unique node id as hexadecimal string representation.
     */
    process_information(std::uint32_t process_id, const std::string& hash);

    /**
     * @brief Creates @c this from @p process_id and @p hash.
     * @param process_id System-wide unique process identifier.
     * @param hash Unique node id.
     */
    process_information(std::uint32_t process_id, const node_id_type& node_id);

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
    inline const node_id_type& node_id() const { return m_node_id; }

    /**
     * @brief Returns the proccess_information for the running process.
     * @returns A pointer to the singleton of this process.
     */
    static const intrusive_ptr<process_information>& get();

    // "inherited" from comparable<process_information>
    int compare(const process_information& other) const;

 private:

    std::uint32_t m_process_id;
    node_id_type m_node_id;

};

void node_id_from_string(const std::string& hash,
                         process_information::node_id_type& node_id);

bool equal(const std::string& hash,
           const process_information::node_id_type& node_id);

inline bool equal(const process_information::node_id_type& node_id,
                  const std::string& hash) {
    return equal(hash, node_id);
}

/**
 * @relates process_information
 */
std::string to_string(const process_information& what);

/**
 * @brief Converts a {@link process_information::node_id_type node_id}
 *        to a hexadecimal string.
 * @param node_id A unique node identifier.
 * @returns A hexadecimal representation of @p node_id.
 * @relates process_information
 */
std::string to_string(const process_information::node_id_type& node_id);

/**
 * @brief A smart pointer type that manages instances of
 *        {@link process_information}.
 * @relates process_information
 */
typedef intrusive_ptr<process_information> process_information_ptr;

} // namespace cppa

#endif // CPPA_PROCESS_INFORMATION_HPP
