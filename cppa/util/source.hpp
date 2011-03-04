#ifndef SOURCE_HPP
#define SOURCE_HPP

#include <cstddef>

#include "cppa/ref_counted.hpp"

namespace cppa { namespace util {

struct source : public virtual ref_counted
{
	virtual std::size_t read_some(std::size_t buf_size, void* buf) = 0;
	virtual void read(std::size_t buf_size, void* buf) = 0;
};

} } // namespace cppa::util

#endif // SOURCE_HPP
