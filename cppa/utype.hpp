#ifndef UTYPE_HPP
#define UTYPE_HPP

#include <string>
#include <typeinfo>

#include "cppa/object.hpp"
#include "cppa/detail/comparable.hpp"

namespace cppa {

/**
 * @brief Uniform type information.
 */
struct utype : detail::comparable<utype>,
			   detail::comparable<utype, std::type_info>
{

	/**
	 * @brief Create an object of this type, initialized with
	 *        the default constructor.
	 */
	virtual object* create() const = 0;

	/**
	 * @brief Get the uniform type name (equal on all supported plattforms).
	 */
	virtual const std::string& name() const = 0;

	/**
	 * @brief Get the result of typeid(T) where T is the native type.
	 */
	virtual const std::type_info& native() const = 0;

	inline bool equal_to(const std::type_info& what) const
	{
		return native() == what;
	}

	inline bool equal_to(const utype& what) const
	{
		return native() == what.native();
	}

};

} // namespace cppa

#endif // UTYPE_HPP
