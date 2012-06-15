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
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
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


#ifndef CPPA_STATIC_FOREACH_HPP
#define CPPA_STATIC_FOREACH_HPP

#include "cppa/get.hpp"

namespace cppa { namespace util {

template<bool BeginLessEnd, size_t Begin, size_t End>
struct static_foreach_impl {
    template<typename Container, typename Fun, typename... Args>
    static inline void _(const Container& c, Fun& f, Args&&... args) {
        f(get<Begin>(c), std::forward<Args>(args)...);
        static_foreach_impl<(Begin+1 < End), Begin+1, End>
               ::_(c, f, std::forward<Args>(args)...);
    }
    template<typename Container, typename Fun, typename... Args>
    static inline void _ref(Container& c, Fun& f, Args&&... args) {
        f(get_ref<Begin>(c), std::forward<Args>(args)...);
        static_foreach_impl<(Begin+1 < End), Begin+1, End>
               ::_ref(c, f, std::forward<Args>(args)...);
    }
    template<typename Container, typename Fun, typename... Args>
    static inline void _auto(Container& c, Fun& f, Args&&... args) {
        _ref(c, f, std::forward<Args>(args)...);
    }
    template<typename Container, typename Fun, typename... Args>
    static inline void _auto(const Container& c, Fun& f, Args&&... args) {
        _(c, f, std::forward<Args>(args)...);
    }
    template<typename Container, typename Fun, typename... Args>
    static inline bool eval(const Container& c, Fun& f, Args&&... args) {
        return    f(get<Begin>(c), std::forward<Args>(args)...)
               && static_foreach_impl<(Begin+1 < End), Begin+1, End>
                  ::eval(c, f, std::forward<Args>(args)...);
    }
    template<typename Container, typename Fun, typename... Args>
    static inline bool eval_or(const Container& c, Fun& f, Args&&... args) {
        return    f(get<Begin>(c), std::forward<Args>(args)...)
               || static_foreach_impl<(Begin+1 < End), Begin+1, End>
                  ::eval_or(c, f, std::forward<Args>(args)...);
    }
};

template<size_t X, size_t Y>
struct static_foreach_impl<false, X, Y> {
    template<typename... Args>
    static inline void _(Args&&...) { }
    template<typename... Args>
    static inline void _ref(Args&&...) { }
    template<typename... Args>
    static inline void _auto(Args&&...) { }
    template<typename... Args>
    static inline bool eval(Args&&...) { return true; }
    template<typename... Args>
    static inline bool eval_or(Args&&...) { return false; }
};

/**
 * @ingroup MetaProgramming
 * @brief A for loop that can be used with tuples.
 */
template<size_t Begin, size_t End>
struct static_foreach : static_foreach_impl<(Begin < End), Begin, End> {
};

} } // namespace cppa::util

#endif // CPPA_STATIC_FOREACH_HPP
