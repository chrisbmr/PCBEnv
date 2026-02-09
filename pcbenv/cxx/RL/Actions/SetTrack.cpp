#include "Log.hpp"
#include "RL/Actions/SetTrack.hpp"
#include "RL/Agent.hpp"
#include "Net.hpp"

namespace actions
{

bool SetTrack::setArguments(Agent &A, PyObject *py)
{
    py::Object args(py);
    if (!args)
        return false;
    if (!args.isTuple(2))
        throw std::invalid_argument("set_track: expected (connection,track)");
    setConnection(A.setConnectionLRU(*args.elem(0)));
    mTrack.setPy(*args.elem(1));
    const auto mask = X->net()->validateTrack(mTrack);
    if (!(mask & 0x1))
        throw std::invalid_argument("set_track: invalid segments or vias detected");
    if (!(mask & 0x2))
        WARN("set_track: track is not comptabile with net parameters");
    return true;
}
Action::Result SetTrack::performAs(Agent &A, PyObject *arg)
{
    A.countActions(mActionCountIncrement);
    const auto move = setArguments(A, arg);
    if (move)
        setConnectionTrack(A.getPCB(), std::move(mTrack));
    else
        setConnectionTrack(A.getPCB(), mTrack);
    return getReward(A, true);
}

Action::Result LockRouted::performAs(Agent &A, PyObject *arg)
{
    const bool lock = py::Object(arg).toBool();
    for (auto X : A.getConnections())
        X->setLocked(X->hasTracks() && lock);
    return Action::Result(0.0f, true);
}

} // namespace actions
