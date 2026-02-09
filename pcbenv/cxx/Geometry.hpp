
#ifndef GYM_PCB_GEOMETRY_H
#define GYM_PCB_GEOMETRY_H

#define CGAL_DISABLE_ROUNDING_MATH_CHECK

#include <iostream>
#include <numbers>
#include "Defs.hpp"

//
// CGAL configuration and type shorthands.
//
#include <CGAL/Bbox_2.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Simple_cartesian.h>
typedef CGAL::Simple_cartesian<Real> Kernel;

#if CGAL_VERSION_MAJOR >= 6
#define std_get_for_cgal std::get_if
#else
#define std_get_for_cgal boost::get
#endif

using Vector_2 = Kernel::Vector_2;
using Vector_3 = Kernel::Vector_3;
using Point_2 = Kernel::Point_2;
using Point_3 = Kernel::Point_3;
using Direction_2 = Kernel::Direction_2;
using Ray_2 = Kernel::Ray_2;
using Ray_3 = Kernel::Ray_3;
using Line_2 = Kernel::Line_2;
using Segment_2 = Kernel::Segment_2;
using Segment_3 = Kernel::Segment_3;
using Triangle_2 = Kernel::Triangle_2;
using Triangle_3 = Kernel::Triangle_3;
using Circle_2 = Kernel::Circle_2;

using Bbox_2 = CGAL::Bbox_2;
using Iso_rectangle_2 = CGAL::Iso_rectangle_2<Kernel>;
using Polygon_2 = CGAL::Polygon_2<Kernel>;

using Aff_transformation_2 = CGAL::Aff_transformation_2<Kernel>;

#include "Math/IPoint2.hpp"
#include "Math/IPoint3.hpp"

namespace geo
{

/**
 * Compute the cosine of the angle between two 2D vectors.
 */
inline Real cosine(const Vector_2 &v1, const Vector_2 &v2)
{
    return CGAL::scalar_product(v1, v2) / std::sqrt(v1.squared_length() *
                                                    v2.squared_length());
}

/**
 * Compute the 45-degree or "Queen's" distance between 2 points.
 * This is analogous to the Manhattan distance but with 45° rather than 90° steps.
 */
inline float distance45(const Point_2 &A, const Point_2 &B)
{
    const Real dx = std::abs(A.x() - B.x());
    const Real dy = std::abs(A.y() - B.y());
    return dx + dy - std::min(dx, dy) * (2.0 - std::sqrt(2.0));
    // return std::max(dx, dy) - std::min(dx, dy) + std::min(dx, dy) * std::sqrt(2.0);
    // return std::max(dx, dy) - std::min(dx, dy) * (1.0 - std::sqrt(2.0));
}

/**
 * @param m Expand the box by this value in both axes on both ends.
 */
inline Bbox_2 bbox_expanded_abs(const Bbox_2 &box, Real m)
{
    return Bbox_2(box.xmin() - m, box.ymin() - m, box.xmax() + m, box.ymax() + m);
}
/**
 * @param r Expand the box by r * max(w,h) in both axes on both ends.
 */
inline Bbox_2 bbox_expanded_rel(const Bbox_2 &box, Real r)
{
    return bbox_expanded_abs(box, std::max(box.xmax() - box.xmin(), box.ymax() - box.ymin()) * r);
}

inline Bbox_2 bbox_intersection(const Bbox_2 &box1, const Bbox_2 &box2)
{
    return Bbox_2(std::max(box1.xmin(), box2.xmin()),
                  std::max(box1.ymin(), box2.ymin()),
                  std::min(box1.xmax(), box2.xmax()),
                  std::min(box1.ymax(), box2.ymax()));
}

inline Real bbox_area(const Bbox_2 &box)
{
    return (box.xmax() - box.xmin()) * (box.ymax() - box.ymin());
}

inline Real bbox_diameter(const Bbox_2 &box)
{
    auto dx = box.xmax() - box.xmin();
    auto dy = box.ymax() - box.ymin();
    return std::sqrt(dx * dx + dy * dy);
}

inline bool bbox_contains(const Bbox_2 &outer, const Bbox_2 &inner)
{
    return (outer.xmin() <= inner.xmin() &&
            outer.ymin() <= inner.ymin() &&
            outer.xmax() >= inner.xmax() &&
            outer.ymax() >= inner.ymax());
}

inline std::string to_string(const Bbox_2 &box)
{
    return fmt::format("{:.2f} {:.2f} {:.2f} {:.2f}", box.xmin(), box.ymin(), box.xmax(), box.ymax());
}

/**
 * Return the point on the segment closest to the specified point.
 */
inline Point_2 closest_on(const Segment_2 &s, const Point_2 &o)
{
    auto x = s.supporting_line().projection(o);
    if (s.has_on(x))
        return x;
    return (CGAL::squared_distance(o, s.source()) <
            CGAL::squared_distance(o, s.target())) ? s.source() : s.target();
}

} // namespace geo


/**
 * 2.5D (2 continuous + 1 discrete) Point class.
 */
class Point_25
{
public:
    Point_25() { }
    Point_25(const Point_2 &x, int z) : _xy(x), _z(z) { }
    Point_25(Real x, Real y, int z) : _xy(x,y), _z(z) { }
    const Point_2& xy() const { return _xy; }
    Real x() const { return _xy.x(); }
    Real y() const { return _xy.y(); }
    int z() const { return _z; }
    Point_25 withZ(int z) const { return Point_25(_xy, z); }
    Bbox_2 bbox() const { return _xy.bbox(); }
    Point_25& operator=(const Point_25 &v) { _xy = v._xy; _z = v._z; return *this; }
    bool operator<(const Point_25 &v) const { return (_z < v._z) || (_z == v._z && _xy < v._xy); }
    bool operator==(const Point_25 &v) const { return _xy == v._xy && _z == v._z; }
    Point_25 operator+(const Vector_2 &v) const { return Point_25(_xy + v, _z); }
private:
    Point_2 _xy;
    int _z;
};

/**
 * Segment_2 with integer layer coordinate.
 */
class Segment_25
{
public:
    Segment_25() { }
    Segment_25(const Segment_2 &s, int layer = 0) : mS2(s), mLayer(layer) {
    }
    Segment_25(const ::Point_2 &p, const ::Point_2 &q, int layer = 0) : mS2(p, q), mLayer(layer) {
    }
    const Segment_2& s2() const { return mS2; }
    int z() const { return mLayer; }
    void setLayer(int z) { mLayer = z; }
    Point_25 source() const { return Point_25(mS2.source(), mLayer); }
    Point_25 target() const { return Point_25(mS2.target(), mLayer); }
    const Point_2& source_2() const { return mS2.source(); }
    const Point_2& target_2() const { return mS2.target(); }
    Real squared_length() const { return mS2.squared_length(); }
    Real length() const { return std::sqrt(squared_length()); }
    bool is_horizontal() const { return mS2.is_horizontal(); }
    bool is_vertical() const { return mS2.is_vertical(); }
    ::Vector_2 getDirection(Real s = 1.0) const;
    ::Vector_2 getPerpendicularCCW(Real s = 1.0) const;
    int angle45() const;
    Real anglePi2() const { const auto v = mS2.to_vector(); return std::atan(v.y() / v.x()); }
    Segment_25& rectify(Real angleTolerance);
    int majorAxis() const { const auto v = mS2.to_vector(); return std::abs(v.y()) > std::abs(v.x()) ? 1 : 0; }
    Segment_25 opposite() const { return Segment_25(mS2.opposite(), z()); }
    bool isXOrdered() const { return mS2.source().x() <= mS2.target().x(); }
    bool isYOrdered() const { return mS2.source().y() <= mS2.target().y(); }
    Segment_25 orderedX() const { return isXOrdered() ? *this : opposite(); }
    Segment_25 orderedY() const { return isYOrdered() ? *this : opposite(); }
    bool operator<(const Segment_25 &s) const { return (source() < s.source()) || (source() == s.source() && target() < s.target()); }
    bool operator==(const Segment_25 &s) const { return mLayer == s.mLayer && mS2 == s.mS2; }
    int equals(const Segment_25 &s) const;
    Real maxNorm() const { return std::max(std::abs(mS2.to_vector().x()), std::abs(mS2.to_vector().y())); }
    Real angleWith(const Segment_25&) const;
private:
    Segment_2 mS2;
    int mLayer;
};

/**
 * @return 1 if the segments are equal, -1 if they have swapped source/target, 0 otherwise.
 */
inline int Segment_25::equals(const Segment_25 &s) const
{
    if (mLayer != s.mLayer)
        return 0;
    if (mS2 == s.mS2)
        return 1;
    return (mS2 == s.mS2.opposite()) ? -1 : 0;
}

/**
 * @return A Vector_2 in the direction of this segment with length s.
 */
inline ::Vector_2 Segment_25::getDirection(Real s) const
{
    const auto v = mS2.to_vector();
    auto s0 = std::sqrt(v.squared_length());
    return v * (s / s0);
}

/**
 * @return A Vector_2 perpendicular (rotated counterclockwise) to this segment with length s.
 */
inline ::Vector_2 Segment_25::getPerpendicularCCW(Real s) const
{
    auto vd = getDirection(s); return ::Vector_2(-vd.y(), vd.x());
}

inline Segment_25& Segment_25::rectify(Real angleTolerance)
{
    const auto v0 = mS2.source();
    const auto v1 = mS2.target();
    const auto A = std::abs(anglePi2());
    if (std::abs(A) < angleTolerance)
        mS2 = Segment_2(v0, Point_2(v1.x(), v0.y()));
    else if (std::abs(A - std::numbers::pi * 0.5) < angleTolerance)
        mS2 = Segment_2(v0, Point_2(v0.x(), v1.y()));
    return *this;
}

/**
 * @return The angle of this segment with the x-axis rounded to mulitples of 45 degrees: either 0, 45, or 90.
 */
inline int Segment_25::angle45() const
{
    const auto dx = std::abs(source_2().x() - target_2().x());
    const auto dy = std::abs(source_2().y() - target_2().y());
    const auto t225 = std::sqrt(2.0) - 1.0;
    const auto t675 = std::sqrt(2.0) + 1.0;
    if (dy < t225 * dx)
        return 0;
    if (dy > t675 * dx)
        return 90;
    return 45;
}

/**
 * @return The angle between the two segments assuming a path this->source -> this->target == s.source -> s.target, between -pi and pi.
 */
inline Real Segment_25::angleWith(const Segment_25 &s) const
{
    const auto u = s2().to_vector();
    const auto v = s.s2().to_vector();
    const auto d = u.x() * v.x() + u.y() * v.y();
    const auto x = u.x() * v.y() - u.y() * v.x();
    return std::atan2(x, d);
}


//
// Stream printing operators.
//

inline std::ostream& operator<<(std::ostream &os, const Point_2 &v)
{
    return os << '(' << v.x() << ',' << v.y() << ')';
}

inline std::ostream& operator<<(std::ostream &os, const Point_25 &v)
{
    return os << '(' << v.x() << ',' << v.y() << ',' << v.z() << ')';
}

inline std::ostream& operator<<(std::ostream &os, const Vector_2 &v)
{
    return os << '(' << v.x() << ',' << v.y() << ')';
}

inline std::ostream& operator<<(std::ostream &os, const Triangle_2 &A)
{
    return os << '<' << A.vertex(0) << ' ' << A.vertex(1) << ' ' << A.vertex(2) << '>';
}

inline std::ostream& operator<<(std::ostream &os, const Segment_2 &s)
{
    return os << '[' << s.source() << '_' << s.target() << ']';
}

inline std::ostream& operator<<(std::ostream &os, const Segment_25 &s)
{
    return os << '[' << s.source() << '_' << s.target() << "|z=" << s.z() << ']';
}

#endif // GYM_PCB_GEOMETRY_H
