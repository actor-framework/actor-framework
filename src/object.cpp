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

void* object::new_instance(const uniform_type_info* type, const void* from)
{
    return type->new_instance(from);
}

object object::copy() const
{
    return (m_value != &s_void) ? object(m_type->new_instance(m_value), m_type)
                                : object();
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
    if (m_value != &s_void) m_type->delete_instance(m_value);
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

bool object::equal_to(const object& other) const
{
    if (m_type == other.m_type)
    {
        return (m_value != &s_void) ? m_type->equal(m_value, other.m_value)
                                    : true;
    }
    return false;
}

const uniform_type_info& object::type() const
{
    return *m_type;
}

std::string object::to_string() const
{
    return "";
}

bool object::empty() const
{
    return m_value == &s_void;
}

const void* object::value() const
{
    return m_value;
}

void* object::mutable_value()
{
    return m_value;
}

} // namespace cppa
