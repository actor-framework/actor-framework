#include "cppa/untyped_tuple.hpp"

namespace {

struct empty_type_list : cppa::util::abstract_type_list
{

	virtual abstract_type_list* copy() const
	{
		return new empty_type_list;
	}

	virtual const_iterator begin() const { return 0; }

	virtual const_iterator end() const { return 0; }

	virtual const cppa::utype& at(std::size_t) const
	{
		throw std::range_error("empty_type_list::at()");
	}

};

struct empty_tuple : cppa::detail::abstract_tuple
{

	empty_type_list m_types;

	virtual std::size_t size() const { return 0; }

	virtual abstract_tuple* copy() const { return new empty_tuple; }

	virtual void* mutable_at(std::size_t)
	{
		throw std::range_error("empty_tuple::mutable_at()");
	}

	virtual const void* at(std::size_t) const
	{
		throw std::range_error("empty_tuple::at()");
	}

	virtual const cppa::utype& utype_at(std::size_t) const
	{
		throw std::range_error("empty_tuple::utype_at()");
	}

	virtual const cppa::util::abstract_type_list& types() const
	{
		return m_types;
	}

	virtual bool equal_to(const abstract_tuple& other) const
	{
		return other.size() == 0;
	}

	virtual void serialize(cppa::serializer&) const
	{
	}

};

} // namespace <anonymous>

namespace cppa {

untyped_tuple::untyped_tuple() : m_vals(new empty_tuple) { }

} // namespace cppa
