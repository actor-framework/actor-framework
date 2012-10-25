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


#ifndef CPPA_ENABLE_WEAK_PTR_MIXIN_HPP
#define CPPA_ENABLE_WEAK_PTR_MIXIN_HPP

#include <mutex>
#include <utility>
#include <type_traits>

#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/weak_ptr_anchor.hpp"

#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"

namespace cppa {

template<class Base>
class enable_weak_ptr_mixin : public Base {

    typedef Base super;

    static_assert(std::is_base_of<ref_counted,Base>::value,
                  "Base needs to be derived from ref_counted");

 public:

    inline intrusive_ptr<weak_ptr_anchor> get_weak_ptr_anchor() const {
        return m_anchor;
    }

 protected:

    template<typename... Args>
    enable_weak_ptr_mixin(Args&&... args)
    : super(std::forward<Args>(args)...)
    , m_anchor(new weak_ptr_anchor(this)) { }

    void request_deletion() {
        if (m_anchor->try_expire()) delete this;
    }

 private:

    intrusive_ptr<weak_ptr_anchor> m_anchor;

};

} // namespace cppa

#endif // CPPA_ENABLE_WEAK_PTR_MIXIN_HPP
