#include <stdexcept>
#include "cppa/detail/decorated_tuple.hpp"

/*
namespace cppa { namespace detail {

decorated_tuple::decorated_tuple(const decorated_tuple::ptr_type& d,
								 const std::vector<std::size_t>& v,
								 util::abstract_type_list* tlist)
	: m_decorated(d), m_mappings(v), m_types(tlist)
{
	if (!tlist)
	{
		throw std::invalid_argument("tlist == nullpr");
	}
}

decorated_tuple::decorated_tuple(const decorated_tuple& other)
	: abstract_tuple()
	, m_decorated(other.m_decorated)
	, m_mappings(other.m_mappings)
	, m_types(other.m_types->copy())
{
}

decorated_tuple::~decorated_tuple()
{
	delete m_types;
}

void* decorated_tuple::mutable_at(std::size_t pos)
{
	return m_decorated->mutable_at(m_mappings[pos]);
}

std::size_t decorated_tuple::size() const
{
	return m_mappings.size();
}

decorated_tuple* decorated_tuple::copy() const
{
	return new decorated_tuple(*this);
}

const void* decorated_tuple::at(std::size_t pos) const
{
	return m_decorated->at(m_mappings[pos]);
}

const utype& decorated_tuple::utype_at(std::size_t pos) const
{
	return m_decorated->utype_at(m_mappings[pos]);
}

const util::abstract_type_list& decorated_tuple::types() const
{
	return *m_types;
//	throw std::runtime_error("not implemented yet");
}

bool decorated_tuple::equal_to(const abstract_tuple&) const
{
	return false;
}

} } // namespace cppa::detail

*/
