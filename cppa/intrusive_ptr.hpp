#ifndef INTRUSIVE_PTR_HPP
#define INTRUSIVE_PTR_HPP

#include <algorithm>
#include <stdexcept>
#include <type_traits>

#include <cppa/detail/comparable.hpp>

namespace cppa {

template<typename T>
class intrusive_ptr : detail::comparable<intrusive_ptr<T>, T*>,
					  detail::comparable<intrusive_ptr<T>>
{

	T* m_ptr;

	inline void set_ptr(T* raw_ptr)
	{
		m_ptr = raw_ptr;
		if (raw_ptr) raw_ptr->ref();
	}

 public:

	intrusive_ptr() : m_ptr(0) { }

	intrusive_ptr(T* raw_ptr) { set_ptr(raw_ptr); }

	intrusive_ptr(const intrusive_ptr& other)
	{
		set_ptr(other.m_ptr);
	}

	intrusive_ptr(intrusive_ptr&& other) : m_ptr(0)
	{
		swap(other);
	}

	template<typename Y>
	intrusive_ptr(const intrusive_ptr<Y>& other)
	{
		static_assert(std::is_convertible<Y*, T*>::value,
					  "Y* is not assignable to T*");
		set_ptr(const_cast<Y*>(other.get()));
	}

	template<typename Y>
	intrusive_ptr(intrusive_ptr<Y>&& other) : m_ptr(0)
	{
		swap(other);
	}

	~intrusive_ptr()
	{
		if (m_ptr && !m_ptr->deref())
		{
			delete m_ptr;
		}
	}

	T* get() { return m_ptr; }

	const T* get() const { return m_ptr; }

	void swap(intrusive_ptr& other)
	{
		std::swap(m_ptr, other.m_ptr);
	}

	void reset(T* new_value = 0)
	{
		if (m_ptr && !m_ptr->deref()) delete m_ptr;
		set_ptr(new_value);
	}

	template<typename Y>
	void reset(Y* new_value)
	{
		static_assert(std::is_convertible<Y*, T*>::value,
					  "Y* is not assignable to T*");
		reset(static_cast<T*>(new_value));
	}

	intrusive_ptr& operator=(const intrusive_ptr& other)
	{
		intrusive_ptr tmp(other);
		swap(tmp);
		return *this;
	}

	intrusive_ptr& operator=(intrusive_ptr&& other)
	{
		reset();
		swap(other);
		return *this;
	}

	template<typename Y>
	intrusive_ptr& operator=(const intrusive_ptr<Y>& other)
	{
		intrusive_ptr tmp(other);
		swap(tmp);
		return *this;
	}

	template<typename Y>
	intrusive_ptr& operator=(intrusive_ptr<Y>&& other)
	{
		static_assert(std::is_convertible<Y*, T*>::value,
					  "Y* is not assignable to T*");
		m_ptr = other.m_ptr;
		other.m_ptr = 0;
		return *this;
	}

	T* operator->() { return m_ptr; }

	T& operator*() { return *m_ptr; }

	const T* operator->() const { return m_ptr; }

	const T& operator*() const { return *m_ptr; }

	inline explicit operator bool() const { return m_ptr != 0; }

	inline bool equal_to(const T* ptr) const
	{
		return get() == ptr;
	}

	inline bool equal_to(const intrusive_ptr& other) const
	{
		return get() == other.get();
	}

};

template<typename X, typename Y>
bool operator==(const intrusive_ptr<X>& lhs, const intrusive_ptr<Y>& rhs)
{
	return lhs.get() == rhs.get();
}

template<typename X, typename Y>
bool operator!=(const intrusive_ptr<X>& lhs, const intrusive_ptr<Y>& rhs)
{
	return lhs.get() != rhs.get();
}

} // namespace cppa

#endif // INTRUSIVE_PTR_HPP
