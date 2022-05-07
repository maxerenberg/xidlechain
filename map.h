#ifndef _MAP_H_
#define _MAP_H_

#include <functional>
#include <iterator>

template<typename InputIt, typename T>
class Map {
    const InputIt _begin;
    const InputIt _end;
    const std::function<T(typename std::iterator_traits<InputIt>::value_type)> lambda;
public:
    class iterator {
        InputIt it;
        Map *map;
    public:
        iterator(InputIt it, Map *map): it{it}, map{map} {}

        using iterator_category = std::input_iterator_tag;
        using difference_type = typename std::iterator_traits<InputIt>::difference_type;
        using value_type = T;
        using pointer = const T*;
        using reference = const T&;

        iterator& operator++() { ++it; return *this; }
        iterator operator++(int) { iterator ret = *this; ++*this; return ret; }
        bool operator==(const iterator &other) const { return it == other.it; }
        bool operator!=(const iterator &other) const { return !(*this == other); }
        T operator*() const { return map->lambda(*it); }
    };

    Map(
        InputIt _begin,
        InputIt _end,
        std::function<T(typename std::iterator_traits<InputIt>::value_type)> &&lambda
    ):
        _begin{_begin}, _end{_end}, lambda{lambda}
    {}

    iterator begin() {
        return iterator(_begin, this);
    }

    iterator end() {
        return iterator(_end, this);
    }
};

#endif
