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


#ifndef CPPA_OPT_IMPLS_HPP
#define CPPA_OPT_IMPLS_HPP

#include <sstream>

#include "cppa/on.hpp"
#include "cppa/option.hpp"

// this header contains implementation details for opt.hpp

namespace cppa { namespace detail {

template<typename T>
struct conv_arg_impl {
    typedef option<T> result_type;
    static inline result_type _(const std::string& arg) {
        std::istringstream iss(arg);
        T result;
        if (iss >> result && iss.eof()) {
            return result;
        }
        return {};
    }
};

template<>
struct conv_arg_impl<std::string> {
    typedef option<std::string> result_type;
    static inline result_type _(const std::string& arg) { return arg; }
};

template<bool> class opt_rvalue_builder;

template<typename T>
struct rd_arg_storage : ref_counted {
    T& storage;
    bool set;
    std::string arg_name;
    rd_arg_storage(T& r) : storage(r), set(false) { }
};

template<typename T>
class rd_arg_functor {

    template<bool> friend class opt_rvalue_builder;

    typedef rd_arg_storage<T> storage_type;

 public:

    rd_arg_functor(const rd_arg_functor&) = default;

    rd_arg_functor(T& storage) : m_storage(new storage_type(storage)) { }

    bool operator()(const std::string& arg) const {
        if (m_storage->set) {
            std::cerr << "*** error: " << m_storage->arg_name
                      << " previously set to " << m_storage->storage
                      << std::endl;
        }
        else {
            auto opt = conv_arg_impl<T>::_(arg);
            if (opt) {
                m_storage->storage = *opt;
                m_storage->set = true;
                return true;
            }
            else {
                std::cerr << "*** error: cannot convert \"" << arg << "\" to "
                          << detail::demangle(typeid(T).name())
                          << " [option: \"" << m_storage->arg_name << "\"]"
                          << std::endl;
            }
        }
        return false;
    }

 private:

    intrusive_ptr<storage_type> m_storage;

};

template<typename T>
class add_arg_functor {

    template<bool> friend class opt_rvalue_builder;

 public:

    typedef std::vector<T> value_type;
    typedef rd_arg_storage<value_type> storage_type;

    add_arg_functor(const add_arg_functor&) = default;

    add_arg_functor(value_type& storage) : m_storage(new storage_type(storage)) { }

    bool operator()(const std::string& arg) const {
        auto opt = conv_arg_impl<T>::_(arg);
        if (opt) {
            m_storage->storage.push_back(*opt);
            return true;
        }
        std::cerr << "*** error: cannot convert \"" << arg << "\" to "
                  << detail::demangle(typeid(T))
                  << " [option: \"" << m_storage->arg_name << "\"]"
                  << std::endl;
        return false;
    }

 private:

    intrusive_ptr<storage_type> m_storage;

};

template<typename T>
struct is_rd_arg : std::false_type { };

template<typename T>
struct is_rd_arg<rd_arg_functor<T> > : std::true_type { };

template<typename T>
struct is_rd_arg<add_arg_functor<T> > : std::true_type { };

template<bool HasShortOpt = true>
class opt_rvalue_builder {

 public:

    typedef decltype(on<std::string, std::string>()
                    .when(cppa::placeholders::_x1.in(std::vector<std::string>())))
            left_type;

    typedef decltype(on(std::function<option<std::string>(const std::string&)>()))
            right_type;

    template<typename Left, typename Right>
    opt_rvalue_builder(char sopt, std::string lopt, Left&& lhs, Right&& rhs)
    : m_short(sopt), m_long(std::move(lopt))
    , m_left(std::forward<Left>(lhs)), m_right(std::forward<Right>(rhs)) { }

    template<typename Expr>
    auto operator>>(Expr expr)
    -> decltype((*(static_cast<left_type*>(nullptr)) >> expr).or_else(
                 *(static_cast<right_type*>(nullptr)) >> expr)) const {
        inject_arg_name(expr);
        return (m_left >> expr).or_else(m_right >> expr);
    }

 private:

    template<typename T>
    inline void inject_arg_name(rd_arg_functor<T>& expr) {
        expr.m_storage->arg_name = m_long;
    }

    template<typename T>
    inline void inject_arg_name(const T&) { }

    char m_short;
    std::string m_long;
    left_type m_left;
    right_type m_right;

};

template<>
class opt_rvalue_builder<false> {

 public:

    typedef decltype(on(std::function<option<std::string>(const std::string&)>()))
            sub_type;

    template<typename SubType>
    opt_rvalue_builder(char sopt, std::string lopt, SubType&& sub)
    : m_short(sopt), m_long(std::move(lopt))
    , m_sub(std::forward<SubType>(sub)) { }

    template<typename Expr>
    auto operator>>(Expr expr)
    -> decltype(*static_cast<sub_type*>(nullptr) >> expr) const {
        inject_arg_name(expr);
        return m_sub >> expr;
    }

 private:

    template<typename T>
    inline void inject_arg_name(rd_arg_functor<T>& expr) {
        expr.m_storage->arg_name = m_long;
    }

    template<typename T>
    inline void inject_arg_name(const T&) { }

    char m_short;
    std::string m_long;
    sub_type m_sub;

};

} } // namespace cppa::detail

#endif // CPPA_OPT_IMPLS_HPP
