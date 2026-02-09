#include "Log.hpp"
#include "Reward.hpp"
#include "PCBoard.hpp"
#include "Connection.hpp"
#include "Net.hpp"
#include "Track.hpp"
#include "UserSettings.hpp"

RewardFunction *RewardFunction::create(PyObject *py)
{
    py::Object args(py);
    if (auto f = args.item("function"))
        if (f.asStringView() == "track_length")
            return rewards::RouteLength::create(py);
    throw std::invalid_argument("reward function must be 'track_length'");
}

PyObject *RouterResult::getPy() const
{
    auto py = PyDict_New();
    if (!py)
        return 0;
    py::Dict_StealItemString(py, "track_len", PyFloat_FromDouble(TrackLen));
    py::Dict_StealItemString(py, "num_vias", PyLong_FromLong(NumVias));
    py::Dict_StealItemString(py, "not_connected", PyLong_FromLong(NumDisconnected));
    return py;
}

namespace rewards {

RouteLength::RouteLength()
{
    FPerVia = UserSettings::get().Reward.PerVia;
    FPerUnrouted = UserSettings::get().Reward.PerDisconnect;
    FAnyUnrouted = UserSettings::get().Reward.AnyDisconnect;
}

void RouteLength::setContext(const PCBoard &PCB)
{
    if (ScaleGlobal == "")
        FScale = 1.0f;
    else if (ScaleGlobal == "area_sqrt")
        FScale = std::sqrt(PCB.getLayoutArea().area());
    else if (ScaleGlobal == "area_diagonal")
        FScale = std::sqrt(PCB.getLayoutArea().diagonalLength());
    else if (ScaleGlobal == "ref_tracks" ||
             ScaleGlobal == "d_connection" ||
             ScaleGlobal == "d45_connection") {
        FScale = 0.0f;
        for (const auto *net : PCB.getNets()) {
            for (const auto *X : net->connections()) {
                if (ScaleGlobal == "ref_tracks")
                    FScale += X->referenceLen();
                else if (ScaleGlobal == "d45_connection")
                    FScale += X->distance45();
                else
                    FScale += X->distance();
            }
        }
    } else {
        throw std::invalid_argument(fmt::format("invalid reward scale: {}", ScaleGlobal));
    }
}

Reward RouteLength::operator()(const Connection &X, RouterResult *R) const
{
    if (R && !R->isSet())
        R->TrackLen = 0.0f;
    if (X.isRouted()) {
        float d = FScale;
        switch (ScaleLength_) {
        case 'd': d *= X.distance(); break;
        case 'q': d *= X.distance45(); break;
        }
        float L = 0.0f;
        int V = 0;
        for (const auto *T : X.getTracks()) {
            L += T->length();
            V += T->numVias();
        }
        if (R) {
            R->TrackLen += L;
            R->NumVias += V;
        }
        assert(V >= int(X.numNecessaryVias()));
        if (IgnoreNecessaryVias)
            V -= X.numNecessaryVias();
        return FPerRouted - L / d + V * FPerVia;
    } else {
        if (R)
            R->NumDisconnected += 1;
        return FPerUnrouted;
    }
}

Reward RouteLength::operator()(const std::vector<Connection *> &conns, RouterResult *RR) const
{
    Reward R = 0.0f;
    RouterResult RR_;
    if (!RR)
        RR = &RR_;
    for (auto X : conns)
        R += (*this)(*X, RR);
    if (RR->NumDisconnected)
        R += FAnyUnrouted;
    return R;
}

Reward RouteLength::operator()(int unrouted) const
{
    return unrouted * FPerUnrouted;
}

RouteLength *RouteLength::create(PyObject *py)
{
    py::Object args(py);

    auto R = new RouteLength();
    if (!R)
        return 0;

    if (auto v = args.item("per_unrouted"))
        R->FPerUnrouted = v.toDouble();
    if (auto v = args.item("any_unrouted"))
        R->FAnyUnrouted = v.toDouble();
    if (auto v = args.item("per_via"))
        R->FPerVia = v.toDouble();
    if (auto v = args.item("per_routed"))
        R->FPerRouted = v.toDouble();
    if (auto v = args.item("scale_length"))
        R->ScaleLength = v.asStringView();
    if (auto v = args.item("scale_global"))
        R->ScaleGlobal = v.asStringView();
    if (auto v = args.item("ignore_necessary_vias"))
        R->IgnoreNecessaryVias = v.asBool();

    if (R->FPerUnrouted > 0.0)
        WARN("reward per unrouted connection is positive");
    if (R->FPerRouted < 0.0)
        WARN("reward per routed connection is negative");
    if (R->FPerVia > 0.0)
        WARN("reward per via is positive");

    if (R->ScaleLength == "d")
        R->ScaleLength_ = 'e';
    else if (R->ScaleLength == "d45")
        R->ScaleLength_ = 'q';
    else if (R->ScaleLength == "")
        R->ScaleLength_ = '1';
    else
        throw std::invalid_argument("reward: scale_length must be one of {'d','d45',''}");

    return R;
}

} // namespace rewards
