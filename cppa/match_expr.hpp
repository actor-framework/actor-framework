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
#include "cppa/guard_expr.hpp"
#include "cppa/tpartial_function.hpp"

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/int_list.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/rm_option.hpp"
#include "cppa/util/purge_refs.hpp"
#include "cppa/util/apply_args.hpp"
#include "cppa/util/left_or_right.hpp"
#include "cppa/util/deduce_ref_type.hpp"

#include "cppa/detail/matches.hpp"
#include "cppa/detail/projection.hpp"
#include "cppa/detail/value_guard.hpp"
#include "cppa/detail/pseudo_tuple.hpp"
#include "cppa/detail/behavior_impl.hpp"

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

        util::fixed_vector<size_t, util::tl_size<filtered_pattern>::value> mv;
        if (type_token == typeid(filtered_pattern) ||  mimpl::_(tup, mv)) {
            typedef typename pseudo_tuple_from_type_list<filtered_pattern>::type
                    ttup_type;
            ttup_type ttup;
            // if we strip const here ...
            for (size_t i = 0; i < util::tl_size<filtered_pattern>::value; ++i) {
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
            auto indices = util::get_indices(ttup_fwd);
            return util::apply_args_prefixed(target.first, ttup_fwd, indices, target.second);
        }
        return false;
    }
};

template<>
struct invoke_policy_impl<wildcard_position::nil,
                          util::empty_type_list,
                          util::empty_type_list  > {

    template<class Target, typename PtrType, typename Tuple>
    static bool invoke(Target& target,
                       const std::type_info&,
                       detail::tuple_impl_info,
                       PtrType*,
                       Tuple&) {
        return target.first(target.second);
    }

    template<class Tuple>
    static bool can_invoke(const std::type_info& arg_types, const Tuple&) {
        return arg_types == typeid(util::empty_type_list);
    }

};

template<class Pattern, typename... Ts>
struct invoke_policy_impl<wildcard_position::nil,
                          Pattern,
                          util::type_list<Ts...> > {

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
        auto indices = util::get_indices(tup);
        return util::apply_args_prefixed(target.first, tup, indices, target.second);
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
                auto indices = util::get_indices(*arg);
                return util::apply_args_prefixed(target.first, *arg, indices, target.second);
            }
            // 'fall through'
        }
        else if (timpl == detail::dynamically_typed) {
            auto& arr = arr_type::arr;
            if (tup.size() != util::tl_size<filtered_pattern>::value) {
                return false;
            }
            for (size_t i = 0; i < util::tl_size<filtered_pattern>::value; ++i) {
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
        auto indices = util::get_indices(ttup_fwd);
        return util::apply_args_prefixed(target.first, ttup_fwd, indices, target.second);
    }

    template<class Tuple>
    static bool can_invoke(const std::type_info& arg_types, const Tuple&) {
        return arg_types == typeid(filtered_pattern);
    }

};

template<>
struct invoke_policy_impl<wildcard_position::leading,
                          util::type_list<anything>,
                          util::empty_type_list> {
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
        return target.first(target.second);
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
        if (tup.size() < util::tl_size<filtered_pattern>::value) {
            return false;
        }
        for (size_t i = 0; i < util::tl_size<filtered_pattern>::value; ++i) {
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
        auto indices = util::get_indices(ttup_fwd);
        return util::apply_args_prefixed(target.first, ttup_fwd, indices, target.second);
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
        if (tup.size() < util::tl_size<filtered_pattern>::value) {
            return false;
        }
        size_t i = tup.size() - util::tl_size<filtered_pattern>::value;
        size_t j = 0;
        while (j < util::tl_size<filtered_pattern>::value) {
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
        size_t i = tup.size() - util::tl_size<filtered_pattern>::value;
        size_t j = 0;
        while (j < util::tl_size<filtered_pattern>::value) {
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
        auto indices = util::get_indices(ttup_fwd);
        return util::apply_args_prefixed(target.first, ttup_fwd, indices, target.second);
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
                util::tl_size<filtered_pattern>::value
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
                    util::tl_size<filtered_pattern>::value
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

// PPFP = projection_partial_function_pair
template<class PPFPs, typename... Args>
inline bool unroll_expr(PPFPs& fs, std::uint64_t bitmask, std::integral_constant<size_t,0>, const std::type_info& arg_types, Args&... as) {
    if ((bitmask & 0x01) == 0) return false;
    auto& f = get<0>(fs);
    typedef typename util::rm_ref<decltype(f)>::type Fun;
    typedef typename Fun::pattern_type pattern_type;
    typedef detail::invoke_policy<pattern_type> policy;
    return policy::invoke(f, arg_types, as...);
}

template<class PPFPs, size_t N, typename... Args>
inline bool unroll_expr(PPFPs& fs, std::uint64_t bitmask, std::integral_constant<size_t,N>, const std::type_info& arg_types, Args&... as) {
    if (unroll_expr(fs, bitmask, std::integral_constant<size_t,N-1>(), arg_types, as...)) {
        return true;
    }
    if ((bitmask & (0x01 << N)) == 0) return false;
    auto& f = get<N>(fs);
    typedef typename util::rm_ref<decltype(f)>::type Fun;
    typedef typename Fun::pattern_type pattern_type;
    typedef detail::invoke_policy<pattern_type> policy;
    return policy::invoke(f, arg_types, as...);
}

// PPFP = projection_partial_function_pair
template<class PPFPs, class Tuple>
inline bool can_unroll_expr(PPFPs& fs, std::integral_constant<size_t,0>, const std::type_info& arg_types, const Tuple& tup) {
    auto& f = get<0>(fs);
    typedef typename util::rm_ref<decltype(f)>::type Fun;
    typedef typename Fun::pattern_type pattern_type;
    typedef detail::invoke_policy<pattern_type> policy;
    return policy::can_invoke(arg_types, tup);
}

template<class PPFPs, size_t N, class Tuple>
inline bool can_unroll_expr(PPFPs& fs, std::integral_constant<size_t,N>, const std::type_info& arg_types, const Tuple& tup) {
    if (can_unroll_expr(fs, std::integral_constant<size_t,N-1>(), arg_types, tup)) {
        return true;
    }
    auto& f = get<N>(fs);
    typedef typename util::rm_ref<decltype(f)>::type Fun;
    typedef typename Fun::pattern_type pattern_type;
    typedef detail::invoke_policy<pattern_type> policy;
    return policy::can_invoke(arg_types, tup);
}

template<class PPFPs, class Tuple>
inline std::uint64_t calc_bitmask(PPFPs& fs, std::integral_constant<size_t,0>, const std::type_info& tinf, const Tuple& tup) {
    auto& f = get<0>(fs);
    typedef typename util::rm_ref<decltype(f)>::type Fun;
    typedef typename Fun::pattern_type pattern_type;
    typedef detail::invoke_policy<pattern_type> policy;
    return policy::can_invoke(tinf, tup) ? 0x01 : 0x00;
}

template<class PPFPs, size_t N, class Tuple>
inline std::uint64_t calc_bitmask(PPFPs& fs, std::integral_constant<size_t,N>, const std::type_info& tinf, const Tuple& tup) {
    auto& f = get<N>(fs);
    typedef typename util::rm_ref<decltype(f)>::type Fun;
    typedef typename Fun::pattern_type pattern_type;
    typedef detail::invoke_policy<pattern_type> policy;
    std::uint64_t result = policy::can_invoke(tinf, tup) ? (0x01 << N) : 0x00;
    return result | calc_bitmask(fs, std::integral_constant<size_t,N-1>(), tinf, tup);
}

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

// detach_if_needed(any_tuple tup, bool do_detach)
inline any_tuple& detach_if_needed(any_tuple& tup, std::true_type) {
    tup.force_detach();
    return tup;
}

inline any_tuple detach_if_needed(const any_tuple& tup, std::true_type) {
    any_tuple cpy{tup};
    cpy.force_detach();
    return std::move(cpy);
}

inline const any_tuple& detach_if_needed(const any_tuple& tup, std::false_type) {
    return tup;
}

inline void* fetch_data(cow_ptr<abstract_tuple>& vals, std::true_type) {
    return vals->mutable_native_data();
}

inline const void* fetch_data(const cow_ptr<abstract_tuple>& vals, std::false_type) {
    return vals->native_data();
}

} } // namespace cppa::detail

namespace cppa {

template<class... Cases>
class match_expr {

    static_assert(sizeof...(Cases) < 64, "too many functions");

 public:

    typedef util::type_list<Cases...> cases_list;

    static constexpr size_t num_cases = sizeof...(Cases);

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

    inline bool invoke(const any_tuple& tup) {
        return invoke_impl(tup);
    }

    inline bool invoke(any_tuple& tup) {
        return invoke_impl(tup);
    }

    inline bool invoke(any_tuple&& tup) {
        any_tuple tmp{tup};
        return invoke_impl(tmp);
    }

    bool can_invoke(const any_tuple& tup) {
        auto type_token = tup.type_token();
        if (tup.impl_type() == detail::statically_typed) {
            auto bitmask = get_cache_entry(type_token, tup);
            return bitmask != 0;
        }
        return can_unroll_expr(m_cases,
                               std::integral_constant<size_t,num_cases-1>(),
                               *type_token,
                               tup);
    }

    inline bool operator()(const any_tuple& tup) {
        return invoke_impl(tup);
    }

    inline bool operator()(any_tuple& tup) {
        return invoke_impl(tup);
    }

    inline bool operator()(any_tuple&& tup) {
        any_tuple tmp{tup};
        return invoke_impl(tmp);
    }

    template<typename... Args>
    bool operator()(Args&&... args) {
        typedef detail::tdata<
                    typename detail::mexpr_fwd<has_manipulator, Args>::type...>
                tuple_type;
        // applies implicit conversions etc
        tuple_type tup{std::forward<Args>(args)...};
        auto& type_token = typeid(typename tuple_type::types);
        auto bitmask = get_cache_entry(&type_token, tup);

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

        auto impl_type = detail::statically_typed;
        ptr_type ptr_arg = nullptr;

        return unroll_expr(m_cases,
                           bitmask,
                           std::integral_constant<size_t,num_cases-1>(),
                           type_token,
                           impl_type,
                           ptr_arg,
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

    struct pfun_impl : detail::behavior_impl {
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
        typedef typename detail::behavior_impl::pointer pointer;
        pointer copy(const generic_timeout_definition& tdef) const {
            return new_default_behavior_impl(pfun, tdef.timeout, tdef.handler);
        }
    };

    intrusive_ptr<detail::behavior_impl> as_behavior_impl() const {
        return new pfun_impl(*this);
    }

 private:

    // structure: tdata< tdata<type_list<...>, ...>,
    //                   tdata<type_list<...>, ...>,
    //                   ...>
    detail::tdata<Cases...> m_cases;

    static constexpr size_t cache_size = 10;

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
            m_cache[i].second = calc_bitmask(m_cases,
                                             std::integral_constant<size_t,num_cases-1>(),
                                             *type_token,
                                             value);
        }
        return m_cache[i].second;
    }

    void init() {
        m_dummy.second = std::numeric_limits<std::uint64_t>::max();
        m_cache.resize(cache_size);
        for (auto& entry : m_cache) { entry.first = nullptr; }
        m_cache_begin = m_cache_end = 0;
    }

    template<class Tuple>
    bool invoke_impl(Tuple& tup) {
        std::integral_constant<bool,has_manipulator> mutator_token;
        decltype(detail::detach_if_needed(tup, mutator_token)) tref = detail::detach_if_needed(tup, mutator_token);
        auto& vals = tref.vals();
        auto ndp = fetch_data(vals, mutator_token);
        auto token_ptr = vals->type_token();
        auto bitmask = get_cache_entry(token_ptr, *vals);
        auto impl_type = vals->impl_type();
        return unroll_expr(m_cases,
                           bitmask,
                           std::integral_constant<size_t,num_cases-1>(),
                           *token_ptr,
                           impl_type,
                           ndp,
                           *vals);
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
match_expr_collect(const Arg0& arg0, const Args&... args) {
    typedef typename match_expr_from_type_list<
                typename util::tl_concat<
                    typename Arg0::cases_list,
                    typename Args::cases_list...
                >::type
            >::type
            combined_type;
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

template<bool HasTimeout>
struct match_expr_concat_impl {
    template<typename Arg0, typename... Args>
    static detail::behavior_impl* _(const Arg0& arg0, const Args&... args) {
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
        auto lvoid = []() { };
        typedef detail::default_behavior_impl<combined_type, decltype(lvoid)>
                impl_type;
        detail::collect_tdata(all_cases, arg0.cases(), args.cases()...);
        return new impl_type(all_cases, util::duration{}, lvoid);
    }
};

template<>
struct match_expr_concat_impl<true> {

    template<class TData, class Token, typename F>
    static detail::behavior_impl* __(const TData& data, Token, const timeout_definition<F>& arg0) {
        typedef typename match_expr_from_type_list<Token>::type combined_type;
        typedef detail::default_behavior_impl<combined_type, F> impl_type;
        return new impl_type(data, arg0);
    }

    template<class TData, class Token, typename... Cases, typename... Args>
    static detail::behavior_impl* __(const TData& data, Token, const match_expr<Cases...>& arg0, const Args&... args) {
        typedef typename util::tl_concat<
                Token,
                util::type_list<Cases...>
            >::type
            next_token_type;
        typename detail::tdata_from_type_list<
                typename util::tl_map<
                    next_token_type,
                    gref_wrapped
                >::type
            >::type
            next_data;
        next_token_type next_token;
        detail::collect_tdata(next_data, data, arg0.cases());
        return __(next_data, next_token, args...);
    }

    template<typename F>
    static detail::behavior_impl* _(const timeout_definition<F>& arg0) {
        typedef detail::default_behavior_impl<detail::dummy_match_expr, F> impl_type;
        return new impl_type(detail::dummy_match_expr{}, arg0);
    }

    template<typename... Cases, typename... Args>
    static detail::behavior_impl* _(const match_expr<Cases...>& arg0, const Args&... args) {
        util::type_list<Cases...> token;
        typename detail::tdata_from_type_list<
                typename util::tl_map<
                    util::type_list<Cases...>,
                    gref_wrapped
                >::type
            >::type
            wrapper;
        detail::collect_tdata(wrapper, arg0.cases());
        return __(wrapper, token, args...);
    }

};

template<typename Arg0, typename... Args>
intrusive_ptr<detail::behavior_impl> match_expr_concat(const Arg0& arg0,
                                                       const Args&... args) {
    constexpr bool has_timeout = util::disjunction<
                                     is_timeout_definition<Arg0>,
                                     is_timeout_definition<Args>...>::value;
    return {match_expr_concat_impl<has_timeout>::_(arg0, args...)};
}

} // namespace cppa

#endif // CPPA_MATCH_EXPR_HPP
