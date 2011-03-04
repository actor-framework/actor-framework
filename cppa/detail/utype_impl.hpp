#ifndef UTYPE_IMPL_HPP
#define UTYPE_IMPL_HPP

#include <map>
#include <string>

#include "cppa/utype.hpp"

#include "cppa/detail/object_impl.hpp"

namespace cppa { namespace detail {

std::string demangle(const char*);

std::map<std::string, utype*>& uniform_types();

template<typename T>
class utype_impl : public utype
{

	const std::string m_name;

	const std::type_info& m_native;

	utype_impl() : m_name(demangle(typeid(T).name())), m_native(typeid(T))
	{
		(uniform_types())[m_name] = this;
	}

 public:

	static const utype_impl instance;

	std::uint8_t announce_helper() const { return 42; }

	virtual object* create() const
	{
		return new obj_impl<T>;
	}

	virtual const std::string& name() const
	{
		return m_name;
	}

	virtual const std::type_info& native() const
	{
		return m_native;
	}

};

template<typename A> const utype_impl<A> utype_impl<A>::instance;

} } // namespace cppa::detail

#endif // UTYPE_IMPL_HPP
