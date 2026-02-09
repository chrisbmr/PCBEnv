#include "Via.hpp"
#include "AShape.hpp"
#include "Layer.hpp"

bool Via::overlaps(const Via &that, Real clearance, Point_25 *x) const
{
    if (zmin() > that.zmax() ||
        zmax() < that.zmin())
        return false;
    const Real c = radius() + that.radius() + clearance;
    const Real d2 = CGAL::squared_distance(location(), that.location());
    const bool v = d2 < (c * c);
    if (v && x)
        *x = Point_25(location() + Vector_2(location(), that.location()) * (radius() / std::sqrt(d2)), std::max(zmin(), that.zmin()));
    return v;
}

bool Via::overlaps(const WideSegment_25 &s, Real clearance, Point_25 *x) const
{
    if (!onLayer(s.z()))
        return false;
    const Real d = radius() + s.halfWidth() + clearance;
    const bool v = CGAL::squared_distance(location(), s.base().s2()) < (d * d);
    if (v && x)
        *x = Point_25(geo::closest_on(s.base().s2(), location()), s.z());
    return v;
}

bool Via::contains(const Point_25 &v) const
{
    if (v.z() < int(zmin()) || v.z() > int(zmax()))
        return false;
    if (v.xy() == location())
        return true;
    return getCircle().has_on_bounded_side(v.xy());
}

Bbox_2 Via::bbox() const
{
    return Bbox_2(mPoint.x() - mRadius,
                  mPoint.y() - mRadius,
                  mPoint.x() + mRadius,
                  mPoint.y() + mRadius);
}

uint Via::cutLayers(uint z0, uint z1)
{
    return cutLayersFromRange(mLayer[0], mLayer[1], z0, z1);
}

void Via::merge(const Via &via)
{
    assert(mPoint == via.mPoint);
    mLayer[0] = std::min(mLayer[0], via.mLayer[0]);
    mLayer[1] = std::max(mLayer[1], via.mLayer[0]);
    mRadius = std::max(mRadius, via.mRadius);
}

PyObject *Via::getPy() const
{
    auto py = PyDict_New();
    if (!py)
        return 0;
    py::Dict_StealItemString(py, "min_layer", PyLong_FromLong(mLayer[0]));
    py::Dict_StealItemString(py, "max_layer", PyLong_FromLong(mLayer[1]));
    py::Dict_StealItemString(py, "pos", *py::Object(mPoint));
    py::Dict_StealItemString(py, "radius", PyFloat_FromDouble(mRadius));
    return py;
}

PyObject *Via::getShortPy() const
{
    auto py = PyTuple_New(5);
    if (!py)
        return 0;
    PyTuple_SetItem(py, 0, PyFloat_FromDouble(mPoint.x()));
    PyTuple_SetItem(py, 1, PyFloat_FromDouble(mPoint.y()));
    PyTuple_SetItem(py, 2, PyLong_FromLong(zmin()));
    PyTuple_SetItem(py, 3, PyLong_FromLong(zmax()));
    PyTuple_SetItem(py, 4, PyFloat_FromDouble(mRadius));
    return py;
}

Via Via::fromPy(PyObject *py)
{
    py::Object data(py);
    if (!data || !data.isTuple())
        throw std::invalid_argument("expected a tuple(x,y,min_layer,max_layer,radius) for via");
    auto x = data.elem(0);
    auto y = data.elem(1);
    auto z0 = data.elem(2);
    auto z1 = data.elem(3);
    auto r = data.elem(4);
    if (!z0.isLong() || !z1.isLong())
        throw std::invalid_argument("via.zmin/zmax are not integers");
    return Via(Point_2(x.toDouble(), y.toDouble()), z0.asLong(), z1.asLong(), r.toDouble());
}

std::string Via::str() const
{
    std::stringstream ss;
    ss << "(via at " << mPoint << " z=[" << mLayer[0] << ',' << mLayer[1] << "] r=" << mRadius << ')';
    return ss.str();
}
