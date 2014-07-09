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

#ifndef CPPA_ACTOR_OSTREAM_HPP
#define CPPA_ACTOR_OSTREAM_HPP

#include "cppa/actor.hpp"
#include "cppa/message.hpp"
#include "cppa/to_string.hpp"

namespace cppa {

class local_actor;
class scoped_actor;

class actor_ostream {

 public:

    using fun_type = actor_ostream& (*)(actor_ostream&);

    actor_ostream(actor_ostream&&) = default;
    actor_ostream(const actor_ostream&) = default;

    actor_ostream& operator=(actor_ostream&&) = default;
    actor_ostream& operator=(const actor_ostream&) = default;

    explicit actor_ostream(actor self);

    actor_ostream& write(std::string arg);

    actor_ostream& flush();

    inline actor_ostream& operator<<(std::string arg) {
        return write(std::move(arg));
    }

    inline actor_ostream& operator<<(const message& arg) {
        return write(cppa::to_string(arg));
    }

    // disambiguate between conversion to string and to message
    inline actor_ostream& operator<<(const char* arg) {
        return *this << std::string{arg};
    }

    template<typename T>
    inline typename std::enable_if<!std::is_convertible<T, std::string>::value && !std::is_convertible<T, message>::value, actor_ostream&>::type operator<<(
        T&& arg) {
        return write(std::to_string(std::forward<T>(arg)));
    }

    inline actor_ostream& operator<<(actor_ostream::fun_type f) {
        return f(*this);
    }

 private:

    actor m_self;
    actor m_printer;

};

inline actor_ostream aout(actor self) {
    return actor_ostream{self};
}

} // namespace cppa

namespace std {
// provide convenience overlaods for aout; implemented in logging.cpp
cppa::actor_ostream& endl(cppa::actor_ostream& o);
cppa::actor_ostream& flush(cppa::actor_ostream& o);
} // namespace std

#endif // CPPA_ACTOR_OSTREAM_HPP
