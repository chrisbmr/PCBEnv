#include "DRC.hpp"
#include "Connection.hpp"

PyObject *DRCViolation::getPy() const
{
    py::Object rv(PyDict_New());
    py::Object conn(PyTuple_New(2));
    conn.setElem(0, mConnections[0] ? mConnections[0]->getEndsPy() : py::None());
    conn.setElem(1, mConnections[0] ? mConnections[1]->getEndsPy() : py::None());
    rv.setItem("x", py::Object(mLocation));
    rv.setItem("connections", conn);
    rv.setItem("pin", mPin ? py::String(mPin->getFullName()) : py::None());
    return *rv;
}
