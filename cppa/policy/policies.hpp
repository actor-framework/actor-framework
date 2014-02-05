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


#ifndef CPPA_POLICY_POLICIES_HPP
#define CPPA_POLICY_POLICIES_HPP

namespace cppa {
namespace policy {

template<class SchedulingPolicy, class PriorityPolicy,
         class ResumePolicy, class InvokePolicy>
class policies {

 public:

    typedef SchedulingPolicy scheduling_policy;
    typedef PriorityPolicy   priority_policy;
    typedef ResumePolicy     resume_policy;
    typedef InvokePolicy     invoke_policy;

    scheduling_policy& get_scheduling_policy() {
        return m_scheduling_policy;
    }

    priority_policy& get_priority_policy() {
        return m_priority_policy;
    }

    resume_policy& get_resume_policy() {
        return m_resume_policy;
    }

    invoke_policy& get_invoke_policy() {
        return m_invoke_policy;
    }

 private:

    scheduling_policy m_scheduling_policy;
    priority_policy   m_priority_policy;
    resume_policy     m_resume_policy;
    invoke_policy     m_invoke_policy;

};

} // namespace policy
} // namespace cppa

#endif // CPPA_POLICY_POLICIES_HPP
