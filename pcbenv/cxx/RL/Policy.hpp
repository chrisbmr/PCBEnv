#ifndef GYM_PCB_RL_POLICY_H
#define GYM_PCB_RL_POLICY_H

#include "Py.hpp"
#include <vector>

/**
 * A Policy class for C++ agents.
 */
class Policy
{
public:
    /**
     * Common policy parameters.
     */
    struct CommonParams
    {
        std::string_view Class{"constant"};
        float Epsilon{0.0f};
        float Temperature{1.0f};
        float Tolerance{0.0f};
        float MinProbability{0.0f}; //!< for stochastic policy only, applied before temperature

        static CommonParams from(PyObject *);
    };
public:
    static Policy *create(const CommonParams&);
public:
    virtual uint decide(const std::vector<float>&) { return 0; }

    virtual ~Policy() { }
    virtual void setFastActionMask(uint32_t) { }
    virtual void setEpsilon(float) { }
    virtual void setTemperature(float) { }
    virtual float getEpsilon() const { return 1.0f; }
    virtual float getTemperature() const { return 1.0f; }
};

#endif // GYM_PCB_RL_POLICY_H
