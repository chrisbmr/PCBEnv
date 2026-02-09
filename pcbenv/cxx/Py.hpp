
#ifndef GYM_PCB_PY_H
#define GYM_PCB_PY_H

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#define NO_IMPORT_ARRAY
#define PY_ARRAY_UNIQUE_SYMBOL _pcbenv2_module_PyArray_API
#include <numpy/ndarrayobject.h>

#include "Geometry.hpp"

namespace py
{

/// Error return helpers

template<typename Ty = PyObject>
inline Ty *ValueError(const char *s)
{
    PyErr_SetString(PyExc_ValueError, s);
    return 0;
}
template<typename Ty = PyObject>
inline Ty *MemoryError(const char *s)
{
    PyErr_SetString(PyExc_MemoryError, s);
    return 0;
}
template<typename Ty = PyObject>
inline Ty *Exception(PyObject *exc, const char *s)
{
    PyErr_SetString(exc, s);
    return 0;
}
inline PyObject *Exception(const std::exception &e)
{
    PyErr_SetString(PyExc_Exception, e.what());
    return 0;
}


/// Conversion to Python

inline PyObject *String(const char *s)
{
    return PyUnicode_FromString(s);
}
inline PyObject *String(const std::string &s)
{
    return PyUnicode_FromStringAndSize(s.c_str(), s.size());
}


/// Conversion from Python

inline bool TupleN_Check(PyObject *py, uint n)
{
    return PyTuple_Check(py) && PyTuple_Size(py) == n;
}
inline bool TupleMin_Check(PyObject *py, uint n)
{
    return PyTuple_Check(py) && PyTuple_Size(py) >= n;
}

inline double Number_AsDouble(PyObject *py)
{
    if (PyFloat_Check(py))
        return PyFloat_AsDouble(py);
    auto F = PyNumber_Float(py);
    assert(F);
    if (!F)
        return std::numeric_limits<double>::quiet_NaN();
    auto d = PyFloat_AsDouble(F);
    Py_DECREF(F);
    return d;
}

inline bool String_Check(PyObject *py)
{
    return py && PyUnicode_Check(py);
}
inline std::string String_AsStdString(PyObject *py)
{
#if PY_VERSION_HEX >= 0x03030000
    return std::string(PyUnicode_AsUTF8(py));
#else
#error "Python version < 3.3 not supported, go upgrade!"
#endif
}
inline std::string_view String_AsStringView(PyObject *py)
{
#if PY_VERSION_HEX >= 0x03030000
    Py_ssize_t size;
    const char *ucs = PyUnicode_AsUTF8AndSize(py, &size);
    return std::string_view(ucs, size);
#else
#error "Python version < 3.3 not supported, go upgrade!"
#endif
}

inline std::string String_AsStdString(PyObject *py, const char *error)
{
    if (!String_Check(py))
        throw std::invalid_argument(error);
    return String_AsStdString(py);
}

inline void Dict_StealItemString(PyObject *dict, const char *key, PyObject *v)
{
    PyDict_SetItemString(dict, key, v);
    Py_DECREF(v);
}
inline void Dict_StealItem(PyObject *dict, PyObject *key, PyObject *v)
{
    PyDict_SetItem(dict, key, v);
    Py_DECREF(v);
}

inline void DECREF_safe(PyObject *v)
{
    if (v)
        Py_DECREF(v);
}

inline PyObject *None()
{
    Py_INCREF(Py_None);
    return Py_None;
}

inline PyObject *NewRef(PyObject *py)
{
    Py_INCREF(py);
    return py;
}


// SAFETY

/// The GIL
class GIL_guard
{
public:
    GIL_guard()
    {
        state = PyGILState_Ensure();
    }
    ~GIL_guard()
    {
        PyGILState_Release(state);
    }
    GIL_guard(GIL_guard&& moved) noexcept : state(moved.state)
    {
        moved.state = PyGILState_UNLOCKED;
    }
    GIL_guard& operator=(GIL_guard&& moved) noexcept
    {
        if (this != &moved) {
            PyGILState_Release(state);
            state = moved.state;
            moved.state = PyGILState_UNLOCKED;
        }
        return *this;
    }
    GIL_guard(const GIL_guard&) = delete;

private:
    PyGILState_STATE state;
};

} // namespace py

#include "PyObject.hpp"

#endif // GYM_PCB_PY_H
