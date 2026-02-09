#include "RL/Actions/Unroute.hpp"
#include "RL/Agent.hpp"
#include "PCBoard.hpp"
#include "Net.hpp"
#include "Track.hpp"
#include "Util/FancyLock.hpp"

namespace actions {

Action::Result Unroute::performAs(Agent &A, PyObject *arg)
{
    if (arg)
        setConnection(A.setConnectionLRU(arg));
    A.countActions(mActionCountIncrement);
    const auto PCB = A.getPCB();
    if (X->hasTracks())
        WITH_WLOCK(PCB,
                   PCB->eraseTracks(*X));
    else
        X->setRouted(false);
    return Action::Result();
}

Action::Result UnrouteNet::performAs(Agent &A, PyObject *arg)
{
    const auto PCB = A.getPCB();
    if (arg)
        mNet = PCB->getNet(arg);
    A.countActions(mActionCountIncrement);
    for (auto X : mNet->connections()) {
        if (X->hasTracks())
            WITH_WLOCK(PCB,
                       PCB->eraseTracks(*X));
        else
            X->setRouted(false);
    }
    return Action::Result();
}

void UnrouteSegment::setArguments(Agent &A, PyObject *py)
{
    py::Object arg(py);
    if (!arg.isTuple(2))
        throw std::invalid_argument("unroute_segment: expected a tuple(connection, endpoint|None)");
    auto v0 = arg.elem(0);
    auto v1 = arg.elem(1);
    setConnection(A.setConnectionLRU(*v0));
    if (!v1.isNone())
        mEnd = v1.asPoint_25("unroute_segment: expected a track endpoint (x,y,x)");
    else if (X->numTracks() == 1)
        mEnd = X->getTrack(0).end();
    else
        throw std::runtime_error("unroute_segment: multiple tracks but no endpoint specified");
}

// TODO:
// 1. Don't rasterize the whole track if CanSafelyEraseOverlappingSegments.
// 2. Rasterize only the modified track.
Action::Result UnrouteSegment::performAs(Agent &A, PyObject *arg)
{
    if (arg)
        setArguments(A, arg);
    A.countActions(mActionCountIncrement);
    const auto T = X->getTrackEndingNear(mEnd, 1.0);
    if (!T || T->empty())
        return Action::Result(0, false);
    if (T->end() != mEnd)
        throw std::invalid_argument("unroute_segment: can erase only from the end of the track");
    const auto PCB = A.getPCB();
    if (mRasterize)
        PCB->unrasterizeTracks(*X);
    WITH_LOCKGUARD(PCB->getLock()) {
        T->popSafe();
        X->setRouted(false);
    }
    if (mRasterize)
        PCB->rasterizeTracks(*X);
    return Action::Result();
}

} // namespace actions
