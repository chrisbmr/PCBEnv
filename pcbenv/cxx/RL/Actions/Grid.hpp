#ifndef GYM_PCB_RL_ACTIONS_GRID_H
#define GYM_PCB_RL_ACTIONS_GRID_H

#include "RL/Action.hpp"
#include "Path.hpp"

namespace actions
{

/// Costs can be:
/// None
/// (float, (x0, y0, z0), (x1, y1, z1))
/// (3D array, (x0, y0, z0))
/// 3D array with the same size as the grid
/// 1D array with the same size as the grid
class SetCostMap final : public Action
{
public:
    SetCostMap() : Action("set_costs") { }
    void setArgument(PyObject *costs) { mCosts = py::BorrowedRef(costs); }
    Result performAs(Agent&, PyObject *) override;
private:
    void setCostsFloatRegion(NavGrid&);
    void setCostsArrayPoint(NavGrid&);
    void setCostsArray(NavGrid&);
    py::BorrowedRef mCosts;
};

/// Must be a list of points [(x,y,z)] creating several polygons when z changes (closed automatically).
class SetRouteGuard final : public Action
{
public:
    SetRouteGuard() : Action("set_guard") { }
    ~SetRouteGuard() { }
    void setArgument(const std::vector<Point_25> &guard);
    void setArgument(PyObject *guard);
    Result performAs(Agent&, PyObject *) override;
    Result undoAs(Agent&) override;
private:
    std::vector<Path> mGuard;
    Point_25 mFirst;
    void addPoint(const Point_25&);
    void setList(py::Object);
    void setArray(PyObject *);
};

} // namespace actions

#endif // GYM_PCB_RL_ACTIONS_GRID_H
