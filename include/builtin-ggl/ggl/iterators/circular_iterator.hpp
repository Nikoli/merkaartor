// Generic Geometry Library
//
// Copyright Barend Gehrels 1995-2009, Geodan Holding B.V. Amsterdam, the Netherlands.
// Copyright Bruno Lalande 2008, 2009
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef GGL_ITERATORS_CIRCULAR_ITERATOR_HPP
#define GGL_ITERATORS_CIRCULAR_ITERATOR_HPP

#include <boost/iterator.hpp>
#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/iterator/iterator_categories.hpp>

#include <ggl/iterators/base.hpp>

namespace ggl
{

/*!
    \brief Iterator which goes circular through a range, starting at a point, ending at that point
    \tparam Iterator iterator on which this class is based on
    \ingroup iterators
*/
template <typename Iterator>
struct circular_iterator :
    public detail::iterators::iterator_base
    <
        circular_iterator<Iterator>,
        Iterator
    >
{
    friend class boost::iterator_core_access;

    explicit inline circular_iterator(Iterator begin, Iterator end, Iterator start)
        : m_begin(begin)
        , m_end(end)
        , m_start(start)
    {
        this->base_reference() = start;
    }

    // Constructor to indicate the end of a range, to enable e.g. std::copy
    explicit inline circular_iterator(Iterator end)
        : m_begin(end)
        , m_end(end)
        , m_start(end)
    {
        this->base_reference() = end;
    }

    /// Navigate to a certain position, should be in [start .. end], it at end
    /// it will circle again.
    inline void moveto(Iterator it)
    {
        this->base_reference() = it;
        check_end();
    }

private:

    inline void increment()
    {
        if (this->base() != m_end)
        {
            (this->base_reference())++;
            check_end();
        }
    }

    inline void check_end()
    {
        if (this->base() == this->m_end)
        {
            this->base_reference() = this->m_begin;
        }

        if (this->base() == m_start)
        {
            this->base_reference() = this->m_end;
        }
    }

    Iterator m_begin;
    Iterator m_end;
    Iterator m_start;
};

} // namespace ggl

#endif // GGL_ITERATORS_CIRCULAR_ITERATOR_HPP
