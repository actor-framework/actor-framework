#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <string>
#include "cppa/ref_counted.hpp"

namespace cppa {

class uniform_type_info;
class serializer;
class deserializer;

class object : public ref_counted
{

public:

	// mutators
	virtual void* mutable_value() = 0;
	virtual void deserialize(deserializer&) = 0;
	virtual void from_string(const std::string&) = 0;
	// accessors
	virtual object* copy() const = 0;
	virtual const uniform_type_info* type() const = 0;
	virtual const void* value() const = 0;
	virtual std::string to_string() const = 0;
	virtual void serialize(serializer&) const = 0;
};

} // namespace cppa

#endif // OBJECT_HPP
