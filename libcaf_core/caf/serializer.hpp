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

#ifndef CAF_SERIALIZER_HPP
#define CAF_SERIALIZER_HPP

#include <string>
#include <cstddef> // size_t

#include "caf/uniform_type_info.hpp"
#include "caf/primitive_variant.hpp"

#include "caf/detail/to_uniform_name.hpp"

namespace caf {

class actor_namespace;
class uniform_type_info;

/**
 * @ingroup TypeSystem
 * @brief Technology-independent serialization interface.
 */
class serializer {

    serializer(const serializer&) = delete;
    serializer& operator=(const serializer&) = delete;

 public:

    /**
     * @note @p addressing must be guaranteed to outlive the serializer
     */
    serializer(actor_namespace* addressing = nullptr);

    virtual ~serializer();

    /**
     * @brief Begins serialization of an object of type @p uti.
     */
    virtual void begin_object(const uniform_type_info* uti) = 0;

    /**
     * @brief Ends serialization of an object.
     */
    virtual void end_object() = 0;

    /**
     * @brief Begins serialization of a sequence of size @p num.
     */
    virtual void begin_sequence(size_t num) = 0;

    /**
     * @brief Ends serialization of a sequence.
     */
    virtual void end_sequence() = 0;

    /**
     * @brief Writes a single value to the data sink.
     * @param value A primitive data value.
     */
    virtual void write_value(const primitive_variant& value) = 0;

    /**
     * @brief Writes a raw block of data.
     * @param num_bytes The size of @p data in bytes.
     * @param data Raw data.
     */
    virtual void write_raw(size_t num_bytes, const void* data) = 0;

    inline actor_namespace* get_namespace() { return m_namespace; }

    template<typename T>
    inline serializer& write(const T& val) {
        write_value(val);
        return *this;
    }

    template<typename T>
    inline serializer& write(const T& val, const uniform_type_info* uti) {
        uti->serialize(&val, this);
        return *this;
    }

 private:

    actor_namespace* m_namespace;

};

/**
 * @brief Serializes a value to @p s.
 * @param s A valid serializer.
 * @param what A value of an announced or primitive type.
 * @returns @p s
 * @relates serializer
 */
template<typename T>
serializer& operator<<(serializer& s, const T& what) {
    auto mtype = uniform_typeid<T>();
    if (mtype == nullptr) {
        throw std::logic_error("no uniform type info found for " +
                               detail::to_uniform_name(typeid(T)));
    }
    mtype->serialize(&what, &s);
    return s;
}

} // namespace caf

#endif // CAF_SERIALIZER_HPP
