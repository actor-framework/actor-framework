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
    if (other.value() != &s_void)
    {
        m_value = other.m_type->new_instance(other.m_value);
        m_type = other.m_type;
    }
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
    // use copy ctor and then swap
    object tmp(other);
    swap(tmp);
    return *this;
}

bool operator==(const object& lhs, const object& rhs)
{
    if (lhs.type() == rhs.type())
    {
        // values might both point to s_void if lhs and rhs are "empty"
        return    lhs.value() == rhs.value()
               || lhs.type().equals(lhs.value(), rhs.value());
    }
    return false;
}

const uniform_type_info& object::type() const
{
    return *m_type;
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
