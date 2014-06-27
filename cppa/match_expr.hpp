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

#ifndef CPPA_MATCH_EXPR_HPP
#define CPPA_MATCH_EXPR_HPP

#include <vector>

#include "cppa/none.hpp"
#include "cppa/variant.hpp"

#include "cppa/unit.hpp"

#include "cppa/util/int_list.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/purge_refs.hpp"
#include "cppa/util/type_traits.hpp"
#include "cppa/util/left_or_right.hpp"

#include "cppa/detail/matches.hpp"
#include "cppa/detail/apply_args.hpp"
#include "cppa/detail/lifted_fun.hpp"
#include "cppa/detail/tuple_dummy.hpp"
#include "cppa/detail/pseudo_tuple.hpp"
#include "cppa/detail/behavior_impl.hpp"

namespace cppa {
namespace detail {

template<long N>
struct long_constant {
    static constexpr long value = N;
};

typedef long_constant<-1l> minus1l;

template<typename T1, typename T2>
inline T2& deduce_const(T1&, T2& rhs) {
    return rhs;
}

template<typename T1, typename T2>
inline const T2& deduce_const(const T1&, T2& rhs) {
    return rhs;
}

template<class FilteredPattern>
struct invoke_util_base {
    typedef typename util::tl_apply<FilteredPattern, pseudo_tuple>::type
    tuple_type;
};

// covers wildcard_position::multiple and wildcard_position::in_between
template<wildcard_position, class Pattern, class FilteredPattern>
struct invoke_util_impl : invoke_util_base<FilteredPattern> {

    typedef invoke_util_base<FilteredPattern> super;

    template<class Tuple>
    static bool can_invoke(const std::type_info& type_token, const Tuple& tup) {
        typename select_matcher<Tuple, Pattern>::type mimpl;
        return type_token == typeid(FilteredPattern) || mimpl(tup);
    }

    template<typename PtrType, class Tuple>
    static bool prepare_invoke(typename super::tuple_type& result,
                               const std::type_info& type_token, bool, PtrType*,
                               Tuple& tup) {
        typename select_matcher<typename std::remove_const<Tuple>::type,
                                Pattern>::type mimpl;
        std::vector<size_t> mv;
        if (type_token == typeid(FilteredPattern)) {
            for (size_t i = 0; i < util::tl_size<FilteredPattern>::value;
                 ++i) {
                result[i] = const_cast<void*>(tup.at(i));
            }
            return true;
        } else if (mimpl(tup, mv)) {
            for (size_t i = 0; i < util::tl_size<FilteredPattern>::value;
                 ++i) {
                result[i] = const_cast<void*>(tup.at(mv[i]));
            }
            return true;
        }
        return false;
    }
};

template<>
struct invoke_util_impl<
    wildcard_position::nil, util::empty_type_list,
    util::empty_type_list> : invoke_util_base<util::empty_type_list> {

    typedef invoke_util_base<util::empty_type_list> super;

    template<typename PtrType, class Tuple>
    static bool prepare_invoke(typename super::tuple_type&,
                               const std::type_info& type_token, bool, PtrType*,
                               Tuple& tup) {
        return can_invoke(type_token, tup);
    }

    template<class Tuple>
    static bool can_invoke(const std::type_info& arg_types, const Tuple&) {
        return arg_types == typeid(util::empty_type_list);
    }
};

template<class Pattern, typename... Ts>
struct invoke_util_impl<
    wildcard_position::nil, Pattern,
    util::type_list<Ts...>> : invoke_util_base<util::type_list<Ts...>> {

    typedef invoke_util_base<util::type_list<Ts...>> super;

    typedef typename super::tuple_type tuple_type;

    typedef std::tuple<Ts...> native_data_type;

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
    static bool prepare_invoke(
        tuple_type& result, const std::type_info&, bool, PtrType*, Tuple& tup,
        typename std::enable_if<
            std::is_same<typename std::remove_const<Tuple>::type,
                         detail::message_data>::value == false>::type* = 0) {
        std::integral_constant<
            bool, util::tl_binary_forall<
                      typename util::tl_map<typename Tuple::types,
                                              util::purge_refs>::type,
                      util::type_list<Ts...>, std::is_same>::value> token;
        return prepare_invoke(token, result, tup);
    }

    template<typename PtrType, class Tuple>
    static bool prepare_invoke(
        typename super::tuple_type& result, const std::type_info& arg_types,
        bool dynamically_typed, PtrType* native_arg, Tuple& tup,
        typename std::enable_if<
            std::is_same<typename std::remove_const<Tuple>::type,
                         detail::message_data>::value == true>::type* = 0) {
        if (arg_types == typeid(util::type_list<Ts...>)) {
            if (native_arg) {
                typedef typename std::conditional<
                    std::is_const<PtrType>::value, const native_data_type*,
                    native_data_type*>::type cast_type;
                auto arg = reinterpret_cast<cast_type>(native_arg);
                for (size_t i = 0; i < sizeof...(Ts); ++i) {
                    result[i] = const_cast<void*>(
                        tup_ptr_access<0, sizeof...(Ts)>::get(i, *arg));
                }
                return true;
            }
            // 'fall through'
        } else if (dynamically_typed) {
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
        } else
            return false;
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
struct invoke_util_impl<
    wildcard_position::leading, util::type_list<anything>,
    util::empty_type_list> : invoke_util_base<util::empty_type_list> {

    typedef invoke_util_base<util::empty_type_list> super;

    template<class Tuple>
    static inline bool can_invoke(const std::type_info&, const Tuple&) {
        return true;
    }

    template<typename PtrType, typename Tuple>
    static inline bool prepare_invoke(typename super::tuple_type&,
                                      const std::type_info&, bool, PtrType*,
                                      Tuple&) {
        return true;
    }
};

template<class Pattern, typename... Ts>
struct invoke_util_impl<
    wildcard_position::trailing, Pattern,
    util::type_list<Ts...>> : invoke_util_base<util::type_list<Ts...>> {

    typedef invoke_util_base<util::type_list<Ts...>> super;

    template<class Tuple>
    static bool can_invoke(const std::type_info& arg_types, const Tuple& tup) {
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
                               const std::type_info& arg_types, bool, PtrType*,
                               Tuple& tup) {
        if (!can_invoke(arg_types, tup)) return false;
        for (size_t i = 0; i < sizeof...(Ts); ++i) {
            result[i] = const_cast<void*>(tup.at(i));
        }
        return true;
    }
};

template<class Pattern, typename... Ts>
struct invoke_util_impl<
    wildcard_position::leading, Pattern,
    util::type_list<Ts...>> : invoke_util_base<util::type_list<Ts...>> {

    typedef invoke_util_base<util::type_list<Ts...>> super;

    template<class Tuple>
    static bool can_invoke(const std::type_info& arg_types, const Tuple& tup) {
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
                               const std::type_info& arg_types, bool, PtrType*,
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
          get_wildcard_position<Pattern>(), Pattern,
          typename util::tl_filter_not_type<Pattern, anything>::type> {};

template<class Expr, class Projections, class Signature, class Pattern>
class match_expr_case
    : public get_lifted_fun<Expr, Projections, Signature>::type {

    typedef typename get_lifted_fun<Expr, Projections, Signature>::type super;

 public:
    template<typename... Ts>
    match_expr_case(Ts&&... args)
            : super(std::forward<Ts>(args)...) {}

    typedef Pattern pattern_type;
};

template<class Expr, class Transformers, class Pattern>
struct get_case_ {
    typedef typename util::get_callable_trait<Expr>::type ctrait;

    typedef typename util::tl_filter_not_type<Pattern, anything>::type
    filtered_pattern;

    typedef typename util::tl_pad_right<
        Transformers, util::tl_size<filtered_pattern>::value>::type
    padded_transformers;

    typedef typename util::tl_map<filtered_pattern, std::add_const,
                                    std::add_lvalue_reference>::type
    base_signature;

    typedef typename util::tl_map_conditional<
        typename util::tl_pad_left<
            typename ctrait::arg_types,
            util::tl_size<filtered_pattern>::value>::type,
        std::is_lvalue_reference, false, std::add_const,
        std::add_lvalue_reference>::type padded_expr_args;

    // override base signature with required argument types of Expr
    // and result types of transformation
    typedef typename util::tl_zip<
        typename util::tl_map<padded_transformers, util::map_to_result_type,
                                util::rm_optional,
                                std::add_lvalue_reference>::type,
        typename util::tl_zip<padded_expr_args, base_signature,
                                util::left_or_right>::type,
        util::left_or_right>::type partial_fun_signature;

    // 'inherit' mutable references from partial_fun_signature
    // for arguments without transformation
    typedef typename util::tl_zip<
        typename util::tl_zip<padded_transformers, partial_fun_signature,
                                util::if_not_left>::type,
        base_signature, util::deduce_ref_type>::type projection_signature;

    typedef match_expr_case<Expr, padded_transformers, projection_signature,
                            Pattern> type;
};

template<bool Complete, class Expr, class Trans, class Pattern>
struct get_case {
    typedef typename get_case_<Expr, Trans, Pattern>::type type;
};

template<class Expr, class Trans, class Pattern>
struct get_case<false, Expr, Trans, Pattern> {
    typedef typename util::tl_pop_back<Pattern>::type lhs_pattern;
    typedef typename util::tl_map<
        typename util::get_callable_trait<Expr>::arg_types,
        util::rm_const_and_ref>::type rhs_pattern;
    typedef typename get_case_<
        Expr, Trans,
        typename util::tl_concat<lhs_pattern, rhs_pattern>::type>::type type;
};

template<typename Fun>
struct has_bool_result {
    typedef typename Fun::result_type result_type;
    static constexpr bool value = std::is_same<bool, result_type>::value;
    typedef std::integral_constant<bool, value> token_type;
};

template<typename T>
inline bool unroll_expr_result_valid(const T&) {
    return true;
}

template<typename T>
inline bool unroll_expr_result_valid(const optional<T>& opt) {
    return static_cast<bool>(opt);
}

inline variant<none_t, unit_t> unroll_expr_result_unbox(bool& value) {
    if (value) return unit;
    return none;
}

template<typename T>
inline T& unroll_expr_result_unbox(optional<T>& opt) {
    return *opt;
}

template<typename Result, class PPFPs, typename PtrType, class Tuple>
Result unroll_expr(PPFPs&, uint64_t, minus1l, const std::type_info&, bool,
                   PtrType*, Tuple&) {
    return none;
}

template<typename Result, class PPFPs, long N, typename PtrType, class Tuple>
Result unroll_expr(PPFPs& fs, uint64_t bitmask, long_constant<N>,
                   const std::type_info& type_token, bool is_dynamic,
                   PtrType* ptr, Tuple& tup) {
    /* recursively evaluate sub expressions */ {
        Result res = unroll_expr<Result>(fs, bitmask, long_constant<N - 1>{},
                                         type_token, is_dynamic, ptr, tup);
        if (!get<none_t>(&res)) return res;
    }
    if ((bitmask & (0x01 << N)) == 0) return none;
    auto& f = get<N>(fs);
    typedef typename util::rm_const_and_ref<decltype(f)>::type Fun;
    typedef typename Fun::pattern_type pattern_type;
    typedef detail::invoke_util<pattern_type> policy;
    typename policy::tuple_type targs;
    if (policy::prepare_invoke(targs, type_token, is_dynamic, ptr, tup)) {
        auto is = util::get_indices(targs);
        auto res = detail::apply_args(f, is, deduce_const(tup, targs));
        if (unroll_expr_result_valid(res)) {
            return std::move(unroll_expr_result_unbox(res));
        }
    }
    return none;
}

template<class PPFPs, class Tuple>
inline uint64_t calc_bitmask(PPFPs&, minus1l, const std::type_info&,
                             const Tuple&) {
    return 0x00;
}

template<class Case, long N, class Tuple>
inline uint64_t calc_bitmask(Case& fs, long_constant<N>,
                             const std::type_info& tinf, const Tuple& tup) {
    auto& f = get<N>(fs);
    typedef typename util::rm_const_and_ref<decltype(f)>::type Fun;
    typedef typename Fun::pattern_type pattern_type;
    typedef detail::invoke_util<pattern_type> policy;
    uint64_t result = policy::can_invoke(tinf, tup) ? (0x01 << N) : 0x00;
    return result | calc_bitmask(fs, long_constant<N - 1l>(), tinf, tup);
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
        IsManipulator, T,
        typename detail::implicit_conversions<
            typename util::rm_const_and_ref<T>::type>::type>::type type;
};

// detach_if_needed(message tup, bool do_detach)
inline message& detach_if_needed(message& tup, std::true_type) {
    tup.force_detach();
    return tup;
}

inline message detach_if_needed(const message& tup, std::true_type) {
    message cpy{tup};
    cpy.force_detach();
    return cpy;
}

inline const message& detach_if_needed(const message& tup, std::false_type) {
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
    // static constexpr bool value = T::second_type::manipulates_args;
    typedef typename T::arg_types arg_types;
    static constexpr bool value = util::tl_exists<arg_types,
                                                  util::is_mutable_ref>::value;
};

template<typename T>
struct get_case_result {
    // typedef typename T::second_type::result_type type;
    typedef typename T::result_type type;
};

} // namespace detail
} // namespace cppa

namespace cppa {

template<class List>
struct match_result_from_type_list;

template<typename... Ts>
struct match_result_from_type_list<util::type_list<Ts...>> {
    typedef variant<none_t, typename lift_void<Ts>::type...> type;
};

/**
 * @brief A match expression encapsulating cases <tt>Cs...</tt>, whereas
 *        each case is a @p util::match_expr_case<...>.
 */
template<class... Cs>
class match_expr {

    static_assert(sizeof...(Cs) < 64, "too many functions");

 public:
    typedef util::type_list<Cs...> cases_list;

    typedef typename match_result_from_type_list<
        typename util::tl_distinct<typename util::tl_map<
            cases_list, detail::get_case_result>::type>::type>::type
    result_type;

    static constexpr bool has_manipulator =
        util::tl_exists<cases_list, detail::is_manipulator_case>::value;

    typedef detail::long_constant<sizeof...(Cs) - 1l> idx_token_type;

    static constexpr idx_token_type idx_token = idx_token_type{};

    template<typename T, typename... Ts>
    match_expr(T arg, Ts&&... args)
            : m_cases(std::move(arg), std::forward<Ts>(args)...) {
        init();
    }

    match_expr(match_expr&& other) : m_cases(std::move(other.m_cases)) {
        init();
    }

    match_expr(const match_expr& other) : m_cases(other.m_cases) { init(); }

    inline result_type operator()(const message& tup) { return apply(tup); }

    inline result_type operator()(message& tup) { return apply(tup); }

    inline result_type operator()(message&& tup) {
        message tmp{tup};
        return apply(tmp);
    }

    template<class... Ds>
    match_expr<Cs..., Ds...> or_else(const match_expr<Ds...>& other) const {
        return {tuple_cat(m_cases, other.cases())};
    }

    /** @cond PRIVATE */

    inline const std::tuple<Cs...>& cases() const { return m_cases; }

    intrusive_ptr<detail::behavior_impl> as_behavior_impl() const {
        // return new pfun_impl(*this);
        auto lvoid = [] {};
        using impl = detail::default_behavior_impl<match_expr, decltype(lvoid)>;
        return new impl(*this, util::duration{}, lvoid);
    }

    /** @endcond */

 private:

    // structure: std::tuple< std::tuple<type_list<...>, ...>,
    //                        std::tuple<type_list<...>, ...>,
    //                        ...>
    std::tuple<Cs...> m_cases;

    static constexpr size_t cache_size = 10;

    typedef std::pair<const std::type_info*, uint64_t> cache_element;

    std::vector<cache_element> m_cache;

    // ring buffer like access to m_cache
    size_t m_cache_begin;
    size_t m_cache_end;

    cache_element m_dummy;

    static inline void advance_(size_t& i) { i = (i + 1) % cache_size; }

    inline size_t find_token_pos(const std::type_info* type_token) {
        for (size_t i = m_cache_begin; i != m_cache_end; advance_(i)) {
            if (m_cache[i].first == type_token) return i;
        }
        return m_cache_end;
    }

    template<class Tuple>
    uint64_t get_cache_entry(const std::type_info* type_token,
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
            m_cache[i].second =
                calc_bitmask(m_cases, idx_token, *type_token, value);
        }
        return m_cache[i].second;
    }

    void init() {
        m_dummy.second = std::numeric_limits<uint64_t>::max();
        m_cache.resize(cache_size);
        for (auto& entry : m_cache) {
            entry.first = nullptr;
        }
        m_cache_begin = m_cache_end = 0;
    }

    template<class Tuple>
    result_type apply(Tuple& tup) {
        if (tup.empty()) {
            detail::tuple_dummy td;
            auto td_token_ptr = td.type_token();
            auto td_bitmask = get_cache_entry(td_token_ptr, td);
            return detail::unroll_expr<result_type>(
                m_cases, td_bitmask, idx_token, *td_token_ptr, false,
                static_cast<void*>(nullptr), td);
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
        return detail::unroll_expr<result_type>(m_cases, bitmask, idx_token,
                                                *token_ptr, dynamically_typed,
                                                ndp, *vals);
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

template<typename... Lhs, typename... Rhs>
inline match_expr<Lhs..., Rhs...> operator, (const match_expr<Lhs...>& lhs,
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
typename util::tl_apply<
    typename util::tl_concat<typename T::cases_list,
                               typename Ts::cases_list...>::type,
    match_expr>::type
match_expr_collect(const T& arg, const Ts&... args) {
    return {std::tuple_cat(arg.cases(), args.cases()...)};
}

namespace detail {

// implemented in message_handler.cpp
message_handler combine(behavior_impl_ptr, behavior_impl_ptr);
behavior_impl_ptr extract(const message_handler&);

template<typename... Cs>
behavior_impl_ptr extract(const match_expr<Cs...>& arg) {
    return arg.as_behavior_impl();
}

template<typename... As, typename... Bs>
match_expr<As..., Bs...> combine(const match_expr<As...>& lhs,
                                 const match_expr<Bs...>& rhs) {
    return lhs.or_else(rhs);
}

// forwards match_expr as match_expr as long as combining two match_expr,
// otherwise turns everything into behavior_impl_ptr
template<typename... As, typename... Bs>
const match_expr<As...>& combine_fwd(const match_expr<As...>& lhs,
                                     const match_expr<Bs...>&) {
    return lhs;
}

template<typename T, typename U>
behavior_impl_ptr combine_fwd(T& lhs, U&) {
    return extract(lhs);
}

template<typename T>
behavior_impl_ptr match_expr_concat(const T& arg) {
    return arg.as_behavior_impl();
}

template<typename F>
behavior_impl_ptr match_expr_concat(const message_handler& arg0,
                                    const timeout_definition<F>& arg) {
    return extract(arg0)->copy(arg);
}

template<typename... Cs, typename F>
behavior_impl_ptr match_expr_concat(const match_expr<Cs...>& arg0,
                                    const timeout_definition<F>& arg) {
    return new default_behavior_impl<match_expr<Cs...>, F>{arg0, arg};
}

template<typename T0, typename T1, typename... Ts>
behavior_impl_ptr match_expr_concat(const T0& arg0, const T1& arg1,
                                    const Ts&... args) {
    return match_expr_concat(
        combine(combine_fwd(arg0, arg1), combine_fwd(arg1, arg0)), args...);
}

// some more convenience functions

template<typename F, class E = typename std::enable_if<
                          util::is_callable<F>::value>::type>
match_expr<typename get_case<false, F, util::empty_type_list,
                             util::empty_type_list>::type>
lift_to_match_expr(F fun) {
    typedef typename get_case<false, F, util::empty_type_list,
                              util::empty_type_list>::type result_type;
    return result_type{std::move(fun)};
}

template<typename T, class E = typename std::enable_if<
                          !util::is_callable<T>::value>::type>
inline T lift_to_match_expr(T arg) {
    return arg;
}

} // namespace detail

} // namespace cppa

#endif // CPPA_MATCH_EXPR_HPP
