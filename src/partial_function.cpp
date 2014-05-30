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


#include "cppa/to_string.hpp"

#include "cppa/config.hpp"
#include "cppa/partial_function.hpp"

namespace cppa {

partial_function::partial_function(impl_ptr ptr) : m_impl(std::move(ptr)) { }

void detail::behavior_impl::handle_timeout() { }

} // namespace cppa

namespace cppa {
namespace detail {

behavior_impl_ptr combine(behavior_impl_ptr lhs, const partial_function& rhs) {
    return lhs->or_else(rhs.as_behavior_impl());
}

behavior_impl_ptr combine(const partial_function& lhs, behavior_impl_ptr rhs) {
    return lhs.as_behavior_impl()->or_else(rhs);
}

behavior_impl_ptr extract(const partial_function& arg) {
    return arg.as_behavior_impl();
}

} // namespace util
} // namespace cppa

