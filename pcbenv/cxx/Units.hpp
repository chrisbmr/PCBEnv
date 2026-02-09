#ifndef GYM_PCB_UNITS_H
#define GYM_PCB_UNITS_H

#include <iostream>

template<typename T, double M1, double Q1> class Length
{
public:
    Length() { }
    explicit Length(int32_t i) : m(i) { assert(int32_t(m) == i); }
    explicit Length(int64_t i) : m(i) { assert(int64_t(m) == i); }
    explicit Length(T v) : m(v) { }
    template<double M2, double Q2> Length(const Length<T, M2, Q2> &l) { m = l.value() * ((M2 * Q1) / (M1 * Q2)); }
    T value() const { return m; }
    Length<T,M1,Q1> operator+(const Length<T,M1,Q1> &l) const { return Length<T,M1,Q1>(m + l.m); }
    Length<T,M1,Q1> operator-(const Length<T,M1,Q1> &l) const { return Length<T,M1,Q1>(m - l.m); }
    Length<T,M1,Q1>& operator+=(const Length<T,M1,Q1> &l) { m += l.m; return *this; }
    Length<T,M1,Q1>& operator-=(const Length<T,M1,Q1> &l) { m += l.m; return *this; }
    Length<T,M1,Q1> operator*(T s) const { return Length<T,M1,Q1>(m * s); }
    Length<T,M1,Q1>& operator*=(T s) { m *= s; return *this; }
    template<double M2, double Q2> bool operator< (const Length<T,M2,Q2> &l) const { return value() * (M1 * Q2) <  l.value() * (Q1 * M2); }
    template<double M2, double Q2> bool operator<=(const Length<T,M2,Q2> &l) const { return value() * (M1 * Q2) <= l.value() * (Q1 * M2); }
    template<double M2, double Q2> bool operator==(const Length<T,M2,Q2> &l) const { return value() * (M1 * Q2) == l.value() * (Q1 * M2); }
private:
    T m;
};

template<typename T, double M1, double Q1> inline std::ostream& operator<<(std::ostream &os, const Length<T,M1,Q1> &l)
{
    const auto Q = M1 / Q1;
    const char *s = "?";
    if (Q == 1e-9) s = "nm";
    else if (Q == 1e-6) s = "µm";
    else if (Q == 1) s = "m";
    return os << l.value() << ' ' << s;
}

using Meters       = Length<double, 1.0, 1.0>;
using Micrometers  = Length<double, 1.0, 1000000.0>;
using FMicrometers = Length<float,  1.0, 1000000.0>;
using Nanometers   = Length<double, 1.0, 1000000000.0>;

#endif // GYM_PCB_UNITS_H
