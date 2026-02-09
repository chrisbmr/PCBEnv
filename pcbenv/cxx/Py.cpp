
#include "Py.hpp"

namespace py
{

Object::Object(const Bbox_2 &box) : mPy(PyTuple_New(4))
{
    PyTuple_SetItem(mPy, 0, PyFloat_FromDouble(box.xmin()));
    PyTuple_SetItem(mPy, 1, PyFloat_FromDouble(box.ymin()));
    PyTuple_SetItem(mPy, 2, PyFloat_FromDouble(box.xmax()));
    PyTuple_SetItem(mPy, 3, PyFloat_FromDouble(box.ymax()));
}

::Bbox_2 Object::asBbox_2() const
{
    if (isList() && PyList_Size(mPy) == 4)
        return ::Bbox_2(item(0u).toDouble(), item(1).toDouble(), item(2).toDouble(), item(3).toDouble());
    if (isVector(4))
        return ::Bbox_2(elem(0u).toDouble(), elem(1).toDouble(), elem(2).toDouble(), elem(3).toDouble());
    if (!isTuple(2))
        throw std::invalid_argument("object is not a 2D box");
    auto min = elem(0).asPoint_2();
    auto max = elem(1).asPoint_2();
    return ::Bbox_2(min.x(), min.y(), max.x(), max.y());
}

::IBox_3 Object::asIBox_3() const
{
    if (!isTuple(2) || !elem(0).isLongVector(3) || !elem(1).isLongVector(3))
        throw std::invalid_argument("object is not a 3D integer box");
    return ::IBox_3(elem(0).asIPoint_3(), elem(1).asIPoint_3());
}

::Bbox_2 Object::asBbox_25(uint &zmin, uint &zmax) const
{
    if (!isTuple(2) || !elem(0).isPoint_25() || !elem(1).isPoint_25())
        throw std::invalid_argument("object is not a 2.5D bounding box");
    const auto min = elem(0).asPoint_25();
    const auto max = elem(1).asPoint_25();
    zmin = min.z();
    zmax = max.z();
    return Bbox_2(min.x(), min.y(), max.x(), max.y());
}

uint64_t Object::asBitmask(uint size) const
{
    if (isLong()) {
        const uint64_t m = asLong();
        if (m & (~0ULL << size))
            throw std::invalid_argument(fmt::format("integer bitmask {:#x} exceeds size {}", m, size));
        return m;
    }
    auto I = PyObject_GetIter(mPy);
    if (!I)
        throw std::invalid_argument("bitmask is neither integer nor iterable");
    uint i = 0;
    uint64_t m = 0;
    while (auto v = PyIter_Next(I)) {
        std::string err = (v && PyBool_Check(v)) ? "" : "bitmask elements must be boolean";
        if (i >= size)
            err = fmt::format("iterable bitmask exceeds size {}", size);
        if (err.empty() && Py_IsTrue(v))
            m |= 1 << i;
        if (v)
            Py_DECREF(v);
        if (!err.empty())
            throw std::invalid_argument(err);
        i++;
    }
    return m;
}
    
} // namespace py
