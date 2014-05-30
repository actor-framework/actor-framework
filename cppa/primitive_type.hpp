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


#ifndef CPPA_PRIMITIVE_TYPE_HPP
#define CPPA_PRIMITIVE_TYPE_HPP

namespace cppa {

/**
 * @ingroup TypeSystem
 * @brief Represents the type flag of
 *        {@link cppa::primitive_variant primitive_variant}.
 *
 * Includes integers (signed and unsigned), floating points
 * and strings (std::string, std::u16string and std::u32string).
 * @relates primitive_variant
 */
enum primitive_type : unsigned char {
    pt_int8,         /**< equivalent of @p std::int8_t */
    pt_int16,        /**< equivalent of @p std::int16_t */
    pt_int32,        /**< equivalent of @p std::int32_t */
    pt_int64,        /**< equivalent of @p std::int64_t */
    pt_uint8,        /**< equivalent of @p std::uint8_t */
    pt_uint16,       /**< equivalent of @p std::uint16_t */
    pt_uint32,       /**< equivalent of @p std::uint32_t */
    pt_uint64,       /**< equivalent of @p std::uint64_t */
    pt_float,        /**< equivalent of @p float */
    pt_double,       /**< equivalent of @p double */
    pt_long_double,  /**< equivalent of <tt>long double</tt> */
    pt_u8string,     /**< equivalent of @p std::string */
    pt_u16string,    /**< equivalent of @p std::u16string */
    pt_u32string,    /**< equivalent of @p std::u32string */
    pt_atom,         /**< equivalent of @p atom_value */
    pt_null          /**< equivalent of @p void */
};

constexpr const char* primitive_type_names[] = {
    "pt_int8",        "pt_int16",       "pt_int32",       "pt_int64",
    "pt_uint8",       "pt_uint16",      "pt_uint32",      "pt_uint64",
    "pt_float",       "pt_double",      "pt_long_double",
    "pt_u8string",    "pt_u16string",   "pt_u32string",
    "pt_atom",        "pt_null"
};

/**
 * @ingroup TypeSystem
 * @brief Maps a #primitive_type value to its name.
 * @param ptype Requestet @p primitive_type.
 * @returns A C-string representation of @p ptype.
 */
constexpr const char* primitive_type_name(primitive_type ptype) {
    return primitive_type_names[static_cast<int>(ptype)];
}

} // namespace cppa

#endif // CPPA_PRIMITIVE_TYPE_HPP
