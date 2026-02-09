
#include "LayoutArea.hpp"
#include "Object.hpp"
#include "Via.hpp"

LayoutArea::LayoutArea()
{
    setRect(Point_2(0,0), Point_2(0,0));
}

void LayoutArea::setRect(const Point_2 &min, const Point_2 &max)
{
    mRect = Iso_rectangle_2(min, max);
    mBox = mRect.bbox();
    mSize = max - min;
    mDiagLen = std::sqrt(CGAL::scalar_product(mSize, mSize));
}

void LayoutArea::setPolygonBbox(const Polygon_2 &A)
{
    setBox(A.bbox());
}

void LayoutArea::setBox(const Bbox_2 &box)
{
    setRect(Point_2(box.xmin(), box.ymin()),
            Point_2(box.xmax(), box.ymax()));
}

bool LayoutArea::isInside(const Via &V) const
{
    return (V.zmax() <= mMaxLayer) && geo::bbox_contains(bbox(), V.bbox());
}

bool LayoutArea::isBboxInside(const Object &A) const
{
    return (uint(A.maxLayer()) <= mMaxLayer) && geo::bbox_contains(bbox(), A.getBbox());
}
