#include "RL/State/Metrics.hpp"
#include "Util/Metrics.hpp"

namespace sreps {

PyObject *Metrics::getPy(PyObject *_arg)
{
    if (!mPCB)
        return py::None();
    py::Object arg(_arg);

    if (arg.isString()) {
        auto name = py::Object(arg).asStringView();
        if (name == "ratsnest_crossings")
            return PyLong_FromLong(metrics::RatsNestCrossings(*mPCB));
    } else if (arg.isTuple()) {
        auto name = arg.elem(0).asStringView();
        if (name == "linkage")
            return PyFloat_FromDouble(metrics::ConnectionLinkage(*mPCB, arg.elem(1).toDouble(), arg.elem(2).asLong()));
        if (name == "ratsnest_crossings") {
            auto nets = arg.elem(1).asStringContainer<std::set<std::string>>();
            return PyLong_FromLong(metrics::RatsNestCrossings(*mPCB, &nets));
        }
    }
    return py::None();
}

} // namespace sreps
