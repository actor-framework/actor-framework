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

#ifndef CONTINUE_HELPER_HPP
#define CONTINUE_HELPER_HPP

#include <functional>

#include "caf/on.hpp"
#include "caf/behavior.hpp"
#include "caf/message_id.hpp"
#include "caf/message_handler.hpp"

namespace caf {

class local_actor;

/**
 * @brief Helper class to enable users to add continuations
 *        when dealing with synchronous sends.
 */
class continue_helper {

 public:

    using message_id_wrapper_tag = int;

    continue_helper(message_id mid, local_actor* self);

    /**
     * @brief Adds a continuation to the synchronous message handler
     *        that is invoked if the response handler successfully returned.
     * @param fun The continuation as functor object.
     */
    template<typename F>
    continue_helper& continue_with(F fun) {
        return continue_with(behavior::continuation_fun{
            message_handler{on(any_vals, arg_match) >> fun}});
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
    message_id get_message_id() const { return m_mid; }

 private:

    message_id m_mid;
    local_actor* m_self;

};

} // namespace caf

#endif // CONTINUE_HELPER_HPP
