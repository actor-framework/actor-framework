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


#ifndef CPPA_TYPED_BEHAVIOR_HPP
#define CPPA_TYPED_BEHAVIOR_HPP

#include "cppa/behavior.hpp"
#include "cppa/match_expr.hpp"
#include "cppa/typed_continue_helper.hpp"

#include "cppa/detail/typed_actor_util.hpp"

namespace cppa {

namespace detail {

template<typename... Rs>
class functor_based_typed_actor;

// converts a list of replies_to<...>::with<...> elements to a list of
// lists containing the replies_to<...> half only
template<typename List>
struct input_only;

template<typename... Ts>
struct input_only<util::type_list<Ts...>> {
    typedef util::type_list<typename Ts::input_types...> type;
};

typedef util::type_list<skip_message_t> skip_list;

template<class List>
struct unbox_typed_continue_helper {
    // do nothing if List is actually a list, i.e., not a typed_continue_helper
    typedef List type;
};

template<class List>
struct unbox_typed_continue_helper<util::type_list<typed_continue_helper<List>>> {
    typedef List type;
};

template<class Input, class RepliesToWith>
struct same_input : std::is_same<Input, typename RepliesToWith::input_types> { };

template<class Output, class RepliesToWith>
struct same_output_or_skip_message_t {
    typedef typename RepliesToWith::output_types other;
    static constexpr bool value =
               std::is_same<Output, typename RepliesToWith::output_types>::value
            || std::is_same<Output, util::type_list<skip_message_t>>::value;
};

template<typename SList>
struct valid_input_predicate {
    //typedef typename input_only<SList>::type s_inputs;
    template<typename Expr>
    struct inner {
        typedef typename Expr::input_types input_types;
        typedef typename unbox_typed_continue_helper<
                    typename Expr::output_types
                >::type
                output_types;
        // get matching elements for input type
        typedef typename util::tl_filter<
                    SList,
                    util::tbind<same_input, input_types>::template type
                >::type
                filtered_slist;
        static_assert(util::tl_size<filtered_slist>::value > 0,
                      "cannot assign given match expression to "
                      "typed behavior, because the expression "
                      "contains at least one pattern that is "
                      "not defined in the actor's type");
        static constexpr bool value = util::tl_exists<
                                          filtered_slist,
                                          util::tbind<
                                              same_output_or_skip_message_t,
                                              output_types
                                          >::template type
                                      >::value;
        // check whether given output matches in the filtered list
        static_assert(value,
                      "cannot assign given match expression to "
                      "typed behavior, because at least one return "
                      "type does not match");
    };
};

// Tests whether the input list (IList) matches the
// signature list (SList) for a typed actor behavior
template<class SList, class IList>
struct valid_input {
    // check for each element in IList that there's an element in SList that
    // (1) has an identical input type list
    // (2)    has an identical output type list
    //     OR the output of the element in IList is skip_message_t
    static_assert(util::tl_is_distinct<IList>::value,
                  "given pattern is not distinct");
    static constexpr bool value =
           util::tl_size<SList>::value == util::tl_size<IList>::value
        && util::tl_forall<
               IList,
               valid_input_predicate<SList>::template inner
           >::value;
};

// this function is called from typed_behavior<...>::set and its whole
// purpose is to give users a nicer error message on a type mismatch
// (this function only has the type informations needed to understand the error)
template<class SignatureList, class InputList>
void static_check_typed_behavior_input() {
    constexpr bool is_valid = valid_input<SignatureList, InputList>::value;
    // note: it might be worth considering to allow a wildcard in the
    //       InputList if its return type is identical to all "missing"
    //       input types ... however, it might lead to unexpected results
    //       and would cause a lot of not-so-straightforward code here
    static_assert(is_valid, "given pattern cannot be used to initialize "
                            "typed behavior (exact match needed)");
}

} // namespace detail

template<typename... Rs>
class typed_actor;

template<class Base, class Subtype, class BehaviorType>
class behavior_stack_based_impl;

template<typename... Rs>
class typed_behavior {

    template<typename... OtherRs>
    friend class typed_actor;

    template<class Base, class Subtype, class BehaviorType>
    friend class behavior_stack_based_impl;

    template<typename...>
    friend class detail::functor_based_typed_actor;

 public:

    typed_behavior(typed_behavior&&) = default;
    typed_behavior(const typed_behavior&) = default;
    typed_behavior& operator=(typed_behavior&&) = default;
    typed_behavior& operator=(const typed_behavior&) = default;

    typedef util::type_list<Rs...> signatures;

    template<typename T, typename... Ts>
    typed_behavior(T arg, Ts&&... args) {
        set(match_expr_collect(detail::lift_to_match_expr(std::move(arg)),
                               detail::lift_to_match_expr(std::forward<Ts>(args))...));
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

    typed_behavior() = default;

    behavior& unbox() { return m_bhvr; }

    template<typename... Cs>
    void set(match_expr<Cs...>&& expr) {
        // check for (the lack of) guards
        static_assert(util::conjunction<
                          detail::match_expr_has_no_guard<Cs>::value...
                      >::value,
                      "typed actors are not allowed to use guard expressions");
        // do some transformation before type-checking the input signatures
        typedef typename util::tl_map<
                    util::type_list<
                        typename detail::deduce_signature<Cs>::type...
                    >
                >::type
                input;
        // check types
        detail::static_check_typed_behavior_input<signatures, input>();
        // final (type-erasure) step
        m_bhvr = std::move(expr);
    }

    behavior m_bhvr;

};

} // namespace cppa

#endif // CPPA_TYPED_BEHAVIOR_HPP
