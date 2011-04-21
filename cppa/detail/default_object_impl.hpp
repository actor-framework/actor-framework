#ifndef DEFAULT_OBJECT_IMPL_HPP
#define DEFAULT_OBJECT_IMPL_HPP

#include <sstream>

#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/util/default_object_base.hpp"

namespace cppa { namespace detail {

template<typename T>
class default_object_impl : public util::default_object_base<T>
{

	typedef util::default_object_base<T> super;

 public:

	default_object_impl(const uniform_type_info* uti, const T& val = T())
		: super(uti, val)
	{
	}

	object* copy /*[[override]]*/ () const
	{
		return new default_object_impl(this->type(), this->m_value);
	}

	std::string to_string /*[[override]]*/ () const
	{
		std::ostringstream sstr;
		sstr << this->m_value;
		return sstr.str();
	}

	void from_string /*[[override]]*/ (const std::string& str)
	{
		T tmp;
		std::istringstream istr(str);
		istr >> tmp;
		this->m_value = std::move(tmp);
	}

	void deserialize /*[[override]]*/ (deserializer& d)
	{
		d >> this->m_value;
	}

	void serialize /*[[override]]*/ (serializer& s) const
	{
		s << this->m_value;
	}

};

} } // namespace cppa::detail

#endif // DEFAULT_OBJECT_IMPL_HPP
