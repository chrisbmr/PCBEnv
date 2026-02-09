
#ifndef GYM_PCB_REWARD_H
#define GYM_PCB_REWARD_H

#include <vector>
#include "Py.hpp"

using Reward = float;

class RewardFunction;
class PCBoard;
class Connection;

struct RouterResult
{
    bool isSet() const { return !std::isnan(TrackLen); }
    float TrackLen{std::numeric_limits<float>::quiet_NaN()};
    int16_t NumVias{0};
    int16_t NumDisconnected{0};
    bool operator==(const RouterResult&) const = default;
    PyObject *getPy() const;
};

class RewardFunction
{
public:
    static RewardFunction *create(PyObject *);
public:
    virtual ~RewardFunction() { }
    virtual void setContext(const PCBoard&) { }
    virtual Reward operator()(const Connection&, RouterResult * = 0) const = 0;
    virtual Reward operator()(const std::vector<Connection *>&, RouterResult * = 0) const = 0;
    virtual Reward operator()(int unrouted) const = 0;
};

namespace rewards
{

class RouteLength : public RewardFunction
{
public:
    static RouteLength *create(PyObject *);
    RouteLength();
    virtual void setContext(const PCBoard&) override;
    Reward operator()(const Connection&, RouterResult * = 0) const override;
    Reward operator()(const std::vector<Connection *>&, RouterResult * = 0) const override;
    Reward operator()(int unrouted) const override;
private:
    float FPerUnrouted{0.0f};
    float FAnyUnrouted{0.0f};
    float FPerVia{0.0f};
    float FPerRouted{1.0f};
    float FScale{0.0f};
    std::string ScaleLength;
    std::string ScaleGlobal;
    char ScaleLength_{0};
    bool IgnoreNecessaryVias{true};
};

} // namespace rewards

#endif // GYM_PCB_REWARD_H
