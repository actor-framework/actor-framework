#ifndef EITHER_HPP
#define EITHER_HPP

#include <new>
#include <stdexcept>
#include <type_traits>

namespace cppa { namespace util {

template<class Left, class Right>
class either
{

    static_assert(std::is_same<Left, Right>::value == false, "Left == Right");

    union
    {
        Left m_left;
        Right m_right;
    };

    bool m_is_left;

    void check_flag(bool flag, const char* side)
    {
        if (m_is_left != flag)
        {
            std::string err = "not a ";
            err += side;
            throw std::runtime_error(err);
        }
    }

    void destroy()
    {
        if (m_is_left)
        {
            m_left.~Left();
        }
        else
        {
            m_right.~Right();
        }
    }

    template<typename L>
    void cr_left(L&& value)
    {
        new (&m_left) Left (std::forward<L>(value));
    }

    template<typename R>
    void cr_right(R&& value)
    {
        new (&m_right) Right (std::forward<R>(value));
    }

 public:

    // default constructor creates a left
    either() : m_is_left(true)
    {
        new (&m_left) Left ();
    }

    either(const Left& value) : m_is_left(true)
    {
        cr_left(value);
    }

    either(Left&& value) : m_is_left(true)
    {
        cr_left(std::move(value));
    }

    either(const Right& value) : m_is_left(false)
    {
        cr_right(value);
    }

    either(Right&& value) : m_is_left(false)
    {
        cr_right(std::move(value));
    }

    either(const either& other) : m_is_left(other.m_is_left)
    {
        if (other.m_is_left)
        {
            cr_left(other.left());
        }
        else
        {
            cr_right(other.right());
        }
    }

    either(either&& other) : m_is_left(other.m_is_left)
    {
        if (other.m_is_left)
        {
            cr_left(std::move(other.left()));
        }
        else
        {
            cr_right(std::move(other.right()));
        }
    }

    ~either()
    {
        destroy();
    }

    either& operator=(const either& other)
    {
        if (m_is_left == other.m_is_left)
        {
            if (m_is_left)
            {
                left() = other.left();
            }
            else
            {
                right() = other.right();
            }
        }
        else
        {
            destroy();
            m_is_left = other.m_is_left;
            if (other.m_is_left)
            {
                cr_left(other.left());
            }
            else
            {
                cr_right(other.right());
            }
        }
        return *this;
    }

    inline bool is_left() const
    {
        return m_is_left;
    }

    inline bool is_right() const
    {
        return !m_is_left;
    }

    Left& left()
    {
        return m_left;
    }

    const Left& left() const
    {
        return m_left;
    }

    Right& right()
    {
        return m_right;
    }

    const Right& right() const
    {
        return m_right;
    }

};

template<typename Left, typename Right>
bool operator==(const either<Left, Right>& lhs,
                const either<Left, Right>& rhs)
{
    if (lhs.is_left() == rhs.is_left())
    {
        if (lhs.is_left())
        {
            return lhs.left() == rhs.left();
        }
        else
        {
            return lhs.right() == rhs.right();
        }
    }
    return false;
}

template<typename Left, typename Right>
bool operator==(const either<Left, Right>& lhs, const Left& rhs)
{
    return lhs.is_left() && lhs.left() == rhs;
}

template<typename Left, typename Right>
bool operator==(const Left& lhs, const either<Left, Right>& rhs)
{
    return rhs == lhs;
}

template<typename Left, typename Right>
bool operator==(const either<Left, Right>& lhs, const Right& rhs)
{
    return lhs.is_right() && lhs.right() == rhs;
}

template<typename Left, typename Right>
bool operator==(const Right& lhs, const either<Left, Right>& rhs)
{
    return rhs == lhs;
}

template<typename Left, typename Right>
bool operator!=(const either<Left, Right>& lhs,
                const either<Left, Right>& rhs)
{
    return !(lhs == rhs);
}

template<typename Left, typename Right>
bool operator!=(const either<Left, Right>& lhs, const Left& rhs)
{
    return !(lhs == rhs);
}

template<typename Left, typename Right>
bool operator!=(const Left& lhs, const either<Left, Right>& rhs)
{
    return !(rhs == lhs);
}

template<typename Left, typename Right>
bool operator!=(const either<Left, Right>& lhs, const Right& rhs)
{
    return !(lhs == rhs);
}

template<typename Left, typename Right>
bool operator!=(const Right& lhs, const either<Left, Right>& rhs)
{
    return !(rhs == lhs);
}

} } // namespace cppa::util

#endif // EITHER_HPP
