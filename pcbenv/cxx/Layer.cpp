
#include "Defs.hpp"
#include "Layer.hpp"

std::string Layer::str() const
{
    return fmt::format("[z={} : {}]", getNumber(), to_string(getType()));
}

/**
 * @return { 'id': integer, 'type': string enum }
 */
PyObject *Layer::getPy() const
{
    auto py = PyDict_New();
    py::Dict_StealItemString(py, "id", PyLong_FromLong(getNumber()));
    py::Dict_StealItemString(py, "type", py::String(to_string(getType())));
    return py;
}
