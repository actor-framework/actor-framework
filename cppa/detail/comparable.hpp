#ifndef COMPARABLE_HPP
#define COMPARABLE_HPP

namespace cppa { namespace detail {

template<typename Subclass, typename T = Subclass>
class comparable
{

	friend bool operator==(const Subclass& lhs, const T& rhs)
	{
		return lhs.equal_to(rhs);
	}

	friend bool operator==(const T& lhs, const Subclass& rhs)
	{
		return rhs.equal_to(lhs);
	}

	friend bool operator!=(const Subclass& lhs, const T& rhs)
	{
		return !lhs.equal_to(rhs);
	}

	friend bool operator!=(const T& lhs, const Subclass& rhs)
	{
		return !rhs.equal_to(lhs);
	}

};

template<typename Subclass>
class comparable<Subclass, Subclass>
{

	friend bool operator==(const Subclass& lhs, const Subclass& rhs)
	{
		return lhs.equal_to(rhs);
	}

	friend bool operator!=(const Subclass& lhs, const Subclass& rhs)
	{
		return !lhs.equal_to(rhs);
	}

};

} } // cppa::detail

#endif // COMPARABLE_HPP
