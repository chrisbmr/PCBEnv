#include "RL/Actions/RouteAStar.hpp"
#include "RL/Agent.hpp"
#include "PCBoard.hpp"
#include "Connection.hpp"

namespace actions {

Action::Result AStarConnect::performAs(Agent &A, PyObject *arg)
{
    if (arg)
        setArguments2(A, arg);
    A.countActions(mActionCountIncrement);
    const auto PCB = A.getPCB();
    if (X->hasTracks())
        WITH_WLOCK(PCB, PCB->eraseTracks(*X));
    auto rv = PCB->runPathFinding(*X, 0, mCostsSet ? &mCosts : 0);
    if (rv && mRasterize)
        PCB->rasterizeTracks(*X);
    return getReward(A, rv);
}

Action::Result AStarToPoint::performAs(Agent &A, PyObject *arg)
{
    if (arg)
        setArguments4(A, arg);
    A.countActions(mActionCountIncrement);
    const auto PCB = A.getPCB();
    Connection Y(*X, mPoint[0], mPoint[1]);
    auto rv = PCB->runPathFinding(Y, X, mCostsSet ? &mCosts : 0);
    const auto res = getReward(A, rv, &Y);
    if (rv)
        WITH_WLOCK(PCB,
                   X->appendTrack(Y.popTrack(0)));
    if (X->hasTracks() && mRasterize)
        PCB->rasterizeTracks(*X);
    return res;
}

} // namespace actions
