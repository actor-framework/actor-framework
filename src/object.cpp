#include <algorithm>

#include "cppa/object.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/void_type.hpp"

namespace {

cppa::util::void_type s_void;

} // namespace <anonymous>

namespace cppa {

void object::swap(object& other)
{
	std::swap(m_value, other.m_value);
	std::swap(m_type, other.m_type);
}

object object::copy() const
{
	return (m_value) ? m_type->copy(*this) : object();
}


object::object(void* val, const uniform_type_info* utype)
	: m_value(val), m_type(utype)
{
	if (val && !utype)
	{
		throw std::invalid_argument("val && !utype");
	}
}

object::object() : m_value(&s_void), m_type(uniform_typeid<util::void_type>())
{
}

object::~object()
{
	if (m_value != &s_void) m_type->destroy(*this);
}


object::object(const object& other) : m_value(&s_void), m_type(uniform_typeid<util::void_type>())
{
	object tmp = other.copy();
	swap(tmp);
}

object::object(object&& other) : m_value(&s_void), m_type(uniform_typeid<util::void_type>())
{
	swap(other);
}

object& object::operator=(object&& other)
{
	object tmp(std::move(other));
	swap(tmp);
	return *this;
}

object& object::operator=(const object& other)
{
	object tmp = other.copy();
	swap(tmp);
	return *this;
}

bool object::equal(const object& other) const
{
	return m_type->equal(*this, other);
}

const uniform_type_info& object::type() const
{
	return *m_type;
}

std::string object::to_string() const
{
	return m_type->to_string(*this);
}

} // namespace cppa
