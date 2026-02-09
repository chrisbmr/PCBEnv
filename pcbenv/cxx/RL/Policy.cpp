
#include "RL/Policy.hpp"
#include <bit>
#include <chrono>
#include <random>

class RandomPolicy : public Policy
{
public:
    uint decide(const std::vector<float>&) override;
    void setFastActionMask(uint32_t m) override { mFastActionMask = m; }
protected:
    std::mt19937 RNG{unsigned(std::chrono::system_clock::now().time_since_epoch().count())};
    uint32_t mFastActionMask{0xffffffff};
};
uint RandomPolicy::decide(const std::vector<float> &W)
{
    if (W.empty())
        throw std::runtime_error("policy asked to choose from empty action set");
    const uint a = std::uniform_int_distribution<uint>(0, W.size() - 1)(RNG);
    if (W.size() > 32)
        return a; // FIXME: use action space to get a set of legal actions
    const uint m = (1 << a);
    if (mFastActionMask & m)
        return a;
    const uint H = mFastActionMask & ~(m - 1); // legal actions > a
    const uint L = mFastActionMask &  (m - 1); // legal actions < a
    assert(H || L); // a legal action must exist
    if (H)
        return std::countr_zero(H);
    return std::countr_zero(L);
}

class EpsilonGreedyPolicy : public RandomPolicy
{
public:
    EpsilonGreedyPolicy(float epsilon, float tolerance) : mEpsilon(epsilon), mTolerance(tolerance) { }
    uint decide(const std::vector<float>&) override;
    void setEpsilon(float eps) override { mEpsilon = std::bernoulli_distribution(eps); }
    float getEpsilon() const override { return mEpsilon.param().p(); }
    void setTolerance(float tol) { mTolerance = tol; }
    float getTolerance() const { return mTolerance; }
private:
    float mTolerance;
    std::bernoulli_distribution mEpsilon;
    std::vector<uint> mChoices;
};
uint EpsilonGreedyPolicy::decide(const std::vector<float> &W)
{
    if (W.empty())
        throw std::runtime_error("policy asked to choose from empty action set");
    if (W.size() == 1)
        return 0;
    if (mEpsilon.param().p() > 0.0f && mEpsilon(RNG))
        return RandomPolicy::decide(W);
    float w_max = W[0];
    for (auto w : W)
        w_max = (w > w_max) ? w : w_max;
    for (uint a = 0; a < W.size(); ++a)
        if (W[a] == w_max || std::abs(W[a] - w_max) <= mTolerance) // NOTE: check equality in case of inf - inf
            mChoices.push_back(a);
    assert(mChoices.size() >= 1);
    uint a = (mChoices.size() == 1) ? mChoices[0] : mChoices[std::uniform_int_distribution<uint>(0, mChoices.size() - 1)(RNG)];
    mChoices.clear();
    return a;
}

class StochasticPolicy : public RandomPolicy
{
public:
    StochasticPolicy(float temperature, float p_min) : mPMin(p_min) { setTemperature(temperature); }
    uint decide(const std::vector<float>&) override;
    virtual void setTemperature(float T) override { mT = T; mTInv = 1.0f / T; }
    virtual float getTemperature() const override { return mT; }
protected:
    std::vector<float> mP;
    float mT;
    float mTInv;
    float mPMin;
    EpsilonGreedyPolicy mGreedy{0.0f, 0.0f};
};
uint StochasticPolicy::decide(const std::vector<float> &P)
{
    if (P.empty())
        throw std::runtime_error("policy asked to choose from empty action set");
    if (P.size() == 1)
        return 0;
    if (mTInv == 1.0f && mPMin == 0.0f)
        return std::discrete_distribution<uint>(P.begin(), P.end())(RNG);
    if (mTInv == 0.0f)
        return RandomPolicy::decide(P);
    float sum = 0.0f;
    mP.resize(P.size());
    for (uint a = 0; a < P.size(); sum += mP[a], ++a)
        mP[a] = std::pow(std::max(P[a], mPMin), mTInv);
    if (sum == 0.0f || !std::isfinite(sum))
        return mGreedy.decide(P);
    const uint a = std::discrete_distribution<uint>(mP.begin(), mP.end())(RNG);
    mP.clear();
    return a;
}

class SoftmaxPolicy : public Policy
{
public:
    SoftmaxPolicy(float temperature) { setTemperature(temperature); }
    uint decide(const std::vector<float>&) override;
    virtual void setTemperature(float T) override { mT = T; mTInv = 1.0f / T; }
    virtual float getTemperature() const override { return mT; }
protected:
    std::vector<float> mP;
    float mT;
    float mTInv;
    std::mt19937 RNG{unsigned(std::chrono::system_clock::now().time_since_epoch().count())};
};
uint SoftmaxPolicy::decide(const std::vector<float> &P)
{
    if (P.empty())
        throw std::runtime_error("policy asked to choose from empty action set");
    if (P.size() == 1)
        return 0;
    if (std::isinf(mTInv))
        return std::uniform_int_distribution<uint>(0, P.size() - 1)(RNG);
    mP.resize(P.size());
    for (uint a = 0; a < P.size(); ++a)
        mP[a] = std::exp(P[a] * mTInv);
    const uint a = std::discrete_distribution<uint>(mP.begin(), mP.end())(RNG);
    mP.clear();
    return a;
}

Policy *Policy::create(const Policy::CommonParams &param)
{
    const auto &name = param.Class;
    if (name == "greedy" || name == "epsilon_greedy")
        return new EpsilonGreedyPolicy(param.Epsilon, param.Tolerance);
    if (name == "stochastic")
        return new StochasticPolicy(param.Temperature, param.MinProbability);
    if (name == "softmax")
        return new SoftmaxPolicy(param.Temperature);
    if (name == "urandom")
        return new RandomPolicy();
    if (name == "constant")
        return new Policy();
    throw std::runtime_error("unknown policy name");
    return 0;
}
Policy::CommonParams Policy::CommonParams::from(PyObject *py)
{
    if (!py || !PyDict_Check(py))
        throw std::invalid_argument("policy specification must be a dict");

    auto k = PyDict_GetItemString(py, "class");
    if (!k)
        throw std::invalid_argument("no policy class specified");

    auto o = PyDict_GetItemString(py, "tolerance");
    auto e = PyDict_GetItemString(py, "ε");
    if (!e)
        e = PyDict_GetItemString(py, "epsilon");
    auto T = PyDict_GetItemString(py, "T");
    if (!T)
        T = PyDict_GetItemString(py, "temperature");

    auto p_min = PyDict_GetItemString(py, "p_min");

    if ((o && !PyNumber_Check(o)) ||
        (T && !PyNumber_Check(T)) ||
        (p_min && !PyNumber_Check(p_min)) ||
        (e && !PyNumber_Check(e)))
        throw std::invalid_argument("tolerance, epsilon, temperature must be numbers if specified");

    CommonParams param;
    param.Class = py::String_AsStringView(k);
    if (e)
        param.Epsilon = py::Number_AsDouble(e);
    if (T)
        param.Temperature = py::Number_AsDouble(T);
    if (o)
        param.Tolerance = py::Number_AsDouble(o);
    if (p_min)
        param.MinProbability = py::Number_AsDouble(p_min);
    if (param.MinProbability < 0.0f ||
        param.MinProbability > 1.0f)
        throw std::invalid_argument("policy min probability must be between 0 and 1");
    return param;
}
