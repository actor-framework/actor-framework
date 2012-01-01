#ifndef COMPARABLE_HPP
#define COMPARABLE_HPP

namespace cppa { namespace util {

/**
 * @brief Barton–Nackman trick implementation.
 *
 * @p Subclass must provide a @c compare member function that compares
 * to instances of @p T and returns an integer @c x with:
 * - <tt>x < 0</tt> if <tt>this < other</tt>
 * - <tt>x > 0</tt> if <tt>this > other</tt>
 * - <tt>x == 0</tt> if <tt>this == other</tt>
 */
template<typename Subclass, typename T = Subclass>
class comparable
{

	friend bool operator==(Subclass const& lhs, T const& rhs)
	{
		return lhs.compare(rhs) == 0;
	}

	friend bool operator==(T const& lhs, Subclass const& rhs)
	{
		return rhs.compare(lhs) == 0;
	}

	friend bool operator!=(Subclass const& lhs, T const& rhs)
	{
		return lhs.compare(rhs) != 0;
	}

	friend bool operator!=(T const& lhs, Subclass const& rhs)
	{
		return rhs.compare(lhs) != 0;
	}

	friend bool operator<(Subclass const& lhs, T const& rhs)
	{
		return lhs.compare(rhs) < 0;
	}

	friend bool operator>(Subclass const& lhs, T const& rhs)
	{
		return lhs.compare(rhs) > 0;
	}

	friend bool operator<(T const& lhs, Subclass const& rhs)
	{
		return rhs > lhs;
	}

	friend bool operator>(T const& lhs, Subclass const& rhs)
	{
		return rhs < lhs;
	}

	friend bool operator<=(Subclass const& lhs, T const& rhs)
	{
		return lhs.compare(rhs) <= 0;
	}

	friend bool operator>=(Subclass const& lhs, T const& rhs)
	{
		return lhs.compare(rhs) >= 0;
	}

	friend bool operator<=(T const& lhs, Subclass const& rhs)
	{
		return rhs >= lhs;
	}

	friend bool operator>=(T const& lhs, Subclass const& rhs)
	{
		return rhs <= lhs;
	}

};

template<typename Subclass>
class comparable<Subclass, Subclass>
{

	friend bool operator==(Subclass const& lhs, Subclass const& rhs)
	{
		return lhs.compare(rhs) == 0;
	}

	friend bool operator!=(Subclass const& lhs, Subclass const& rhs)
	{
		return lhs.compare(rhs) != 0;
	}

	friend bool operator<(Subclass const& lhs, Subclass const& rhs)
	{
		return lhs.compare(rhs) < 0;
	}

	friend bool operator<=(Subclass const& lhs, Subclass const& rhs)
	{
		return lhs.compare(rhs) <= 0;
	}

	friend bool operator>(Subclass const& lhs, Subclass const& rhs)
	{
		return lhs.compare(rhs) > 0;
	}

	friend bool operator>=(Subclass const& lhs, Subclass const& rhs)
	{
		return lhs.compare(rhs) >= 0;
	}

};

} } // cppa::util

#endif // COMPARABLE_HPP
