
#ifndef GYM_PCB_RRRAGENT_H
#define GYM_PCB_RRRAGENT_H

#include "RL/Action.hpp"
#include "RL/Agent.hpp"
#include "RL/Reward.hpp"
#include "Track.hpp"
#include "NavGrid.hpp"
#include <random>

/**
 * A simple automatic rip-up and reroute (RRR) agent like Pathfinder [L. McMurchie and C. Ebeling, 1995].
 */
class RRRAgent : public Agent
{
public:
    RRRAgent();
    ~RRRAgent();

    bool _run() override;
    PyObject *get_state(PyObject *) override;

    /**
     * Override to process A-star cost parameter.
     */
    void setParameter(const std::string &varName, int index, PyObject *) override;

    void setMinIterations(uint);
    void setMaxIterations(uint);
    void setMaxIterationsStagnant(uint);
    void setNumTidyIterations(uint);
    void setCheckStagnationBeforeSuccess(bool);
    void setHistoryCostDecay(float);
    void setHistoryCostIncrement(float);
    void setHistoryCostMaxIncrements(int32_t);
    void setRandomizeOrder(bool);

private:
    uint mMinIterations{1};
    uint mMaxIterations{256};
    uint mMaxIterationsStagnant{8};
    uint mNumTidyIterations{2};
    bool mCheckStagnationBeforeSuccess{false};
    bool mRandomizeOrder{false};
    bool mPostrouteStage{false};
    float mHistoryCostDecay{1.0f};
    float mHistoryCostIncrement{1.0f/16.0f};
    int32_t mHistoryCostMaxIncrements{0xfffe};
    std::vector<uint> mConnectionOrder;
    std::mt19937 RNG{unsigned(std::chrono::system_clock::now().time_since_epoch().count())}; //!< for random order
    AStarCosts mAStarCosts;
private:
    bool init();
    void initParameters();
    bool routeHistoryAll();
    bool rerouteHistoryOneByOne();
    void unrouteHistoryAll();
    Action::Result routeProperlyAll();
    void unrouteAll();
    Action::Result postroute();
    Action::Result checkRouting();
    void saveTracks(std::vector<Track>&);
    void restoreTracks(std::vector<Track>&);
    uint mIterationsStagnant;
    Action::Result mScore;
    Action::Result mScoreMax;
    std::vector<Track> mScoreMaxTracks; //!< best result
    std::vector<Track> mSavedTracks;
    bool mErrorState;
    py::ObjectRef mConnectionsPy{0}; //!< argument for reroute policy call

    uint rasterize(const Connection&, int8_t value, bool updateHistoryCost) const;
    void decayHistoryCosts(float);

    void updateSpacings(const Connection&);

    bool routeHistory(Connection&);
    void unrouteHistory(Connection&);
    bool rerouteHistory(Connection&);
    bool route(Connection&);
    void unroute(Connection&);
    bool reroute(Connection&);
    void restoreTrack(uint i, Track&);

    void reroutePolicy(); //!< let Python update connection order (and selection)
};

inline void RRRAgent::setMinIterations(uint n)
{
    mMinIterations = n;
}
inline void RRRAgent::setMaxIterations(uint n)
{
    mMaxIterations = n;
}
inline void RRRAgent::setMaxIterationsStagnant(uint n)
{
    mMaxIterationsStagnant = n;
}
inline void RRRAgent::setNumTidyIterations(uint n)
{
    mNumTidyIterations = n;
}
inline void RRRAgent::setCheckStagnationBeforeSuccess(bool b)
{
    mCheckStagnationBeforeSuccess = b;
}
inline void RRRAgent::setHistoryCostIncrement(float v)
{
    if (v < 0.0f)
        throw std::runtime_error("history cost increment must be positive");
    mHistoryCostIncrement = v;
}
inline void RRRAgent::setHistoryCostDecay(float v)
{
    if (v < 0.0f || v > 1.0f)
        throw std::runtime_error("must have 0 <= history cost decay <= 1");
    mHistoryCostDecay = v;
}
inline void RRRAgent::setHistoryCostMaxIncrements(int32_t v)
{
    if (v < 0 || v > 0xfffe) // -1 so we can add 1 before doing max()
        throw std::runtime_error("history cost maximum increments must be between 0 and 65534");
    mHistoryCostMaxIncrements = v;
}
inline void RRRAgent::setRandomizeOrder(bool b)
{
    mRandomizeOrder = b;
}

#endif // GYM_PCB_RRRAGENT_H
