#ifndef GYM_PCB_RL_ACTIONS_ROUTEASTAR_H
#define GYM_PCB_RL_ACTIONS_ROUTEASTAR_H

#include "RL/Actions/Route.hpp"

namespace actions
{

class AStarConnect : public RouteAction
{
public:
    AStarConnect(Connection *Y, const std::string &name = "astar") : RouteAction(name, Y) { }
    Result performAs(Agent&, PyObject *) override;
};

class AStarToPoint final : public RouteToAction
{
public:
    AStarToPoint(Connection *Y, const Point_25 &A, const Point_25 &B) : RouteToAction("astar_to", Y, A, B) { }
    Result performAs(Agent&, PyObject *) override;
};

} // namespace actions

#endif // GYM_PCB_RL_ACTIONS_ROUTEASTAR_H
