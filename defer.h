#ifndef _DEFER_H_
#define _DEFER_H_

template<typename F>
class Defer {
    F lambda;
public:
    Defer(F &&lambda): lambda{lambda} {}
    ~Defer() { lambda(); }
};

#endif
