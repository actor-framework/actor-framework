#ifndef SINGLY_LINKED_LIST_HPP
#define SINGLY_LINKED_LIST_HPP

#include <utility>

namespace cppa { namespace util {

template<typename T>
class singly_linked_list
{

	T* m_head;
	T* m_tail;

 public:

	typedef T element_type;

	singly_linked_list() : m_head(0), m_tail(0) { }

	inline bool empty() const { return m_head == 0; }

	void push_back(element_type* what)
	{
		what->next = 0;
		if (m_tail)
		{
			m_tail->next = what;
			m_tail = what;
		}
		else
		{
			m_head = m_tail = what;
		}
	}

	std::pair<element_type*, element_type*> take()
	{
		element_type* first = m_head;
		element_type* last = m_tail;
		m_head = m_tail = 0;
		return { first, last };
	}

};

} } // namespace cppa::util

#endif // SINGLY_LINKED_LIST_HPP
