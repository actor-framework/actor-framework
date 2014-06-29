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

#ifndef CPPA_MEMORY_MANAGED_HPP
#define CPPA_MEMORY_MANAGED_HPP

namespace cppa {

namespace detail {
struct disposer;
}

/**
 * @brief This base enables derived classes to enforce a different
 *        allocation strategy than new/delete by providing a virtual
 *        protected @p request_deletion() function and non-public destructor.
 */
class memory_managed {

    friend struct detail::disposer;

 public:

    /**
     * @brief Default implementations calls <tt>delete this</tt>, but can
     *        be overriden in case deletion depends on some condition or
     *        the class doesn't use default new/delete.
     */
    virtual void request_deletion();

 protected:

    virtual ~memory_managed();

};

} // namespace cppa

#endif // CPPA_MEMORY_MANAGED_HPP
