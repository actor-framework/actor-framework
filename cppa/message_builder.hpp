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

#ifndef CPPA_MESSAGE_BUILDER_HPP
#define CPPA_MESSAGE_BUILDER_HPP

#include <vector>

#include "cppa/message.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/detail/message_data.hpp"
#include "cppa/detail/implicit_conversions.hpp"

namespace cppa {

class message_builder {

 public:

    message_builder() = default;

    message_builder(const message_builder&) = delete;

    message_builder& operator=(const message_builder&) = delete;

    template<typename Iter>
    message_builder(Iter first, Iter last) {
        append(first, last);
    }

    template<typename T>
    message_builder& append(T what) {
        return append_impl<T>(std::move(what));
    }

    template<typename Iter>
    message_builder& append(Iter first, Iter last) {
        using vtype = typename util::rm_const_and_ref<decltype(*first)>::type;
        using converted = typename detail::implicit_conversions<vtype>::type;
        auto uti = uniform_typeid<converted>();
        for (; first != last; ++first) {
            auto uval = uti->create();
            *reinterpret_cast<converted*>(uval->val) = *first;
            append(std::move(uval));
        }
        return *this;
    }

    message_builder& append(uniform_value what);

    message to_message();

    optional<message> apply(message_handler handler);

 private:

    template<typename T>
    message_builder&
    append_impl(typename detail::implicit_conversions<T>::type what) {
        typedef decltype(what) type;
        auto uti = uniform_typeid<type>();
        auto uval = uti->create();
        *reinterpret_cast<type*>(uval->val) = std::move(what);
        return append(std::move(uval));
    }

    std::vector<uniform_value> m_elements;

};

} // namespace cppa

#endif // CPPA_MESSAGE_BUILDER_HPP
