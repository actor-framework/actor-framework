#ifndef OBJECT_HPP
#define OBJECT_HPP

#include "cppa/ref_counted.hpp"

namespace cppa {

struct utype;
class serializer;
class deserializer;

struct object : public ref_counted
{
	// mutators
	virtual void* mutable_value() = 0;
	virtual void deserialize(deserializer&) = 0;
	// accessors
	virtual object* copy() const = 0;
	virtual const utype& type() const = 0;
	virtual const void* value() const = 0;
	virtual void serialize(serializer&) const = 0;
};

} // namespace cppa

#endif // OBJECT_HPP
