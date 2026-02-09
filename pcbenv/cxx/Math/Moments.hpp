#ifndef GYM_PCB_MATH_STATS_H
#define GYM_PCB_MATH_STATS_H

#include "Defs.hpp"

namespace math {

struct Moments
{
    float m1{0.0f};
    float m2{0.0f};
    uint N{0};

    Moments& operator+=(float v)
    {
        const auto m1o = m1;
        N  += 1;
        m1 += (v - m1) / N;
        m2 += (v - m1) * (v - m1o);
        return *this;
    }

    Moments& operator+=(const Moments &A)
    {
        const uint64_t No = N;
        const auto D = m1 - A.m1;
        N  += A.N;
        m1 += (A.m1 - m1) * A.N / float(N);
        m2 += A.m2 + float(No * A.N) / N * (D * D);
        return *this;
    }
};

} // namespace math

#endif // GYM_PCB_MATH_STATS_H
