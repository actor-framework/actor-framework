#ifndef INVOKE_RULES_HPP
#define INVOKE_RULES_HPP

#include <list>

#include "cppa/invoke.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/detail/invokable.hpp"
#include "cppa/detail/intermediate.hpp"

namespace cppa {

struct invoke_rules
{

	std::list<intrusive_ptr<detail::invokable>> m_list;

 public:

	invoke_rules() { }

	// equal to the move constructor
	invoke_rules(invoke_rules& other) : m_list(std::move(other.m_list))
	{
		other.m_list.clear();
	}

	invoke_rules(invoke_rules&& other) : m_list(std::move(other.m_list))
	{
		other.m_list.clear();
	}

	invoke_rules(intrusive_ptr<detail::invokable>&& arg)
	{
		if (arg) m_list.push_back(arg);
	}

	bool operator()(const any_tuple& t) const
	{
		for (auto i = m_list.begin(); i != m_list.end(); ++i)
		{
			if ((*i)->invoke(t)) return true;
		}
		return false;
	}

	intrusive_ptr<detail::intermediate> get_intermediate(const any_tuple& t) const
	{
		detail::intermediate* result;
		for (auto i = m_list.begin(); i != m_list.end(); ++i)
		{
			result = (*i)->get_intermediate(t);
			if (result) return result;
		}
		return 0;
	}

	invoke_rules& operator,(invoke_rules&& other)
	{
		for (auto i = other.m_list.begin(); i != other.m_list.end(); ++i)
		{
			if (*i) m_list.push_back(std::move(*i));
		}
		other.m_list.clear();
		return *this;
	}

};

} // namespace cppa

#endif // INVOKE_RULES_HPP
