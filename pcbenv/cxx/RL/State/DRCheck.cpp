#include "RL/State/DRCheck.hpp"
#include "DRC.hpp"
#include "PCBoard.hpp"
#include "Net.hpp"
#include "Track.hpp"
#include "Util/PCBItemSets.hpp"

namespace sreps {

// { (item set), '2d': boolean = False, 'limit': int }

PyObject *ClearanceCheck::getPy(PyObject *arg)
{
    if (!mPCB)
        return py::None();
    if (!PyDict_Check(arg))
        throw std::invalid_argument("clearance_check: expected dict { items, '2d': boolean, 'limit': int }");

    PCBItemSets items;
    items.populate(*mPCB, arg);
    items.populateConnectionsFromComs();
    items.populateConnectionsFromNets();
    items.populateConnectionsFromPins();

    m2D = false;
    mLimit = 32768;
    py::Object args(arg);
    if (auto v = args.item("limit"))
        mLimit = v.toLong();
    if (auto v = args.item("2d"))
        m2D = v.toBool();

    check(items);
    return *py::Object::new_ListFrom<DRCViolation>(mDRC);
}
uint ClearanceCheck::check(Connection &X, Pin &P)
{
    const auto c = std::max(X.clearance(), P.hasNet() ? P.net()->getMinClearance() : 0.0);
    Point_25 x;
    for (const auto *T : X.getTracks()) {
        if (!T->violatesClearance(P, c, &x))
            continue;
        mDRC.emplace_back(DRCViolation(x, &X, 0, &P));
        if (mDRC.size() >= mLimit)
            break;
    }
    return mDRC.size();
}
uint ClearanceCheck::check(Connection &X, Connection &Y)
{
    const auto c = std::max(X.clearance(), Y.clearance());
    Point_25 x;
    for (const auto *T2 : Y.getTracks()) {
    for (const auto *T1 : X.getTracks()) {
        if (!(m2D ? T1->violatesClearance2D(*T2, c, &x) : T1->violatesClearance(*T2, c, &x)))
            continue;
        mDRC.push_back(DRCViolation(x, &X, &Y, 0));
        if (mDRC.size() >= mLimit)
            break;
    }}
    return mDRC.size();
}
uint ClearanceCheck::check(const PCBItemSets &items)
{
    mDRC.clear();
    Point_25 x;
    for (const auto X : *items.cons) {
        for (const auto net : mPCB->getNets()) {
            if (net == X->net())
                continue;
            for (const auto Y : net->connections())
                if (check(*X, *Y) >= mLimit)
                    return mLimit;
            for (const auto P : net->getPins())
                if (check(*X, *P) >= mLimit)
                    return mLimit;
        }
    }
    for (const auto P : *items.pins) {
        const auto *N = P->net();
        for (const auto net : mPCB->getNets()) {
            if (net == N)
                continue;
            for (const auto X : net->connections())
                if (check(*X, *P) >= mLimit)
                    return mLimit;
        }
    }
    return mDRC.size();
}

} // namespace sreps
