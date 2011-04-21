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

} } // cppa::util

#endif // COMPARABLE_HPP
