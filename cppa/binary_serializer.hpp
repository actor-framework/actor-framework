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


#ifndef CPPA_BINARY_SERIALIZER_HPP
#define CPPA_BINARY_SERIALIZER_HPP

#include <utility>

#include "cppa/serializer.hpp"
#include "cppa/util/buffer.hpp"

namespace cppa {

namespace detail { class binary_writer; }

/**
 * @brief Implements the serializer interface with
 *        a binary serialization protocol.
 */
class binary_serializer : public serializer {

    typedef serializer super;

 public:

    /**
     * @brief Creates a binary serializer writing to @p write_buffer.
     * @warning @p write_buffer must be guaranteed to outlive @p this
     */
    binary_serializer(util::buffer* write_buffer,
                      actor_namespace* ns = nullptr,
                      type_lookup_table* lookup_table = nullptr);

    void begin_object(const uniform_type_info*) override;

    void end_object() override;

    void begin_sequence(size_t list_size) override;

    void end_sequence() override;

    void write_value(const primitive_variant& value) override;

    void write_tuple(size_t size, const primitive_variant* values) override;

    void write_raw(size_t num_bytes, const void* data) override;

 private:

    util::buffer* m_sink;

};

} // namespace cppa

#endif // CPPA_BINARY_SERIALIZER_HPP
