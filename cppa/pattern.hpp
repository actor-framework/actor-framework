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


#ifndef CPPA_PATTERN_HPP
#define CPPA_PATTERN_HPP

#include <iostream>

#include <list>
#include <memory>
#include <vector>
#include <cstddef>
#include <type_traits>

#include "cppa/option.hpp"
#include "cppa/anything.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/guard.hpp"
#include "cppa/util/tbind.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/arg_match_t.hpp"
#include "cppa/util/fixed_vector.hpp"
#include "cppa/util/is_primitive.hpp"
#include "cppa/util/static_foreach.hpp"

#include "cppa/detail/boxed.hpp"
#include "cppa/detail/tdata.hpp"
#include "cppa/detail/types_array.hpp"

namespace cppa {

/**
 * @brief Denotes the position of {@link cppa::anything anything} in a
 *        template parameter pack.
 */
enum class wildcard_position {
    nil,
    trailing,
    leading,
    in_between,
    multiple
};

/**
 * @brief Gets the position of {@link cppa::anything anything} from the
 *        type list @p Types.
 * @tparam A template parameter pack as {@link cppa::util::type_list type_list}.
 */
template<typename Types>
constexpr wildcard_position get_wildcard_position() {
    return util::tl_exists<Types, is_anything>::value
           ? ((util::tl_count<Types, is_anything>::value == 1)
              ? (std::is_same<typename Types::head, anything>::value
                 ? wildcard_position::leading
                 : (std::is_same<typename Types::back, anything>::value
                    ? wildcard_position::trailing
                    : wildcard_position::in_between))
              : wildcard_position::multiple)
           : wildcard_position::nil;
}

struct value_matcher {
    const bool is_dummy;
    inline value_matcher(bool dummy_impl = false) : is_dummy(dummy_impl) { }
    virtual ~value_matcher();
    virtual bool operator()(const any_tuple&) const = 0;
};

struct dummy_matcher : value_matcher {
    inline dummy_matcher() : value_matcher(true) { }
    bool operator()(const any_tuple&) const;
};

struct cmp_helper {
    size_t i;
    const any_tuple& tup;
    cmp_helper(const any_tuple& tp, size_t pos = 0) : i(pos), tup(tp) { }
    template<typename T>
    inline bool operator()(const T& what) {
        return what == tup.get_as<T>(i++);
    }
    template<typename T>
    inline bool operator()(std::unique_ptr<util::guard<T> const >& g) {
        return (*g)(tup.get_as<T>(i++));
    }
    template<typename T>
    inline bool operator()(const util::wrapped<T>&) {
        ++i;
        return true;
    }
};

template<wildcard_position WP, class PatternTypes, class ValueTypes>
class value_matcher_impl;

template<typename... Ts, typename... Vs>
class value_matcher_impl<wildcard_position::nil,
                         util::type_list<Ts...>,
                         util::type_list<Vs...> > : public value_matcher {

    detail::tdata<Vs...> m_values;

 public:

    template<typename... Args>
    value_matcher_impl(Args&&... args) : m_values(std::forward<Args>(args)...) { }

    bool operator()(const any_tuple& tup) const {
        cmp_helper h{tup};
        return util::static_foreach<0, sizeof...(Vs)>::eval(m_values, h);
    }

};

template<typename... Ts, typename... Vs>
class value_matcher_impl<wildcard_position::trailing,
                         util::type_list<Ts...>,
                         util::type_list<Vs...> >
        : public value_matcher_impl<wildcard_position::nil,
                                    util::type_list<Ts...>,
                                    util::type_list<Vs...> > {

    typedef value_matcher_impl<wildcard_position::nil,
                               util::type_list<Ts...>,
                               util::type_list<Vs...> >
            super;

 public:

    template<typename... Args>
    value_matcher_impl(Args&&... args) : super(std::forward<Args>(args)...) { }

};

template<typename... Ts, typename... Vs>
class value_matcher_impl<wildcard_position::leading,
                         util::type_list<Ts...>,
                         util::type_list<Vs...> > : public value_matcher {

    detail::tdata<Vs...> m_values;

 public:

    template<typename... Args>
    value_matcher_impl(Args&&... args) : m_values(std::forward<Args>(args)...) { }

    bool operator()(const any_tuple& tup) const {
        cmp_helper h{tup, tup.size() - sizeof...(Ts)};
        return util::static_foreach<0, sizeof...(Vs)>::eval(m_values, h);
    }

};

template<typename... Ts, typename... Vs>
class value_matcher_impl<wildcard_position::in_between,
                         util::type_list<Ts...>,
                         util::type_list<Vs...> > : public value_matcher {

    detail::tdata<Vs...> m_values;

 public:

    template<typename... Args>
    value_matcher_impl(Args&&... args) : m_values(std::forward<Args>(args)...) { }

    bool operator()(const any_tuple& tup) const {
        static constexpr size_t wcpos =
                static_cast<size_t>(util::tl_find<util::type_list<Ts...>, anything>::value);
        static_assert(wcpos < sizeof...(Ts), "illegal wildcard position");
        cmp_helper h0{tup, 0};
        cmp_helper h1{tup, tup.size() - (sizeof...(Ts) - (wcpos + 1))};
        static constexpr size_t range_end = (sizeof...(Vs) > wcpos)
                                            ? wcpos
                                            : sizeof...(Vs);
        return    util::static_foreach<0, range_end>::eval(m_values, h0)
               && util::static_foreach<range_end + 1, sizeof...(Vs)>::eval(m_values, h1);
    }

};

template<typename... Ts, typename... Vs>
class value_matcher_impl<wildcard_position::multiple,
                         util::type_list<Ts...>,
                         util::type_list<Vs...> > : public value_matcher {

 public:

    template<typename... Args>
    value_matcher_impl(Args&&...) { }

    bool operator()(const any_tuple&) const {
        throw std::runtime_error("not implemented yet, sorry");
    }

};

/**
 * @brief A pattern matching for type and optionally value of tuple elements.
 */
template<typename... Types>
class pattern {

    static_assert(sizeof...(Types) > 0, "empty pattern");

    pattern(const pattern&) = delete;
    pattern& operator=(const pattern&) = delete;

 public:

    static constexpr size_t size = sizeof...(Types);

    static constexpr wildcard_position wildcard_pos =
            get_wildcard_position<util::type_list<Types...> >();

    /**
     * @brief Parameter pack as {@link cppa::util::type_list type_list}.
     */
    typedef util::type_list<Types...> types;

    typedef typename types::head head_type;

    typedef typename util::tl_filter_not<types, is_anything>::type
            filtered_types;

    static constexpr size_t filtered_size = filtered_types::size;

    typedef util::fixed_vector<size_t, filtered_types::size> mapping_vector;

    typedef const uniform_type_info* const_iterator;

    typedef std::reverse_iterator<const_iterator> reverse_const_iterator;

    inline const_iterator begin() const {
        return detail::static_types_array<Types...>::arr.begin();
    }

    inline const_iterator end() const {
        return detail::static_types_array<Types...>::arr.end();
    }

    inline reverse_const_iterator rbegin() const {
        return reverse_const_iterator{end()};
    }

    inline reverse_const_iterator rend() const {
        return reverse_const_iterator{begin()};
    }

    inline bool has_values() const { return m_vm->is_dummy == false; }

    // @warning does NOT check types
    bool _matches_values(const any_tuple& tup) const {
        return (*m_vm)(tup);
    }

    pattern() : m_vm(new dummy_matcher) {
    }

    template<typename... Args>
    pattern(head_type arg0, Args&&... args)
        : m_vm(get_value_matcher(std::move(arg0), std::forward<Args>(args)...)) {
    }

    template<typename... Args>
    pattern(const util::wrapped<head_type>& arg0, Args&&... args)
        : m_vm(get_value_matcher(arg0, std::forward<Args>(args)...)) {
    }

    template<typename... Args>
    pattern(const detail::tdata<Args...>& data) {
        m_vm.reset(new value_matcher_impl<wildcard_pos, types, util::type_list<Args...> >{data});
    }

    pattern(std::unique_ptr<value_matcher>&& vm) : m_vm(std::move(vm)) {
        if (!m_vm) m_vm.reset(new dummy_matcher);
    }

    static inline value_matcher* get_value_matcher() {
        return nullptr;
    }

    template<typename Arg0, typename... Args>
    static value_matcher* get_value_matcher(Arg0&& arg0, Args&&... args) {
        using namespace util;
        typedef typename tl_filter_not<
                    type_list<typename detail::strip_and_convert<Arg0>::type,
                              typename detail::strip_and_convert<Args>::type...>,
                    tbind<std::is_same, wrapped<arg_match_t> >::type
                >::type
                arg_types;
        static_assert(arg_types::size <= size, "too many arguments");
        if (tl_forall<arg_types, detail::is_boxed>::value) {
            return new dummy_matcher;
        }
        return new value_matcher_impl<wildcard_pos, types, arg_types>{std::forward<Arg0>(arg0), std::forward<Args>(args)...};
    }

 private:

    std::unique_ptr<value_matcher> m_vm;

};

template<typename TypeList>
struct pattern_from_type_list;

template<typename... Types>
struct pattern_from_type_list<util::type_list<Types...>> {
    typedef pattern<Types...> type;
};

} // namespace cppa

#endif // CPPA_PATTERN_HPP
