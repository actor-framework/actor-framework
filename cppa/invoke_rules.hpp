#ifndef INVOKE_RULES_HPP
#define INVOKE_RULES_HPP

#include <list>
#include <memory>

#include "cppa/invoke.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/detail/invokable.hpp"
#include "cppa/detail/intermediate.hpp"

namespace cppa {

struct invoke_rules
{

    typedef std::list< std::unique_ptr<detail::invokable> > list_type;

    list_type m_list;

    invoke_rules(const invoke_rules&) = delete;
    invoke_rules& operator=(const invoke_rules&) = delete;

    invoke_rules(list_type&& ll) : m_list(std::move(ll)) { }

 public:

    invoke_rules() { }

    invoke_rules(invoke_rules&& other) : m_list(std::move(other.m_list))
    {
    }

    invoke_rules(detail::invokable* arg)
    {
        if (arg) m_list.push_back(std::unique_ptr<detail::invokable>(arg));
    }

    invoke_rules(std::unique_ptr<detail::invokable>&& arg)
    {
        if (arg) m_list.push_back(std::move(arg));
    }

    bool operator()(const any_tuple& t) const
    {
        for (auto i = m_list.begin(); i != m_list.end(); ++i)
        {
            if ((*i)->invoke(t)) return true;
        }
        return false;
    }

    detail::intermediate* get_intermediate(const any_tuple& t) const
    {
        detail::intermediate* result;
        for (auto i = m_list.begin(); i != m_list.end(); ++i)
        {
            result = (*i)->get_intermediate(t);
            if (result != nullptr) return result;
        }
        return nullptr;
    }

    invoke_rules& append(invoke_rules&& other)
    {
        m_list.splice(m_list.end(), other.m_list);
        return *this;
    }

    invoke_rules operator,(invoke_rules&& other)
    {
        m_list.splice(m_list.end(), other.m_list);
        return std::move(m_list);
    }

};

} // namespace cppa

#endif // INVOKE_RULES_HPP
