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


#ifndef CPPA_PROJECTION_HPP
#define CPPA_PROJECTION_HPP

#include "cppa/option.hpp"
#include "cppa/guard_expr.hpp"

#include "cppa/util/rm_option.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/apply_args.hpp"
#include "cppa/util/apply_tuple.hpp"
#include "cppa/util/left_or_right.hpp"

#include "cppa/detail/tdata.hpp"

namespace cppa { namespace detail {

template<class PartialFun>
struct projection_helper {
    const PartialFun& fun;
    projection_helper(const PartialFun& pfun) : fun(pfun) { }
    template<typename... Args>
    bool operator()(Args&&... args) const {
        if (fun.defined_at(std::forward<Args>(args)...)) {
            fun(std::forward<Args>(args)...);
            return true;
        }
        return false;
    }
};

/**
 * @brief Projection implemented by a set of functors.
 */
template<class ProjectionFuns, typename... Args>
class projection {

 public:

    typedef typename tdata_from_type_list<ProjectionFuns>::type fun_container;

    projection() = default;

    projection(fun_container&& args) : m_funs(std::move(args)) { }

    projection(const fun_container& args) : m_funs(args) { }

    projection(const projection&) = default;

    /**
     * @brief Invokes @p fun with a projection of <tt>args...</tt>.
     */
    template<class PartialFun>
    bool operator()(PartialFun& fun, Args... args) const {
        typedef typename util::tl_zip<
                    typename util::tl_map<
                        ProjectionFuns,
                        util::get_result_type,
                        util::rm_option
                    >::type,
                    typename util::tl_map<
                        util::type_list<Args...>,
                        mutable_gref_wrapped
                    >::type,
                    util::left_or_right
                >::type
                collected_args;

        typename tdata_from_type_list<collected_args>::type pargs;
        if (collect(pargs, m_funs, std::forward<Args>(args)...)) {
            projection_helper<PartialFun> helper{fun};
            return util::unchecked_apply_tuple<bool>(helper, pargs);
        }
        return false;
    }

 private:

    template<typename Storage, typename T>
    static inline  bool fetch_(Storage& storage, T&& value) {
        storage = std::forward<T>(value);
        return true;
    }

    template<class Storage>
    static inline bool fetch_(Storage& storage, option<Storage>&& value) {
        if (value) {
            storage = std::move(*value);
            return true;
        }
        return false;
    }

    template<class Storage, typename Fun, typename T>
    static inline  bool fetch(Storage& storage, const Fun& fun, T&& arg) {
        return fetch_(storage, fun(std::forward<T>(arg)));
    }

    template<typename Storage, typename T>
    static inline bool fetch(Storage& storage, const util::void_type&, T&& arg) {
        return fetch_(storage, std::forward<T>(arg));
    }

    static inline bool collect(tdata<>&, const tdata<>&) {
        return true;
    }

    template<class TData, class Trans, typename T0, typename... Ts>
    static inline bool collect(TData& td, const Trans& tr,
                               T0&& arg0, Ts&&... args) {
        return    fetch(td.head, tr.head, std::forward<T0>(arg0))
               && collect(td.tail(), tr.tail(), std::forward<Ts>(args)...);
    }

    fun_container m_funs;

};

template<>
class projection<util::type_list<> > {

 public:

    projection() = default;
    projection(const tdata<>&) { }
    projection(const projection&) = default;

    template<class PartialFun>
    bool operator()(PartialFun& fun) const {
        if (fun.defined_at()) {
            fun();
            return true;
        }
        return false;
    }

};

template<class ProjectionFuns, class List>
struct projection_from_type_list;

template<class ProjectionFuns, typename... Args>
struct projection_from_type_list<ProjectionFuns, util::type_list<Args...> > {
    typedef projection<ProjectionFuns, Args...> type;
};

} } // namespace cppa::detail

#endif // CPPA_PROJECTION_HPP
