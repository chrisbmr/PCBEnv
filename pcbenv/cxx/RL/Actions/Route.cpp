#include "RL/Actions/Route.hpp"
#include "RL/Agent.hpp"
#include "PCBoard.hpp"
#include "Connection.hpp"
#include "Track.hpp"

namespace actions {

Action::Result RouteAction::getReward(const Agent &A, bool success, Connection *Y)
{
    Action::Result res;
    res.Success = success;
    res.R = A.getRewardFn()(Y ? *Y : *X, &res.Router);
    assert(res.Router.isSet());
    return res;
}

void RouteAction::setConnectionTrack(PCBoard *PCB, Track &&data)
{
    std::lock_guard lock(PCB->getLock());
    if (X->hasTracks())
        PCB->eraseTracks(*X);
    if (data.empty())
        return;
    X->setTrack(std::move(data));
    X->getTrack(0).resetRasterizedCount();
    if (mRasterize)
        PCB->rasterizeTracks(*X);
}
void RouteAction::setConnectionTrack(PCBoard *PCB, const Track &data)
{
    std::lock_guard lock(PCB->getLock());
    if (X->hasTracks())
        PCB->eraseTracks(*X);
    if (data.empty())
        return;
    X->setTrack(data);
    X->getTrack(0).resetRasterizedCount();
    if (mRasterize)
        PCB->rasterizeTracks(*X);
}
void RouteAction::setArguments2(Agent &A, PyObject *py)
{
    // connection-ref or (connection-ref,astar-costs) where connection-ref is (pin,pin) or int, astar-costs is dict
    py::Object args(py);
    if (!args.isTuple(2) || !args.elem(1).isDict()) {
        setConnection(A.setConnectionLRU(py));
    } else {
        setConnection(A.setConnectionLRU(*args.elem(0)));
        setCostsArg(py, 1);
    }
}
void RouteAction::setCostsArg(PyObject *py, uint i)
{
    py::Object args(py);
    mCostsSet = args.hasTupleElem(i);
    if (mCostsSet)
        mCosts.setPy(*args.elem(i));
}

void RouteToAction::setPoint(uint i, PyObject *arg)
{
    setPoint(i, py::Object(arg).asPoint_25("expected a target point: (x: number, y: number, z: integer)"));
}
void RouteToAction::setArguments4(Agent &A, PyObject *py)
{
    py::Object args(py);
    if (!args.hasTupleElem(2))
        throw std::invalid_argument("route_to: expected a tuple(connection, point, point[, astar_costs])");
    auto v0 = args.elem(0);
    auto v1 = args.elem(1);
    auto v2 = args.elem(2);
    setConnection(A.setConnectionLRU(*v0));
    if (!v1.isNone()) {
        setPoint(0, *v1);
    } else {
        if (X->numTracks() > 1)
            throw std::runtime_error("route_to: multiple tracks but no starting point specified");
        setPoint(0, X->numTracks() ? X->getTrack(0).end() : X->source());
    }
    setPoint(1, *v2);
    setCostsArg(*args, 3);
}

} // namespace actions
