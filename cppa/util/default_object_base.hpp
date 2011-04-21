#ifndef DEFAULT_OBJECT_BASE_HPP
#define DEFAULT_OBJECT_BASE_HPP

#include "cppa/object.hpp"
#include "cppa/uniform_type_info.hpp"

namespace cppa { namespace util {

template<typename T>
class default_object_base : public object
{

	const uniform_type_info* m_type;

 protected:

	T m_value;

 public:

	default_object_base(const uniform_type_info* uti, const T& val = T())
		: m_type(uti), m_value(val)
	{
	}

	// mutators
	void* mutable_value /*[[override]]*/ ()
	{
		return &m_value;
	}

	const uniform_type_info* type /*[[override]]*/ () const
	{
		return m_type;
	}

	const void* value /*[[override]]*/ () const
	{
		return &m_value;
	}

};

} } // namespace cppa::util

#endif // DEFAULT_OBJECT_BASE_HPP
