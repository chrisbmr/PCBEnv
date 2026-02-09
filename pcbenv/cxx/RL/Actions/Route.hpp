#ifndef GYM_PCB_RL_ACTIONS_ROUTE_H
#define GYM_PCB_RL_ACTIONS_ROUTE_H

#include "RL/Action.hpp"
#include "NavGrid.hpp"

class Connection;
class Net;
class Track;

namespace actions
{

class RouteAction : public Action
{
public:
    RouteAction(const std::string &name, Connection *Y) : Action(name), X(Y) { }
    Connection *getConnection() const { return X; }
    void setConnection(Connection *Y);
    void setRasterization(bool enable);
    void setCosts(AStarCosts *);
protected:
    Connection *X;
    bool mRasterize{true};
    bool mCostsSet{false};
    AStarCosts mCosts;
protected:
    void setArguments2(Agent&, PyObject *connection_costs);
    void setCostsArg(PyObject *, uint);
    void setConnectionTrack(PCBoard *, const Track&);
    void setConnectionTrack(PCBoard *, Track&&);
    Action::Result getReward(const Agent&, bool success, Connection *Y = 0);
};
inline void RouteAction::setConnection(Connection *Y)
{
    X = Y;
}
inline void RouteAction::setRasterization(bool enable)
{
    mRasterize = enable;
    if (auto undo = dynamic_cast<RouteAction *>(mUndo))
        undo->setRasterization(enable);
}
inline void RouteAction::setCosts(AStarCosts *costs)
{
    mCostsSet = !!costs;
    if (mCostsSet)
        mCosts = *costs;
}

class RouteToAction : public RouteAction
{
public:
    RouteToAction(const std::string &name, Connection *Y, const Point_25 &A, const Point_25 &B) : RouteAction(name, Y), mPoint{A,B} { }
    void setPoint(uint i, PyObject *);
    void setPoint(uint i, const Point_25 &v) { mPoint[i] = v; }
protected:
    Point_25 mPoint[2];
    void setArguments4(Agent&, PyObject *connection_points);
};

} // namespace actions

#endif // GYM_PCB_RL_ACTIONS_ROUTE_H
