#ifndef SINK_HPP
#define SINK_HPP

#include <cstddef>

#include "cppa/ref_counted.hpp"

namespace cppa { namespace util {

struct sink : public virtual ref_counted
{
	virtual void write(std::size_t buf_size, const void* buf) = 0;
	virtual void flush() = 0;
};

} } // namespace cppa::util

#endif // SINK_HPP
