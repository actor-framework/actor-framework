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


#ifndef CPPA_PROJECTION_HPP
#define CPPA_PROJECTION_HPP

#include "cppa/optional.hpp"
#include "cppa/guard_expr.hpp"

#include "cppa/util/call.hpp"
#include "cppa/util/int_list.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/type_traits.hpp"
#include "cppa/util/left_or_right.hpp"

#include "cppa/detail/tdata.hpp"

namespace cppa {
namespace detail {

template<typename Fun, typename Tuple, long... Is>
inline bool is_defined_at(Fun& f, Tuple& tup, util::int_list<Is...>) {
    return f.defined_at(get_cv_aware<Is>(tup)...);
}

template<typename ProjectionFuns, typename... Ts>
struct collected_args_tuple {
    typedef typename tdata_from_type_list<
            typename util::tl_zip<
                typename util::tl_map<
                    ProjectionFuns,
                    util::map_to_result_type,
                    util::rm_optional
                >::type,
                typename util::tl_map<
                    util::type_list<Ts...>,
                    mutable_gref_wrapped
                >::type,
                util::left_or_right
            >::type
        >::type
        type;
};

template<typename Fun>
struct is_void_fun {
    static constexpr bool value = std::is_same<typename Fun::result_type, void>::value;
};

/**
 * @brief Projection implemented by a set of functors.
 */
template<class ProjectionFuns, typename... Ts>
class projection {

 public:

    typedef typename tdata_from_type_list<ProjectionFuns>::type fun_container;

    typedef util::type_list<typename util::rm_const_and_ref<Ts>::type...> arg_types;

    projection() = default;

    projection(fun_container&& args) : m_funs(std::move(args)) { }

    projection(const fun_container& args) : m_funs(args) { }

    projection(const projection&) = default;

    /**
     * @brief Invokes @p fun with a projection of <tt>args...</tt>.
     */
    template<class PartFun>
    optional<typename PartFun::result_type> operator()(PartFun& fun, Ts... args) const {
        typename collected_args_tuple<ProjectionFuns, Ts...>::type pargs;
        auto indices = util::get_indices(pargs);
        if (collect(pargs, m_funs, std::forward<Ts>(args)...)) {
            if (is_defined_at(fun, pargs, indices)) {
                return util::apply_args(fun, pargs, indices);
            }
        }
        return none;
    }

 private:

    template<typename Storage, typename T>
    static inline  bool store(Storage& storage, T&& value) {
        storage = std::forward<T>(value);
        return true;
    }

    template<class Storage>
    static inline bool store(Storage& storage, optional<Storage>&& value) {
        if (value) {
            storage = std::move(*value);
            return true;
        }
        return false;
    }

    template<typename T>
    static inline auto fetch(const unit_t&, T&& arg)
    -> decltype(std::forward<T>(arg)) {
        return std::forward<T>(arg);
    }

    template<typename Fun, typename T>
    static inline auto fetch(const Fun& fun, T&& arg)
    -> decltype(fun(std::forward<T>(arg))) {
        return fun(std::forward<T>(arg));
    }

    static inline bool collect(tdata<>&, const tdata<>&) {
        return true;
    }

    template<class TData, class Trans, typename U, typename... Us>
    static inline bool collect(TData& td, const Trans& tr,
                               U&& arg, Us&&... args) {
        return    store(td.head, fetch(tr.head, std::forward<U>(arg)))
               && collect(td.tail(), tr.tail(), std::forward<Us>(args)...);
    }

    fun_container m_funs;

};

template<>
class projection<util::empty_type_list> {

 public:

    projection() = default;
    projection(const tdata<>&) { }
    projection(const projection&) = default;

    template<class PartFun>
    optional<typename PartFun::result_type> operator()(PartFun& fun) const {
        if (fun.defined_at()) {
            return fun();
        }
        return none;
    }

};

template<class ProjectionFuns, class List>
struct projection_from_type_list;

template<class ProjectionFuns, typename... Ts>
struct projection_from_type_list<ProjectionFuns, util::type_list<Ts...> > {
    typedef projection<ProjectionFuns, Ts...> type;
};

} // namespace detail
} // namespace cppa

#endif // CPPA_PROJECTION_HPP
