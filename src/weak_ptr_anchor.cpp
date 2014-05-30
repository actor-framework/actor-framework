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


#include <mutex> // std::lock_guard

#include "cppa/weak_ptr_anchor.hpp"

namespace cppa {

weak_ptr_anchor::~weak_ptr_anchor() { }

weak_ptr_anchor::weak_ptr_anchor(ref_counted* ptr) : m_ptr(ptr) { }

bool weak_ptr_anchor::try_expire() {
    std::lock_guard<util::shared_spinlock> guard(m_lock);
    // double-check reference count
    if (m_ptr->get_reference_count() == 0) {
        m_ptr = nullptr;
        return true;
    }
    return false;
}

} // namespace cppa
