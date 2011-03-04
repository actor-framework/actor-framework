#ifndef ABSTRACT_TYPE_LIST_HPP
#define ABSTRACT_TYPE_LIST_HPP

#include "cppa/util/utype_iterator.hpp"

namespace cppa { class serializer; }

namespace cppa { namespace util {

struct abstract_type_list
{

	typedef utype_iterator iterator;

	typedef utype_iterator const_iterator;

	virtual abstract_type_list* copy() const = 0;

	virtual const_iterator begin() const = 0;

	virtual const_iterator end() const = 0;

	virtual const utype& at(std::size_t pos) const = 0;

};

} } // namespace cppa::util

#endif // ABSTRACT_TYPE_LIST_HPP
