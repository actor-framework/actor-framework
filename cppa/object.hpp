#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <string>
#include <typeinfo>
#include <stdexcept>

namespace cppa {

class object;
class uniform_type_info;

template<typename T>
T& object_cast(object& obj);

template<typename T>
const T& object_cast(const object& obj);

/**
 * @brief foobar.
 */
class object
{

	friend class uniform_type_info;

	template<typename T>
	friend T& object_cast(object& obj);

	template<typename T>
	friend const T& object_cast(const object& obj);

	void* m_value;
	const uniform_type_info* m_type;

	void swap(object& other);

	object copy() const;

public:

	/**
	 * @brief Create an object of type @p utinfo with value @p val.
	 * @warning {@link object} takes ownership of @p val.
	 * @pre {@code val != nullptr && utinfo != nullptr}
	 */
	object(void* val, const uniform_type_info* utinfo);

	/**
	 * @brief Create a void object.
	 * @post {@code type() == uniform_typeid<util::void_type>()}
	 */
	object();

	~object();

	/**
	 * @brief Create an object and move type and value from @p other to this.
	 * @post {@code other.type() == uniform_typeid<util::void_type>()}
	 */
	object(object&& other);

	/**
	 * @brief Create a copy of @p other.
	 */
	object(const object& other);

	/**
	 * @brief Move the content from @p other to this.
	 * @post {@code other.type() == nullptr}
	 */
	object& operator=(object&& other);
	object& operator=(const object& other);

	bool equal(const object& other) const;

	const uniform_type_info& type() const;

	std::string to_string() const;

};

// forward declaration of the == operator
bool operator==(const uniform_type_info& lhs, const std::type_info& rhs);

template<typename T>
T& object_cast(object& obj)
{
//	if (!(obj.type() == typeid(T)))
	if (!operator==(obj.type(), typeid(T)))
	{
		throw std::logic_error("object type doesnt match T");
	}
	return *reinterpret_cast<T*>(obj.m_value);
}

template<typename T>
const T& object_cast(const object& obj)
{
	if (!(obj.type() == typeid(T)))
	{
		throw std::logic_error("object type doesnt match T");
	}
	return *reinterpret_cast<const T*>(obj.m_value);
}

} // namespace cppa

inline bool operator==(const cppa::object& lhs, const cppa::object& rhs)
{
	return lhs.equal(rhs);
}

inline bool operator!=(const cppa::object& lhs, const cppa::object& rhs)
{
	return !(lhs == rhs);
}

#endif // OBJECT_HPP
