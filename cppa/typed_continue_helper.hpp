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
 * Copyright (C) 2011-2014                                                    *
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


#ifndef CPPA_TYPED_CONTINUE_HELPER_HPP
#define CPPA_TYPED_CONTINUE_HELPER_HPP

#include "cppa/continue_helper.hpp"

#include "cppa/util/type_traits.hpp"

#include "cppa/detail/typed_actor_util.hpp"

namespace cppa {

template<typename OutputList>
class typed_continue_helper {

 public:

    typedef int message_id_wrapper_tag;

    typed_continue_helper(message_id mid, local_actor* self) : m_ch(mid, self) { }

    template<typename F>
    typed_continue_helper<typename util::get_callable_trait<F>::result_type>
    continue_with(F fun) {
        detail::assert_types<OutputList, F>();
        m_ch.continue_with(std::move(fun));
        return {m_ch};
    }

    inline message_id get_message_id() const {
        return m_ch.get_message_id();
    }

    typed_continue_helper(continue_helper ch) : m_ch(std::move(ch)) { }

 private:

    continue_helper m_ch;

};

} // namespace cppa

#endif // CPPA_TYPED_CONTINUE_HELPER_HPP
