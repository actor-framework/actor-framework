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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_BINARY_DESERIALIZER_HPP
#define CPPA_BINARY_DESERIALIZER_HPP

#include "cppa/deserializer.hpp"

namespace cppa {

/**
 * @brief Implements the deserializer interface with
 *        a binary serialization protocol.
 */
class binary_deserializer : public deserializer {

    typedef deserializer super;

 public:

    binary_deserializer(const void* buf, size_t buf_size,
                        actor_namespace* ns = nullptr,
                        type_lookup_table* table = nullptr);

    binary_deserializer(const void* begin, const void* m_end,
                        actor_namespace* ns = nullptr,
                        type_lookup_table* table = nullptr);

    const uniform_type_info* begin_object() override;
    void end_object() override;
    size_t begin_sequence() override;
    void end_sequence() override;
    primitive_variant read_value(primitive_type ptype) override;
    void read_tuple(size_t, const primitive_type*, primitive_variant*) override;
    void read_raw(size_t num_bytes, void* storage) override;

 private:

    const void* m_pos;
    const void* m_end;

};

} // namespace cppa

#endif // CPPA_BINARY_DESERIALIZER_HPP
