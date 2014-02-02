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
 * Copyright (C) 2011-2013                                                    *
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


#ifndef TYPED_BEHAVIOR_HPP
#define TYPED_BEHAVIOR_HPP

#include "cppa/behavior.hpp"
#include "cppa/match_expr.hpp"

#include "cppa/detail/typed_actor_util.hpp"

namespace cppa {

namespace detail {

template<typename T>
struct match_hint_to_void {
    typedef T type;
};

template<>
struct match_hint_to_void<match_hint> {
    typedef void type;
};

template<typename T>
struct all_match_hints_to_void {
    typedef typename T::input_types input_types;
    typedef typename util::tl_map<
                typename T::output_types,
                match_hint_to_void
            >::type
            output_types;
    typedef typename replies_to_from_type_list<
                input_types,
                output_types
            >::type
            type;
};

// this function is called from typed_behavior<...>::set and its whole
// purpose is to give users a nicer error message on a type mismatch
// (this function only has the type informations needed to understand the error)
template<class SignatureList, class InputList>
void static_check_typed_behavior_input() {
    constexpr bool is_equal = util::tl_equal<SignatureList, InputList>::value;
    // note: it might be worth considering to allow a wildcard in the
    //       InputList if its return type is identical to all "missing"
    //       input types ... however, it might lead to unexpected results
    //       and would cause a lot of not-so-straightforward code here
    static_assert(is_equal, "given pattern cannot be used to initialize "
                            "typed behavior (exact match needed)");
}

} // namespace detail

template<typename... Signatures>
class typed_actor;

template<class Base, class Subtype, class BehaviorType>
class behavior_stack_based_impl;

template<typename... Signatures>
class typed_behavior {

    template<typename... OtherSignatures>
    friend class typed_actor;

    template<class Base, class Subtype, class BehaviorType>
    friend class behavior_stack_based_impl;

    typed_behavior() = delete;

 public:

    typed_behavior(typed_behavior&&) = default;
    typed_behavior(const typed_behavior&) = default;
    typed_behavior& operator=(typed_behavior&&) = default;
    typed_behavior& operator=(const typed_behavior&) = default;

    typedef util::type_list<Signatures...> signatures;

    template<typename... Cs, typename... Ts>
    typed_behavior(match_expr<Cs...> expr, Ts&&... args) {
        set(match_expr_collect(std::move(expr), std::forward<Ts>(args)...));
    }

    template<typename... Cs>
    typed_behavior& operator=(match_expr<Cs...> expr) {
        set(std::move(expr));
        return *this;
    }

    explicit operator bool() const {
        return static_cast<bool>(m_bhvr);
    }

   /**
     * @brief Invokes the timeout callback.
     */
    inline void handle_timeout() {
        m_bhvr.handle_timeout();
    }

    /**
     * @brief Returns the duration after which receives using
     *        this behavior should time out.
     */
    inline const util::duration& timeout() const {
        return m_bhvr.timeout();
    }

 private:

    behavior& unbox() { return m_bhvr; }

    template<typename... Cs>
    void set(match_expr<Cs...>&& expr) {
        // check for (the lack of) guards
        static_assert(util::conjunction<
                          detail::match_expr_has_no_guard<Cs>::value...
                      >::value,
                      "typed actors are not allowed to use guard expressions");
        // returning a match_hint from a message handler does
        // not send anything back, so we can consider match_hint to be void
        typedef typename util::tl_map<
                    util::type_list<
                        typename detail::deduce_signature<Cs>::type...
                    >,
                    detail::all_match_hints_to_void
                >::type
                input;
        detail::static_check_typed_behavior_input<signatures, input>();
        // final (type-erasure) step
        m_bhvr = std::move(expr);
    }

    behavior m_bhvr;

};

} // namespace cppa

#endif // TYPED_BEHAVIOR_HPP
