#include "cppa/invoke.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/util/duration.hpp"
#include "cppa/detail/invokable.hpp"

namespace cppa {

util::duration timed_invoke_rules::default_timeout;
// invoke_rules_base

invoke_rules_base::invoke_rules_base(invokable_list&& ilist)
    : m_list(std::move(ilist))
{
}

invoke_rules_base::~invoke_rules_base()
{
}

bool invoke_rules_base::operator()(const any_tuple& t) const
{
    for (auto i = m_list.begin(); i != m_list.end(); ++i)
    {
        if ((*i)->invoke(t)) return true;
    }
    return false;
}

detail::intermediate*
invoke_rules_base::get_intermediate(const any_tuple& t) const
{
    detail::intermediate* result;
    for (auto i = m_list.begin(); i != m_list.end(); ++i)
    {
        result = (*i)->get_intermediate(t);
        if (result != nullptr) return result;
    }
    return nullptr;
}

// timed_invoke_rules

timed_invoke_rules::timed_invoke_rules()
{
}

timed_invoke_rules::timed_invoke_rules(timed_invoke_rules&& other)
    : super(std::move(other.m_list)), m_ti(std::move(other.m_ti))
{
}

timed_invoke_rules::timed_invoke_rules(detail::timed_invokable_ptr&& arg)
    : super(), m_ti(std::move(arg))
{
}

timed_invoke_rules::timed_invoke_rules(invokable_list&& lhs,
                                       timed_invoke_rules&& rhs)
    : super(std::move(lhs)), m_ti(std::move(rhs.m_ti))
{
    m_list.splice(m_list.begin(), rhs.m_list);
}

timed_invoke_rules& timed_invoke_rules::operator=(timed_invoke_rules&& other)
{
    m_list = std::move(other.m_list);
    other.m_list.clear();
    std::swap(m_ti, other.m_ti);
    return *this;
}

const util::duration& timed_invoke_rules::timeout() const
{
    return (m_ti != nullptr) ? m_ti->timeout() : default_timeout;
}

void timed_invoke_rules::handle_timeout() const
{
    // safe, because timed_invokable ignores the given argument
    if (m_ti != nullptr) m_ti->invoke(*static_cast<any_tuple*>(nullptr));
}

// invoke_rules

invoke_rules::invoke_rules(invokable_list&& ll) : super(std::move(ll))
{
}

invoke_rules::invoke_rules(invoke_rules&& other) : super(std::move(other.m_list))
{
}

invoke_rules::invoke_rules(std::unique_ptr<detail::invokable>&& arg)
{
    if (arg) m_list.push_back(std::move(arg));
}

invoke_rules& invoke_rules::splice(invokable_list&& ilist)
{
    m_list.splice(m_list.end(), ilist);
    return *this;
}

invoke_rules& invoke_rules::splice(invoke_rules&& other)
{
    return splice(std::move(other.m_list));
}

timed_invoke_rules invoke_rules::splice(timed_invoke_rules&& other)
{
    return timed_invoke_rules(std::move(m_list), std::move(other));
}

invoke_rules invoke_rules::operator,(invoke_rules&& other)
{
    m_list.splice(m_list.end(), other.m_list);
    return std::move(m_list);
}

timed_invoke_rules invoke_rules::operator,(timed_invoke_rules&& other)
{
    return timed_invoke_rules(std::move(m_list), std::move(other));
}

invoke_rules& invoke_rules::operator=(invoke_rules&& other)
{
    m_list = std::move(other.m_list);
    other.m_list.clear();
    return *this;
}

} // namespace cppa
