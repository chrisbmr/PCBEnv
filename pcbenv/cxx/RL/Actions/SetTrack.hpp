#ifndef GYM_PCB_RL_ACTIONS_SETTRACK_H
#define GYM_PCB_RL_ACTIONS_SETTRACK_H

#include "RL/Actions/Route.hpp"
#include "Net.hpp"
#include "Track.hpp"

namespace actions {

class SetTrack : public RouteAction
{
public:
    SetTrack(Connection *Y, const std::string &name = "set_track") : RouteAction(name, Y) { }
    Result performAs(Agent&, PyObject *) override;
    void setTrack(const Track &T) { mTrack = T; }
    void setTrack(Track &&T) { mTrack = T; }
private:
    Track mTrack{Point_25(0,0,-1)};
    bool setArguments(Agent&, PyObject *connection_and_track);
};

class LockRouted final : public Action
{
public:
    LockRouted() : Action("lock_routed") { }
    Result performAs(Agent&, PyObject *) override;
};

} // namespace actions

#endif // GYM_PCB_RL_ACTIONS_SETTRACK_H
