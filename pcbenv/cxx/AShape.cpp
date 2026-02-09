
#include "AShape.hpp"
#include "Math/Misc.hpp"
#include <CGAL/centroid.h>

namespace geo
{

// Circle
// ======

Real squared_distance(const Circle_2 &C, const Point_2 &v)
{
    const auto r2 = C.squared_radius();
    const auto d2 = CGAL::squared_distance(C.center(), v);
    return d2 + r2 - 2.0 * std::sqrt(d2 * r2);
}
Real squared_distance(const Circle_2 &C1, const Circle_2 &C2)
{
    const auto d0 = std::sqrt(CGAL::squared_distance(C1.center(), C2.center()));
    const auto r1 = std::sqrt(C1.squared_radius());
    const auto r2 = std::sqrt(C2.squared_radius());
    const auto  d = std::max(d0 - r1 - r2, 0.0);
    return d * d;
}
Real squared_distance(const Circle_2 &C, const Triangle_2 &T)
{
    const auto d2 = CGAL::squared_distance(C.center(), T);
    const auto r2 = C.squared_radius();
    if (d2 <= r2)
        return 0.0;
    return d2 + r2 - 2.0 * std::sqrt(d2 * r2);
}
Real squared_distance(const Circle_2 &C, const Polygon_2 &poly)
{
    if (!poly.has_on_unbounded_side(C.center()))
        return 0.0;
    const Real r2 = C.squared_radius();
    Real d2 = std::numeric_limits<Real>::infinity();
    for (auto e = poly.edges_begin(); e != poly.edges_end() && d2 > r2; ++e)
        d2 = std::min(d2, CGAL::squared_distance(C.center(), *e));
    return d2 + r2 - 2.0 * std::sqrt(d2 * r2);
}
Real squared_distance(const Circle_2 &C, const Iso_rectangle_2 &R)
{
    if (CGAL::do_intersect(C, R))
        return 0.0;
    assert(!R.has_on_bounded_side(C.center()));
    const Real r2 = C.squared_radius();
    Real d2 = CGAL::squared_distance(C.center(), Segment_2(R.vertex(0), R.vertex(1)));
    for (uint i = 1; (i < 4) && d2 > r2; ++i)
        d2 = std::min(d2, CGAL::squared_distance(C.center(), Segment_2(R.vertex(i), R.vertex(i+1))));
    assert(d2 > r2);
    return d2 + r2 - 2.0 * std::sqrt(d2 * r2);
}
Real squared_distance(const Circle_2 &C, const Segment_2 &s)
{
    const auto r2 = C.squared_radius();
    const auto d2 = CGAL::squared_distance(C.center(), s);
    if (d2 <= r2)
        return 0.0;
    return math::squared(std::sqrt(d2) - std::sqrt(r2));
}
Real squared_distance(const Circle_2 &C, const WideSegment_25 &s)
{
    const auto r2 = C.squared_radius();
    const auto d2 = CGAL::squared_distance(C.center(), s.base().s2());
    if (d2 <= r2)
        return 0.0;
    return math::squared(std::max(std::sqrt(d2) - std::sqrt(r2) - s.halfWidth(), 0.0));
}

// Triangle
// ========

Real squared_distance(const Triangle_2 &T, const Iso_rectangle_2 &R)
{
    if (CGAL::do_intersect(T, R))
        return 0.0;
    Real d2 = CGAL::squared_distance(T, Segment_2(R.vertex(0), R.vertex(1)));
    for (uint i = 1; i < 4 && d2 > 0.0; ++i)
        d2 = std::min(d2, CGAL::squared_distance(T, Segment_2(R.vertex(i), R.vertex(i+1))));
    assert(d2 > 0.0);
    return d2;
}
Real squared_distance(const Triangle_2 &T, const Polygon_2 &poly)
{
    for (auto v = poly.vertices_begin(); v != poly.vertices_end(); ++v)
        if (!T.has_on_unbounded_side(*v))
            return 0.0;
    if (!poly.has_on_unbounded_side(T.vertex(0)) ||
        !poly.has_on_unbounded_side(T.vertex(1)) ||
        !poly.has_on_unbounded_side(T.vertex(2)))
        return 0.0;
    Real d2 = std::numeric_limits<Real>::infinity();
    for (auto e = poly.edges_begin(); e != poly.edges_end() && d2 > 0.0; ++e)
        d2 = std::min(d2, CGAL::squared_distance(T, *e));
    assert(d2 > 0.0);
    return d2;
}
Real squared_distance(const Triangle_2 &T, const WideSegment_25 &s)
{
    if (!T.has_on_unbounded_side(s.source_2()) || !T.has_on_unbounded_side(s.target_2()))
        return 0.0;
    const Real d2 = squared_distance(T, s.base().s2());
    if (d2 <= math::squared(s.halfWidth()))
        return 0.0;
    return math::squared(std::sqrt(d2) - s.halfWidth());
}

// Rectangle
// =========

Real squared_distance(const Iso_rectangle_2 &R, const Point_2 &v)
{
    if (!R.has_on_unbounded_side(v))
        return 0.0;
    Real d2 = CGAL::squared_distance(Segment_2(R.vertex(0), R.vertex(1)), v);
    for (uint i = 1; i < 4 && d2 > 0.0; ++i)
        d2 = std::min(d2, CGAL::squared_distance(Segment_2(R.vertex(i), R.vertex(i+1)), v));
    assert(d2 > 0.0);
    return d2;
}
Real squared_distance(const Iso_rectangle_2 &R1, const Iso_rectangle_2 &R2)
{
    if (CGAL::do_intersect(R1, R2))
        return 0.0;
#if 1
    const auto v1 = R1.min() - R2.max();
    const auto v2 = R2.min() - R1.max();
    const auto dx = std::max(std::max(v1.x(), v2.x()), 0.0);
    const auto dy = std::max(std::max(v1.y(), v2.y()), 0.0);
    Real d2 = dx * dx + dy * dy;
#else
    Real d2 = std::numeric_limits<Real>::infinity();
    for (uint i = 0; i < 4 && d2 > 0.0; ++i)
        for (uint k = 0; k < 4 && d2 > 0.0; ++k)
            d2 = std::min(d2, CGAL::squared_distance(Segment_2(R1.vertex(i), R1.vertex(i+1)), Segment_2(R2.vertex(k), R2.vertex(k+1))));
    assert(d2 > 0.0);
#endif
    return d2;
}
Real squared_distance(const Iso_rectangle_2 &R, const Segment_2 &s)
{
    if (!R.has_on_unbounded_side(s.source()) || !R.has_on_unbounded_side(s.target()))
        return 0.0;
    Real d2 = std::numeric_limits<Real>::infinity();
    for (uint i = 0; i < 4 && d2 > 0.0; ++i)
        d2 = std::min(d2, CGAL::squared_distance(s, Segment_2(R.vertex(i), R.vertex(i+1))));
    return d2;
}
Real squared_distance(const Iso_rectangle_2 &R, const WideSegment_25 &s)
{
    if (!R.has_on_unbounded_side(s.source_2()) || !R.has_on_unbounded_side(s.target_2()))
        return 0.0;
    const Real d2 = squared_distance(R, s.base().s2());
    if (d2 <= math::squared(s.halfWidth()))
        return 0.0;
    return math::squared(std::sqrt(d2) - s.halfWidth());
}
Real squared_distance(const Iso_rectangle_2 &R, const Polygon_2 &poly)
{
    for (auto v = poly.vertices_begin(); v != poly.vertices_end(); ++v)
        if (!R.has_on_unbounded_side(*v))
            return 0.0;
    for (uint i = 0; i < 4; ++i)
        if (!poly.has_on_unbounded_side(R.vertex(i)))
            return 0.0;
    Real d2 = std::numeric_limits<Real>::infinity();
    for (uint i = 0; i < 4; ++i)
        for (auto e = poly.edges_begin(); e != poly.edges_end() && d2 > 0.0; ++e)
            d2 = std::min(d2, CGAL::squared_distance(Segment_2(R.vertex(i), R.vertex(i+1)), *e));
    return d2;
}

// Polygon
// =======

Real squared_distance(const Polygon_2 &G, const Point_2 &v)
{
    if (!G.has_on_unbounded_side(v))
        return 0.0;
    Real d2 = std::numeric_limits<Real>::infinity();
    for (auto e = G.edges_begin(); e != G.edges_end() && d2 > 0.0; ++e)
        d2 = std::min(d2, CGAL::squared_distance(*e, v));
    return d2;
}
Real squared_distance(const Polygon_2 &G, const Segment_2 &s)
{
    if (!G.has_on_unbounded_side(s.source()) || !G.has_on_unbounded_side(s.target()))
        return 0.0;
    Real d2 = std::numeric_limits<Real>::infinity();
    for (auto e = G.edges_begin(); e != G.edges_end() && d2 > 0.0; ++e)
        d2 = std::min(d2, CGAL::squared_distance(s, *e));
    return d2;
}
Real squared_distance(const Polygon_2 &G, const WideSegment_25 &s)
{
    if (!G.has_on_unbounded_side(s.source_2()) || !G.has_on_unbounded_side(s.target_2()))
        return 0.0;
    const Real d2 = squared_distance(G, s.base().s2());
    if (d2 <= math::squared(s.halfWidth()))
        return 0.0;
    return math::squared(std::sqrt(d2) - s.halfWidth());
}
Real squared_distance(const Polygon_2 &A, const Polygon_2 &B)
{
    for (auto v = B.vertices_begin(); v != B.vertices_end(); ++v)
        if (!A.has_on_unbounded_side(*v))
            return 0.0;
    for (auto v = A.vertices_begin(); v != A.vertices_end(); ++v)
        if (!B.has_on_unbounded_side(*v))
            return 0.0;
    Real d2 = std::numeric_limits<Real>::infinity();
    for (auto e = B.edges_begin(); e != B.edges_end(); ++e)
        d2 = std::min(d2, geo::squared_distance(A, *e));
    assert(d2 > 0.0);
    return d2;
}

// Wide Segment
// ============

Real squared_distance(const WideSegment_25 &ws, const Point_2 &v)
{
    const Real d2 = CGAL::squared_distance(ws.base().s2(), v);
    return math::squared(std::max(std::sqrt(d2) - ws.halfWidth(), 0.0));
}
Real squared_distance(const WideSegment_25 &ws, const Segment_2 &s)
{
    Real d2 = CGAL::squared_distance(ws.base().s2(), s);
    if (d2 <= 0.0)
        return d2;
    return math::squared(std::max(std::sqrt(d2) - ws.halfWidth(), 0.0));
}
Real squared_distance(const WideSegment_25 &s1, const WideSegment_25 &s2)
{
    Real d2 = CGAL::squared_distance(s1.base().s2(), s2.base().s2());
    if (d2 <= 0.0)
        return d2;
    return math::squared(std::max(std::sqrt(d2) - s1.halfWidth() - s2.halfWidth(), 0.0));
}

Point_2 closest_endpoint(const Segment_2 &s1, const Segment_2 &s2)
{
    const auto d1s = CGAL::squared_distance(s1.source(), s2);
    const auto d1t = CGAL::squared_distance(s1.target(), s2);
    const auto d2s = CGAL::squared_distance(s2.source(), s1);
    const auto d2t = CGAL::squared_distance(s2.target(), s1);
    auto v = s1.source();
    Real d = d1s;
    if (d1t < d) { d = d1t; v = s1.target(); }
    if (d2s < d) { d = d2s; v = s2.source(); }
    if (d2t < d) { d = d2t; v = s2.target(); }
    return v;
}
Point_2 closest_point(const Segment_2 &s1, const Segment_2 &s2)
{
    auto x = CGAL::intersection(s1, s2);
    if (!x)
        return closest_endpoint(s1, s2);
    if (const Segment_2 *s = std_get_for_cgal<Segment_2>(&*x))
        return s->source();
    return *std_get_for_cgal<Point_2>(&*x);
}

} // namespace geo


// Circle
// ======

void CircleEx::translate(const ::Vector_2 &dv)
{
    Circle_2::operator=(Circle_2(center() + dv, squared_radius()));
}
bool CircleEx::transformPreservesType(const CGAL::Aff_transformation_2<Kernel> &T) const
{
    // Similarity transforms only.
    const auto d1 = T.hm(0,0) * T.hm(0,0) + T.hm(1,0) * T.hm(1,0);
    const auto d2 = T.hm(0,1) * T.hm(0,1) + T.hm(1,1) * T.hm(1,1);
    const auto od = T.hm(0,0) * T.hm(0,1) + T.hm(1,0) * T.hm(1,1);
    return std::abs(d1 - d2) < 0x1.0p-20 && std::abs(od) < 0x1.0p-20;
}
void CircleEx::transform(const CGAL::Aff_transformation_2<Kernel> &T)
{
    if (!transformPreservesType(T))
        throw std::invalid_argument("circle transformation must be a similiarty transform");
    Circle_2::operator=(orthogonal_transform(T));
}
bool CircleEx::contains(const ::Point_2 &v) const
{
    return has_on_bounded_side(v);
}
bool CircleEx::intersects(const Circle_2 &circle) const
{
    return CGAL::do_intersect(*this, circle);
}
bool CircleEx::intersects(const Triangle_2 &A) const
{
    return squared_distance(A) <= 0.0;
}
bool CircleEx::intersects(const Iso_rectangle_2 &rect) const
{
    return CGAL::do_intersect(*this, rect);
}
bool CircleEx::intersects(const Polygon_2 &poly) const
{
    return squared_distance(poly) <= 0.0;
}
bool CircleEx::intersects(const WideSegment_25 &s) const
{
    return squared_distance(s.base().s2()) < math::squared(s.halfWidth());
}
Real CircleEx::squared_distance(const Segment_2 &s) const
{
    return geo::squared_distance(*this, s);
}
Real CircleEx::squared_distance(const WideSegment_25 &s) const
{
    return geo::squared_distance(*this, s);
}
Real CircleEx::squared_distance(const Circle_2 &circle) const
{
    return geo::squared_distance(*this, circle);
}
Real CircleEx::squared_distance(const Triangle_2 &tri) const
{
    return geo::squared_distance(*this, tri);
}
Real CircleEx::squared_distance(const Iso_rectangle_2 &rect) const
{
    return geo::squared_distance(*this, rect);
}
Real CircleEx::squared_distance(const Polygon_2 &poly) const
{
    return geo::squared_distance(*this, poly);
}
Real CircleEx::squared_distance(const ::Point_2 &v) const
{
    return geo::squared_distance(*this, v);
}


// Triangle
// ========

void TriangleEx::translate(const ::Vector_2 &dv)
{
    Triangle_2::operator=(Triangle_2(vertex(0) + dv,
                                     vertex(1) + dv,
                                     vertex(2) + dv));
}
void TriangleEx::transform(const CGAL::Aff_transformation_2<Kernel> &T)
{
    Triangle_2::operator=(Triangle_2::transform(T));
}
bool TriangleEx::contains(const ::Point_2 &v) const
{
    return has_on_bounded_side(v);
}
bool TriangleEx::intersects(const Circle_2 &circle) const
{
    return squared_distance(circle) <= 0.0;
}
bool TriangleEx::intersects(const Triangle_2 &A) const
{
    return CGAL::do_intersect(*this, A);
}
bool TriangleEx::intersects(const Iso_rectangle_2 &rect) const
{
    return CGAL::do_intersect(*this, rect);
}
bool TriangleEx::intersects(const Polygon_2 &poly) const
{
    return squared_distance(poly) <= 0.0;
}
bool TriangleEx::intersects(const WideSegment_25 &s) const
{
    return geo::squared_distance(*this, s) <= 0.0;
}
Real TriangleEx::squared_distance(const Segment_2 &s) const
{
    return CGAL::squared_distance(*this, s);
}
Real TriangleEx::squared_distance(const WideSegment_25 &s) const
{
    return geo::squared_distance(*this, s);
}
Real TriangleEx::squared_distance(const Circle_2 &circle) const
{
    return geo::squared_distance(circle, *this);
}
Real TriangleEx::squared_distance(const Triangle_2 &tri) const
{
    return CGAL::squared_distance(*this, tri);
}
Real TriangleEx::squared_distance(const Iso_rectangle_2 &rect) const
{
    return geo::squared_distance(*this, rect);
}
Real TriangleEx::squared_distance(const Polygon_2 &poly) const
{
    return geo::squared_distance(*this, poly);
}
Real TriangleEx::squared_distance(const ::Point_2 &v) const
{
    return CGAL::squared_distance(*this, v);
}
::Point_2 TriangleEx::centroid() const
{
    return CGAL::centroid(*this);
}


// IsoRect
// =======

void IsoRectEx::translate(const ::Vector_2 &dv)
{
    Iso_rectangle_2::operator=(Iso_rectangle_2(min() + dv, max() + dv));
}
bool IsoRectEx::transformPreservesType(const CGAL::Aff_transformation_2<Kernel> &T) const
{
    auto cs = std::abs(T.hm(0,0));
    return (cs <= 0x1.0p-20) || ((1.0 - cs) <= 0x1.0p-20);
}
void IsoRectEx::transform(const CGAL::Aff_transformation_2<Kernel> &T)
{
    if (!transformPreservesType(T))
        throw std::invalid_argument("isorect can only be rotated by multiples of 90°");
    Iso_rectangle_2::operator=(Iso_rectangle_2::transform(T));
}
AShape *IsoRectEx::transformType(const CGAL::Aff_transformation_2<Kernel> &T)
{
    AShape *A = this;
    if (transformPreservesType(T)) {
        Iso_rectangle_2::operator=(Iso_rectangle_2::transform(T));
    } else {
        Polygon_2 P;
        P.push_back(vertex(0));
        P.push_back(vertex(1));
        P.push_back(vertex(2));
        P.push_back(vertex(3));
        A = new PolygonEx(std::move(P));
        A->transform(T);
    }
    return A;
}
bool IsoRectEx::contains(const ::Point_2 &v) const
{
    return has_on_bounded_side(v);
}
Real IsoRectEx::squared_distance(const Segment_2 &s) const
{
    return geo::squared_distance(*this, s);
}
Real IsoRectEx::squared_distance(const WideSegment_25 &s) const
{
    return geo::squared_distance(*this, s);
}
Real IsoRectEx::squared_distance(const Circle_2 &circle) const
{
    return geo::squared_distance(circle, *this);
}
Real IsoRectEx::squared_distance(const Triangle_2 &tri) const
{
    return geo::squared_distance(tri, *this);
}
Real IsoRectEx::squared_distance(const Iso_rectangle_2 &rect) const
{
    return geo::squared_distance(*this, rect);
}
Real IsoRectEx::squared_distance(const Polygon_2 &poly) const
{
    return geo::squared_distance(*this, poly);
}
Real IsoRectEx::squared_distance(const ::Point_2 &v) const
{
    return geo::squared_distance(*this, v);
}
bool IsoRectEx::intersects(const Circle_2 &circle) const
{
    return CGAL::do_intersect(*this, circle);
}
bool IsoRectEx::intersects(const Triangle_2 &A) const
{
    return CGAL::do_intersect(*this, A);
}
bool IsoRectEx::intersects(const Iso_rectangle_2 &rect) const
{
    return CGAL::do_intersect(*this, rect);
}
bool IsoRectEx::intersects(const Polygon_2 &poly) const
{
    return squared_distance(poly) <= 0.0;
}
bool IsoRectEx::intersects(const WideSegment_25 &s) const
{
    return squared_distance(s) <= 0.0;
}
::Point_2 IsoRectEx::centroid() const
{
    return ::Point_2((xmin() + xmax()) * 0.5, (ymin() + ymax()) * 0.5);
}
Real IsoRectEx::lengthOutside(const Segment_2 &s) const
{
    const auto x = CGAL::intersection(s, *this);
    if (!x.has_value())
        return 0.0;
    const Real l = std::sqrt(s.squared_length());

    const Segment_2 *si = std_get_for_cgal<Segment_2>(&*x);
    if (!si)
        return l;
    return l - std::sqrt(si->squared_length());
}


// Polygon
// =======

void PolygonEx::translate(const ::Vector_2 &dv)
{
    for (uint i = 0; i < size(); ++i)
        (*this)[i] = (*this)[i] + dv;
}
void PolygonEx::transform(const CGAL::Aff_transformation_2<Kernel> &T)
{
    Polygon_2::operator=(CGAL::transform(T, *this));
}
bool PolygonEx::contains(const ::Point_2 &v) const
{
    return has_on_bounded_side(v);
}
Real PolygonEx::squared_distance(const Segment_2 &s) const
{
    return geo::squared_distance(*this, s);
}
Real PolygonEx::squared_distance(const WideSegment_25 &s) const
{
    return geo::squared_distance(*this, s);
}
Real PolygonEx::squared_distance(const Circle_2 &circle) const
{
    return geo::squared_distance(circle, *this);
}
Real PolygonEx::squared_distance(const Triangle_2 &tri) const
{
    return geo::squared_distance(tri, *this);
}
Real PolygonEx::squared_distance(const Iso_rectangle_2 &rect) const
{
    return geo::squared_distance(rect, *this);
}
Real PolygonEx::squared_distance(const ::Point_2 &v) const
{
    return geo::squared_distance(*this, v);
}
Real PolygonEx::squared_distance(const Polygon_2 &poly) const
{
    return geo::squared_distance(*this, poly);
}
bool PolygonEx::intersects(const Circle_2 &circle) const
{
    return squared_distance(circle) <= 0.0;
}
bool PolygonEx::intersects(const Triangle_2 &A) const
{
    return squared_distance(A) <= 0.0;
}
bool PolygonEx::intersects(const Iso_rectangle_2 &rect) const
{
    return squared_distance(rect) <= 0.0;
}
bool PolygonEx::intersects(const Polygon_2 &poly) const
{
    return squared_distance(poly) <= 0.0;
}
bool PolygonEx::intersects(const WideSegment_25 &s) const
{
    return squared_distance(s) <= 0.0;
}
::Point_2 PolygonEx::centroid() const
{
    return CGAL::centroid(vertices_begin(), vertices_end());
}
PolygonEx PolygonEx::grown(Real ds, Real eps) const
{
    return PolygonEx(grow(*this, ds, eps));
}


// Wide Segment
// ============

Segment_2 WideSegment_25::s2_extended(uint mask) const
{
    const auto v = mBase.getDirection(mHW);
    return Segment_2((mask & 0x1) ? (source_2() - v) : source_2(),
                     (mask & 0x2) ? (target_2() + v) : target_2());
}

Bbox_2 WideSegment_25::bbox() const
{
    const auto ds = mBase.getDirection(mHW);
    const auto dw = mBase.getPerpendicularCCW(mHW);
    const auto v0 = source_2() - ds;
    const auto v1 = target_2() + ds;
    return Bbox_2(std::min(v0.x(), v1.x()) - std::abs(dw.x()),
                  std::min(v0.y(), v1.y()) - std::abs(dw.y()),
                  std::max(v0.x(), v1.x()) + std::abs(dw.x()),
                  std::max(v0.y(), v1.y()) + std::abs(dw.y()));
}

void WideSegment_25::translate(const ::Vector_2 &dv)
{
    mBase = Segment_25(source_2() + dv, target_2() + dv, mBase.z());
}
void WideSegment_25::transform(const CGAL::Aff_transformation_2<Kernel> &T)
{
    mBase = Segment_25(mBase.s2().transform(T), mBase.z());
    mBase.rectify(math::Radians(0.25));
}
bool WideSegment_25::contains(const ::Point_2 &v) const
{
    return CGAL::squared_distance(mBase.s2(), v) < (mHW * mHW);
}
Real WideSegment_25::squared_distance(const Segment_2 &s) const
{
    return geo::squared_distance(*this, s);
}
Real WideSegment_25::squared_distance(const WideSegment_25 &s) const
{
    return geo::squared_distance(*this, s);
}
Real WideSegment_25::squared_distance(const Circle_2 &circle) const
{
    return geo::squared_distance(circle, *this);
}
Real WideSegment_25::squared_distance(const Triangle_2 &tri) const
{
    return geo::squared_distance(tri, *this);
}
Real WideSegment_25::squared_distance(const Iso_rectangle_2 &rect) const
{
    return geo::squared_distance(rect, *this);
}
Real WideSegment_25::squared_distance(const ::Point_2 &v) const
{
    return geo::squared_distance(*this, v);
}
Real WideSegment_25::squared_distance(const Polygon_2 &poly) const
{
    return geo::squared_distance(poly, *this);
}
bool WideSegment_25::intersects(const Circle_2 &circle) const
{
    return squared_distance(circle) <= 0.0;
}
bool WideSegment_25::intersects(const Triangle_2 &A) const
{
    return squared_distance(A) <= 0.0;
}
bool WideSegment_25::intersects(const Iso_rectangle_2 &rect) const
{
    return squared_distance(rect) <= 0.0;
}
bool WideSegment_25::intersects(const Polygon_2 &poly) const
{
    return squared_distance(poly) <= 0.0;
}
bool WideSegment_25::intersects(const WideSegment_25 &s) const
{
    if (z() != s.z())
        return false;
    return CGAL::squared_distance(base().s2(), s.base().s2()) < math::squared(mHW + s.mHW);
}
bool WideSegment_25::violatesClearance2D(const WideSegment_25 &s, Real c, Point_25 *x) const
{
    const auto d2 = CGAL::squared_distance(base().s2(), s.base().s2());
    const auto e = mHW + s.mHW + c;
    const bool v = d2 < (e * e);
    if (v && x)
        *x = Point_25(d2 == 0 ? geo::closest_point(base().s2(), s.base().s2()) : geo::closest_endpoint(base().s2(), s.base().s2()), z());
    return v;
}
bool WideSegment_25::violatesClearance(const WideSegment_25 &s, Real c, Point_25 *x) const
{
    return (z() == s.z()) && violatesClearance2D(s, c, x);
}
int WideSegment_25::joins2D(const Point_2 &v, Real leeway2) const
{
    if (source_2() == v)
        return -1;
    if (target_2() == v)
        return +1;
    Real ds = CGAL::squared_distance(source_2(), v);
    Real dt = CGAL::squared_distance(target_2(), v);
    if (leeway2 < 0.0)
        leeway2 = mHW * mHW;
    if (ds < dt)
        return (ds <= leeway2) ? -2 : 0;
    return (dt <= leeway2) ? 2 : 0;
}
WideSegment_25 WideSegment_25::grown(Real ds) const
{
    const Vector_2 v = base().getDirection(ds);
    return WideSegment_25(source_2() - v, target_2() + v, z(), mHW + ds);
}
void WideSegment_25::nudge(int s, int t)
{
    const auto v = (source_2() != target_2()) ? mBase.s2().to_vector() * (halfWidth() * 0x1.0p-4) : Vector_2(0x1.0p-10, 0.0);
    mBase = Segment_25(source_2() + v * s,
                       target_2() + v * t, z());
}


AShape *AShape::transformType(const CGAL::Aff_transformation_2<Kernel> &T)
{
    transform(T);
    return this;
}


// PRINT

std::string CircleEx::str() const
{
    std::stringstream ss;
    ss << "Circle{" << center() << '+' << std::sqrt(squared_radius()) << '}';
    return ss.str();
}
std::string TriangleEx::str() const
{
    std::stringstream ss;
    ss << "Triangle{" << vertex(0) << '-' << vertex(1) << '-' << vertex(2) << '}';
    return ss.str();
}
std::string IsoRectEx::str() const
{
    std::stringstream ss;
    ss << "IsoRect{" << min() << '-' << max() << '}';
    return ss.str();
}
std::string PolygonEx::str() const
{
    if (!size())
        return "Polygon{}";
    std::stringstream ss;
    ss << (is_convex() ? "Convex" : "Concave");
    ss << "Polygon{";
    for (uint i = 0; i < size() - 1; ++i)
        ss << vertex(i) << '-';
    ss << vertex(size() - 1) << '}';
    return ss.str();
}
std::string WideSegment_25::str() const
{
    std::stringstream ss;
    ss << '[' << source() << '_' << target() << "|z=" << z() << "|hw=" << halfWidth() << ']';
    return ss.str();
}


// PYTHON

PyObject *CircleEx::getPy() const
{
    auto py = PyTuple_New(4);
    if (!py)
        return 0;
    PyTuple_SetItem(py, 0, py::String("circle"));
    PyTuple_SetItem(py, 1, PyFloat_FromDouble(std::sqrt(squared_radius())));
    PyTuple_SetItem(py, 2, PyFloat_FromDouble(center().x()));
    PyTuple_SetItem(py, 3, PyFloat_FromDouble(center().y()));
    return py;
}
PyObject *TriangleEx::getPy() const
{
    auto py = PyTuple_New(4);
    if (!py)
        return 0;
    PyTuple_SetItem(py, 0, py::String("triangle"));
    PyTuple_SetItem(py, 1, *py::Object(vertex(0)));
    PyTuple_SetItem(py, 2, *py::Object(vertex(1)));
    PyTuple_SetItem(py, 3, *py::Object(vertex(2)));
    return py;
}
PyObject *IsoRectEx::getPy() const
{
    auto py = PyTuple_New(5);
    if (!py)
        return 0;
    PyTuple_SetItem(py, 0, py::String("rect_iso"));
    PyTuple_SetItem(py, 1, PyFloat_FromDouble(xmin()));
    PyTuple_SetItem(py, 2, PyFloat_FromDouble(ymin()));
    PyTuple_SetItem(py, 3, PyFloat_FromDouble(xmax()));
    PyTuple_SetItem(py, 4, PyFloat_FromDouble(ymax()));
    return py;
}
PyObject *PolygonEx::getPy() const
{
    auto py = PyTuple_New(2);
    auto vs = PyList_New(size());
    if (!py)
        return 0;
    PyTuple_SetItem(py, 0, py::String("polygon"));
    PyTuple_SetItem(py, 1, vs);
    uint i = 0;
    for (auto I = vertices_begin(); I != vertices_end(); ++I)
        PyList_SetItem(vs, i++, *py::Object(*I));
    return py;
}
PyObject *WideSegment_25::getPy() const
{
    auto py = PyTuple_New(7);
    PyTuple_SetItem(py, 0, py::String("wide_segment"));
    PyTuple_SetItem(py, 1, PyFloat_FromDouble(source().x()));
    PyTuple_SetItem(py, 2, PyFloat_FromDouble(source().y()));
    PyTuple_SetItem(py, 3, PyFloat_FromDouble(target().x()));
    PyTuple_SetItem(py, 4, PyFloat_FromDouble(target().y()));
    PyTuple_SetItem(py, 5, PyLong_FromLong(z()));
    PyTuple_SetItem(py, 6, PyFloat_FromDouble(width()));
    return py;
}
PyObject *WideSegment_25::getShortPy() const
{
    auto py = PyTuple_New(6);
    PyTuple_SetItem(py, 0, PyFloat_FromDouble(source().x()));
    PyTuple_SetItem(py, 1, PyFloat_FromDouble(source().y()));
    PyTuple_SetItem(py, 2, PyFloat_FromDouble(target().x()));
    PyTuple_SetItem(py, 3, PyFloat_FromDouble(target().y()));
    PyTuple_SetItem(py, 4, PyLong_FromLong(z()));
    PyTuple_SetItem(py, 5, PyFloat_FromDouble(width()));
    return py;
}

WideSegment_25 WideSegment_25::fromPy(PyObject *py)
{
    py::Object data(py);
    if (!data || !data.isTuple(6))
        throw std::invalid_argument("expected a tuple(source_x,source_y,target_x,target_y,z,width) for segment");
    auto sx = data.elem(0).toDouble();
    auto sy = data.elem(1).toDouble();
    auto tx = data.elem(2).toDouble();
    auto ty = data.elem(3).toDouble();
    auto z = data.elem(4);
    auto w = data.elem(5);
    if (!z.isLong())
        throw std::invalid_argument("segment.z must be an integer");
    return WideSegment_25(Point_2(sx, sy), Point_2(tx, ty), z.asLong(), w.toDouble() * 0.5);
}
