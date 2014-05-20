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


#ifndef CPPA_CONTINUE_HELPER_HPP
#define CPPA_CONTINUE_HELPER_HPP

#include <functional>

#include "cppa/on.hpp"
#include "cppa/behavior.hpp"
#include "cppa/message_id.hpp"
#include "cppa/partial_function.hpp"

namespace cppa {

class local_actor;

/**
 * @brief Helper class to enable users to add continuations
 *        when dealing with synchronous sends.
 */
class continue_helper {

 public:

    typedef int message_id_wrapper_tag;

    continue_helper(message_id mid, local_actor* self);

    /**
     * @brief Adds a continuation to the synchronous message handler
     *        that is invoked if the response handler successfully returned.
     * @param fun The continuation as functor object.
     */
    template<typename F>
    continue_helper& continue_with(F fun) {
        return continue_with(behavior::continuation_fun{partial_function{
                   on(any_vals, arg_match) >> fun
               }});
    }

    /**
     * @brief Adds a continuation to the synchronous message handler
     *        that is invoked if the response handler successfully returned.
     * @param fun The continuation as functor object.
     */
    continue_helper& continue_with(behavior::continuation_fun fun);

    /**
     * @brief Returns the ID of the expected response message.
     */
    message_id get_message_id() const {
        return m_mid;
    }

 private:

    message_id m_mid;
    local_actor* m_self;

};

} // namespace cppa

#endif // CPPA_CONTINUE_HELPER_HPP
