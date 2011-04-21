#ifndef ABSTRACT_TYPE_LIST_HPP
#define ABSTRACT_TYPE_LIST_HPP

#include <iterator>
#include "cppa/uniform_type_info.hpp"

namespace cppa { class serializer; }

namespace cppa { namespace util {

struct abstract_type_list
{

	class const_iterator : public std::iterator<std::bidirectional_iterator_tag,
												const uniform_type_info*>
	{

		const uniform_type_info* const* p;

	 public:

		inline const_iterator(const uniform_type_info* const* x = nullptr) : p(x) { }

		const_iterator(const_iterator&&) = default;
		const_iterator(const const_iterator&) = default;

		const_iterator& operator=(const_iterator&&) = default;
		const_iterator& operator=(const const_iterator&) = default;

		inline bool operator==(const const_iterator& other) const
		{
			return p == other.p;
		}

		inline bool operator!=(const const_iterator& other) const
		{
			return p != other.p;
		}

		inline const uniform_type_info* operator*()
		{
			return *p;
		}

		inline const uniform_type_info* const* operator->()
		{
			return p;
		}

		inline const_iterator& operator++()
		{
			++p;
			return *this;
		}

		inline const_iterator operator++(int)
		{
			const_iterator tmp(*this);
			operator++();
			return tmp;
		}

		inline const_iterator& operator--()
		{
			--p;
			return *this;
		}

		inline const_iterator operator--(int)
		{
			const_iterator tmp(*this);
			operator--();
			return tmp;
		}

	};

	virtual abstract_type_list* copy() const = 0;

	virtual const_iterator begin() const = 0;

	virtual const_iterator end() const = 0;

	virtual const uniform_type_info* at(std::size_t pos) const = 0;

};

} } // namespace cppa::util

#endif // ABSTRACT_TYPE_LIST_HPP
