#ifndef GYM_PCB_LAYOUTAREA_H
#define GYM_PCB_LAYOUTAREA_H

#include "Geometry.hpp"

class Object;
class Via;

/**
 * This class represents the routable area, which currently must be a rectangle.
 */
class LayoutArea
{
public:
    LayoutArea();

    const Vector_2& size() const { return mSize; }
    Real area() const { return mRect.area(); }
    const Bbox_2& bbox() const { return mBox; }
    Point_2 origin() const { return mRect.min(); }

    Real aspectRatio() const { return mSize.x() / mSize.y(); }

    Real diagonalLength() const { return mDiagLen; }

    bool isInside(const Point_2&) const;
    bool isInside(const Point_25&) const;
    bool isInside(const Segment_25&) const;
    bool isInside(const Via&) const;
    bool isBboxInside(const Object&) const;

    void setOrigin(const Point_2 &v0) { setRect(v0, v0 + mSize); }
    void setSize(const Vector_2 &size) { setRect(mRect.min(), mRect.min() + size); }
    void setMaxLayer(uint z) { mMaxLayer = z; }
    void setBox(const Bbox_2&);
    void setRect(const Point_2 &min, const Point_2 &max);
    void setPolygonBbox(const Polygon_2&);

    void expand(const Bbox_2 &box) { setBox(bbox() + box); }
private:
    Iso_rectangle_2 mRect;
    Bbox_2 mBox;
    Vector_2 mSize{0.0, 0.0};
    Real mDiagLen{0.0};
    uint mMaxLayer{0};
};

inline bool LayoutArea::isInside(const Point_25 &v) const
{
    return (uint(v.z()) <= mMaxLayer) && isInside(v.xy());
}
inline bool LayoutArea::isInside(const Point_2 &v) const
{
    return !mRect.has_on_unbounded_side(v);
}
inline bool LayoutArea::isInside(const Segment_25 &s) const
{
    return isInside(s.source()) && isInside(s.target_2());
}

#endif // GYM_PCB_LAYOUTAREA_H
