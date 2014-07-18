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

#ifndef CAF_DETAIL_TUPLE_DUMMY_HPP
#define CAF_DETAIL_TUPLE_DUMMY_HPP

#include "caf/fwd.hpp"

#include "caf/detail/type_list.hpp"

#include "caf/detail/message_iterator.hpp"

namespace caf {
namespace detail {

struct tuple_dummy {
    using types = detail::empty_type_list;
    using const_iterator = message_iterator<tuple_dummy>;
    inline size_t size() const { return 0; }
    inline void* mutable_at(size_t) { return nullptr; }
    inline const void* at(size_t) const { return nullptr; }
    inline const uniform_type_info* type_at(size_t) const { return nullptr; }
    inline const std::type_info* type_token() const {
        return &typeid(detail::empty_type_list);
    }
    inline bool dynamically_typed() const { return false; }
    inline const_iterator begin() const {
        return {this};
    }
    inline const_iterator end() const {
        return {this, 0};
    }

};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TUPLE_DUMMY_HPP
