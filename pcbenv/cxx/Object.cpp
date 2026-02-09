
#include "Py.hpp"
#include "Log.hpp"
#include "Via.hpp"
#include "Object.hpp"

constexpr const bool AlwaysTurnPolygonsIntoBoxes = false;

Object::Object(Object *parent, const std::string &name)
    : mParent(parent),
      mName(name)
{
}

Object::~Object()
{
    for (auto I : mChildren)
        delete I;
}

void Object::addChild(Object &child)
{
    assert(child.getParent() == this);
    child.mChildIndex = mChildren.size();
    mChildren.push_back(&child);
}

void Object::removeChild(Object &child)
{
    if (child.getParent() != this)
        throw std::runtime_error("tried to remove child from wrong parent");
    const uint i = child.mChildIndex;
    assert(i < mChildren.size() && mChildren[i] == &child);
    mChildren[i] = mChildren.back();
    mChildren[i]->mChildIndex = i;
    mChildren.pop_back();
}

void Object::setLayer(int m)
{
    assert(m >= -1 && m <= 255);
    mLayerRange[0] = mLayerRange[1] = m;
}
void Object::setLayerRange(int k, int m)
{
    assert(m >= -1 && k >= -1 && m <= 255 && k <= 255);
    mLayerRange[0] = std::min(k, m);
    mLayerRange[1] = std::max(k, m);
}

void Object::setShape(const Circle_2 &circle)
{
    mShape = std::make_unique<CircleEx>(circle);
    update();
}
void Object::setShape(const Triangle_2 &triangle)
{
    mShape = std::make_unique<TriangleEx>(triangle);
    update();
}
void Object::setShape(const Iso_rectangle_2 &isoRect)
{
    mShape = std::make_unique<IsoRectEx>(isoRect);
    update();
}
void Object::setShape(const WideSegment_25 &s, Real angleTolerance)
{
    if (s.source_2() == s.target_2()) {
        const auto r = s.halfWidth();
        const auto c = s.source_2();
        mShape = std::make_unique<CircleEx>(Circle_2(c, r * r));
    } else if (angleTolerance > 0.0) {
        auto sr = s.base();
        mShape = std::make_unique<WideSegment_25>(sr.rectify(angleTolerance), s.halfWidth());
    } else {
        mShape = std::make_unique<WideSegment_25>(s);
    }
    update();
}
void Object::setShape(const Polygon_2 &polygon)
{
    if (AlwaysTurnPolygonsIntoBoxes) {
        WARN("Replacing polygon shape with " << polygon.size() << " vertices with its bounding box (area ratio = " << (geo::bbox_area(polygon.bbox()) / polygon.area()) << ").");
        setShape(Iso_rectangle_2(polygon.bbox()));
    } else {
        mShape = std::make_unique<PolygonEx>(polygon);
        update();
    }
}
void Object::setShape(Polygon_2 &&polygon)
{
    if (AlwaysTurnPolygonsIntoBoxes) {
        WARN("Replacing polygon shape with " << polygon.size() << " vertices with its bounding box (area ratio = " << (geo::bbox_area(polygon.bbox()) / polygon.area()) << ").");
        setShape(Iso_rectangle_2(polygon.bbox()));
    } else {
        mShape = std::make_unique<PolygonEx>(std::move(polygon));
        update();
    }
}

void Object::translate(const Vector_2 &dv)
{
    mShape->translate(dv);
    update();
    for (auto I : mChildren)
        I->translate(dv);
}

void Object::transform(const Aff_transformation_2 &T)
{
    auto A = mShape->transformType(T);
    if (mShape.get() != A)
        mShape.reset(A);
    update();
    for (auto I : mChildren)
        I->transform(T);
}

void Object::rotateAround(const Point_2 &refPoint, Real angleRadians)
{
    const auto dv = Vector_2(refPoint.x(), refPoint.y());
    transform(Aff_transformation_2(CGAL::TRANSLATION, dv) *
              Aff_transformation_2(CGAL::ROTATION, std::sin(angleRadians), std::cos(angleRadians)) *
              Aff_transformation_2(CGAL::TRANSLATION, -dv));
}

bool Object::isInsideBbox(const Point_2 &v) const
{
    const auto &bbox = mShape->bbox();
    return
        v.x() >= bbox.xmin() && v.x() <= bbox.xmax() &&
        v.y() >= bbox.ymin() && v.y() <= bbox.ymax();
}
bool Object::isInsideBbox(const Point_2 &v, int zmin, int zmax) const
{
    if (minLayer() > zmax || maxLayer() < zmin)
        return false;
    return isInsideBbox(v);
}

bool Object::isContainerOf(const Object &that) const
{
    const auto O = getBbox();
    const auto o = that.getBbox();
    return (that.minLayer() >= minLayer() &&
            that.maxLayer() <= maxLayer() &&
            o.xmin() > O.xmin() &&
            o.xmax() < O.xmax() &&
            o.ymin() > O.ymin() &&
            o.ymax() < O.ymax());
}

Object *Object::getChildAt(const Point_2 &v) const
{
    for (auto I : mChildren)
        if (I->isInsideBbox(v)) // FIXME: contains
            return I;
    return 0;
}

void Object::setSelected(bool b, uint recursion)
{
    mSelectionFlag = b;
    if (recursion == 1) {
        for (auto I : getChildren())
            I->setSelected(b);
    } else if (recursion > 1) {
        for (auto I : getChildren())
            I->setSelected(b, recursion - 1);
    }
}

std::string Object::getFullName() const
{
    if (getParent())
        return getParent()->getFullName() + '-' + name();
    return name();
}

bool Object::violatesClearance(const Via &V, Real clearance, Point_25 *x) const
{
    if (minLayer() > int(V.zmin()) ||
        maxLayer() < int(V.zmax()))
        return false;
    const Real d2 = mShape->squared_distance(V.getCircle());
    const Real c = clearance + V.radius();
    const bool v = d2 < (c * c);
    if (v && x)
        *x = Point_25(getCenter(), std::max(int(V.zmin()), minLayer()));
    return v;
}
bool Object::violatesClearance(const WideSegment_25 &s, Real clearance, Point_25 *x) const
{
    if (!isOnLayer(s.z()))
        return false;
    const Real d2 = mShape->squared_distance(s.base().s2());
    const Real c = clearance + s.halfWidth();
    const bool v = d2 < (c * c);
    if (v && x)
        *x = Point_25(getCenter(), s.z());
    return v;
}

void Object::copyFrom(CloneEnv &env, const Object &ref)
{
    mCenter = ref.mCenter;
    assert(mChildren.size() == ref.mChildren.size());
    mName = ref.mName;
    mId = ref.mId;
    mChildIndex = ref.mChildIndex;
    mLayerRange[0] = ref.mLayerRange[0];
    mLayerRange[1] = ref.mLayerRange[1];
    mCanRouteInside = ref.mCanRouteInside;
    mClearance = ref.mClearance;
    mShape.reset(ref.mShape->clone());
}


// Python interface

PyObject *Object::getPy(uint depth) const
{
    auto py = PyDict_New();
    if (!py)
        return 0;
    py::Dict_StealItemString(py, "center", *py::Object(mCenter));
    py::Dict_StealItemString(py, "z", (mLayerRange[0] != mLayerRange[1]) ? *py::Object::new_TupleOfLongs(mLayerRange, 2) : PyLong_FromLong(mLayerRange[0]));
    py::Dict_StealItemString(py, "shape", mShape->getPy());
    py::Dict_StealItemString(py, "clearance", PyFloat_FromDouble(mClearance));
    return py;
}
