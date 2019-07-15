//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <cmath>

template <class R>
struct Amp_Follower
{
    R p_ = 0;
    R mem_ = 0;
    void release(R t); // t = fs * release time
    R process(R x);
};

template <class R>
void Amp_Follower<R>::release(R t)
{
    p_ = std::exp(-1 / t);
}

template <class R>
R Amp_Follower<R>::process(R x)
{
    x = std::fabs(x);
    if (x > mem_) {
        mem_ = x;
        return x;
    }
    else {
        mem_ *= p_;
        return mem_ + (1 - p_) * x;
    }
}
