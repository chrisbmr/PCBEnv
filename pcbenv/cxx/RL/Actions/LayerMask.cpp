#include "RL/Actions/LayerMask.hpp"
#include "RL/Agent.hpp"
#include "PCBoard.hpp"
#include "Net.hpp"

namespace actions {

Action::Result SetLayerMask::performAs(Agent &A, PyObject *py)
{
    py::Object arg(py);
    if (!arg.isTuple(2))
        throw std::invalid_argument("set_layer_mask: expected a tuple(net, mask)");
    const auto PCB = A.getPCB();
    auto net = PCB->getNet(*arg.elem(0));
    if (net)
        net->setLayerMask(arg.elem(1).asBitmask(32));
    A.countActions(mActionCountIncrement);
    return Action::Result();
}

} // namespace actions
