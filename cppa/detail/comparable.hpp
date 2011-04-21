#ifndef COMPARABLE_HPP
#define COMPARABLE_HPP

namespace cppa { namespace detail {

// Barton–Nackman trick
template<typename Subclass, typename T = Subclass>
class comparable
{

	friend bool operator==(const Subclass& lhs, const T& rhs)
	{
		return lhs.compare(rhs) == 0;
	}

	friend bool operator==(const T& lhs, const Subclass& rhs)
	{
		return rhs.compare(lhs) == 0;
	}

	friend bool operator!=(const Subclass& lhs, const T& rhs)
	{
		return lhs.compare(rhs) != 0;
	}

	friend bool operator!=(const T& lhs, const Subclass& rhs)
	{
		return rhs.compare(lhs) != 0;
	}

	friend bool operator<(const Subclass& lhs, const T& rhs)
	{
		return lhs.compare(rhs) < 0;
	}

	friend bool operator>(const Subclass& lhs, const T& rhs)
	{
		return lhs.compare(rhs) > 0;
	}

	friend bool operator<(const T& lhs, const Subclass& rhs)
	{
		return rhs > lhs;
	}

	friend bool operator>(const T& lhs, const Subclass& rhs)
	{
		return rhs < lhs;
	}

	friend bool operator<=(const Subclass& lhs, const T& rhs)
	{
		return lhs.compare(rhs) <= 0;
	}

	friend bool operator>=(const Subclass& lhs, const T& rhs)
	{
		return lhs.compare(rhs) >= 0;
	}

	friend bool operator<=(const T& lhs, const Subclass& rhs)
	{
		return rhs >= lhs;
	}

	friend bool operator>=(const T& lhs, const Subclass& rhs)
	{
		return rhs <= lhs;
	}

};

template<typename Subclass>
class comparable<Subclass, Subclass>
{

	friend bool operator==(const Subclass& lhs, const Subclass& rhs)
	{
		return lhs.compare(rhs) == 0;
	}

	friend bool operator!=(const Subclass& lhs, const Subclass& rhs)
	{
		return lhs.compare(rhs) != 0;
	}

	friend bool operator<(const Subclass& lhs, const Subclass& rhs)
	{
		return lhs.compare(rhs) < 0;
	}

	friend bool operator<=(const Subclass& lhs, const Subclass& rhs)
	{
		return lhs.compare(rhs) <= 0;
	}

	friend bool operator>(const Subclass& lhs, const Subclass& rhs)
	{
		return lhs.compare(rhs) > 0;
	}

	friend bool operator>=(const Subclass& lhs, const Subclass& rhs)
	{
		return lhs.compare(rhs) >= 0;
	}

};

} } // cppa::detail

#endif // COMPARABLE_HPP
