#include "RL/Actions/AddSegment.hpp"
#include "RL/Agent.hpp"
#include "PCBoard.hpp"
#include "Connection.hpp"

namespace actions {

Action::Result SegmentToPoint::performAs(Agent &A, PyObject *arg)
{
    if (arg)
        setArguments4(A, arg);
    A.countActions(mActionCountIncrement);
    const auto PCB = A.getPCB();
    Connection Y(*X, mPoint[0], mPoint[1]);
    Y.makeDirectTrack(MinSegmentLenSq, 0);
    Real AV = PCB->sumViolationArea(Y, X);
    Y.setRouted(AV == 0.0);
    const auto res = getReward(A, AV == 0.0, &Y);
    if (Y.isRouted())
        WITH_WLOCK(PCB,
                   X->appendTrack(Y.popTrack(0)));
    if (X->hasTracks() && mRasterize)
        PCB->rasterizeTracks(*X);
    return res;
}

} // namespace actions
