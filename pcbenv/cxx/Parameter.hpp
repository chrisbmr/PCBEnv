#ifndef GYM_PCB_PARAMETER_H
#define GYM_PCB_PARAMETER_H

#include <set>
#include <variant>
#include "Py.hpp"

class Parameter
{
public:
    Parameter(const char *name, const std::function<void(const Parameter&)> &commit);
    const std::string& name() const { return mName; }
    char dtype() const;
    void set(double);
    void set(int64_t);
    void set(const std::string&);
    void set(const char *s) { set(std::string(s)); }
    void set(bool);
    void set(PyObject *);
    void setLimits(double def, double min, double max);
    void setLimits(int64_t def, int64_t min, int64_t max);
    void addEnum(const std::string &s) { mEnum.insert(s); }
    void setVisible(bool b) { mVisible = b; }
    bool visible() const { return mVisible; }
    void init(bool b) { mValue = b; }
    void init(const std::string &s) { mValue = s; }
    void init(const char *s) { init(std::string(s)); }
    void setNotifier(const std::function<void()> &notify) { mNotify = notify; }
    double min() const { return mLimitMin; }
    double max() const { return mLimitMax; }
    double d() const;
    bool b() const;
    const std::string& s() const;
    const std::set<std::string>& getEnum() const { return mEnum; }
    int64_t i() const;
    int64_t as_i() const;
    double as_d() const;
    std::string as_s() const;
    bool as_b() const;
    template<typename T> bool is_a() const { return std::holds_alternative<T>(mValue); }
private:
    const std::string mName;
    std::variant<double, int64_t, std::string, bool> mValue;
    std::set<std::string> mEnum;
    double mLimitMin{-std::numeric_limits<double>::infinity()};
    double mLimitMax{+std::numeric_limits<double>::infinity()};
    const std::function<void(const Parameter&)> mCommit;
    bool mVisible{true};
    std::function<void()> mNotify;
};

inline char Parameter::dtype() const
{
    assert(mValue.index() >= 0 && mValue.index() < 4);
    return "disb"[mValue.index()];
}
inline double Parameter::d() const
{
    return std::get<double>(mValue);
}
inline int64_t Parameter::i() const
{
    return std::get<int64_t>(mValue);
}
inline const std::string& Parameter::s() const
{
    return std::get<std::string>(mValue);
}
inline bool Parameter::b() const
{
    return std::get<bool>(mValue);
}

#endif // GYM_PCB_PARAMETER_H
