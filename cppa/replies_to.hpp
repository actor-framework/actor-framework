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


#ifndef CPPA_REPLIES_TO_HPP
#define CPPA_REPLIES_TO_HPP

#include "cppa/util/type_list.hpp"

namespace cppa {

template<typename... Is>
struct replies_to {
    template<typename... Os>
    struct with {
        typedef util::type_list<Is...> input_types;
        typedef util::type_list<Os...> output_types;
    };
};

template<class InputList, class OutputList>
struct replies_to_from_type_list;

template<typename... Is, typename... Os>
struct replies_to_from_type_list<util::type_list<Is...>, util::type_list<Os...>> {
    typedef typename replies_to<Is...>::template with<Os...> type;
};

} // namespace cppa

#endif // CPPA_REPLIES_TO_HPP
