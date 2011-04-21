#ifndef DECORATED_TUPLE_HPP
#define DECORATED_TUPLE_HPP

#include <vector>

#include "cppa/cow_ptr.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/type_list.hpp"

#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/detail/serialize_tuple.hpp"

namespace cppa { namespace detail {

template<typename... ElementTypes>
class decorated_tuple : public abstract_tuple
{

 public:

	typedef util::type_list<ElementTypes...> element_types;

	typedef cow_ptr<abstract_tuple> ptr_type;

	decorated_tuple(const ptr_type& d,
					const std::vector<std::size_t>& v)
		: m_decorated(d), m_mappings(v)
	{
	}

	virtual void* mutable_at(std::size_t pos)
	{
		return m_decorated->mutable_at(m_mappings[pos]);
	}

	virtual std::size_t size() const
	{
		return m_mappings.size();
	}

	virtual decorated_tuple* copy() const
	{
		return new decorated_tuple(*this);
	}

	virtual const void* at(std::size_t pos) const
	{
		return m_decorated->at(m_mappings[pos]);
	}

	virtual const uniform_type_info* utype_at(std::size_t pos) const
	{
		return m_decorated->utype_at(m_mappings[pos]);
	}

	virtual const util::abstract_type_list& types() const
	{
		return m_types;
	}

	virtual bool equal_to(const abstract_tuple&) const
	{
		return false;
	}

	virtual void serialize(serializer& s) const
	{
		s << static_cast<std::uint8_t>(element_types::type_list_size);
		serialize_tuple<element_types>::_(s, this);
	}

 private:

	ptr_type m_decorated;
	std::vector<std::size_t> m_mappings;
	element_types m_types;

	decorated_tuple(const decorated_tuple& other)
		: abstract_tuple()
		, m_decorated(other.m_decorated)
		, m_mappings(other.m_mappings)
	{
	}

	decorated_tuple& operator=(const decorated_tuple&) = delete;

};

} } // namespace cppa::detail

#endif // DECORATED_TUPLE_HPP
