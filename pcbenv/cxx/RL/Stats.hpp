#ifndef GYM_PCB_RL_STATS_H
#define GYM_PCB_RL_STATS_H

#include "RL/Action.hpp"
#include <vector>

class Agent;

struct ResultRecord
{
    ResultRecord() { }
    ResultRecord(const Agent&, const Action::Result&);
    ResultRecord(const Agent&);
    Action::Result asResult() const { return Action::Result(RewardSum, Success, true); }

    Reward RewardSum;
    bool Success;
    RouterResult Router;
    uint64_t TimeMicroseconds;
    uint64_t TimeStep;
    uint64_t Iteration;

    void reset(bool success, Reward r = std::numeric_limits<Reward>::infinity());
    std::string str() const;
    PyObject *getPy() const;
};

struct ResultCollection
{
    std::vector<ResultRecord> TimeLine;
    ResultRecord WorstSuccess;
    ResultRecord Worst;
    std::map<std::string, float> F32;
    std::map<std::string, int64_t> I64;

    bool shouldAdd(const Action::Result&) const;
    void add(ResultRecord&&);
    void reset();
    PyObject *getPy() const;
};

inline void ResultRecord::reset(bool success, Reward r)
{
    Success = success;
    RewardSum = r;
    Iteration = 0;
    TimeMicroseconds = 0;
    TimeStep = 0;
}

inline void ResultCollection::reset()
{
    TimeLine.clear();
    WorstSuccess.reset(false);
    Worst.reset(true);
    F32.clear();
    I64.clear();
}
inline bool ResultCollection::shouldAdd(const Action::Result &res) const
{
    return TimeLine.empty() ||
        (res > TimeLine.back().asResult()) ||
        (res < Worst.asResult()) ||
        (res.Success && res < WorstSuccess.asResult());
}
inline void ResultCollection::add(ResultRecord &&rr)
{
    const auto res = rr.asResult();
    if (TimeLine.empty() || res > TimeLine.back().asResult())
        TimeLine.emplace_back(rr);
    if (rr.Success && res < WorstSuccess.asResult())
        WorstSuccess = rr;
    if (res < Worst.asResult())
        Worst = rr;
}

#endif // GYM_PCB_RL_STATS_H
