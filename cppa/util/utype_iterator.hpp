#ifndef UTYPE_ITERATOR_HPP
#define UTYPE_ITERATOR_HPP

#include "cppa/uniform_type_info.hpp"

namespace cppa { namespace util {

struct utype_iterator
{

	typedef const utype* const* arr_ptr;

	arr_ptr m_pos;

 public:

	utype_iterator(arr_ptr p) : m_pos(p) { }

	inline utype_iterator& operator++()
	{
		++m_pos;
		return *this;
	}

	inline bool operator==(const utype_iterator& other) const
	{
		return m_pos == other.m_pos;
	}

	inline bool operator!=(const utype_iterator& other) const
	{
		return !(*this == other);
	}

	inline const utype& operator*()
	{
		return *m_pos[0];
	}

	inline const utype* operator->()
	{
		return m_pos[0];
	}

};

} } // namespace cppa::util

#endif // UTYPE_ITERATOR_HPP
