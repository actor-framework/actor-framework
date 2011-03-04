#ifndef COW_PTR_HPP
#define COW_PTR_HPP

#include <stdexcept>
#include "cppa/util/detach.hpp"
#include "cppa/intrusive_ptr.hpp"

namespace cppa {

template<typename T>
class cow_ptr
{

	intrusive_ptr<T> m_ptr;

	T* do_detach()
	{
		T* ptr = m_ptr.get();
		if (!ptr->unique())
		{
			T* new_ptr = util::detach(ptr);
			cow_ptr tmp(new_ptr);
			swap(tmp);
			return new_ptr;
		}
		return ptr;
	}

 public:

	template<typename Y>
	cow_ptr(Y* raw_ptr) : m_ptr(raw_ptr) { }

	cow_ptr(T* raw_ptr) : m_ptr(raw_ptr) { }

	cow_ptr(const cow_ptr& other) : m_ptr(other.m_ptr) { }

	template<typename Y>
	cow_ptr(const cow_ptr<Y>& other) : m_ptr(const_cast<Y*>(other.get())) { }

	const T* get() const { return m_ptr.get(); }

	inline void swap(cow_ptr& other)
	{
		m_ptr.swap(other.m_ptr);
	}

	cow_ptr& operator=(const cow_ptr& other)
	{
		cow_ptr tmp(other);
		swap(tmp);
		return *this;
	}

	template<typename Y>
	cow_ptr& operator=(const cow_ptr<Y>& other)
	{
		cow_ptr tmp(other);
		swap(tmp);
		return *this;
	}

	T* operator->() { return do_detach(); }

	T& operator*() { return do_detach(); }

	const T* operator->() const { return m_ptr.get(); }

	const T& operator*() const { return *m_ptr.get(); }

	explicit operator bool() const { return m_ptr.get() != 0; }

};

} // namespace cppa

#endif // COW_PTR_HPP
