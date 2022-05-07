#ifndef _BOX_FILTER_H_
#define _BOX_FILTER_H_

#include <utility>

#include "filter.h"

/**
 * Convenience class which keeps a reference to the container being
 * filtered so that the Filter object can be returned from a function
 * without the container being destroyed.
 */
template<typename Container>
class BoxFilter: public Filter<typename Container::iterator> {
    Container container;
public:
    BoxFilter(Container &&c, std::function<bool(typename std::iterator_traits<typename Container::iterator>::value_type)> &&f):
        Filter<typename Container::iterator>(c.begin(), c.end(), std::move(f)),
        container(c)
    {
        // c will go out of scope, so we need to use the newly constructed
        // object instead
        this->_begin = container.begin();
        this->_end = container.end();
    }
};

#endif
