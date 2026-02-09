#include "PyArray.hpp"
#include "RL/Actions/Grid.hpp"
#include "RL/Agent.hpp"
#include "PCBoard.hpp"
#include "NavGrid.hpp"

namespace actions {

void SetCostMap::setCostsFloatRegion(NavGrid &nav)
{
    if (!mCosts->isTuple(3))
        throw std::invalid_argument("set_cost_map: expected a 3-tuple");
    auto V = mCosts->elem(0);
    auto A = mCosts->elem(1);
    auto B = mCosts->elem(2);
    if (!V.isFloat() || !A.isLongVector(3) || !B.isLongVector(3))
        throw std::invalid_argument("set_cost_map: expected a float cost and two 3D integer bounding box corners");
    nav.setCosts(IBox_3(A.asIPoint_3(), B.asIPoint_3()), V.toDouble());
}
void SetCostMap::setCostsArrayPoint(NavGrid &nav)
{
    assert(mCosts->isTuple(2));
    auto V = mCosts->elem(0);
    auto P = mCosts->elem(1);
    if (!P.isLongVector(3))
        throw std::invalid_argument("set_cost_map: expected an 3D integer grid location");
    FloatNPArray A(*V);
    if (!A.valid())
        throw std::invalid_argument("set_cost_map: expected a numpy float array");
    A.INCREF();
    if (A.degree() != 3 || !A.isPacked())
        throw std::invalid_argument("set_cost_map: expected a packed 3D array [z][y][x]");
    IBox_3 box;
    box.min = P.asIPoint_3();
    box.max = box.min + IVector_3(A.dim(2), A.dim(0), A.dim(0));
    nav.setCosts(box, A.data(), 1.0f);
}
void SetCostMap::setCostsArray(NavGrid &nav)
{
    FloatNPArray A(**mCosts);
    if (!A.valid())
        throw std::invalid_argument("set_cost_map: expected a numpy float array");
    A.INCREF();
    if (A.degree() == 1) {
        if (size_t(A.size()) != nav.getNumPoints() * sizeof(float))
            throw std::invalid_argument("set_cost_map: 1D array must match the grid size");
    } else if (A.degree() == 3 && A.isPacked()) {
        if (A.dim(2) != nav.getSize(0) || A.dim(1) != nav.getSize(1) || A.dim(0) != nav.getSize(2))
            throw std::invalid_argument("set_cost_map: 3D array must match the grid size");
    } else {
        throw std::invalid_argument("set_cost_map: array must be 1D or 3D and packed");
    }
    nav.setCosts(A.data(), 1.0f);
}

Action::Result SetCostMap::performAs(Agent &A, PyObject *arg)
{
    NavGrid &nav = A.getPCB()->getNavGrid();
    if (arg)
        setArgument(arg);
    if (!mCosts) {
        nav.setCosts(1.0f);
    } else if (mCosts->isTuple()) {
        if (mCosts->isTuple(2))
            setCostsArrayPoint(nav);
        else setCostsFloatRegion(nav);
    } else {
        setCostsArray(nav);
    }
    A.countActions(mActionCountIncrement);
    if (true)
        A.getPCB()->setChanged(PCB_CHANGED_NAV_GRID);
    return Action::Result();
}

void SetRouteGuard::addPoint(const Point_25 &v)
{
    Path *L = &mGuard.back();
    if (!L->empty() && L->back().z() != v.z()) {
        if (L->numPoints() < 2)
            L->clear();
        else
            mGuard.emplace_back(Path());
    }
    mGuard.back()._add(v);
}
void SetRouteGuard::setList(py::Object arg)
{
    if (!arg.hasListItem(1))
        return;
    for (uint i = 0; i < arg.listSize(); ++i)
        addPoint(arg.item(i).asPoint_25());
}
void SetRouteGuard::setArray(PyObject *py)
{
    FloatNPArray A(py);
    if (!A.valid())
        throw std::invalid_argument("set_route_guard: expected a 1D or 2D numpy array with a list of 2.5D points");
    A.INCREF();
    if (!((A.degree() == 2 && A.dim(1) == 3 && A.isPacked()) ||
          (A.degree() == 1 && !(A.count() % 3))))
        throw std::invalid_argument("set_route_guard: expected a packed array of size (n,3) or (n*3)");
    const float *v = A.data();
    for (uint i = 0; i < A.count(); i += 3)
        addPoint(Point_25(v[i], v[i+1], v[i+2]));
}
void SetRouteGuard::setArgument(PyObject *py)
{
    mGuard.clear();
    if (Py_IsNone(py))
        return;
    mGuard.resize(1);
    if (PyList_Check(py))
        setList(py);
    else
        setArray(py);
}
void SetRouteGuard::setArgument(const std::vector<Point_25> &points)
{
    mGuard.clear();
    mGuard.resize(1);
    for (auto P : points)
        addPoint(P);
}
Action::Result SetRouteGuard::performAs(Agent &A, PyObject *arg)
{
    undoAs(A); // clear old guard represented by this action
    if (arg)
        setArgument(arg);
    NavRasterizeParams param;
    param.FlagsOr = NAV_POINT_FLAG_ROUTE_GUARD;
    for (const auto &P : mGuard)
        A.getPCB()->getNavGrid().rasterize(P, param);
    A.countActions(mActionCountIncrement);
    return Action::Result();
}
Action::Result SetRouteGuard::undoAs(Agent &A)
{
    NavRasterizeParams param;
    param.FlagsAnd = ~NAV_POINT_FLAG_ROUTE_GUARD;
    for (const auto &P : mGuard)
        A.getPCB()->getNavGrid().rasterize(P, param);
    A.countActions(mActionCountIncrement);
    return Action::Result();
}

} // namespace actions
