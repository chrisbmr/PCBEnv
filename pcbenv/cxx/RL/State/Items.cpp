#include "RL/State/Items.hpp"
#include "PCBoard.hpp"
#include "Net.hpp"
#include "Track.hpp"

namespace sreps {

PyObject *WholeBoard::getPy(PyObject *args)
{
    assert(mPCB);
    if (!mPCB)
        return 0;
    return mPCB->getPy(args);
}

PyObject *ItemSelection::getPy(PyObject *_args)
{
    py::Object args(_args);
    if (!args.isDict())
        throw std::invalid_argument("select: argument must be a dict");

    auto kind = args.item("object").asStringView("select: dict must contain 'object' key");

    auto pos = args.item("pos");
    auto box = args.item("box");
    if (!pos && !box)
        throw std::invalid_argument("select: either 'box' or 'pos' must be specified");

    std::set<Object *> oset;
    Bbox_2 bbox;
    uint zmin, zmax;
    if (box) {
        bbox = box.asBbox_25(zmin, zmax);
    } else {
        const auto v = pos.asPoint_25("select: pos must be a 2.5d point");
        bbox = Bbox_2(v.x(), v.y(), v.x(), v.y());
        zmin = zmax = v.z();
    }
    if (kind == "pin")
        mPCB->getPinBVH()->select(oset, bbox, zmin, zmax);
    else if (kind.starts_with("com"))
        mPCB->getComBVH()->select(oset, bbox, zmin, zmax);
    else
        throw std::invalid_argument(fmt::format("select: invalid object type {}", kind));

    auto rv = PyList_New(oset.size());
    uint i = 0;
    for (const auto A : oset)
        PyList_SetItem(rv, i++, py::String(A->getFullName()));
    return rv;
}
void ItemSelection::selectNets(PyObject *d, PyObject *_args)
{
    py::Object args(_args);
    auto zminArg = args.item("zmin");
    auto zmaxArg = args.item("zmax");
    uint zmin = zminArg ? zminArg.asLong() : 0;
    uint zmax = zmaxArg ? zmaxArg.asLong() : (mPCB->getNumLayers() - 1);
    Bbox_2 bbox;
    if (auto bboxArg = args.item("bbox"))
        bbox = bboxArg.asBbox_2();

    std::vector<PyObject *> nets;
    for (const auto net : mPCB->getNets()) {
        bool intersects = false;
        for (const auto P : net->getPins()) {
            intersects = P->bboxTest(bbox, zmin, zmax);
            if (intersects)
                break;
        }
        if (!intersects) {
            for (const auto *X : net->connections()) {
                for (const auto *T : X->getTracks()) {
                    intersects = T->intersects(bbox, zmin, zmax);
                    if (intersects)
                        break;
                }
                if (intersects)
                    break;
            }
        }
        if (intersects)
            nets.push_back(py::String(net->name()));
    }
    auto L = PyList_New(nets.size());
    for (uint i = 0; i < nets.size(); ++i)
        PyList_SetItem(L, i, nets[i]);
    py::Dict_StealItemString(d, "nets", L);
}

} // namespace sreps
