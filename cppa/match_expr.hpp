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


#ifndef CPPA_MATCH_EXPR_HPP
#define CPPA_MATCH_EXPR_HPP

#include "cppa/option.hpp"
#include "cppa/pattern.hpp"
#include "cppa/guard_expr.hpp"
#include "cppa/partial_function.hpp"
#include "cppa/tpartial_function.hpp"

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/rm_option.hpp"
#include "cppa/util/purge_refs.hpp"
#include "cppa/util/left_or_right.hpp"
#include "cppa/util/deduce_ref_type.hpp"

#include "cppa/detail/matches.hpp"
#include "cppa/detail/projection.hpp"
#include "cppa/detail/value_guard.hpp"
#include "cppa/detail/pseudo_tuple.hpp"

namespace cppa { namespace detail {

// covers wildcard_position::multiple and wildcard_position::in_between
template<wildcard_position, class Pattern, class FilteredPattern>
struct invoke_policy_impl {
    typedef FilteredPattern filtered_pattern;

    template<class Tuple>
    static bool can_invoke(const std::type_info& type_token,
                           const Tuple& tup) {
        typedef typename match_impl_from_type_list<Tuple, Pattern>::type mimpl;
        return type_token == typeid(filtered_pattern) ||  mimpl::_(tup);
    }

    template<class Target, typename PtrType, class Tuple>
    static bool invoke(Target& target,
                       const std::type_info& type_token,
                       detail::tuple_impl_info,
                       PtrType*,
                       Tuple& tup) {
        typedef typename match_impl_from_type_list<
                    typename std::remove_const<Tuple>::type,
                    Pattern
                >::type
                mimpl;

        util::fixed_vector<size_t, filtered_pattern::size> mv;
        if (type_token == typeid(filtered_pattern) ||  mimpl::_(tup, mv)) {
            typedef typename pseudo_tuple_from_type_list<filtered_pattern>::type
                    ttup_type;
            ttup_type ttup;
            // if we strip const here ...
            for (size_t i = 0; i < filtered_pattern::size; ++i) {
                ttup[i] = const_cast<void*>(tup.at(mv[i]));
            }
            // ... we restore it here again
            typedef typename util::if_else<
                        std::is_const<Tuple>,
                        const ttup_type&,
                        util::wrapped<ttup_type&>
                    >::type
                    ttup_ref;
            ttup_ref ttup_fwd = ttup;
            return util::unchecked_apply_tuple<bool>(target, ttup_fwd);
        }
        return false;
    }
};

template<class Pattern, typename... Ts>
struct invoke_policy_impl<wildcard_position::nil,
                          Pattern, util::type_list<Ts...> > {
    typedef util::type_list<Ts...> filtered_pattern;

    typedef detail::tdata<Ts...> native_data_type;

    typedef typename detail::static_types_array<Ts...> arr_type;

    template<class Target, class Tup>
    static bool invoke(std::integral_constant<bool, false>, Target&, Tup&) {
        return false;
    }

    template<class Target, class Tup>
    static bool invoke(std::integral_constant<bool, true>,
                       Target& target, Tup& tup) {
        return util::unchecked_apply_tuple<bool>(target, tup);
    }

    template<class Target, typename PtrType, class Tuple>
    static bool invoke(Target& target,
                       const std::type_info&,
                       detail::tuple_impl_info,
                       PtrType*,
                       Tuple& tup,
                       typename std::enable_if<
                           std::is_same<
                               typename std::remove_const<Tuple>::type,
                               detail::abstract_tuple
                           >::value == false
                       >::type* = 0) {
        static constexpr bool can_apply =
                    util::tl_binary_forall<
                        typename util::tl_map<
                            typename Tuple::types,
                            util::purge_refs
                        >::type,
                        filtered_pattern,
                        std::is_same
                    >::value;
        return invoke(std::integral_constant<bool, can_apply>{}, target, tup);
    }

    template<class Target, typename PtrType, typename Tuple>
    static bool invoke(Target& target,
                       const std::type_info& arg_types,
                       detail::tuple_impl_info timpl,
                       PtrType* native_arg,
                       Tuple& tup,
                       typename std::enable_if<
                           std::is_same<
                               typename std::remove_const<Tuple>::type,
                               detail::abstract_tuple
                           >::value
                       >::type* = 0) {
        if (arg_types == typeid(filtered_pattern)) {
            if (native_arg) {
                typedef typename util::if_else_c<
                            std::is_const<PtrType>::value,
                            const native_data_type*,
                            util::wrapped<native_data_type*>
                        >::type
                        cast_type;
                auto arg = reinterpret_cast<cast_type>(native_arg);
                return util::unchecked_apply_tuple<bool>(target, *arg);
            }
            // 'fall through'
        }
        else if (timpl == detail::dynamically_typed) {
            auto& arr = arr_type::arr;
            if (tup.size() != filtered_pattern::size) {
                return false;
            }
            for (size_t i = 0; i < filtered_pattern::size; ++i) {
                if (arr[i] != tup.type_at(i)) {
                    return false;
                }
            }
            // 'fall through'
        }
        else {
            return false;
        }
        typedef pseudo_tuple<Ts...> ttup_type;
        ttup_type ttup;
        // if we strip const here ...
        for (size_t i = 0; i < sizeof...(Ts); ++i)
            ttup[i] = const_cast<void*>(tup.at(i));
        // ... we restore it here again
        typedef typename util::if_else<
                    std::is_const<PtrType>,
                    const ttup_type&,
                    util::wrapped<ttup_type&>
                >::type
                ttup_ref;
        ttup_ref ttup_fwd = ttup;
        return util::unchecked_apply_tuple<bool>(target, ttup_fwd);
    }

    template<class Tuple>
    static bool can_invoke(const std::type_info& arg_types, const Tuple&) {
        return arg_types == typeid(filtered_pattern);
    }
};

template<>
struct invoke_policy_impl<wildcard_position::leading,
                          util::type_list<anything>,
                          util::type_list<> > {
    template<class Tuple>
    static inline bool can_invoke(const std::type_info&,
                                  const Tuple&) {
        return true;
    }

    template<class Target, typename PtrType, typename Tuple>
    static bool invoke(Target& target,
                       const std::type_info&,
                       detail::tuple_impl_info,
                       PtrType*,
                       Tuple&) {
        return target();
    }

};

template<class Pattern, typename... Ts>
struct invoke_policy_impl<wildcard_position::trailing,
                          Pattern, util::type_list<Ts...> > {
    typedef util::type_list<Ts...> filtered_pattern;

    template<class Tuple>
    static bool can_invoke(const std::type_info& arg_types,
                           const Tuple& tup) {
        if (arg_types == typeid(filtered_pattern)) {
            return true;
        }
        typedef detail::static_types_array<Ts...> arr_type;
        auto& arr = arr_type::arr;
        if (tup.size() < filtered_pattern::size) {
            return false;
        }
        for (size_t i = 0; i < filtered_pattern::size; ++i) {
            if (arr[i] != tup.type_at(i)) {
                return false;
            }
        }
        return true;
    }

    template<class Target, typename PtrType, class Tuple>
    static bool invoke(Target& target,
                       const std::type_info& arg_types,
                       detail::tuple_impl_info,
                       PtrType*,
                       Tuple& tup) {
        if (!can_invoke(arg_types, tup)) return false;
        typedef pseudo_tuple<Ts...> ttup_type;
        ttup_type ttup;
        for (size_t i = 0; i < sizeof...(Ts); ++i)
            ttup[i] = const_cast<void*>(tup.at(i));
        // ensure const-correctness
        typedef typename util::if_else<
                    std::is_const<Tuple>,
                    const ttup_type&,
                    util::wrapped<ttup_type&>
                >::type
                ttup_ref;
        ttup_ref ttup_fwd = ttup;
        return util::unchecked_apply_tuple<bool>(target, ttup_fwd);
    }

};

template<class Pattern, typename... Ts>
struct invoke_policy_impl<wildcard_position::leading,
                          Pattern, util::type_list<Ts...> > {
    typedef util::type_list<Ts...> filtered_pattern;

    template<class Tuple>
    static bool can_invoke(const std::type_info& arg_types,
                           const Tuple& tup) {
        if (arg_types == typeid(filtered_pattern)) {
            return true;
        }
        typedef detail::static_types_array<Ts...> arr_type;
        auto& arr = arr_type::arr;
        if (tup.size() < filtered_pattern::size) {
            return false;
        }
        size_t i = tup.size() - filtered_pattern::size;
        size_t j = 0;
        while (j < filtered_pattern::size) {
            if (arr[i++] != tup.type_at(j++)) {
                return false;
            }
        }
        return true;
    }

    template<class Target, typename PtrType, class Tuple>
    static bool invoke(Target& target,
                       const std::type_info& arg_types,
                       detail::tuple_impl_info,
                       PtrType*,
                       Tuple& tup) {
        if (!can_invoke(arg_types, tup)) return false;
        typedef pseudo_tuple<Ts...> ttup_type;
        ttup_type ttup;
        size_t i = tup.size() - filtered_pattern::size;
        size_t j = 0;
        while (j < filtered_pattern::size) {
            ttup[j++] = const_cast<void*>(tup.at(i++));
        }
        // ensure const-correctness
        typedef typename util::if_else<
                    std::is_const<Tuple>,
                    const ttup_type&,
                    util::wrapped<ttup_type&>
                >::type
                ttup_ref;
        ttup_ref ttup_fwd = ttup;
        return util::unchecked_apply_tuple<bool>(target, ttup_fwd);
    }

};

template<class Pattern>
struct invoke_policy
        : invoke_policy_impl<
            get_wildcard_position<Pattern>(),
            Pattern,
            typename util::tl_filter_not_type<Pattern, anything>::type> {
};


template<class Pattern, class Projection, class PartialFunction>
struct projection_partial_function_pair : std::pair<Projection, PartialFunction> {
    template<typename... Args>
    projection_partial_function_pair(Args&&... args)
        : std::pair<Projection, PartialFunction>(std::forward<Args>(args)...) {
    }

    typedef Pattern pattern_type;
};

template<class Expr, class Guard, class Transformers, class Pattern>
struct get_case_ {
    typedef typename util::get_callable_trait<Expr>::type ctrait;

    typedef typename util::tl_filter_not_type<
                Pattern,
                anything
            >::type
            filtered_pattern;

    typedef typename util::tl_pad_right<
                Transformers,
                filtered_pattern::size
            >::type
            padded_transformers;

    typedef typename util::tl_map<
                filtered_pattern,
                std::add_const,
                std::add_lvalue_reference
            >::type
            base_signature;

    typedef typename util::tl_map_conditional<
                typename util::tl_pad_left<
                    typename ctrait::arg_types,
                    filtered_pattern::size
                >::type,
                std::is_lvalue_reference,
                false,
                std::add_const,
                std::add_lvalue_reference
            >::type
            padded_expr_args;


    // override base signature with required argument types of Expr
    // and result types of transformation
    typedef typename util::tl_zip<
                typename util::tl_map<
                    padded_transformers,
                    util::get_result_type,
                    util::rm_option,
                    std::add_lvalue_reference
                >::type,
                typename util::tl_zip<
                    padded_expr_args,
                    base_signature,
                    util::left_or_right
                >::type,
                util::left_or_right
            >::type
            partial_fun_signature;

    // 'inherit' mutable references from partial_fun_signature
    // for arguments without transformation
    typedef typename util::tl_zip<
                typename util::tl_zip<
                    padded_transformers,
                    partial_fun_signature,
                    util::if_not_left
                >::type,
                base_signature,
                util::deduce_ref_type
            >::type
            projection_signature;

    typedef typename projection_from_type_list<
                padded_transformers,
                projection_signature
            >::type
            type1;

    typedef typename get_tpartial_function<
                Expr,
                Guard,
                partial_fun_signature
            >::type
            type2;

    typedef projection_partial_function_pair<Pattern, type1, type2> type;

};

template<bool IsComplete, class Expr, class Guard, class Transformers, class Pattern>
struct get_case {
    typedef typename get_case_<Expr, Guard, Transformers, Pattern>::type type;
};

template<class Expr, class Guard, class Transformers, class Pattern>
struct get_case<false, Expr, Guard, Transformers, Pattern> {
    typedef typename util::tl_pop_back<Pattern>::type lhs_pattern;
    typedef typename util::tl_map<
                typename util::get_arg_types<Expr>::types,
                util::rm_ref
            >::type
            rhs_pattern;
    typedef typename get_case_<
                Expr,
                Guard,
                Transformers,
                typename util::tl_concat<lhs_pattern, rhs_pattern>::type
            >::type
            type;
};

/*
template<class Expr>
struct get_case<true, Expr, value_guard<util::type_list<> >,
                util::type_list<>, util::type_list<anything> > {
    typedef typename get_case_<
                Expr,
                value_guard<util::type_list<> >,
                util::type_list<>,
                util::type_list<anything>
            >::type
            type;
};
*/

template<typename First, typename Second>
struct pjf_same_pattern
        : std::is_same<typename First::second::pattern_type,
                       typename Second::second::pattern_type> {
};

// last invocation step; evaluates a {projection, tpartial_function} pair
template<typename Data>
struct invoke_helper3 {
    const Data& data;
    invoke_helper3(const Data& mdata) : data(mdata) { }
    template<size_t Pos, typename T, typename... Args>
    inline bool operator()(util::type_pair<std::integral_constant<size_t, Pos>, T>,
                           Args&&... args) const {
        const auto& target = get<Pos>(data);
        return target.first(target.second, std::forward<Args>(args)...);
        //return (get<Pos>(data))(args...);
    }
};

template<class Data, class Token, class Pattern>
struct invoke_helper2 {
    typedef Pattern pattern_type;
    typedef typename util::tl_filter_not_type<pattern_type, anything>::type arg_types;
    const Data& data;
    invoke_helper2(const Data& mdata) : data(mdata) { }
    template<typename... Args>
    bool invoke(Args&&... args) const {
        typedef invoke_policy<Pattern> impl;
        return impl::invoke(*this, std::forward<Args>(args)...);
    }
    // resolved argument list (called from invoke_policy)
    template<typename... Args>
    bool operator()(Args&&... args) const {
        //static_assert(false, "foo");
        Token token;
        invoke_helper3<Data> fun{data};
        return util::static_foreach<0, Token::size>::eval_or(token, fun, std::forward<Args>(args)...);
    }
};

// invokes a group of {projection, tpartial_function} pairs
template<typename Data>
struct invoke_helper {
    const Data& data;
    std::uint64_t bitfield;
    invoke_helper(const Data& mdata, std::uint64_t bits) : data(mdata), bitfield(bits) { }
    // token: type_list<type_pair<integral_constant<size_t, X>,
    //                            std::pair<projection, tpartial_function>>,
    //                  ...>
    // all {projection, tpartial_function} pairs have the same pattern
    // thus, can be invoked from same data
    template<class Token, typename... Args>
    bool operator()(Token, Args&&... args) {
        typedef typename Token::head type_pair;
        typedef typename type_pair::second leaf_pair;
        if (bitfield & 0x01) {
            // next invocation step
            invoke_helper2<Data,
                           Token,
                           typename leaf_pair::pattern_type> fun{data};
            return fun.invoke(std::forward<Args>(args)...);
        }
        bitfield >>= 1;
        //++enabled;
        return false;
    }
};

struct can_invoke_helper {
    std::uint64_t& bitfield;
    size_t i;
    can_invoke_helper(std::uint64_t& mbitfield) : bitfield(mbitfield), i(0) { }
    template<class Token, typename... Args>
    void operator()(Token, Args&&... args) {
        typedef typename Token::head type_pair;
        typedef typename type_pair::second leaf_pair;
        typedef invoke_policy<typename leaf_pair::pattern_type> impl;
        if (impl::can_invoke(std::forward<Args>(args)...)) {
            bitfield |= (0x01 << i);
        }
        ++i;
    }
};

template<typename T>
struct is_manipulator_case {
    static constexpr bool value = T::second_type::manipulates_args;
};

template<bool IsManipulator, typename T0, typename T1>
struct mexpr_fwd_ {
    typedef T1 type;
};

template<typename T>
struct mexpr_fwd_<false, const T&, T> {
    typedef std::reference_wrapper<const T> type;
};

template<typename T>
struct mexpr_fwd_<true, T&, T> {
    typedef std::reference_wrapper<T> type;
};

template<bool IsManipulator, typename T>
struct mexpr_fwd {
    typedef typename mexpr_fwd_<
                IsManipulator,
                T,
                typename detail::implicit_conversions<
                    typename util::rm_ref<T>::type
                >::type
            >::type
            type;
};

} } // namespace cppa::detail

namespace cppa {

template<class... Cases>
class match_expr {

    static_assert(sizeof...(Cases) < 64, "too many functions");

 public:

    typedef util::type_list<Cases...> cases_list;

    typedef typename util::tl_group_by<
                typename util::tl_zip_with_index<cases_list>::type,
                detail::pjf_same_pattern
            >::type
            eval_order;

    static constexpr bool has_manipulator =
            util::tl_exists<cases_list, detail::is_manipulator_case>::value;

    template<typename... Args>
    match_expr(Args&&... args) : m_cases(std::forward<Args>(args)...) {
        init();
    }

    match_expr(match_expr&& other) : m_cases(std::move(other.m_cases)) {
        init();
    }

    match_expr(const match_expr& other) : m_cases(other.m_cases) {
        init();
    }

    bool invoke(const any_tuple& tup) {
        return _invoke(tup);
    }

    bool invoke(any_tuple& tup) {
        return _invoke(tup);
    }

    bool invoke(any_tuple&& tup) {
        any_tuple tmp{tup};
        return _invoke(tmp);
    }

    bool can_invoke(any_tuple const tup) {
        auto& type_token = *(tup.type_token());
        eval_order token;
        std::uint64_t tmp = 0;
        detail::can_invoke_helper fun{tmp};
        util::static_foreach<0, eval_order::size>
        ::_(token, fun, type_token, tup);
        return tmp != 0;
    }

    bool operator()(const any_tuple& tup) {
        return _invoke(tup);
    }

    bool operator()(any_tuple& tup) {
        return _invoke(tup);
    }

    bool operator()(any_tuple&& tup) {
        any_tuple tmp{tup};
        return _invoke(tmp);
    }

    template<typename... Args>
    bool operator()(Args&&... args) {
        typedef detail::tdata<
                    typename detail::mexpr_fwd<has_manipulator, Args>::type...>
                tuple_type;
        // applies implicit conversions etc
        tuple_type tup{std::forward<Args>(args)...};

        auto& type_token = typeid(typename tuple_type::types);
        auto enabled_begin = get_cache_entry(&type_token, tup);

        typedef typename util::if_else_c<
                    has_manipulator,
                    tuple_type&,
                    const util::wrapped<tuple_type&>
                >::type
                ref_type;

        typedef typename util::if_else_c<
                    has_manipulator,
                    void*,
                    util::wrapped<const void*>
                >::type
                ptr_type;

        eval_order token;
        detail::invoke_helper<decltype(m_cases)> fun{m_cases, enabled_begin};
        return util::static_foreach<0, eval_order::size>
                ::eval_or(token,
                          fun,
                          type_token,
                          detail::statically_typed,
                          static_cast<ptr_type>(nullptr),
                          static_cast<ref_type>(tup));
    }

    template<class... OtherCases>
    match_expr<Cases..., OtherCases...>
    or_else(const match_expr<OtherCases...>& other) const {
        detail::tdata<ge_reference_wrapper<Cases>...,
                      ge_reference_wrapper<OtherCases>...    > all_cases;
        collect_tdata(all_cases, m_cases, other.cases());
        return {all_cases};
    }

    inline const detail::tdata<Cases...>& cases() const {
        return m_cases;
    }

    struct pfun_impl : partial_function::impl {
        match_expr pfun;
        template<typename Arg>
        pfun_impl(const Arg& from) : pfun(from) { }
        bool invoke(any_tuple& tup) {
            return pfun.invoke(tup);
        }
        bool invoke(const any_tuple& tup) {
            return pfun.invoke(tup);
        }
        bool defined_at(const any_tuple& tup) {
            return pfun.can_invoke(tup);
        }
    };

    inline partial_function as_partial_function() const {
        return {partial_function::impl_ptr{new pfun_impl(*this)}};
    }

    inline operator partial_function() const {
        return as_partial_function();
    }

 private:

    // structure: tdata< tdata<type_list<...>, ...>,
    //                   tdata<type_list<...>, ...>,
    //                   ...>
    detail::tdata<Cases...> m_cases;

    static constexpr size_t cache_size = 10;
    //typedef std::array<bool, eval_order::size> cache_entry;
    //typedef typename cache_entry::iterator cache_entry_iterator;
    //typedef std::pair<const std::type_info*, cache_entry> cache_element;

    // std::uint64_t is used as a bitmask to enable/disable groups

    typedef std::pair<const std::type_info*, std::uint64_t> cache_element;

    util::fixed_vector<cache_element, cache_size> m_cache;

    // ring buffer like access to m_cache
    size_t m_cache_begin;
    size_t m_cache_end;

    cache_element m_dummy;

    static inline void advance_(size_t& i) {
        i = (i + 1) % cache_size;
    }

    inline size_t find_token_pos(const std::type_info* type_token) {
        for (size_t i = m_cache_begin ; i != m_cache_end; advance_(i)) {
            if (m_cache[i].first == type_token) return i;
        }
        return m_cache_end;
    }

    template<class Tuple>
    std::uint64_t get_cache_entry(const std::type_info* type_token,
                                  const Tuple& value) {
        CPPA_REQUIRE(type_token != nullptr);
        if (value.impl_type() == detail::dynamically_typed) {
            return m_dummy.second; // all groups enabled
        }
        size_t i = find_token_pos(type_token);
        // if we didn't found a cache entry ...
        if (i == m_cache_end) {
            // ... 'create' one (override oldest element in cache if full)
            advance_(m_cache_end);
            if (m_cache_end == m_cache_begin) advance_(m_cache_begin);
            m_cache[i].first = type_token;
            m_cache[i].second = 0;
            eval_order token;
            detail::can_invoke_helper fun{m_cache[i].second};
            util::static_foreach<0, eval_order::size>
            ::_(token, fun, *type_token, value);
        }
        return m_cache[i].second;
    }

    void init() {
        m_dummy.second = std::numeric_limits<std::uint64_t>::max();
        m_cache.resize(cache_size);
        for (auto& entry : m_cache) { entry.first = nullptr; }
        m_cache_begin = m_cache_end = 0;
    }

    template<typename AbstractTuple, typename NativeDataPtr>
    bool _do_invoke(AbstractTuple& vals, NativeDataPtr ndp) {
        const std::type_info* type_token = vals.type_token();
        auto bitfield = get_cache_entry(type_token, vals);
        eval_order token;
        detail::invoke_helper<decltype(m_cases)> fun{m_cases, bitfield};
        return util::static_foreach<0, eval_order::size>
               ::eval_or(token,
                         fun,
                         *type_token,
                         vals.impl_type(),
                         ndp,
                         vals);
    }

    template<typename AnyTuple>
    bool _invoke(AnyTuple& tup,
                 typename std::enable_if<
                        std::is_const<AnyTuple>::value == false
                     && has_manipulator == true
                 >::type* = 0) {
        tup.force_detach();
        auto& vals = *(tup.vals());
        return _do_invoke(vals, vals.mutable_native_data());
    }

    template<typename AnyTuple>
    bool _invoke(AnyTuple& tup,
                 typename std::enable_if<
                        std::is_const<AnyTuple>::value == false
                     && has_manipulator == false
                 >::type* = 0) {
        return _invoke(static_cast<const AnyTuple&>(tup));
    }

    template<typename AnyTuple>
    bool _invoke(AnyTuple& tup,
                 typename std::enable_if<
                        std::is_const<AnyTuple>::value == true
                     && has_manipulator == false
                 >::type* = 0) {
        const auto& cvals = *(tup.cvals());
        return _do_invoke(cvals, cvals.native_data());
    }

    template<typename AnyTuple>
    bool _invoke(AnyTuple& tup,
                 typename std::enable_if<
                        std::is_const<AnyTuple>::value == true
                     && has_manipulator == true
                 >::type* = 0) {
        any_tuple tup_copy{tup};
        return _invoke(tup_copy);
    }

};

template<class List>
struct match_expr_from_type_list;

template<typename... Args>
struct match_expr_from_type_list<util::type_list<Args...> > {
    typedef match_expr<Args...> type;
};

template<typename... Lhs, typename... Rhs>
inline match_expr<Lhs..., Rhs...> operator,(const match_expr<Lhs...>& lhs,
                                            const match_expr<Rhs...>& rhs) {
    return lhs.or_else(rhs);
}

template<typename Arg0, typename... Args>
typename match_expr_from_type_list<
    typename util::tl_concat<
        typename Arg0::cases_list,
        typename Args::cases_list...
    >::type
>::type
mexpr_concat(const Arg0& arg0, const Args&... args) {
    typename detail::tdata_from_type_list<
        typename util::tl_map<
            typename util::tl_concat<
                typename Arg0::cases_list,
                typename Args::cases_list...
            >::type,
            gref_wrapped
        >::type
    >::type
    all_cases;
    detail::collect_tdata(all_cases, arg0.cases(), args.cases()...);
    return {all_cases};
}

template<typename Arg0, typename... Args>
partial_function mexpr_concat_convert(const Arg0& arg0, const Args&... args) {
    typename detail::tdata_from_type_list<
        typename util::tl_map<
            typename util::tl_concat<
                typename Arg0::cases_list,
                typename Args::cases_list...
            >::type,
            gref_wrapped
        >::type
    >::type
    all_cases;
    typedef typename match_expr_from_type_list<
                typename util::tl_concat<
                    typename Arg0::cases_list,
                    typename Args::cases_list...
                >::type
            >::type
            combined_type;
    typedef typename combined_type::pfun_impl impl_type;
    detail::collect_tdata(all_cases, arg0.cases(), args.cases()...);
    return {partial_function::impl_ptr{new impl_type(all_cases)}};
}

} // namespace cppa

#endif // CPPA_MATCH_EXPR_HPP
