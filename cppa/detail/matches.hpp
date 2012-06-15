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


#ifndef CPPA_MATCHES_HPP
#define CPPA_MATCHES_HPP

#include <numeric>
#include "cppa/pattern.hpp"
#include "cppa/any_tuple.hpp"

#include "cppa/util/type_list.hpp"

namespace cppa { namespace detail {

template<wildcard_position, class Tuple, typename...>
struct matcher;

template<class Tuple, typename... T>
struct matcher<wildcard_position::nil, Tuple, T...> {
    static inline bool tmatch(const Tuple& tup) {
        if (tup.impl_type() == tuple_impl_info::statically_typed) {
            // statically typed tuples return &typeid(type_list<T...>)
            // as type token
            return typeid(util::type_list<T...>) == *(tup.type_token());
        }
        // always use a full dynamic match for dynamic typed tuples
        else if (tup.size() == sizeof...(T)) {
            auto& tarr = static_types_array<T...>::arr;
            return std::equal(tup.begin(), tup.end(), tarr.begin(),
                              types_only_eq);
        }
        return false;
    }

    static inline bool tmatch(const Tuple& tup,
                              util::fixed_vector<size_t, sizeof...(T)>& mv) {
        if (tmatch(tup)) {
            mv.resize(sizeof...(T));
            std::iota(mv.begin(), mv.end(), 0);
            return true;
        }
        return false;
    }

    static inline bool vmatch(const Tuple& tup, const pattern<T...>& ptrn) {
        CPPA_REQUIRE(tup.size() == sizeof...(T));
        return ptrn._matches_values(tup);
    }
};

template<class Tuple, typename... T>
struct matcher<wildcard_position::trailing, Tuple, T...> {
    static constexpr size_t size = sizeof...(T) - 1;

    static inline bool tmatch(const Tuple& tup) {
        if (tup.size() >= size) {
            auto& tarr = static_types_array<T...>::arr;
            auto begin = tup.begin();
            return std::equal(begin, begin + size, tarr.begin(),
                              types_only_eq);
        }
        return false;
    }

    static inline bool tmatch(const Tuple& tup,
                             util::fixed_vector<size_t, size>& mv) {
        if (tmatch(tup)) {
            mv.resize(size);
            std::iota(mv.begin(), mv.end(), 0);
            return true;
        }
        return false;
    }

    static inline bool vmatch(const Tuple& tup, const pattern<T...>& ptrn) {
        return ptrn._matches_values(tup);
    }
};

template<class Tuple>
struct matcher<wildcard_position::leading, Tuple, anything> {
    static inline bool tmatch(const Tuple&) {
        return true;
    }
    static inline bool tmatch(const Tuple&, util::fixed_vector<size_t, 0>&) {
        return true;
    }
    static inline bool vmatch(const Tuple&, const pattern<anything>&) {
        return true;
    }
};

template<class Tuple, typename... T>
struct matcher<wildcard_position::leading, Tuple, T...> {
    static constexpr size_t size = sizeof...(T) - 1;

    static inline bool tmatch(const Tuple& tup) {
        auto tup_size = tup.size();
        if (tup_size >= size) {
            auto& tarr = static_types_array<T...>::arr;
            auto begin = tup.begin();
            begin += (tup_size - size);
            return std::equal(begin, tup.end(), (tarr.begin() + 1), // skip 'anything'
                              types_only_eq);
        }
        return false;
    }

    static inline bool tmatch(const Tuple& tup,
                              util::fixed_vector<size_t, size>& mv) {
        if (tmatch(tup)) {
            mv.resize(size);
            std::iota(mv.begin(), mv.end(), tup.size() - size);
            return true;
        }
        return false;
    }

    static inline bool vmatch(const Tuple& tup, const pattern<T...>& ptrn) {
        return ptrn._matches_values(tup);
    }
};

template<class Tuple, typename... T>
struct matcher<wildcard_position::in_between, Tuple, T...> {
    static constexpr int signed_wc_pos =
            util::tl_find<util::type_list<T...>, anything>::value;
    static constexpr size_t size = sizeof...(T);
    static constexpr size_t wc_pos = static_cast<size_t>(signed_wc_pos);

    static_assert(   signed_wc_pos != -1
                  && signed_wc_pos != 0
                  && signed_wc_pos != (sizeof...(T) - 1),
                  "illegal wildcard position");

    static inline bool tmatch(const Tuple& tup) {
        auto tup_size = tup.size();
        if (tup_size >= size) {
            auto& tarr = static_types_array<T...>::arr;
            // first range [0, X1)
            auto begin = tup.begin();
            auto end = begin + wc_pos;
            if (std::equal(begin, end, tarr.begin(), types_only_eq)) {
                // second range [X2, N)
                begin = end = tup.end();
                begin -= (size - (wc_pos + 1));
                auto arr_begin = tarr.begin() + (wc_pos + 1);
                return std::equal(begin, end, arr_begin, types_only_eq);
            }
        }
        return false;
    }

    static inline bool tmatch(const Tuple& tup,
                              util::fixed_vector<size_t, size - 1>& mv) {
        if (tmatch(tup)) {
            // first range
            mv.resize(size - 1);
            auto begin = mv.begin();
            std::iota(begin, begin + wc_pos, 0);
            // second range
            begin = mv.begin() + wc_pos;
            std::iota(begin, mv.end(), tup.size() - (size - (wc_pos + 1)));
            return true;
        }
        return false;
    }

    static inline bool vmatch(const Tuple& tup, const pattern<T...>& ptrn) {
        return ptrn._matches_values(tup);
    }
};

template<class Tuple, typename... T>
struct matcher<wildcard_position::multiple, Tuple, T...> {
    static constexpr size_t wc_count =
            util::tl_count<util::type_list<T...>, is_anything>::value;

    static_assert(sizeof...(T) > wc_count, "only wildcards given");

    template<class TupleIter, class PatternIter,
             class Push, class Commit, class Rollback>
    static bool match(TupleIter tbegin, TupleIter tend,
                      PatternIter pbegin, PatternIter pend,
                      Push& push, Commit& commit, Rollback& rollback) {
        while (!(pbegin == pend && tbegin == tend)) {
            if (pbegin == pend) {
                // reached end of pattern while some values remain unmatched
                return false;
            }
            else if (*pbegin == nullptr) { // nullptr == wildcard (anything)
                // perform submatching
                ++pbegin;
                // always true at the end of the pattern
                if (pbegin == pend) return true;
                // safe current mapping as fallback
                commit();
                // iterate over tuple values until we found a match
                for (; tbegin != tend; ++tbegin) {
                    if (match(tbegin, tend, pbegin, pend,
                              push, commit, rollback)) {
                        return true;
                    }
                    // restore mapping to fallback (delete invalid mappings)
                    rollback();
                }
                return false; // no submatch found
            }
            // compare types
            else if (tbegin.type() == *pbegin) push(tbegin);
            // no match
            else return false;
            // next iteration
            ++tbegin;
            ++pbegin;
        }
        return true; // pbegin == pend && tbegin == tend
    }

    static inline bool tmatch(const Tuple& tup) {
        auto& tarr = static_types_array<T...>::arr;
        if (tup.size() >= (sizeof...(T) - wc_count)) {
            auto fpush = [](const typename Tuple::const_iterator&) { };
            auto fcommit = []() { };
            auto frollback = []() { };
            return match(tup.begin(), tup.end(), tarr.begin(), tarr.end(),
                         fpush, fcommit, frollback);
        }
        return false;
    }

    template<class MappingVector>
    static inline bool tmatch(const Tuple& tup, MappingVector& mv) {
        auto& tarr = static_types_array<T...>::arr;
        if (tup.size() >= (sizeof...(T) - wc_count)) {
            size_t commited_size = 0;
            auto fpush = [&](const typename Tuple::const_iterator& iter) {
                mv.push_back(iter.position());
            };
            auto fcommit = [&]() { commited_size = mv.size(); };
            auto frollback = [&]() { mv.resize(commited_size); };
            return match(tup.begin(), tup.end(), tarr.begin(), tarr.end(),
                         fpush, fcommit, frollback);
        }
        return false;
    }

    static inline bool vmatch(const Tuple& tup,
                              const pattern<T...>& ptrn,
                              const typename pattern<T...>::mapping_vector& mv) {
        return ptrn._matches_values(tup, &mv);
    }
};

// implementation for zero or one wildcards
template<wildcard_position PC, class Tuple, typename... Ts>
struct match_impl {
    static inline bool _(const Tuple& tup) {
        return matcher<PC, Tuple, Ts...>::tmatch(tup);
    }

    template<size_t Size>
    static inline bool _(const Tuple& tup,
                         util::fixed_vector<size_t, Size>& mv) {
        return matcher<PC, Tuple, Ts...>::tmatch(tup, mv);
    }

    static inline bool _(const Tuple& tup,
                         const pattern<Ts...>& p) {
        return    matcher<PC, Tuple, Ts...>::tmatch(tup)
               && (   p.has_values() == false
                   || matcher<PC, Tuple, Ts...>::vmatch(tup, p));
    }

    static inline bool _(const Tuple& tup,
                         const pattern<Ts...>& p,
                         typename pattern<Ts...>::mapping_vector& mv) {
        return    matcher<PC, Tuple, Ts...>::tmatch(tup, mv)
               && (   p.has_values() == false
                   || matcher<PC, Tuple, Ts...>::vmatch(tup, p));
    }
};

// implementation for multiple wildcards
template<class Tuple, typename... Ts>
struct match_impl<wildcard_position::multiple, Tuple, Ts...> {
    static constexpr auto PC = wildcard_position::multiple;

    static inline bool _(const Tuple& tup) {
        return matcher<PC, Tuple, Ts...>::tmatch(tup);
    }

    template<size_t Size>
    static inline bool _(const Tuple& tup,
                         util::fixed_vector<size_t, Size>& mv) {
        return matcher<PC, Tuple, Ts...>::tmatch(tup, mv);
    }

    static inline bool _(const Tuple& tup, const pattern<Ts...>& p) {
        if (p.has_values()) {
            typename pattern<Ts...>::mapping_vector mv;
            return    matcher<PC, Tuple, Ts...>::tmatch(tup, mv)
                   && matcher<PC, Tuple, Ts...>::vmatch(tup, p, mv);
        }
        return matcher<PC, Tuple, Ts...>::tmatch(tup);
    }

    static inline bool _(const Tuple& tup, const pattern<Ts...>& p,
                         typename pattern<Ts...>::mapping_vector& mv) {
        if (p.has_values()) {
            typename pattern<Ts...>::mapping_vector mv;
            return    matcher<PC, Tuple, Ts...>::tmatch(tup, mv)
                   && matcher<PC, Tuple, Ts...>::vmatch(tup, p, mv);
        }
        return matcher<PC, Tuple, Ts...>::tmatch(tup, mv);
    }
};

template<class Tuple, class List>
struct match_impl_from_type_list;

template<class Tuple, typename... Ts>
struct match_impl_from_type_list<Tuple, util::type_list<Ts...> > {
    typedef match_impl<get_wildcard_position<util::type_list<Ts...> >(),
                       Tuple,
                       Ts...>
            type;
};

/*
 * @brief Returns true if this tuple matches the pattern <tt>{Ts...}</tt>.
 */
template<typename... Ts>
bool matches(const any_tuple& tup) {
    typedef util::type_list<Ts...> tl;
    return match_impl<get_wildcard_position<tl>(), any_tuple, Ts...>
           ::_(tup);
}

/*
 * @brief Returns true if this tuple matches the pattern <tt>{Ts...}</tt>.
 */
template<typename... Ts>
bool matches(const any_tuple& tup,
             util::fixed_vector<
                                 size_t,
                                 util::tl_count_not<
                                     util::type_list<Ts...>,
                                     is_anything>::value>& mv) {
    typedef util::type_list<Ts...> tl;
    return match_impl<get_wildcard_position<tl>(), any_tuple, Ts...>
           ::_(tup, mv);
}

/*
 * @brief Returns true if this tuple matches @p pn.
 */
template<typename... Ts>
bool matches(const any_tuple& tup, const pattern<Ts...>& pn) {
    typedef util::type_list<Ts...> tl;
    return match_impl<get_wildcard_position<tl>(), any_tuple, Ts...>
           ::_(tup, pn);
}

/*
 * @brief Returns true if this tuple matches @p pn.
 */
template<typename... Ts>
bool matches(const any_tuple& tup, const pattern<Ts...>& pn,
             typename pattern<Ts...>::mapping_vector& mv) {
    typedef util::type_list<Ts...> tl;
    return match_impl<get_wildcard_position<tl>(), any_tuple, Ts...>
           ::_(tup, pn, mv);
}

// support for type_list based matching
template<typename... Ts>
inline bool matches(const any_tuple& tup, const util::type_list<Ts...>&) {
    return matches<Ts...>(tup);
}

template<typename... Ts>
inline bool matches(const any_tuple& tup, const util::type_list<Ts...>&,
                    util::fixed_vector<
                                        size_t,
                                        util::tl_count_not<
                                            util::type_list<Ts...>,
                                            is_anything>::value>& mv) {
    return matches<Ts...>(tup, mv);
}

/*
 * @brief Returns true if this tuple matches the pattern <tt>{Ts...}</tt>
 *        (does not match for values).
 */
template<typename... Ts>
inline bool matches_types(const any_tuple& tup, const pattern<Ts...>&) {
    return matches<Ts...>(tup);
}

template<typename... Ts>
inline bool matches_types(const any_tuple& tup, const util::type_list<Ts...>&) {
    return matches<Ts...>(tup);
}

} } // namespace cppa::detail

#endif // CPPA_MATCHES_HPP
