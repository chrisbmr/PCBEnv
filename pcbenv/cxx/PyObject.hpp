
#include "Py.hpp"

namespace py
{

/// NOTE: This does not do any automatic reference counting.
class Object
{
public:
    Object(PyObject *py) : mPy(py) { }

    // These create a new reference:
    Object(bool);
    Object(double);
    Object(int64_t);
    Object(const char *);
    Object(const std::string&);
    Object(const Point_2&);
    Object(const Point_25&);
    Object(const IPoint_3&);
    Object(const Bbox_2&);

    static Object new_List(uint n, PyObject *v = 0);
    static Object new_TupleOfLongs(const Point_25 &v, Real s);
    template<typename T> static Object new_TupleOfLongs(const T *, uint size);
    template<typename T> static Object new_ListFrom(const std::vector<T>&, auto&&...);

    const Object& incRef() const;
    const Object& decRef() const;

    PyObject **o() { return &mPy; }
    PyObject *operator*() const { return mPy; }

    Object at(uint i) const;
    Object elem(uint i, const char *err = 0) const;
    Object item(uint i) const;
    Object item(const char *name) const;

    void setElem(uint i, const Object&) const;
    void setElem(uint i, PyObject *) const;
    void setItem(uint i, const Object&) const;
    void setItem(uint i, PyObject *) const;
    void setDictItem(uint i, PyObject *) const;
    void setItem(const char *name, const Object&) const;
    void refItem(const char *name, const Object&) const;
    void setItem(const char *name, PyObject *) const;
    void refItem(const char *name, PyObject *) const;

    bool isNone() const { return mPy == Py_None; }
    bool isDict() const;
    bool isList() const;
    bool isList(uint n) const;
    bool hasListItem(uint i) const;
    bool isTuple() const;
    bool isTuple(uint n) const;
    bool hasTupleElem(uint i) const;
    bool isTupleOrList(uint n) const;

    uint listSize(const char *err = "object is not a list") const;

    template<typename T> T asIntegerContainer() const;
    template<typename T> T asStringContainer() const;

    operator bool() const;
    // Amazing C++ feature: disable cast to ints
    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    operator T() const = delete;

    bool isBool() const;
    bool isBool(bool value) const;
    bool isFloat() const;
    bool isLong() const;
    bool isNumber() const;
    bool asBool(const char *err = "object is not a boolean") const;
    bool toBool() const;
    double asDouble(const char *err = "object is not a float") const;
    double toDouble(const char *err = "object is not a number") const;
    long asLong() const;
    long toLong(const char *err = "object is not a number") const;

    bool isString() const;
    std::string asString(const char *err = "object is not a string") const;
    std::string toString() const;
    std::string_view asStringView(const char *err = "object is not a string") const;

    bool isVector(uint n) const;
    bool isLongVector(uint n) const;
    bool isPoint_25() const;
    IPoint_2 asIPoint_2(const char *err = "object is not a 2D integer point") const;
    IPoint_3 asIPoint_3(const char *err = "object is not a 3D integer point") const;
    IVector_2 asIVector_2(const char *err = "object is not a 2D integer vector") const;
    IVector_3 asIVector_3(const char *err = "object is not a 3D integer vector") const;
    Vector_2 asVector_2(const char *err = "object is not a 2D vector") const;
    Point_2 asPoint_2(const char *err = "object is not a 2D point") const;
    Point_25 asPoint_25(const char *err = "object is not a 2.5D point") const;

    ::Bbox_2 asBbox_2() const;
    ::IBox_3 asIBox_3() const;
    ::Bbox_2 asBbox_25(uint &zmin, uint &zmax) const;

    uint64_t asBitmask(uint size) const;
private:
    PyObject *mPy;
};

class BorrowedRef
{
public:
    BorrowedRef(PyObject *py = 0) : mRef(py) { }
    Object* operator->() { return &mRef; }
    Object& operator*() { return mRef; }
    operator bool() const { return (bool)mRef; }
private:
    Object mRef;
};

class ObjectRef
{
public:
    ObjectRef(PyObject *, uint refs = 0);
    ~ObjectRef();
    void reset(PyObject *, uint refs = 0);
    const Object* operator->() const { return &mRef; }
    const Object& operator*() const { return mRef; }
    PyObject *getRef() const;
private:
    Object mRef;
};
inline ObjectRef::ObjectRef(PyObject *py, uint refs) : mRef(py)
{
    while (refs--)
        mRef.incRef();
}
inline ObjectRef::~ObjectRef()
{
    mRef.decRef();
}
inline PyObject *ObjectRef::getRef() const
{
    return *mRef.incRef();
}
inline void ObjectRef::reset(PyObject *py, uint refs)
{
    if (py && py == *mRef)
        throw std::runtime_error("objectref reset object must differ");
    if (refs && !py)
        throw std::runtime_error("objectref asked to reference 0");
    while (refs--)
        Py_INCREF(py);
    mRef.decRef();
    *(mRef.o()) = py;
}

struct DictIterator
{
    DictIterator(PyObject *py);
    bool next();
    Object d;
    Object k;
    Object v;
    Py_ssize_t pos{0};
};
inline DictIterator::DictIterator(PyObject *py) : d(py), k(Py_None), v(Py_None)
{
    if (!d.isDict())
        throw std::invalid_argument("object is not a dictionary");
}
inline bool DictIterator::next()
{
    return PyDict_Next(*d, &pos, k.o(), v.o());
}

inline Object::Object(bool b) : mPy(PyBool_FromLong(b))
{
}

inline Object::Object(double d) : mPy(PyFloat_FromDouble(d))
{
}

inline Object::Object(int64_t i) : mPy(PyLong_FromLong(i))
{
}

inline Object::Object(const char *s) : mPy(PyUnicode_FromString(s))
{
}

inline Object::Object(const std::string &s) : mPy(PyUnicode_FromStringAndSize(s.c_str(), s.size()))
{
}

inline Object::Object(const Point_2 &v) : mPy(PyTuple_New(2))
{
    PyTuple_SetItem(mPy, 0, PyFloat_FromDouble(v.x()));
    PyTuple_SetItem(mPy, 1, PyFloat_FromDouble(v.y()));
}

inline Object::Object(const Point_25 &v) : mPy(PyTuple_New(3))
{                                                      
    PyTuple_SetItem(mPy, 0, PyFloat_FromDouble(v.x()));
    PyTuple_SetItem(mPy, 1, PyFloat_FromDouble(v.y()));
    PyTuple_SetItem(mPy, 2, PyLong_FromLong(v.z()));
}

inline Object::Object(const IPoint_3 &v) : mPy(PyTuple_New(3))
{
    PyTuple_SetItem(mPy, 0, PyLong_FromLong(v.x));
    PyTuple_SetItem(mPy, 1, PyLong_FromLong(v.y));
    PyTuple_SetItem(mPy, 2, PyLong_FromLong(v.z));
}

inline const Object& Object::incRef() const
{
    if (mPy)
        Py_INCREF(mPy);
    return *this;
}
inline const Object& Object::decRef() const
{
    if (mPy)
        Py_DECREF(mPy);
    return *this;
}

inline Object Object::new_List(uint n, PyObject *v)
{
    auto py = PyList_New(n);
    if (v)
        for (uint i = 0; i < n; ++i)
            PyList_SetItem(py, i, NewRef(v));
    return Object(py);
}

template<typename T> inline Object Object::new_ListFrom(const std::vector<T> &data, auto&&... args)
{
    auto py = PyList_New(data.size());
    for (uint i = 0; i < data.size(); ++i)
        PyList_SetItem(py, i, data[i].getPy(std::forward<decltype(args)>(args)...));
    return Object(py);
}

inline Object Object::new_TupleOfLongs(const Point_25 &v, Real s)
{
    auto py = PyTuple_New(3);
    PyTuple_SetItem(py, 0, PyFloat_FromDouble(v.x() * s));
    PyTuple_SetItem(py, 1, PyFloat_FromDouble(v.y() * s));
    PyTuple_SetItem(py, 2, PyLong_FromLong(v.z()));
    return Object(py);
}
template<typename T> Object Object::new_TupleOfLongs(const T *array, uint size)
{
    auto py = PyTuple_New(size);
    for (uint i = 0; i < size; ++i)
        PyTuple_SetItem(py, i, PyLong_FromLong(array[i]));
    return Object(py);
}

inline Object::operator bool() const
{
    return mPy && mPy != Py_None;
}

inline bool Object::isBool() const
{
    return mPy && PyBool_Check(mPy);
}
inline bool Object::isBool(bool value) const
{
    return mPy == (value ? Py_True : Py_False);
}

inline bool Object::isFloat() const
{
    return mPy && PyFloat_Check(mPy);
}

inline bool Object::isLong() const
{
    return mPy && PyLong_Check(mPy);
}

inline bool Object::isNumber() const
{
    return mPy && PyNumber_Check(mPy);
}

inline bool Object::asBool(const char *err) const
{
    if (err && !isBool())
        throw std::invalid_argument(err);
    return Py_IsTrue(mPy);
}

inline bool Object::toBool() const
{
    return mPy && PyObject_IsTrue(mPy);
}

inline double Object::asDouble(const char *err) const
{
    if (!isFloat())
        throw std::invalid_argument(err);
    return PyFloat_AsDouble(mPy);
}

inline double Object::toDouble(const char *err) const
{
    if (err && !isNumber())
        throw std::invalid_argument(err);
    if (isFloat())
        return asDouble();
    auto F = PyNumber_Float(mPy);
    assert(F);
    if (!F)
        return std::numeric_limits<double>::quiet_NaN();
    auto d = PyFloat_AsDouble(F);
    Py_DECREF(F);
    return d;
}

inline long Object::asLong() const
{
    assert(isLong());
    return PyLong_AsLong(mPy);
}

inline long Object::toLong(const char *err) const
{
    if (err && !PyNumber_Check(mPy))
        throw std::invalid_argument(err);
    if (isLong())
        return asLong();
    auto L = PyNumber_Long(mPy);
    assert(L);
    if (!L)
        return 0L;
    auto li = PyLong_AsLong(L);
    Py_DECREF(L);
    return li;
}

inline bool Object::isString() const
{
    return mPy && PyUnicode_Check(mPy);
}

inline std::string Object::asString(const char *err) const
{
    if (err && !isString())
        throw std::invalid_argument(err);
    assert(isString());
#if PY_VERSION_HEX >= 0x03030000
    return std::string(PyUnicode_AsUTF8(mPy));
#else
#error "Python version < 3.3 not supported, go upgrade!"
#endif
}

inline std::string Object::toString() const
{
    if (!mPy)
        return "";
    if (isString())
        return asString();
    return ObjectRef(PyObject_Str(mPy))->asString();
}

inline std::string_view Object::asStringView(const char *err) const
{
    if (err && !isString())
        throw std::invalid_argument(err);
    assert(isString());
#if PY_VERSION_HEX >= 0x03030000
    Py_ssize_t size;
    const char *ucs = PyUnicode_AsUTF8AndSize(mPy, &size);
    return std::string_view(ucs, size);
#else
#error "Python version < 3.3 not supported, go upgrade!"
#endif
}

inline bool Object::isDict() const
{
    return mPy && PyDict_Check(mPy);
}
inline bool Object::isList() const
{
    return mPy && PyList_Check(mPy);
}
inline bool Object::isList(uint n) const
{
    return isList() && PyList_Size(mPy) == n;
}
inline bool Object::hasListItem(uint i) const
{
    return isList() && PyList_Size(mPy) > i;
}
inline bool Object::isTuple() const
{
    return mPy && PyTuple_Check(mPy);
}
inline bool Object::isTuple(uint n) const
{
    return isTuple() && PyTuple_Size(mPy) == n;
}
inline bool Object::hasTupleElem(uint i) const
{
    return isTuple() && PyTuple_Size(mPy) > i;
}
inline bool Object::isTupleOrList(uint n) const
{
    return isTuple(n) || isList(n);
}

template<typename T> T Object::asIntegerContainer() const
{
    auto I = PyObject_GetIter(mPy);
    if (!I)
        throw std::invalid_argument("object is not an iterable");
    T res;
    while (auto v = PyIter_Next(I)) {
        if (!PyLong_Check(v))
            throw std::invalid_argument("expected an iterable of integers");
        res.insert(res.end(), PyLong_AsLong(v));
        Py_DECREF(v);
    }
    Py_DECREF(I);
    return res;
}
template<typename T> T Object::asStringContainer() const
{
    auto I = PyObject_GetIter(mPy);
    if (!I)
        throw std::invalid_argument("object is not an iterable");
    T res;
    while (auto _v = PyIter_Next(I)) {
        Object v(_v);
        if (!v.isString())
            throw std::invalid_argument("expected an iterable of strings");
        res.insert(res.end(), v.asString());
        v.decRef();
    }
    Py_DECREF(I);
    return res;
}

inline Object Object::at(uint i) const
{
    if (isList())
        return item(i);
    if (!isTuple())
        throw std::invalid_argument("expected a list or tuple");
    return elem(i);
}
inline Object Object::elem(uint i, const char *err) const
{
    if (err && !hasTupleElem(i))
        throw std::invalid_argument(err);
    assert(hasTupleElem(i));
    return PyTuple_GetItem(mPy, i);
}
inline Object Object::item(uint i) const
{
    assert(hasListItem(i));
    return PyList_GetItem(mPy, i);
}
inline Object Object::item(const char *name) const
{
    assert(isDict());
    return PyDict_GetItemString(mPy, name);
}

inline void Object::setElem(uint i, const Object &v) const
{
    setElem(i, *v);
}
inline void Object::setElem(uint i, PyObject *v) const
{
    assert(hasTupleElem(i));
    PyTuple_SetItem(mPy, i, v);
}

inline void Object::setItem(uint i, const Object &v) const
{
    setItem(i, *v);
}
inline void Object::setItem(uint i, PyObject *v) const
{
    assert(hasListItem(i));
    PyList_SetItem(mPy, i, v);
}

inline void Object::setItem(const char *name, const Object &v) const
{
    setItem(name, *v);
}
inline void Object::setItem(const char *name, PyObject *v) const
{
    assert(isDict());
    assert(v);
    PyDict_SetItemString(mPy, name, v);
    Py_DECREF(v);
}
inline void Object::setDictItem(uint i, PyObject *v) const
{
    assert(isDict());
    PyDict_SetItem(mPy, PyLong_FromLong(i), v);
    Py_DECREF(v);
}
inline void Object::refItem(const char *name, PyObject *v) const
{
    assert(isDict());
    PyDict_SetItemString(mPy, name, v);
}
inline void Object::refItem(const char *name, const Object &v) const
{
    refItem(name, *v);
}

inline bool Object::isVector(uint n) const
{
    if (!isTuple(n))
        return false;
    for (uint i = 0; i < n; ++i)
        if (!elem(i).isNumber())
            return false;
    return true;
}
inline bool Object::isLongVector(uint n) const
{
    if (!isTuple(n))
        return false;
    for (uint i = 0; i < n; ++i)
        if (!elem(i).isLong())
            return false;
    return true;
}
inline bool Object::isPoint_25() const
{
    return isTuple(3) && elem(0).isNumber() && elem(1).isNumber() && elem(2).isLong();
}

inline IPoint_2 Object::asIPoint_2(const char *err) const
{
    if (err && !isLongVector(2))
        throw std::invalid_argument(err);
    return IPoint_2(elem(0).asLong(), elem(1).asLong());
}

inline IPoint_3 Object::asIPoint_3(const char *err) const
{
    if (err && !isLongVector(3))
        throw std::invalid_argument(err);
    return IPoint_3(elem(0).asLong(), elem(1).asLong(), elem(2).asLong());
}

inline IVector_2 Object::asIVector_2(const char *err) const
{
    if (err && !isLongVector(2))
        throw std::invalid_argument(err);
    return IVector_2(elem(0).asLong(), elem(1).asLong());
}

inline IVector_3 Object::asIVector_3(const char *err) const
{
    if (err && !isLongVector(3))
        throw std::invalid_argument(err);
    return IVector_3(elem(0).asLong(), elem(1).asLong(), elem(2).asLong());
}

inline Vector_2 Object::asVector_2(const char *err) const
{
    if (err && !isVector(2))
        throw std::invalid_argument(err);
    return Vector_2(elem(0).toDouble(), elem(1).toDouble());
}

inline Point_2 Object::asPoint_2(const char *err) const
{
    if (err && !isVector(2))
        throw std::invalid_argument("object is not a 2D point");
    return Point_2(elem(0).toDouble(), elem(1).toDouble());
}

inline Point_25 Object::asPoint_25(const char *err) const
{
    if (err && !isPoint_25())
        throw std::invalid_argument(err);
    return Point_25(elem(0).toDouble(), elem(1).toDouble(), elem(2).asLong());
}

inline uint Object::listSize(const char *err) const
{
    if (err && !PyList_Check(mPy))
        throw std::invalid_argument(err);
    return PyList_Size(mPy);
}

} // namespace py
