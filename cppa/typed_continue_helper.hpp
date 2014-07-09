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

#ifndef CPPA_TYPED_CONTINUE_HELPER_HPP
#define CPPA_TYPED_CONTINUE_HELPER_HPP

#include "cppa/continue_helper.hpp"

#include "cppa/detail/type_traits.hpp"

#include "cppa/detail/typed_actor_util.hpp"

namespace cppa {

template<typename OutputList>
class typed_continue_helper {

 public:

    using message_id_wrapper_tag = int;

    typed_continue_helper(message_id mid, local_actor* self)
            : m_ch(mid, self) {}

    template<typename F>
    typed_continue_helper<typename detail::get_callable_trait<F>::result_type>
    continue_with(F fun) {
        detail::assert_types<OutputList, F>();
        m_ch.continue_with(std::move(fun));
        return {m_ch};
    }

    inline message_id get_message_id() const { return m_ch.get_message_id(); }

    typed_continue_helper(continue_helper ch) : m_ch(std::move(ch)) {}

 private:

    continue_helper m_ch;

};

} // namespace cppa

#endif // CPPA_TYPED_CONTINUE_HELPER_HPP
