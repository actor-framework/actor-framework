#ifndef PATTER_DETAILS_HPP
#define PATTER_DETAILS_HPP

#include <type_traits>

#include "cppa/anything.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/any_tuple_iterator.hpp"

#include "cppa/detail/boxed.hpp"
#include "cppa/detail/unboxed.hpp"

namespace cppa { namespace detail {

template<typename OutputIterator, typename T0>
void fill_uti_vec(OutputIterator& ii)
{
    typedef typename unboxed<T0>::type real_type;
    *ii = std::is_same<real_type,anything>::value ? nullptr
                                                  : uniform_typeid<real_type>();
}

template<typename OutputIterator, typename T0, typename T1, typename... Tn>
void fill_uti_vec(OutputIterator& ii)
{
    fill_uti_vec<OutputIterator, T0>(ii);
    fill_uti_vec<OutputIterator, T1, Tn...>(++ii);
}

template<typename DataTuple, typename T0>
struct fill_vecs_util
{
    inline static void _(size_t pos,
                         size_t dt_size,
                         const DataTuple& dt,
                         const cppa::uniform_type_info** utis,
                         const void** data_ptrs)
    {
        utis[pos] = uniform_typeid<T0>();
        data_ptrs[pos] = (pos < dt_size) ? dt.at(pos) : nullptr;
    }
};

template<typename DataTuple>
struct fill_vecs_util<DataTuple, anything>
{
    inline static void _(size_t pos,
                         size_t,
                         const DataTuple&,
                         const cppa::uniform_type_info** utis,
                         const void** data_ptrs)
    {
        utis[pos] = nullptr;
        data_ptrs[pos] = nullptr;
    }
};

template<typename DataTuple, typename T0>
struct fill_vecs_util<DataTuple, util::wrapped<T0>>
{
    inline static void _(size_t pos,
                         size_t,
                         const DataTuple&,
                         const cppa::uniform_type_info** utis,
                         const void** data_ptrs)
    {
        utis[pos] = uniform_typeid<T0>();
        data_ptrs[pos] = nullptr;
    }
};

template<typename DataTuple, typename T0>
void fill_vecs(size_t pos,
               size_t dt_size,
               const DataTuple& dt,
               const cppa::uniform_type_info** utis,
               const void** data_ptrs)
{
    fill_vecs_util<DataTuple, T0>::_(pos, dt_size, dt, utis, data_ptrs);
}

template<typename DataTuple, typename T0, typename T1, typename... Tn>
void fill_vecs(size_t pos,
               size_t dt_size,
               const DataTuple& dt,
               const cppa::uniform_type_info** utis,
               const void** data_ptrs)
{
    fill_vecs_util<DataTuple, T0>::_(pos, dt_size, dt, utis, data_ptrs);
    fill_vecs<DataTuple, T1, Tn...>(pos + 1, dt_size, dt, utis, data_ptrs);
}

struct pattern_arg
{

    size_t m_pos;
    size_t m_size;
    void const* const* m_data;
    cppa::uniform_type_info const* const* m_types;

 public:

    inline pattern_arg(size_t msize,
                       void const* const* mdata,
                       cppa::uniform_type_info const* const* mtypes)
        : m_pos(0)
        , m_size(msize)
        , m_data(mdata)
        , m_types(mtypes)
    {
    }

    pattern_arg(const pattern_arg&) = default;

    pattern_arg& operator=(const pattern_arg&) = default;

    inline bool at_end() const { return m_pos == m_size; }

    inline pattern_arg& next()
    {
        ++m_pos;
        return *this;
    }

    inline const uniform_type_info* type() const
    {
        return m_types[m_pos];
    }

    inline const void* value() const
    {
        return m_data[m_pos];
    }

    inline bool has_value() const { return value() != nullptr; }

};

template<typename VectorType>
struct tuple_iterator_arg
{

    typedef VectorType vector_type;

    util::any_tuple_iterator iter;
    vector_type* mapping;

    inline tuple_iterator_arg(const any_tuple& tup,
                              vector_type* mv = nullptr)
        : iter(tup), mapping(mv)
    {
    }

    inline tuple_iterator_arg(const util::any_tuple_iterator& from_iter,
                              vector_type* mv = nullptr)
        : iter(from_iter), mapping(mv)
    {
    }

    inline bool at_end() const { return iter.at_end(); }

    inline tuple_iterator_arg& next()
    {
        iter.next();
        return *this;
    }

    inline tuple_iterator_arg& push_mapping()
    {
        if (mapping) mapping->push_back(iter.position());
        return *this;
    }

    inline const uniform_type_info* type() const
    {
        return &(iter.type());
    }

    inline const void* value() const
    {
        return iter.value_ptr();
    }

};

template<typename VectorType>
bool do_match(pattern_arg& pargs, tuple_iterator_arg<VectorType>& targs)
{
    for (;;)
    {
        if (pargs.at_end() && targs.at_end())
        {
            return true;
        }
        else if (pargs.type() == nullptr) // nullptr == wildcard (anything)
        {
            // perform submatching
            pargs.next();
            if (pargs.at_end())
            {
                // always true at the end of the pattern
                return true;
            }
            auto pargs_copy = pargs;
            VectorType mv;
            auto mv_ptr = (targs.mapping) ? &mv : nullptr;
            // iterate over tu_args until we found a match
            while (targs.at_end() == false)
            {
                tuple_iterator_arg<VectorType> targs_copy(targs.iter, mv_ptr);
                if (do_match(pargs_copy, targs_copy))
                {
                    if (mv_ptr)
                    {
                        targs.mapping->insert(targs.mapping->end(),
                                              mv.begin(),
                                              mv.end());
                    }
                    return true;
                }
                // next iteration
                mv.clear();
                targs.next();
            }
            // no successfull submatch found
            return false;
        }
        // compare types
        else if (targs.at_end() == false && pargs.type() == targs.type())
        {
            // compare values if needed
            if (   pargs.has_value() == false
                || pargs.type()->equal(pargs.value(), targs.value()))
            {
                targs.push_mapping();
                // ok, go to next iteration
            }
            else
            {
                // values didn't match
                return false;
            }
        }
        else
        {
            // no match
            return false;
        }
        // next iteration
        pargs.next();
        targs.next();
    }
}

} } // namespace cppa::detail


#endif // PATTER_DETAILS_HPP
