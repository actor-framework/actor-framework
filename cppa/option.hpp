#ifndef OPTION_HPP
#define OPTION_HPP

namespace cppa {

template<typename What>
class option
{

    union
    {
        What m_value;
    };

    bool m_valid;

    void destroy()
    {
        if (m_valid) m_value.~What();
    }

    template<typename V>
    void cr(V&& value)
    {
        m_valid = true;
        new (&m_value) What (std::forward<V>(value));
    }

 public:

    option() : m_valid(false) { }

    template<typename V>
    option(V&& value)
    {
        cr(std::forward<V>(value));
    }

    option(option const& other)
    {
        if (other.m_valid) cr(other.m_value);
        else m_valid = false;
    }

    option(option&& other)
    {
        if (other.m_valid) cr(std::move(other.m_value));
        else m_valid = false;
    }

    ~option()
    {
        destroy();
    }

    option& operator=(option const& other)
    {
        if (m_valid)
        {
            if (other.m_valid)
            {
                m_value = other.m_value;
            }
            else
            {
                destroy();
                m_valid = false;
            }
        }
        else
        {
            if (other.m_valid)
            {
                cr(other.m_value);
            }
        }
        return *this;
    }

    option& operator=(option&& other)
    {
        if (m_valid)
        {
            if (other.m_valid)
            {
                m_value = std::move(other.m_value);
            }
            else
            {
                destroy();
                m_valid = false;
            }
        }
        else
        {
            if (other.m_valid)
            {
                cr(std::move(other.m_value));
            }
        }
        return *this;
    }

    option& operator=(What const& value)
    {
        if (m_valid) m_value = value;
        else cr(value);
        return *this;
    }

    option& operator=(What& value)
    {
        if (m_valid) m_value = std::move(value);
        else cr(std::move(value));
        return *this;
    }

    inline bool valid() const { return m_valid; }

    inline explicit operator bool() const { return m_valid; }

    inline bool operator!() const { return !m_valid; }

    inline What& operator*() { return m_value; }
    inline What const& operator*() const { return m_value; }

    inline What& get() { return m_value; }

    inline What const& get() const { return m_value; }

    inline What& get_or_else(What const& val)
    {
        if (!m_valid) cr(val);
        return m_value;
    }

    inline What& get_or_else(What&& val)
    {
        if (!m_valid) cr(std::move(val));
        return m_value;
    }

};

} // namespace cppa::util

#endif // OPTION_HPP
