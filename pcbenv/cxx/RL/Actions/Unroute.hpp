#ifndef GYM_PCB_RL_ACTIONS_UNROUTE_H
#define GYM_PCB_RL_ACTIONS_UNROUTE_H

#include "RL/Actions/Route.hpp"

namespace actions
{

class Unroute final : public RouteAction
{
public:
    Unroute(Connection *Y) : RouteAction("unroute", Y) { }
    Result performAs(Agent&, PyObject *) override;
};

class UnrouteNet final : public Action
{
public:
    UnrouteNet(Net *net) : Action("unroute_net"), mNet(net) { }
    Result performAs(Agent&, PyObject *) override;
private:
    Net *mNet;
};

class UnrouteSegment final : public RouteAction
{
public:
    UnrouteSegment(Connection *Y, const Point_25 &end) : RouteAction("unroute_segment", Y), mEnd(end) { }
    Result performAs(Agent&, PyObject *) override;
private:
    Point_25 mEnd;
    void setArguments(Agent&, PyObject *connection_and_endpoint);
};

} // namespace actions

#endif // GYM_PCB_RL_ACTIONS_UNROUTE_H
