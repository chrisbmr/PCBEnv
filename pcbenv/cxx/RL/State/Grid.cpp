#include "RL/State/Grid.hpp"
#include "PCBoard.hpp"

namespace sreps {

void GridData::init(PCBoard &PCB)
{
    StateRepresentation::init(PCB);
    const NavGrid &nav = mPCB->getNavGrid();
    mBox = IBox_3(IPoint_3(0,0,0), IPoint_3(nav.getSize(0) - 1, nav.getSize(1) - 1, nav.getSize(2) - 1));
}
PyObject *GridData::getPy(PyObject *args)
{
    assert(mPCB);
    if (!mPCB)
        return 0;
    const NavGrid &nav = mPCB->getNavGrid();
    IBox_3 box = mBox;
    if (args && PyTuple_Check(args))
        box = py::Object(args).asIBox_3();
    auto rv = nav.getPy(box);
    return rv;
}

} // namespace sreps
