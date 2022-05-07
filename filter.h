#ifndef _FILTER_H_
#define _FILTER_H_

#include <functional>
#include <iterator>
#include <utility>

template<typename InputIt>
class Filter {
protected:
    InputIt _begin;
    InputIt _end;
private:
    const std::function<bool(typename std::iterator_traits<InputIt>::value_type)> lambda;
public:
    class iterator {
        InputIt it;
        Filter *filter;
    public:
        void go_to_valid_elem() {
            while (it != filter->_end && !filter->lambda(*it)) {
                ++it;
            }
        }
    private:
        void inc() {
            ++it;
            go_to_valid_elem();
        }
    public:
        iterator(InputIt it, Filter *filter): it{it}, filter{filter} {}

        // Technically we should use std::conditional and std::is_same
        // to check the category of InputIt
        using iterator_category = std::forward_iterator_tag;
        using difference_type = typename std::iterator_traits<InputIt>::difference_type;
        using value_type = typename std::iterator_traits<InputIt>::value_type;
        using pointer = typename std::iterator_traits<InputIt>::pointer;
        using reference = typename std::iterator_traits<InputIt>::reference;

        iterator& operator++() { inc(); return *this; }
        iterator operator++(int) { iterator ret = *this; ++*this; return ret; }
        bool operator==(const iterator &other) const { return it == other.it; }
        bool operator!=(const iterator &other) const { return !(*this == other); }
        decltype(*it) operator*() const { return *it; }
    };

    Filter(
        InputIt _begin,
        InputIt _end,
        std::function<bool(typename std::iterator_traits<InputIt>::value_type)> &&lambda
    ):
        _begin{_begin}, _end{_end}, lambda{lambda}
    {}

    iterator begin() {
        iterator it = iterator(_begin, this);
        it.go_to_valid_elem();
        return it;
    }

    iterator end() {
        return iterator(_end, this);
    }
};

#endif
