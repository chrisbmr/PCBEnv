
#ifndef GYM_PCB_MATH_MISC_H
#define GYM_PCB_MATH_MISC_H

#if defined(__x86_64__)
#include <xmmintrin.h>
#endif
#include "Defs.hpp"
#include <cmath>
#include <vector>

namespace math {

constexpr inline Real Radians(Real D)
{
    return M_PI * (D / 180.0);
}

template<typename T> inline T squared(T v) { return v * v; }

namespace approx {

inline float rsqrt(float x)
{
#if defined(__x86_64__)
    return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x)));
#else
    return 1.0f / std::sqrtf(x);
#endif
}

} // approx

template<typename T> T reduce_max(const std::vector<T> &V)
{
    if (V.empty())
        throw std::runtime_error("maximum(empty vector)");
    T mx = V[0];
    for (size_t i = 1; i < V.size(); ++i)
        mx = std::max(mx, V[i]);
    return mx;
}

template<typename T> void multiply(std::vector<T> &V, T s)
{
    if (s != T(1)) {
        for (T &x : V)
            x *= s;
    }
}

} // math

#endif // GYM_PCB_MATH_MISC_H
