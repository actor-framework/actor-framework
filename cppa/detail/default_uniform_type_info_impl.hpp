#ifndef DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP
#define DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP

#include <sstream>

#include "cppa/uniform_type_info.hpp"

namespace cppa { namespace detail {

template<typename T>
class default_uniform_type_info_impl : public uniform_type_info
{

	inline T& to_ref(object& what) const
	{
		return *reinterpret_cast<T*>(this->value_of(what));
	}

	inline const T& to_ref(const object& what) const
	{
		return *reinterpret_cast<const T*>(this->value_of(what));
	}

 protected:

	object copy(const object& what) const
	{
		return { new T(to_ref(what)), this };
	}

	object from_string(const std::string& str) const
	{
		std::istringstream istr(str);
		T tmp;
		istr >> tmp;
		return { new T(tmp), this };
	}

	void destroy(object& what) const
	{
		delete reinterpret_cast<T*>(value_of(what));
		value_of(what) = nullptr;
	}

	void deserialize(deserializer& d, object& what) const
	{
		d >> to_ref(what);
	}

	std::string to_string(const object& obj) const
	{
		std::ostringstream ostr;
		ostr << to_ref(obj);
		return ostr.str();
	}

	bool equal(const object& lhs, const object& rhs) const
	{
		return (lhs.type() == *this && rhs.type() == *this)
			   ? to_ref(lhs) == to_ref(rhs)
			   : false;
	}

	void serialize(serializer& s, const object& what) const
	{
		s << to_ref(what);
	}

 public:

	default_uniform_type_info_impl() : cppa::uniform_type_info(typeid(T)) { }

	object create() const
	{
		return { new T(), this };
	}

};

} } // namespace detail

#endif // DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP
