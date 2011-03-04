#ifndef INTRUSIVE_PTR_HPP
#define INTRUSIVE_PTR_HPP

#include <algorithm>
#include <stdexcept>

namespace cppa {

template<typename T>
class intrusive_ptr
{

	T* m_ptr;

	inline void init(T* raw_ptr)
	{
		m_ptr = raw_ptr;
		if (raw_ptr) raw_ptr->ref();
	}

 public:

	intrusive_ptr() : m_ptr(0) { }

	template<typename Y>
	intrusive_ptr(Y* raw_ptr) { init(raw_ptr); }

	intrusive_ptr(T* raw_ptr) { init(raw_ptr); }

	intrusive_ptr(intrusive_ptr&& other) : m_ptr(other.m_ptr)
	{
		other.m_ptr = 0;
	}

	intrusive_ptr(const intrusive_ptr& other) { init(other.m_ptr); }

	template<typename Y>
	intrusive_ptr(const intrusive_ptr<Y>& other)
	{
		init(const_cast<Y*>(other.get()));
	}

	~intrusive_ptr() { if (m_ptr && !m_ptr->deref()) delete m_ptr; }

	T* get() { return m_ptr; }

	const T* get() const { return m_ptr; }

	void swap(intrusive_ptr& other)
	{
		std::swap(m_ptr, other.m_ptr);
	}

	intrusive_ptr& operator=(const intrusive_ptr& other)
	{
		intrusive_ptr tmp(other);
		swap(tmp);
		return *this;
	}

	template<typename Y>
	intrusive_ptr& operator=(const intrusive_ptr<Y>& other)
	{
		intrusive_ptr tmp(other);
		swap(tmp);
		return *this;
	}

	T* operator->() { return m_ptr; }

	T& operator*() { return *m_ptr; }

	const T* operator->() const { return m_ptr; }

	const T& operator*() const { return *m_ptr; }

	inline explicit operator bool() const { return m_ptr != 0; }

};

template<typename X, typename Y>
bool operator==(const intrusive_ptr<X>& lhs, const Y* rhs)
{
	return lhs.get() == rhs;
}

template<typename X, typename Y>
bool operator==(const X* lhs, const intrusive_ptr<Y>& rhs)
{
	return lhs == rhs.get();
}

template<typename X, typename Y>
bool operator==(const intrusive_ptr<X>& lhs, const intrusive_ptr<Y>& rhs)
{
	return lhs.get() == rhs.get();
}

template<typename X, typename Y>
bool operator!=(const intrusive_ptr<X>& lhs, const Y* rhs)
{
	return !(lhs == rhs);
}

template<typename X, typename Y>
bool operator!=(const X* lhs, const intrusive_ptr<Y>& rhs)
{
	return !(lhs == rhs);
}

template<typename X, typename Y>
bool operator!=(const intrusive_ptr<X>& lhs, const intrusive_ptr<Y>& rhs)
{
	return !(lhs == rhs);
}

template<typename X>
bool operator==(const intrusive_ptr<X>& lhs, const X* rhs)
{
	return lhs.get() == rhs;
}

template<typename X>
bool operator==(const X* lhs, const intrusive_ptr<X>& rhs)
{
	return lhs == rhs.get();
}

template<typename X>
bool operator==(const intrusive_ptr<X>& lhs, const intrusive_ptr<X>& rhs)
{
	return lhs.get() == rhs.get();
}

template<typename X>
bool operator!=(const intrusive_ptr<X>& lhs, const X* rhs)
{
	return !(lhs == rhs);
}

template<typename X>
bool operator!=(const X* lhs, const intrusive_ptr<X>& rhs)
{
	return !(lhs == rhs);
}

template<typename X>
bool operator!=(const intrusive_ptr<X>& lhs, const intrusive_ptr<X>& rhs)
{
	return !(lhs == rhs);
}

} // namespace cppa

#endif // INTRUSIVE_PTR_HPP
