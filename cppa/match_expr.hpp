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


#ifndef CPPA_MATCH_EXPR_HPP
#define CPPA_MATCH_EXPR_HPP

#include "cppa/optional.hpp"
#include "cppa/guard_expr.hpp"
#include "cppa/optional_variant.hpp"
#include "cppa/tpartial_function.hpp"

#include "cppa/util/call.hpp"
#include "cppa/util/int_list.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/purge_refs.hpp"
#include "cppa/util/type_traits.hpp"
#include "cppa/util/left_or_right.hpp"
#include "cppa/util/rebindable_reference.hpp"

#include "cppa/detail/matches.hpp"
#include "cppa/detail/projection.hpp"
#include "cppa/detail/value_guard.hpp"
#include "cppa/detail/tuple_dummy.hpp"
#include "cppa/detail/pseudo_tuple.hpp"
#include "cppa/detail/behavior_impl.hpp"

namespace cppa {
namespace detail {

template<long N>
struct long_constant { static constexpr long value = N; };

typedef long_constant<-1l> minus1l;

template<typename T1, typename T2>
inline T2& deduce_const(T1&, T2& rhs) { return rhs; }

template<typename T1, typename T2>
inline const T2& deduce_const(const T1&, T2& rhs) { return rhs; }

template<class FilteredPattern>
struct invoke_util_base {
    typedef FilteredPattern filtered_pattern;
    typedef typename pseudo_tuple_from_type_list<filtered_pattern>::type
            tuple_type;
};

// covers wildcard_position::multiple and wildcard_position::in_between
template<wildcard_position, class Pattern, class FilteredPattern>
struct invoke_util_impl : invoke_util_base<FilteredPattern> {

    typedef invoke_util_base<FilteredPattern> super;

    template<class Tuple>
    static bool can_invoke(const std::type_info& type_token,
                           const Tuple& tup) {
        typedef typename match_impl_from_type_list<Tuple, Pattern>::type mimpl;
        return type_token == typeid(FilteredPattern) ||  mimpl::_(tup);
    }

    template<typename PtrType, class Tuple>
    static bool prepare_invoke(typename super::tuple_type& result,
                               const std::type_info& type_token,
                               bool,
                               PtrType*,
                               Tuple& tup) {
        typedef typename match_impl_from_type_list<
                    typename std::remove_const<Tuple>::type,
                    Pattern
                >::type
                mimpl;
        util::limited_vector<size_t, util::tl_size<FilteredPattern>::value> mv;
        if (type_token == typeid(FilteredPattern)) {
            for (size_t i = 0; i < util::tl_size<FilteredPattern>::value; ++i) {
                result[i] = const_cast<void*>(tup.at(i));
            }
            return true;
        }
        else if (mimpl::_(tup, mv)) {
            for (size_t i = 0; i < util::tl_size<FilteredPattern>::value; ++i) {
                result[i] = const_cast<void*>(tup.at(mv[i]));
            }
            return true;
        }
        return false;
    }

};

template<>
struct invoke_util_impl<wildcard_position::nil,
                          util::empty_type_list,
                          util::empty_type_list  >
: invoke_util_base<util::empty_type_list> {

    typedef invoke_util_base<util::empty_type_list> super;

    template<typename PtrType, class Tuple>
    static bool prepare_invoke(typename super::tuple_type&,
                               const std::type_info& type_token,
                               bool,
                               PtrType*,
                               Tuple& tup) {
        return can_invoke(type_token, tup);
    }

    template<class Tuple>
    static bool can_invoke(const std::type_info& arg_types, const Tuple&) {
        return arg_types == typeid(util::empty_type_list);
    }

};

template<class Pattern, typename... Ts>
struct invoke_util_impl<wildcard_position::nil,
                          Pattern,
                          util::type_list<Ts...>>
: invoke_util_base<util::type_list<Ts...>> {

    typedef invoke_util_base<util::type_list<Ts...>> super;

    typedef typename super::tuple_type tuple_type;

    typedef detail::tdata<Ts...> native_data_type;

    typedef typename detail::static_types_array<Ts...> arr_type;

    template<class Tup>
    static bool prepare_invoke(std::false_type, tuple_type&, Tup&) {
        return false;
    }

    template<class Tup>
    static bool prepare_invoke(std::true_type, tuple_type& result, Tup& tup) {
        for (size_t i = 0; i < sizeof...(Ts); ++i) {
            result[i] = const_cast<void*>(tup.at(i));
        }
        return true;
    }

    template<typename PtrType, class Tuple>
    static bool prepare_invoke(tuple_type& result,
                               const std::type_info&,
                               bool,
                               PtrType*,
                               Tuple& tup,
                               typename std::enable_if<
                                   std::is_same<
                                       typename std::remove_const<Tuple>::type,
                                       detail::abstract_tuple
                                   >::value == false
                               >::type* = 0) {
        std::integral_constant<bool,
                               util::tl_binary_forall<
                                   typename util::tl_map<
                                       typename Tuple::types,
                                       util::purge_refs
                                   >::type,
                                   util::type_list<Ts...>,
                                   std::is_same
                               >::value                       > token;
        return prepare_invoke(token, result, tup);
    }

    template<typename PtrType, class Tuple>
    static bool prepare_invoke(typename super::tuple_type& result,
                               const std::type_info& arg_types,
                               bool dynamically_typed,
                               PtrType* native_arg,
                               Tuple& tup,
                               typename std::enable_if<
                                   std::is_same<
                                       typename std::remove_const<Tuple>::type,
                                       detail::abstract_tuple
                                   >::value == true
                               >::type* = 0) {
        if (arg_types == typeid(util::type_list<Ts...>)) {
            if (native_arg) {
                typedef typename std::conditional<
                            std::is_const<PtrType>::value,
                            const native_data_type*,
                            native_data_type*
                        >::type
                        cast_type;
                auto arg = reinterpret_cast<cast_type>(native_arg);
                for (size_t i = 0; i < sizeof...(Ts); ++i) {
                    result[i] = const_cast<void*>(arg->at(i));
                }
                return true;
            }
            // 'fall through'
        }
        else if (dynamically_typed) {
            auto& arr = arr_type::arr;
            if (tup.size() != sizeof...(Ts)) {
                return false;
            }
            for (size_t i = 0; i < sizeof...(Ts); ++i) {
                if (arr[i] != tup.type_at(i)) {
                    return false;
                }
            }
            // 'fall through'
        }
        else return false;
        for (size_t i = 0; i < sizeof...(Ts); ++i) {
            result[i] = const_cast<void*>(tup.at(i));
        }
        return true;
    }

    template<class Tuple>
    static bool can_invoke(const std::type_info& arg_types, const Tuple&) {
        return arg_types == typeid(util::type_list<Ts...>);
    }

};

template<>
struct invoke_util_impl<wildcard_position::leading,
                          util::type_list<anything>,
                          util::empty_type_list>
: invoke_util_base<util::empty_type_list> {

    typedef invoke_util_base<util::empty_type_list> super;

    template<class Tuple>
    static inline bool can_invoke(const std::type_info&, const Tuple&) {
        return true;
    }

    template<typename PtrType, typename Tuple>
    static inline bool prepare_invoke(typename super::tuple_type&,
                                      const std::type_info&,
                                      bool,
                                      PtrType*,
                                      Tuple&) {
        return true;
    }

};

template<class Pattern, typename... Ts>
struct invoke_util_impl<wildcard_position::trailing,
                          Pattern,
                          util::type_list<Ts...>>
: invoke_util_base<util::type_list<Ts...>> {

    typedef invoke_util_base<util::type_list<Ts...>> super;

    template<class Tuple>
    static bool can_invoke(const std::type_info& arg_types,
                           const Tuple& tup) {
        if (arg_types == typeid(util::type_list<Ts...>)) {
            return true;
        }
        typedef detail::static_types_array<Ts...> arr_type;
        auto& arr = arr_type::arr;
        if (tup.size() < sizeof...(Ts)) {
            return false;
        }
        for (size_t i = 0; i < sizeof...(Ts); ++i) {
            if (arr[i] != tup.type_at(i)) {
                return false;
            }
        }
        return true;
    }

    template<typename PtrType, class Tuple>
    static bool prepare_invoke(typename super::tuple_type& result,
                               const std::type_info& arg_types,
                               bool,
                               PtrType*,
                               Tuple& tup) {
        if (!can_invoke(arg_types, tup)) return false;
        for (size_t i = 0; i < sizeof...(Ts); ++i) {
            result[i] = const_cast<void*>(tup.at(i));
        }
        return true;
    }

};

template<class Pattern, typename... Ts>
struct invoke_util_impl<wildcard_position::leading,
                          Pattern,
                          util::type_list<Ts...>>
: invoke_util_base<util::type_list<Ts...>> {

    typedef invoke_util_base<util::type_list<Ts...>> super;

    template<class Tuple>
    static bool can_invoke(const std::type_info& arg_types,
                           const Tuple& tup) {
        if (arg_types == typeid(util::type_list<Ts...>)) {
            return true;
        }
        typedef detail::static_types_array<Ts...> arr_type;
        auto& arr = arr_type::arr;
        if (tup.size() < sizeof...(Ts)) return false;
        size_t i = tup.size() - sizeof...(Ts);
        size_t j = 0;
        while (j < sizeof...(Ts)) {
            if (arr[j++] != tup.type_at(i++)) {
                return false;
            }
        }
        return true;
    }

    template<typename PtrType, class Tuple>
    static bool prepare_invoke(typename super::tuple_type& result,
                               const std::type_info& arg_types,
                               bool,
                               PtrType*,
                               Tuple& tup) {
        if (!can_invoke(arg_types, tup)) return false;
        size_t i = tup.size() - sizeof...(Ts);
        size_t j = 0;
        while (j < sizeof...(Ts)) {
            result[j++] = const_cast<void*>(tup.at(i++));
        }
        return true;
    }

};

template<class Pattern>
struct invoke_util
        : invoke_util_impl<
            get_wildcard_position<Pattern>(),
            Pattern,
            typename util::tl_filter_not_type<Pattern, anything>::type> {
};

template<class Pattern, class Projection, class PartialFun>
struct projection_partial_function_pair : std::pair<Projection, PartialFun> {
    template<typename... Ts>
    projection_partial_function_pair(Ts&&... args)
    : std::pair<Projection, PartialFun>(std::forward<Ts>(args)...) { }
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
                    util::map_to_result_type,
                    util::rm_optional,
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

template<bool Complete, class Expr, class Guard, class Trans, class Pattern>
struct get_case {
    typedef typename get_case_<Expr, Guard, Trans, Pattern>::type type;
};

template<class Expr, class Guard, class Trans, class Pattern>
struct get_case<false, Expr, Guard, Trans, Pattern> {
    typedef typename util::tl_pop_back<Pattern>::type lhs_pattern;
    typedef typename util::tl_map<
                typename util::get_callable_trait<Expr>::arg_types,
                util::rm_const_and_ref
            >::type
            rhs_pattern;
    typedef typename get_case_<
                Expr,
                Guard,
                Trans,
                typename util::tl_concat<lhs_pattern, rhs_pattern>::type
            >::type
            type;
};

template<typename Fun>
struct has_bool_result {
    typedef typename Fun::result_type result_type;
    static constexpr bool value = std::is_same<bool, result_type>::value;
    typedef std::integral_constant<bool, value> token_type;
};

template<typename Result, class PPFPs, typename PtrType, class Tuple>
Result unroll_expr(PPFPs&, std::uint64_t, minus1l, const std::type_info&,
                   bool, PtrType*, Tuple&) {
    return none;
}

template<typename Result, class PPFPs, long N, typename PtrType, class Tuple>
Result unroll_expr(PPFPs& fs,
                   std::uint64_t bitmask,
                   long_constant<N>,
                   const std::type_info& type_token,
                   bool is_dynamic,
                   PtrType* ptr,
                   Tuple& tup) {
    /* recursively evaluate sub expressions */ {
        Result res = unroll_expr<Result>(fs, bitmask, long_constant<N-1>{},
                                         type_token, is_dynamic, ptr, tup);
        if (res) return res;
    }
    if ((bitmask & (0x01 << N)) == 0) return none;
    auto& f = get<N>(fs);
    typedef typename util::rm_const_and_ref<decltype(f)>::type Fun;
    typedef typename Fun::pattern_type pattern_type;
    typedef detail::invoke_util<pattern_type> policy;
    typename policy::tuple_type targs;
    if (policy::prepare_invoke(targs, type_token, is_dynamic, ptr, tup)) {
        auto is = util::get_indices(targs);
        return util::apply_args_prefixed(f.first,
                                         deduce_const(tup, targs),
                                         is,
                                         f.second);
    }
    return none;
}

// PPFP = projection_partial_function_pair
template<class PPFPs, class T>
inline bool can_unroll_expr(PPFPs&, minus1l, const std::type_info&, const T&) {
    return false;
}

template<class PPFPs, long N, class Tuple>
inline bool can_unroll_expr(PPFPs& fs,
                            long_constant<N>,
                            const std::type_info& arg_types,
                            const Tuple& tup) {
    if (can_unroll_expr(fs, long_constant<N-1l>(), arg_types, tup)) {
        return true;
    }
    auto& f = get<N>(fs);
    typedef typename util::rm_const_and_ref<decltype(f)>::type Fun;
    typedef typename Fun::pattern_type pattern_type;
    typedef detail::invoke_util<pattern_type> policy;
    return policy::can_invoke(arg_types, tup);
}

template<class PPFPs, class Tuple>
inline std::uint64_t calc_bitmask(PPFPs&,
                                  minus1l,
                                  const std::type_info&,
                                  const Tuple&) {
    return 0x00;
}

template<class PPFPs, long N, class Tuple>
inline std::uint64_t calc_bitmask(PPFPs& fs,
                                  long_constant<N>,
                                  const std::type_info& tinf,
                                  const Tuple& tup) {
    auto& f = get<N>(fs);
    typedef typename util::rm_const_and_ref<decltype(f)>::type Fun;
    typedef typename Fun::pattern_type pattern_type;
    typedef detail::invoke_util<pattern_type> policy;
    std::uint64_t result = policy::can_invoke(tinf, tup) ? (0x01 << N) : 0x00;
    return result | calc_bitmask(fs, long_constant<N-1l>(), tinf, tup);
}

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
                    typename util::rm_const_and_ref<T>::type
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
    return cpy;
}

inline const any_tuple& detach_if_needed(const any_tuple& tup, std::false_type) {
    return tup;
}

template<typename Ptr>
inline void* fetch_native_data(Ptr& ptr, std::true_type) {
    return ptr->mutable_native_data();
}

template<typename Ptr>
inline const void* fetch_native_data(const Ptr& ptr, std::false_type) {
    return ptr->native_data();
}

template<typename T>
struct is_manipulator_case {
    static constexpr bool value = T::second_type::manipulates_args;
};

template<typename T>
struct get_case_result {
    typedef typename T::second_type::result_type type;
};

} // namespace detail
} // namespace cppa

namespace cppa {

/**
 * @brief A match expression encapsulating cases <tt>Cs...</tt>, whereas
 *        each case is a @p detail::projection_partial_function_pair.
 */
template<class... Cs>
class match_expr {

    static_assert(sizeof...(Cs) < 64, "too many functions");

 public:

    typedef util::type_list<Cs...> cases_list;

    typedef typename optional_variant_from_type_list<
                typename util::tl_distinct<
                    typename util::tl_map<
                        cases_list,
                        detail::get_case_result
                    >::type
                >::type
            >::type
            result_type;

    static constexpr bool has_manipulator = util::tl_exists<
                                                cases_list,
                                                detail::is_manipulator_case
                                            >::value;

    typedef detail::long_constant<sizeof...(Cs)-1l> idx_token_type;

    static constexpr idx_token_type idx_token = idx_token_type{};

    template<typename T, typename... Ts>
    match_expr(T arg, Ts&&... args)
            : m_cases(std::move(arg), std::forward<Ts>(args)...) {
        init();
    }

    match_expr(match_expr&& other) : m_cases(std::move(other.m_cases)) {
        init();
    }

    match_expr(const match_expr& other) : m_cases(other.m_cases) {
        init();
    }

    bool can_invoke(const any_tuple& tup) {
        auto type_token = tup.type_token();
        if (not tup.dynamically_typed()) {
            auto bitmask = get_cache_entry(type_token, tup);
            return bitmask != 0;
        }
        return can_unroll_expr(m_cases,
                               idx_token,
                               *type_token,
                               tup);
    }

    inline result_type operator()(const any_tuple& tup) {
        return apply(tup);
    }

    inline result_type operator()(any_tuple& tup) {
        return apply(tup);
    }

    inline result_type operator()(any_tuple&& tup) {
        any_tuple tmp{tup};
        return apply(tmp);
    }

    template<typename T, typename... Ts>
    typename std::enable_if<
           not std::is_same<
               typename util::rm_const_and_ref<T>::type,
               any_tuple
           >::value
        && not is_cow_tuple<T>::value,
        result_type
    >::type
    operator()(T&& arg0, Ts&&... args) {
        // wraps and applies implicit conversions to args
        typedef detail::tdata<
                    typename detail::mexpr_fwd<
                        has_manipulator,
                        T
                    >::type,
                    typename detail::mexpr_fwd<
                        has_manipulator,
                        Ts
                    >::type...
                >
                tuple_type;
        tuple_type tup{std::forward<T>(arg0), std::forward<Ts>(args)...};
        auto& type_token = typeid(typename tuple_type::types);
        auto bitmask = get_cache_entry(&type_token, tup);
        // ref_type keeps track of whether this match_expr is a mutator
        typedef typename std::conditional<
                    has_manipulator,
                    tuple_type&,
                    const tuple_type&
                >::type
                ref_type;
        // same here
        typedef typename std::conditional<
                    has_manipulator,
                    void*,
                    const void*
                >::type
                ptr_type;
        // iterate over cases and return if any case was invoked
        return detail::unroll_expr<result_type>(m_cases,
                                                bitmask,
                                                idx_token,
                                                type_token,
                                                false, // not dynamically_typed
                                                static_cast<ptr_type>(nullptr),
                                                static_cast<ref_type>(tup));
    }

    template<class... Ds>
    match_expr<Cs..., Ds...> or_else(const match_expr<Ds...>& other) const {
        detail::tdata<util::rebindable_reference<const Cs>...,
                      util::rebindable_reference<const Ds>...    > all_cases;
        rebind_tdata(all_cases, m_cases, other.cases());
        return {all_cases};
    }

    /** @cond PRIVATE */

    inline const detail::tdata<Cs...>& cases() const {
        return m_cases;
    }

    intrusive_ptr<detail::behavior_impl> as_behavior_impl() const {
        //return new pfun_impl(*this);
        auto lvoid = [] { };
        using impl = detail::default_behavior_impl<match_expr, decltype(lvoid)>;
        return new impl(*this, util::duration{}, lvoid);
    }

    /** @endcond */

 private:

    // structure: tdata< tdata<type_list<...>, ...>,
    //                   tdata<type_list<...>, ...>,
    //                   ...>
    detail::tdata<Cs...> m_cases;

    static constexpr size_t cache_size = 10;

    typedef std::pair<const std::type_info*, std::uint64_t> cache_element;

    util::limited_vector<cache_element, cache_size> m_cache;

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
        if (value.dynamically_typed()) {
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
                                             idx_token,
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
    result_type apply(Tuple& tup) {
        if (tup.empty()) {
            detail::tuple_dummy td;
            auto td_token_ptr = td.type_token();
            auto td_bitmask = get_cache_entry(td_token_ptr, td);
            return detail::unroll_expr<result_type>(m_cases,
                                                    td_bitmask,
                                                    idx_token,
                                                    *td_token_ptr,
                                                    false,
                                                    static_cast<void*>(nullptr),
                                                    td);
        }
        std::integral_constant<bool, has_manipulator> mutator_token;
        // returns either a reference or a new object
        typedef decltype(detail::detach_if_needed(tup, mutator_token)) detached;
        detached tref = detail::detach_if_needed(tup, mutator_token);
        auto& vals = tref.vals();
        auto ndp = fetch_native_data(vals, mutator_token);
        auto token_ptr = vals->type_token();
        auto bitmask = get_cache_entry(token_ptr, *vals);
        auto dynamically_typed = vals->dynamically_typed();
        return detail::unroll_expr<result_type>(m_cases,
                                                bitmask,
                                                idx_token,
                                                *token_ptr,
                                                dynamically_typed,
                                                ndp,
                                                *vals);
    }

};

template<typename T>
struct is_match_expr {
    static constexpr bool value = false;
};

template<typename... Cs>
struct is_match_expr<match_expr<Cs...>> {
    static constexpr bool value = true;
};

template<class List>
struct match_expr_from_type_list;

template<typename... Ts>
struct match_expr_from_type_list<util::type_list<Ts...> > {
    typedef match_expr<Ts...> type;
};

template<typename... Lhs, typename... Rhs>
inline match_expr<Lhs..., Rhs...> operator,(const match_expr<Lhs...>& lhs,
                                            const match_expr<Rhs...>& rhs) {
    return lhs.or_else(rhs);
}

template<typename... Cs>
match_expr<Cs...>& match_expr_collect(match_expr<Cs...>& arg) {
    return arg;
}

template<typename... Cs>
match_expr<Cs...>&& match_expr_collect(match_expr<Cs...>&& arg) {
    return std::move(arg);
}

template<typename... Cs>
const match_expr<Cs...>& match_expr_collect(const match_expr<Cs...>& arg) {
    return arg;
}

template<typename T, typename... Ts>
typename match_expr_from_type_list<
                typename util::tl_concat<
                    typename T::cases_list,
                    typename Ts::cases_list...
                >::type
            >::type
match_expr_collect(const T& arg, const Ts&... args) {
    typename detail::tdata_from_type_list<
        typename util::tl_map<
            typename util::tl_concat<
                typename T::cases_list,
                typename Ts::cases_list...
            >::type,
            gref_wrapped
        >::type
    >::type
    all_cases;
    detail::rebind_tdata(all_cases, arg.cases(), args.cases()...);
    return {all_cases};
}

namespace detail {

//typedef std::true_type  with_timeout;
//typedef std::false_type without_timeout;

// end of recursion
template<class Data, class Token>
behavior_impl_ptr concat_rec(const Data& data, Token) {
    typedef typename match_expr_from_type_list<Token>::type combined_type;
    auto lvoid = [] { };
    typedef default_behavior_impl<combined_type, decltype(lvoid)> impl_type;
    return new impl_type(data, util::duration{}, lvoid);
}

// end of recursion with nothing but a partial function
inline behavior_impl_ptr concat_rec(const tdata<>&,
                                    util::empty_type_list,
                                    const partial_function& pfun) {
    return extract(pfun);
}

// end of recursion with timeout
template<class Data, class Token, typename F>
behavior_impl_ptr concat_rec(const Data& data,
                             Token,
                             const timeout_definition<F>& arg) {
    typedef typename match_expr_from_type_list<Token>::type combined_type;
    return new default_behavior_impl<combined_type, F>{data, arg};
}

// recursive concatenation function
template<class Data, class Token, typename T, typename... Ts>
behavior_impl_ptr concat_rec(const Data& data,
                             Token,
                             const T& arg,
                             const Ts&... args) {
    typedef typename util::tl_concat<
            Token,
            typename T::cases_list
        >::type
        next_token_type;
    typename tdata_from_type_list<
            typename util::tl_map<
                next_token_type,
                gref_wrapped
            >::type
        >::type
        next_data;
    next_token_type next_token;
    rebind_tdata(next_data, data, arg.cases());
    return concat_rec(next_data, next_token, args...);
}

// handle partial functions at end of recursion
template<class Data, class Token>
behavior_impl_ptr concat_rec(const Data& data,
                             Token token,
                             const partial_function& pfun) {
    return combine(concat_rec(data, token), pfun);
}

// handle partial functions in between
template<class Data, class Token, typename T, typename... Ts>
behavior_impl_ptr concat_rec(const Data& data,
                             Token token,
                             const partial_function& pfun,
                             const T& arg,
                             const Ts&... args) {
    auto lhs = concat_rec(data, token);
    detail::tdata<> dummy;
    auto rhs = concat_rec(dummy, util::empty_type_list{}, arg, args...);
    return combine(lhs, pfun)->or_else(rhs);
}

// handle partial functions at recursion start
template<typename T, typename... Ts>
behavior_impl_ptr concat_rec(const tdata<>& data,
                             util::empty_type_list token,
                             const partial_function& pfun,
                             const T& arg,
                             const Ts&... args) {
    return combine(pfun, concat_rec(data, token, arg, args...));
}

template<typename T0, typename T1, typename... Ts>
behavior_impl_ptr match_expr_concat(const T0& arg0,
                                    const T1& arg1,
                                    const Ts&... args) {
    detail::tdata<> dummy;
    return concat_rec(dummy, util::empty_type_list{}, arg0, arg1, args...);
}

template<typename T>
behavior_impl_ptr match_expr_concat(const T& arg) {
    return arg.as_behavior_impl();
}

// some more convenience functions

template<typename F,
         class E = typename std::enable_if<util::is_callable<F>::value>::type>
match_expr<
    typename get_case<
        false,
        F,
        empty_value_guard,
        util::empty_type_list,
        util::empty_type_list
    >::type>
lift_to_match_expr(F fun) {
    typedef typename get_case<
                false,
                F,
                empty_value_guard,
                util::empty_type_list,
                util::empty_type_list
            >::type
            result_type;
    return result_type{typename result_type::first_type{},
                       typename result_type::second_type{
                           std::move(fun),
                           empty_value_guard{}}};
}

template<typename T,
         class E = typename std::enable_if<!util::is_callable<T>::value>::type>
inline T lift_to_match_expr(T arg) {
    return arg;
}

} // namespace detail
} // namespace cppa

#endif // CPPA_MATCH_EXPR_HPP
