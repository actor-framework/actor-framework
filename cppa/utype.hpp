#ifndef UTYPE_HPP
#define UTYPE_HPP

#include <string>
#include <typeinfo>

#include "cppa/object.hpp"

namespace cppa {

/**
 * @brief Uniform type information.
 */
struct utype
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

};

} // namespace cppa

#endif // UTYPE_HPP
