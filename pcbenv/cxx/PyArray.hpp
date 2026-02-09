
#ifndef GYM_PCB_PYARRAY_H
#define GYM_PCB_PYARRAY_H

#include "Py.hpp"
#include <vector>

namespace py {

template<typename T> class NPArray
{
public:
    static NPArray<T> *create(PyObject *);
    NPArray() : mPy(0) { }
    NPArray<T>& operator=(const NPArray<T> &avoid) { assert(!avoid.mPy); mPy = 0; return *this; }
    NPArray<T>& operator=(NPArray<T> &&move) { own(move.mPy); move.mPy = 0; return *this; }
    NPArray(const NPArray<T> &avoid) : mPy(0) { assert(!avoid.mPy); }
    NPArray(NPArray<T> &&move) : mPy(move.mPy) { move.mPy = 0; }
    NPArray(const std::vector<T>&);
    NPArray(const std::vector<T>&, npy_intp d1, npy_intp d2);
    NPArray(T *, npy_intp const *dims, uint num_dims);
    NPArray(T *, npy_intp d1);
    NPArray(T *, npy_intp d1, npy_intp d2);
    NPArray(T *, npy_intp d1, npy_intp d2, npy_intp d3);
    NPArray(T *, npy_intp d1, npy_intp d2, npy_intp d3, npy_intp d4);
    NPArray(PyObject *py) : mPy(0) { own(py); }
    bool own(PyObject *);
    bool own(PyArrayObject *);
    void INCREF() { assert(mPy); Py_INCREF(mPy); }
    NPArray<T>& ownData();
    ~NPArray() { own((PyArrayObject *)0); }
    bool valid() const { return mPy; }
    bool isPacked() const;
    void Assert() const { if (!valid()) throw std::runtime_error("Invalid numpy array"); }
    PyObject *release() { auto rv = py(); mPy = 0; return rv; }
    PyObject *py() const { return reinterpret_cast<PyObject *>(mPy); }
    PyArrayObject *pyArray() const { return mPy; }
    T *data() const { return static_cast<T *>(PyArray_DATA(mPy)); }
    intptr_t count() const { return PyArray_SIZE(mPy); }
    intptr_t stride(uint d) const { return PyArray_STRIDE(mPy, d); }
    intptr_t size() const { return PyArray_NBYTES(mPy); }
    uint dim(uint d) const { return PyArray_DIM(mPy, d); }
    uint degree() const { return PyArray_NDIM(mPy); }
    T operator()(uint x) const { return *(T *)PyArray_GETPTR1(mPy, x); }
    T operator()(uint x, uint y) const { return *(T *)PyArray_GETPTR2(mPy, x, y); }
    T operator()(uint x, uint y, uint z) const { return *(T *)PyArray_GETPTR3(mPy, x, y, z); }
    T operator()(uint x, uint y, uint z, uint i) const { return *(T *)PyArray_GETPTR4(mPy, x, y, z, i); }
    void copyto(std::vector<T>&) const;
    std::vector<T> vector() const { std::vector<T> v; copyto(v); return v; }
    static void checkAPI() { assert(PY_ARRAY_UNIQUE_SYMBOL); }
    static bool checkType(PyArrayObject *);
    constexpr static int classTypenum();
private:
    PyArrayObject *mPy;
};

template<typename T> NPArray<T>::NPArray(T *data, npy_intp const *dims, uint num_dims)
{
    checkAPI();
    mPy = reinterpret_cast<PyArrayObject *>(PyArray_SimpleNewFromData(num_dims, dims, classTypenum(), data));
}
template<typename T> NPArray<T>::NPArray(const std::vector<T> &v)
{
    const auto size = v.size() * sizeof(T);
    const npy_intp dims[1] = { (uint)v.size() };
    checkAPI();
    auto data = (T *)malloc(size);
    std::memcpy(data, v.data(), size);
    mPy = reinterpret_cast<PyArrayObject *>(PyArray_SimpleNewFromData(1, dims, classTypenum(), data));
    ownData();
}
template<typename T> NPArray<T>::NPArray(const std::vector<T> &v, npy_intp d1, npy_intp d2)
{
    auto srcSize = v.size() * sizeof(T);
    auto dstSize = d1 * d2 * sizeof(T);
    assert(srcSize == dstSize);
    auto data = (T *)malloc(dstSize);
    std::memcpy(data, v.data(), std::min(srcSize, dstSize));
    const npy_intp dims[2] = { d1, d2 };
    checkAPI();
    mPy = reinterpret_cast<PyArrayObject *>(PyArray_SimpleNewFromData(2, dims, classTypenum(), data));
    ownData();
}
template<typename T> NPArray<T>::NPArray(T *data, npy_intp d1)
{
    checkAPI();
    mPy = reinterpret_cast<PyArrayObject *>(PyArray_SimpleNewFromData(1, &d1, classTypenum(), data));
}
template<typename T> NPArray<T>::NPArray(T *data, npy_intp d1, npy_intp d2)
{
    const npy_intp dims[2] = { d1, d2 };
    checkAPI();
    mPy = reinterpret_cast<PyArrayObject *>(PyArray_SimpleNewFromData(2, dims, classTypenum(), data));
}
template<typename T> NPArray<T>::NPArray(T *data, npy_intp d1, npy_intp d2, npy_intp d3)
{
    const npy_intp dims[3] = { d1, d2, d3 };
    checkAPI();
    mPy = reinterpret_cast<PyArrayObject *>(PyArray_SimpleNewFromData(3, dims, classTypenum(), data));
}
template<typename T> NPArray<T>::NPArray(T *data, npy_intp d1, npy_intp d2, npy_intp d3, npy_intp d4)
{
    const npy_intp dims[4] = { d1, d2, d3, d4 };
    checkAPI();
    mPy = reinterpret_cast<PyArrayObject *>(PyArray_SimpleNewFromData(4, dims, classTypenum(), data));
}

inline void _capsuleDtor(PyObject *capsule)
{
    std::free(PyCapsule_GetPointer(capsule, 0));
}
template<typename T> NPArray<T>& NPArray<T>::ownData()
{
    if (!PyArray_BASE(mPy))
        PyArray_SetBaseObject(mPy, PyCapsule_New(data(), 0, _capsuleDtor));
    return *this;
}

template<typename T> NPArray<T> *NPArray<T>::create(PyObject *py)
{
    if (py && PyArray_Check(py) && checkType(reinterpret_cast<PyArrayObject *>(py)))
        return new NPArray(py);
    return 0;
}

template<typename T> bool NPArray<T>::own(PyArrayObject *py)
{
    checkAPI();
    if (mPy)
        Py_DECREF(mPy);
    mPy = (py && checkType(py)) ? py : 0;
    return mPy;
}
template<typename T> bool NPArray<T>::own(PyObject *py)
{
    checkAPI();
    if (mPy)
        Py_DECREF(mPy);
    mPy = (py && PyArray_Check(py)) ? reinterpret_cast<PyArrayObject *>(py) : 0;
    if (mPy && !checkType(mPy))
        mPy = 0;
    return mPy;
}

template<typename T> constexpr int NPArray<T>::classTypenum()
{
    if constexpr (std::is_same_v<float, T>)
        return NPY_FLOAT32;
    else if constexpr (std::is_same_v<double, T>)
        return NPY_FLOAT64;
    else if constexpr (std::is_same_v<int32_t, T>)
        return NPY_INT32;
    else if constexpr (std::is_same_v<uint32_t, T>)
        return NPY_UINT32;
    else if constexpr (std::is_same_v<uint64_t, T>)
        return NPY_UINT64;
    else if constexpr (std::is_same_v<int8_t, T>)
        return NPY_INT8;
    else if constexpr (std::is_same_v<uint8_t, T>)
        return NPY_UBYTE;
    else if constexpr (std::is_same_v<int16_t, T>)
        return NPY_INT16;
    else if constexpr (std::is_same_v<uint16_t, T>)
        return NPY_UINT16;
    else
        static_assert(sizeof(T) == 0, "unsupported type for NumPy");
    return NPY_VOID;
}

template<typename T> bool NPArray<T>::checkType(PyArrayObject *py)
{
    if (PyArray_TYPE(py) == classTypenum())
        return true;
    if constexpr (std::is_same_v<uint8_t, T>)
        return PyArray_TYPE(py) == NPY_BOOL ||
               PyArray_TYPE(py) == NPY_BYTE ||
               PyArray_TYPE(py) == NPY_UINT8;
    return false;
}

template<typename T> bool NPArray<T>::isPacked() const
{
    uint sz = sizeof(T);
    for (int d = degree() - 1; d >= 0; --d) {
        if (sz != stride(d))
            return false;
        sz *= dim(d);
    }
    if (size() != sz)
        return false;
    return true;
}

template<typename T> void NPArray<T>::copyto(std::vector<T> &v) const
{
    assert(isPacked());
    v.resize(count());
    std::memcpy(v.data(), data(), v.size() * sizeof(T));
}

} // namespace py

using FloatNPArray = py::NPArray<float>;

#endif // GYM_PCB_PYARRAY_H
