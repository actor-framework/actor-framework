#ifndef BUFFER_HPP
#define BUFFER_HPP

#include <ios> // std::ios_base::failure
#include <iostream>

#include <string.h>

#include "cppa/detail/native_socket.hpp"

namespace cppa { namespace detail {

template<size_t ChunkSize, size_t MaxBufferSize, typename DataType = char>
class buffer
{

    DataType* m_data;
    size_t m_written;
    size_t m_allocated;
    size_t m_final_size;

    template<typename F>
    bool append_impl(F&& fun, bool throw_on_error)
    {
        auto recv_result = fun();
        if (recv_result == 0)
        {
            // connection closed
            if (throw_on_error)
            {
                std::ios_base::failure("cannot read from a closed pipe/socket");
            }
            return false;
        }
        else if (recv_result < 0)
        {
            switch (errno)
            {
                case EAGAIN:
                {
                    // rdflags or sfd is set to non-blocking,
                    // this is not treated as error
                    return true;
                }
                default:
                {
                    // a "real" error occured;
                    if (throw_on_error)
                    {
                        char* cstr = strerror(errno);
                        std::string errmsg = cstr;
                        free(cstr);
                        throw std::ios_base::failure(std::move(errmsg));
                    }
                    return false;
                }
            }
        }
        inc_written(static_cast<size_t>(recv_result));
        return true;
    }

 public:

    buffer() : m_data(nullptr), m_written(0), m_allocated(0), m_final_size(0)
    {
    }

    buffer(buffer&& other)
        : m_data(other.m_data), m_written(other.m_written)
        , m_allocated(other.m_allocated), m_final_size(other.m_final_size)
    {
        other.m_data = nullptr;
        other.m_written = other.m_allocated = other.m_final_size = 0;
    }

    ~buffer()
    {
        delete[] m_data;
    }

    void clear()
    {
        m_written = 0;
    }

    void reset(size_t new_final_size = 0)
    {
        m_written = 0;
        m_final_size = new_final_size;
        if (new_final_size > m_allocated)
        {
            if (new_final_size > MaxBufferSize)
            {
                throw std::ios_base::failure("maximum buffer size exceeded");
            }
            auto remainder = (new_final_size % ChunkSize);
            if (remainder == 0)
            {
                m_allocated = new_final_size;
            }
            else
            {
                m_allocated = (new_final_size - remainder) + ChunkSize;
            }
            delete[] m_data;
            m_data = new DataType[m_allocated];
        }
    }

    bool ready()
    {
        return m_written == m_final_size;
    }

    // pointer to the current write position
    DataType* wr_ptr()
    {
        return m_data + m_written;
    }

    size_t size()
    {
        return m_written;
    }

    size_t final_size()
    {
        return m_final_size;
    }

    size_t remaining()
    {
        return m_final_size - m_written;
    }

    void inc_written(size_t value)
    {
        m_written += value;
    }

    DataType* data()
    {
        return m_data;
    }

    bool append_from_file_descriptor(int fd, bool throw_on_error = false)
    {
        auto _this = this;
        auto fun = [_this, fd]() -> int
        {
            return ::read(fd, _this->wr_ptr(), _this->remaining());
        };
        return append_impl(fun, throw_on_error);
    }

    bool append_from(native_socket_t sfd, int rdflags,
                     bool throw_on_error = false)
    {
        auto _this = this;
        auto fun = [_this, sfd, rdflags]() -> int
        {
            return ::recv(sfd, _this->wr_ptr(), _this->remaining(), rdflags);
        };
        return append_impl(fun, throw_on_error);
    }

};

} } // namespace cppa::detail

#endif // BUFFER_HPP
