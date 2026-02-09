#include "Log.hpp"
#include "Parameter.hpp"
#include "Util/Util.hpp"

Parameter::Parameter(const char *name, const std::function<void(const Parameter&)> &commit) : mName(name), mCommit(commit) { }

void Parameter::setLimits(double def, double min, double max)
{
    mLimitMin = min;
    mLimitMax = max;
    mValue    = def;
}
void Parameter::setLimits(int64_t def, int64_t min, int64_t max)
{
    min = std::max(-(int64_t(1) << 51), min);
    max = std::min(max, int64_t(1) << 51);
    assert(min <= max);
    mLimitMin = min;
    mLimitMax = max;
    mValue    = def;
    assert(int64_t(mLimitMin) == min);
    assert(int64_t(mLimitMax) == max);
}

void Parameter::set(double _v)
{
    double v = std::min(std::max(mLimitMin, _v), mLimitMax);
    if (v != _v)
        WARN("Parameter '" << mName << "' value " << _v << " bounded to " << v);
    mValue = v;
    mCommit(*this);
    if (mNotify)
        mNotify();
}
void Parameter::set(int64_t _v)
{
    int64_t v = _v;
    if (!std::isinf(mLimitMin)) v = std::max(int64_t(mLimitMin), v);
    if (!std::isinf(mLimitMax)) v = std::min(int64_t(mLimitMax), v);
    if (v != _v)
        WARN("Parameter '" << mName << "' value " << _v << " bounded to " << v << " (min: " << int64_t(mLimitMin) << " / max: " << int64_t(mLimitMax) << ')');
    mValue = v;
    mCommit(*this);
    if (mNotify)
        mNotify();
}
void Parameter::set(const std::string &v)
{
    if (mEnum.size() && !mEnum.contains(v))
        throw std::invalid_argument(fmt::format("invalid enum value {} for parameter '{}'", v, mName));
    mValue = v;
    mCommit(*this);
    if (mNotify)
        mNotify();
}
void Parameter::set(bool v)
{
    mValue = v;
    mCommit(*this);
    if (mNotify)
        mNotify();
}

void Parameter::set(PyObject *py)
{
    if (!py)
        throw std::invalid_argument("Parameter: PyObject must not be null");
    else if (PyFloat_Check(py))
        set(PyFloat_AsDouble(py));
    else if (PyBool_Check(py))
        set(bool(Py_IsTrue(py)));
    else if (PyLong_Check(py))
        set(int64_t(PyLong_AsLong(py)));
    else if (py::String_Check(py))
        set(py::String_AsStdString(py));
    else
        throw std::runtime_error("Parameter: PyObject is not a recognized type");
}

int64_t Parameter::as_i() const
{
    switch (dtype()) {
    default: return i();
    case 'd': return d();
    case 's': return std::stol(s());
    case 'b': return b();
    }
}
double Parameter::as_d() const
{
    switch (dtype()) {
    default: return d();
    case 'i': return i();
    case 's': return std::stol(s());
    case 'b': return b();
    }
}
std::string Parameter::as_s() const
{
    switch (dtype()) {
    default: return s();
    case 'd': return std::to_string(d());
    case 'i': return std::to_string(i());
    case 'b': return std::to_string(b());
    }
}
bool Parameter::as_b() const
{
    switch (dtype()) {
    default: return b();
    case 'd': return d();
    case 'i': return i();
    case 's': {
        const std::string &v = s();
        return !v.empty() && util::is1TrueYesOrPlus(v);
    }
    }
}
