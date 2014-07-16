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

#ifndef CAF_UNIT_HPP
#define CAF_UNIT_HPP

namespace caf {

struct unit_t {
    constexpr unit_t() {}
    constexpr unit_t(const unit_t&) {}
    template<typename T>
    explicit constexpr unit_t(T&&) {}

};

static constexpr unit_t unit = unit_t{};

template<typename T>
struct lift_void {
    using type = T;

};

template<>
struct lift_void<void> {
    using type = unit_t;

};

template<typename T>
struct unlift_void {
    using type = T;

};

template<>
struct unlift_void<unit_t> {
    using type = void;

};

} // namespace caf

#endif // CAF_UNIT_HPP
