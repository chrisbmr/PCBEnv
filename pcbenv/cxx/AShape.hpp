
#ifndef GYM_PCB_ASHAPE_H
#define GYM_PCB_ASHAPE_H

#include "Py.hpp"
#include "Geometry.hpp"
#include "Math/Misc.hpp"
#include <cassert>
#include <sstream>

class CircleEx;
class IsoRectEx;
class TriangleEx;
class PolygonEx;
class WideSegment_25;

/**
 * Abstract shape class, or simply "a shape".
 */
class AShape
{
public:
    virtual ~AShape() { }

    template<typename T>       T *as()       { return dynamic_cast<      T *>(this); }
    template<typename T> const T *as() const { return dynamic_cast<const T *>(this); }

    virtual AShape *clone() const = 0;

    /**
     * For rendering:
     */
    virtual uint vertexCount() const = 0;
    virtual uint indexCountWithRestart() const { return vertexCount() + 1; }
    virtual bool canDrawAsTriFan() const { return true; }

    virtual bool isRound() const { return false; }

    virtual Real unsignedArea() const = 0;

    virtual Bbox_2 bbox() const = 0;

    Real boundingRadius() const;

    /**
     * Check whether the transformation would change the shape's underlying type (sorry).
     * - Iso_rectangle_2 may become a Polygon_2 when (angle % 90) != 0
     * - Circle_2 may become an ellipse when scaled (this is not supported)
     */
    virtual bool transformPreservesType(const CGAL::Aff_transformation_2<Kernel>&) const { return true; }

    /**
     * Use transformType() if you are not sure whether the transformation preserves the shape type.
     */
    virtual void transform(const CGAL::Aff_transformation_2<Kernel>&) = 0;
    virtual void translate(const Vector_2&) = 0;

    /**
     * Transform this shape without restrictions.
     * @return @this or a new AShape.
     */
    virtual AShape *transformType(const CGAL::Aff_transformation_2<Kernel>&);

    virtual bool contains(const Point_2&) const = 0;

    virtual Real squared_distance(const AShape&) const = 0;

    virtual Real squared_distance(const ::Point_2&) const = 0;
    virtual Real squared_distance(const Segment_2&) const = 0;
    virtual Real squared_distance(const Circle_2&) const = 0;
    virtual Real squared_distance(const Iso_rectangle_2&) const = 0;
    Real squared_distance(const Bbox_2&) const;
    virtual Real squared_distance(const Triangle_2&) const = 0;
    virtual Real squared_distance(const Polygon_2&) const = 0;
    virtual Real squared_distance(const WideSegment_25&) const = 0;

    virtual bool intersects(const AShape&) const = 0;

    virtual bool intersects(const Circle_2&) const = 0;
    virtual bool intersects(const Iso_rectangle_2&) const = 0;
    bool intersects(const Bbox_2&) const;
    virtual bool intersects(const Triangle_2&) const = 0;
    virtual bool intersects(const Polygon_2&) const = 0;
    virtual bool intersects(const WideSegment_25&) const = 0;

    virtual ::Point_2 centroid() const = 0;

    virtual std::string str() const = 0;

    /**
     * Return a Python representation of this object.
     * Formats:
     * - ('circle', radius, x, y)
     * - ('triangle', v0, v1, v2) where v is (x,y,z)
     * - ('rect_iso', xmin, ymin, xmax, ymax)
     * - ('wide_segment', x0, y0, x1, y1, z, width)
     * - ('polygon', [v0, v1, v2, v3, ...]) where v is (x,y,z)
     */
    virtual PyObject *getPy() const = 0;
};

inline Real AShape::boundingRadius() const
{
    const auto B = bbox();
    return std::max(B.xmax() - B.xmin(), B.ymax() - B.ymin());
}

inline bool AShape::intersects(const Bbox_2 &bbox) const
{
    return intersects(Iso_rectangle_2(bbox));
}
inline Real AShape::squared_distance(const Bbox_2 &bbox) const
{
    return squared_distance(Iso_rectangle_2(bbox));
}

class CircleEx : public AShape, public Circle_2
{
public:
    CircleEx(const Circle_2 &base) : Circle_2(base) { }

    AShape *clone() const override { return new CircleEx(*this); }

    CircleEx grown(Real) const;

    uint vertexCount() const override { return 4; } /**< assumes this is drawn as a quad */
    bool canDrawAsTriFan() const override { return true; }

    bool isRound() const override { return true; }

    Real unsignedArea() const override { return squared_radius() * std::numbers::pi; }

    Bbox_2 bbox() const override { return Circle_2::bbox(); }

    bool transformPreservesType(const CGAL::Aff_transformation_2<Kernel>&) const override;

    void transform(const CGAL::Aff_transformation_2<Kernel>&) override;
    void translate(const ::Vector_2&) override;

    bool contains(const ::Point_2&) const override;

    Real squared_distance(const AShape &S) const override { return S.squared_distance(*static_cast<const Circle_2 *>(this)); }

    Real squared_distance(const ::Point_2&) const override;
    Real squared_distance(const Segment_2&) const override;
    Real squared_distance(const Circle_2&) const override;
    Real squared_distance(const Iso_rectangle_2&) const override;
    Real squared_distance(const Triangle_2&) const override;
    Real squared_distance(const Polygon_2&) const override;
    Real squared_distance(const WideSegment_25&) const override;

    bool intersects(const AShape &S) const override { return S.intersects(*static_cast<const Circle_2 *>(this)); }

    bool intersects(const Circle_2&) const override;
    bool intersects(const Iso_rectangle_2&) const override;
    bool intersects(const Triangle_2&) const override;
    bool intersects(const Polygon_2&) const override;
    bool intersects(const WideSegment_25&) const override;

    ::Point_2 centroid() const override { return center(); }

    std::string str() const override;
    PyObject *getPy() const override;
};
inline CircleEx CircleEx::grown(Real dr) const
{
    return CircleEx(Circle_2(center(), math::squared(std::sqrt(squared_radius()) + dr)));
}

class TriangleEx : public AShape, public Triangle_2
{
public:
    TriangleEx(const Triangle_2 &base) : Triangle_2(base) { }
    TriangleEx(Triangle_2 &&base) : Triangle_2(base) { }

    AShape *clone() const override { return new TriangleEx(*this); }

    uint vertexCount() const override { return 3; }

    Real unsignedArea() const override { return std::abs(area()); }

    Bbox_2 bbox() const override { return Triangle_2::bbox(); }

    void transform(const CGAL::Aff_transformation_2<Kernel>&) override;
    void translate(const ::Vector_2&) override;

    bool contains(const ::Point_2&) const override;

    Real squared_distance(const AShape &S) const override { return S.squared_distance(*static_cast<const Triangle_2 *>(this)); }

    Real squared_distance(const ::Point_2&) const override;
    Real squared_distance(const Segment_2&) const override;
    Real squared_distance(const Circle_2 &C) const override;
    Real squared_distance(const Iso_rectangle_2&) const override;
    Real squared_distance(const Triangle_2&) const override;
    Real squared_distance(const Polygon_2&) const override;
    Real squared_distance(const WideSegment_25&) const override;

    bool intersects(const AShape &S) const override { return S.intersects(*static_cast<const Triangle_2 *>(this)); }

    bool intersects(const Circle_2&) const override;
    bool intersects(const Iso_rectangle_2&) const override;
    bool intersects(const Triangle_2&) const override;
    bool intersects(const Polygon_2&) const override;
    bool intersects(const WideSegment_25&) const override;

    ::Point_2 centroid() const override;

    std::string str() const override;
    PyObject *getPy() const override;
};

class IsoRectEx : public AShape, public Iso_rectangle_2
{
public:
    IsoRectEx(const Iso_rectangle_2 &base) : Iso_rectangle_2(base) { }
    IsoRectEx(Iso_rectangle_2 &&base) : Iso_rectangle_2(base) { }

    IsoRectEx(const Bbox_2 &bbox) : Iso_rectangle_2(bbox) { }

    AShape *clone() const override { return new IsoRectEx(*this); }

    uint vertexCount() const override { return 4; }

    Real unsignedArea() const override { return std::abs(area()); }

    Bbox_2 bbox() const override { return Iso_rectangle_2::bbox(); }

    bool transformPreservesType(const CGAL::Aff_transformation_2<Kernel>&) const override;

    void transform(const CGAL::Aff_transformation_2<Kernel>&) override;
    void translate(const ::Vector_2&) override;

    AShape *transformType(const CGAL::Aff_transformation_2<Kernel>&) override;

    bool contains(const ::Point_2&) const override;

    Real squared_distance(const AShape &S) const override { return S.squared_distance(*static_cast<const Iso_rectangle_2 *>(this)); }

    Real squared_distance(const ::Point_2&) const override;
    Real squared_distance(const Segment_2&) const override;
    Real squared_distance(const Circle_2 &C) const override;
    Real squared_distance(const Triangle_2 &T) const override;
    Real squared_distance(const Iso_rectangle_2&) const override;
    Real squared_distance(const Polygon_2&) const override;
    Real squared_distance(const WideSegment_25&) const override;

    bool intersects(const AShape &S) const override { return S.intersects(*static_cast<const Iso_rectangle_2 *>(this)); }

    bool intersects(const Circle_2 &C) const override;
    bool intersects(const Triangle_2 &T) const override;
    bool intersects(const Iso_rectangle_2&) const override;
    bool intersects(const Polygon_2&) const override;
    bool intersects(const WideSegment_25&) const override;

    /**
     * Return the length of the part of the segment outside of this rectangle.
     */
    Real lengthOutside(const Segment_2&) const;

    ::Point_2 centroid() const override;

    std::string str() const override;
    PyObject *getPy() const override;
};

class PolygonEx : public AShape, public Polygon_2
{
public:
    PolygonEx(const Polygon_2 &base) : Polygon_2(base) { }
    PolygonEx(Polygon_2 &&base) : Polygon_2(base) { }

    AShape *clone() const override { return new PolygonEx(*this); }

    PolygonEx grown(Real size, Real epsilon = 0.125) const;

    /**
     * Grow (expand) the given polygon outwards.
     * This is not well-defined so the quality of the result will vary.
     * @param size Absolute size of growth.
     * @param epsilon Tolerance for CGAL::approximated_offset_2.
     */
    static Polygon_2 grow(const Polygon_2&, Real size, Real epsilon = 0.125);

    uint vertexCount() const override { return size(); }
    uint indexCountWithRestart() const override { return is_convex() ? (vertexCount() + 1) : 0; }
    bool canDrawAsTriFan() const override { return is_convex(); }

    Real unsignedArea() const override { return std::abs(area()); }

    Bbox_2 bbox() const override { return Polygon_2::bbox(); }

    void transform(const CGAL::Aff_transformation_2<Kernel>&) override;
    void translate(const ::Vector_2&) override;

    bool contains(const ::Point_2&) const override;

    Real squared_distance(const AShape &S) const override { return S.squared_distance(*static_cast<const Polygon_2 *>(this)); }

    Real squared_distance(const ::Point_2&) const override;
    Real squared_distance(const Segment_2&) const override;
    Real squared_distance(const Circle_2 &C) const override;
    Real squared_distance(const Iso_rectangle_2 &R) const override;
    Real squared_distance(const Triangle_2 &T) const override;
    Real squared_distance(const Polygon_2&) const override;
    Real squared_distance(const WideSegment_25&) const override;

    bool intersects(const AShape &S) const override { return S.intersects(*static_cast<const Polygon_2 *>(this)); }

    bool intersects(const Circle_2 &C) const override;
    bool intersects(const Iso_rectangle_2 &R) const override;
    bool intersects(const Triangle_2 &T) const override;
    bool intersects(const Polygon_2&) const override;
    bool intersects(const WideSegment_25&) const override;

    ::Point_2 centroid() const override;

    std::string str() const override;
    PyObject *getPy() const override;

private:
    static Polygon_2 _grow_offset(const Polygon_2&, Real size, Real epsilon = 0.125);
    static Polygon_2 _grow_stupid(const Polygon_2&, Real size);
};

/**
 * A segment in 2.5D with a given constant width and round caps with radius = width/2.
 * This is the Minkowski sum of a segment and a circle.
 * The caps extend beyond the endpoints and are not accounted for in the segment's length.
 */
class WideSegment_25 : public AShape
{
public:
    WideSegment_25(const Segment_25 &s, Real halfWidth) : mBase(s), mHW(halfWidth) { }
    WideSegment_25(const Segment_2 &s, int z, Real halfWidth) : mBase(s, z), mHW(halfWidth) { }
    WideSegment_25(const Point_2 &v0, const Point_2 &v1, int z, Real halfWidth) : mBase(v0, v1, z), mHW(halfWidth) { }

    void _setBase(const Segment_25 &s) { mBase = s; }

    Segment_25& _base() { return mBase; }
    const Segment_25& base() const { return mBase; }

    /**
     * Get a segment extended by the radius of the caps (half its width).
     * @param mask 0x1 to extend the source end | 0x2 to extend the target end
     */
    Segment_2 s2_extended(uint mask = 0x3) const;

    Real halfWidth() const { return mHW; }
    Real width() const { return mHW + mHW; }
    Real squared_width() const { return width() * width(); }

    int z() const { return mBase.z(); }
    Point_25 source() const { return mBase.source(); }
    Point_25 target() const { return mBase.target(); }
    const Point_2& source_2() const { return mBase.source_2(); }
    const Point_2& target_2() const { return mBase.target_2(); }

    Circle_2 sourceCap() const { return Circle_2(source_2(), mHW * mHW); }
    Circle_2 targetCap() const { return Circle_2(target_2(), mHW * mHW); }

    /**
     * Test whether the segment ends at the specified point or within a certain distance.
     * @param leewaySquared maximum squared distance from end, < 0 defaults to halfwidth
     * @return -1 for source, 1 for target, 0 for no contact or too far from the ends
     */
    int joins2D(const Point_2&, Real leewaySquared = -1.0) const;

    bool widerThanBaseLen() const { return squared_width() > mBase.squared_length(); }

    /**
     * Get a vector perpendicular to the segment of length halfwidth + @delta.
     */
    ::Vector_2 getHalfWidthSpan(Real delta = 0.0) const { return mBase.getPerpendicularCCW(mHW + delta); }

    /**
     * Get a new segment perpendicular to this one with width and length swapped.
     * @param deltaHW value added to new halfwidth
     * @param deltaL value added to new length
     */
    WideSegment_25 swapWL(Real deltaHW = 0.0, Real deltaL = 0.0) const;

    WideSegment_25 opposite() const { return WideSegment_25(mBase.opposite(), halfWidth()); } /**< swap endpoints */
    WideSegment_25 orderedX() const { return mBase.isXOrdered() ? *this : opposite(); }
    WideSegment_25 orderedY() const { return mBase.isYOrdered() ? *this : opposite(); }

    AShape *clone() const override { return new WideSegment_25(*this); }

    uint vertexCount() const override { return 4; }
    bool canDrawAsTriFan() const override { return true; }

    bool isRound() const override { return true; }

    Real unsignedArea() const override { return width() * mBase.length() + mHW * mHW * std::numbers::pi; }

    Bbox_2 bbox() const override;

    void transform(const CGAL::Aff_transformation_2<Kernel>&) override;
    void translate(const ::Vector_2&) override;

    /**
     * Move the source/target point by @s/@t times (target - source) * halfwidth / 16.
     */
    void nudge(int s, int t);

    /**
     * If the angle of this segment falls with @angleTolerance of the x- or y-axis, align it.
     */
    WideSegment_25& rectify(Real angleTolerance) { mBase.rectify(angleTolerance); return *this; }

    /**
     * Get this segment extended by the specified value in both directions as well as its halfwidth.
     */
    WideSegment_25 grown(Real) const;

    bool contains(const ::Point_2&) const override;

    Real squared_distance(const AShape &S) const override { return S.squared_distance(*static_cast<const WideSegment_25 *>(this)); }

    Real squared_distance(const ::Point_2&) const override;
    Real squared_distance(const Segment_2&) const override;
    Real squared_distance(const Circle_2 &C) const override;
    Real squared_distance(const Iso_rectangle_2 &R) const override;
    Real squared_distance(const Triangle_2 &T) const override;
    Real squared_distance(const Polygon_2 &P) const override;
    Real squared_distance(const WideSegment_25&) const override;

    bool intersects(const AShape &S) const override { return S.intersects(*static_cast<const WideSegment_25 *>(this)); }

    bool intersects(const Circle_2 &C) const override;
    bool intersects(const Iso_rectangle_2 &R) const override;
    bool intersects(const Triangle_2 &T) const override;
    bool intersects(const Polygon_2 &P) const override;
    bool intersects(const WideSegment_25&) const override;

    /**
     * Check if another segment falls within @clearance of this one.
     * If nonzero, set the pointer value to the intersection or, if none, the endpoint with the smallest point to segment distance.
     */
    bool violatesClearance(const WideSegment_25&, Real clearance, Point_25 * = 0) const;
    bool violatesClearance2D(const WideSegment_25&, Real clearance, Point_25 * = 0) const;

    ::Point_2 centroid() const override { return source_2() + mBase.s2().to_vector() * 0.5; }

    static WideSegment_25 fromPy(PyObject *); /**< Expects (x0,y0,x1,y1,z,w). */

    std::string str() const override;
    PyObject *getPy() const override;

    PyObject *getShortPy() const; /** (x0,y0,x1,y1,z,w) */
protected:
    Segment_25 mBase;
    Real mHW;
};
inline WideSegment_25 WideSegment_25::swapWL(Real deltaHW, Real deltaL) const
{
    const auto M = mBase.source_2() + mBase.s2().to_vector() * 0.5;
    const auto v = getHalfWidthSpan(deltaL * 0.5);
    return WideSegment_25(M + v, M - v, z(), mBase.length() * 0.5 + deltaHW);
}

inline std::ostream& operator<<(std::ostream &os, const WideSegment_25 &s)
{
    return os << s.str();
}

#endif // GYM_PCB_ASHAPE_H
