/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_POLICY_POLICIES_HPP
#define CAF_POLICY_POLICIES_HPP

namespace caf {
namespace policy {

/**
 * A container for actor-related policies.
 */
template <class SchedulingPolicy,
     class PriorityPolicy,
     class ResumePolicy,
     class InvokePolicy>
class actor_policies {

 public:

  using scheduling_policy = SchedulingPolicy;

  using priority_policy = PriorityPolicy;

  using resume_policy = ResumePolicy;

  using invoke_policy = InvokePolicy;

  inline scheduling_policy& get_scheduling_policy() {
    return m_scheduling_policy;
  }

  inline priority_policy& get_priority_policy() {
    return m_priority_policy;
  }

  inline resume_policy& get_resume_policy() {
    return m_resume_policy;
  }

  inline invoke_policy& get_invoke_policy() {
    return m_invoke_policy;
  }

 private:

  scheduling_policy m_scheduling_policy;
  priority_policy m_priority_policy;
  resume_policy m_resume_policy;
  invoke_policy m_invoke_policy;

};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_POLICIES_HPP
