
#include "RL/RRRAgent.hpp"
#include "Log.hpp"
#include "PCBoard.hpp"
#include "Connection.hpp"
#include "Track.hpp"
#include "Via.hpp"
#include "Net.hpp"
#include "RasterizerMidpoint.hpp"

RRRAgent::RRRAgent() : Agent("rrr")
{
    initParameters();
    mAStarCosts.reset();
}
RRRAgent::~RRRAgent()
{
}

bool RRRAgent::_run()
{
    if (!init())
        return false;
    for (mIteration = 0; mIteration < mMaxIterations && !hasTimerExpired(); ++mIteration) {
        DEBUG("RRR iteration " << mIteration);
        reroutePolicy();
        mProgress = float(mIteration) / mMaxIterations;
        mScore.Success = rerouteHistoryOneByOne();
        if (mErrorState)
            break;
        mScore = checkRouting();
        INFO("Iteration " << mIteration << ": score " << mScore.R << " success=" << mScore.Success);
        if (mScoreMax.Success || mCheckStagnationBeforeSuccess)
            mIterationsStagnant++;
        if (mScore > mScoreMax) {
            mScoreMax = mScore;
            mIterationsStagnant = 0;
        }
        if (mStats.shouldAdd(mScore))
            mStats.add(ResultRecord(*this, mScore));
        if ((mIteration + 1) >= mMinIterations && mIterationsStagnant >= mMaxIterationsStagnant)
            break;
    }
    mProgress = 1.0f;
    mScore = postroute();
    if (mStats.shouldAdd(mScore))
        mStats.add(ResultRecord(*this, mScore));
    return mScore.Success;
}

bool RRRAgent::init()
{
    startTimer();
    mPostrouteStage = false;
    mErrorState = false;
    mIterationsStagnant = 0;
    mScore = Action::Result(-std::numeric_limits<Reward>::infinity(), false);
    mScoreMax = mScore;
    const auto W = mConnections[0]->defaultTraceWidth();
    const auto C = mConnections[0]->clearance();
    mConnectionOrder.clear();
    for (auto X : mConnections) {
        mConnectionOrder.push_back(mConnectionOrder.size());
        if (X->defaultTraceWidth() != W || X->clearance() != C || X->defaultViaDiameter() != W)
            WARN("RRR requires all track widths " << X->defaultTraceWidth() << " and clearances " << X->clearance() << " to be the same and via diameter " << X->defaultViaDiameter() << " to equal track width to work as expected!");
    }
    if (true)
        std::shuffle(mConnectionOrder.begin(), mConnectionOrder.end(), RNG);
    return true;
}

void RRRAgent::setParameter(const std::string &varName, int index, PyObject *py)
{
    if (varName == "astar_costs")
        mAStarCosts.setPy(py);
    else
        Agent::setParameter(varName, index, py);
}

bool RRRAgent::routeHistoryAll()
{
    bool rv = true;
    for (auto X : mConnections)
        if (!routeHistory(*X))
            rv = false;
    return rv;
}

bool RRRAgent::rerouteHistoryOneByOne()
{
    decayHistoryCosts(mHistoryCostDecay);

    if (mRandomizeOrder) // don't shuffle mConnections as it must match with mSavedTracks
        std::shuffle(mConnectionOrder.begin(), mConnectionOrder.end(), RNG);

    bool rv = true;
    try {
        for (auto i : mConnectionOrder)
            if (!rerouteHistory(*mConnections[i]))
                rv = false;
    } catch (const std::runtime_error &e) {
        ERROR(e.what());
        mErrorState = true;
    }
    return rv;
}

void RRRAgent::unrouteHistoryAll()
{
    for (auto X : mConnections)
        if (X->isRouted())
            unrouteHistory(*X);
}

void RRRAgent::unrouteAll()
{
    for (auto X : mConnections)
        if (X->isRouted())
            unroute(*X);
}

Action::Result RRRAgent::routeProperlyAll()
{
    DEBUG("RRR: rerouting without overlap");
    bool rv = true;
    for (auto X : mConnections) {
        unrouteHistory(*X);
        if (rv && !route(*X)) // NOTE: the reward doesn't make sense when rv = false
            rv = false;
    }
    Action::Result res(0.0f, rv, true);
    res.R = getRewardFn()(mConnections, &res.Router);
    return res;
}

Action::Result RRRAgent::postroute()
{
    mPostrouteStage = true;

    unrouteHistoryAll();
    mPCB->getNavGrid().setCosts(1.0f);
    mPCB->getNavGrid().resetUserKeepouts();

    INFO("RRR: restoring best routing with success=" << mScoreMax.Success << " score=" << mScoreMax.R);

    assert(mScoreMaxTracks.size() == mConnections.size());
    restoreTracks(mScoreMaxTracks);

    INFO("RRR: tidying up");
    bool rv = true;
    for (uint n = 0; n < mNumTidyIterations && rv; ++n)
        for (uint i = 0; i < mConnections.size(); ++i)
            if (!reroute(*mConnections[i]))
                if (mScoreMax.Success)
                    restoreTrack(i, mScoreMaxTracks[i]);
    Action::Result res(0.0f, rv, true);
    res.R = getRewardFn()(mConnections, &res.Router);
    return res;
}

void RRRAgent::saveTracks(std::vector<Track> &tracks)
{
    tracks.clear();
    for (auto X : mConnections)
        tracks.emplace_back(X->hasTracks() ? X->getTrack(0) : Track(Point_25(0,0,0)));
}
void RRRAgent::restoreTracks(std::vector<Track> &tracks)
{
    for (uint i = 0; i < mConnections.size(); ++i)
        restoreTrack(i, tracks.at(i));
}
void RRRAgent::restoreTrack(uint i, Track &T)
{
    getStepLock().wait();
    auto X = mConnections.at(i);
    if (T.empty())
        INFO("No track for connection " << X->name());
    if (X->isRouted())
        throw std::runtime_error("cannot restore tracks for routed connection");
    if (T.empty())
        return;
    T.resetRasterizedCount();
    std::lock_guard wlock(mPCB->getLock());
    X->setTrack(T);
    if (!mPostrouteStage) {
        rasterize(*X, 1, false);
        mPCB->setChanged(PCB_CHANGED_ROUTES | PCB_CHANGED_NAV_GRID);
    } else {
        mPCB->rasterizeTracks(*X);
    }
}

Action::Result RRRAgent::checkRouting()
{
    DEBUG("RRR: Checking routes...");
    saveTracks(mSavedTracks);
    auto res = routeProperlyAll();
    if (res > mScoreMax)
        saveTracks(mScoreMaxTracks);
    unrouteAll();
    restoreTracks(mSavedTracks);
    return res;
}

class PathfinderROP final : public BaseROP
{
private:
    NavGrid *mGrid;
public:
    void setTarget(NavGrid &nav) { mGrid = &nav; }
    void write(NavPoint&);
    void writeRangeZYX(uint Z0, uint Z1, uint Y0, uint Y1, uint X0, uint X1) override;
    float HistCostIncrementSize;
    int32_t HistCostNumIncrements;
    int32_t HistCostMaxIncrements;
    uint32_t OverlapCount{0};
    uint16_t WriteSeq;
    int16_t Value;
};
inline void PathfinderROP::writeRangeZYX(uint Z0, uint Z1, uint Y0, uint Y1, uint X0, uint X1)
{
    for (uint Z = Z0; Z <= Z1; ++Z) {
    for (uint Y = Y0; Y <= Y1; ++Y) {
        const auto I0 = mGrid->LinearIndex(Z, Y, X0);
        const auto I1 = I0 + (X1 - X0);
        for (uint i = I0; i <= I1; ++i)
            write(mGrid->getPoint(i));
    }}
}
inline void PathfinderROP::write(NavPoint &nav)
{
    if (nav.getWriteSeq() == WriteSeq)
        return;
    nav.setWriteSeq(WriteSeq);

    auto &KO = nav.getKOCounts();
    KO._User[0] += Value;
    assert(KO._User[0] >= 0 && "probable inconsistency after spacings change");
    if (KO._User[0] > 1)
        OverlapCount++;

    uint16_t H = KO._User[1];
    if (Value > 0 && KO._User[0] > 1 && HistCostNumIncrements)
        KO._User[1] = H = std::min(int32_t(H) + HistCostNumIncrements, HistCostMaxIncrements);
    nav.setCost((1.0f + H * HistCostIncrementSize) * (KO._User[0] + 1));
}

bool RRRAgent::routeHistory(Connection &X)
{
    getStepLock().wait();
    countActions(1);
    updateSpacings(X);
    auto rv = mPCB->runPathFinding(X, 0, &mAStarCosts);
    if (!rv)
        throw std::runtime_error("route cannot be realized in reroute stage");
    std::lock_guard wlock(mPCB->getLock());
    const uint ov = rasterize(X, 1, true);
    mPCB->setChanged(PCB_CHANGED_ROUTES | PCB_CHANGED_NAV_GRID);
    return ov == 0;
}

void RRRAgent::unrouteHistory(Connection &X)
{
    getStepLock().wait();
    std::lock_guard wlock(mPCB->getLock());
    rasterize(X, -1, false);
    X.clearTracks();
    mPCB->setChanged(PCB_CHANGED_ROUTES | PCB_CHANGED_NAV_GRID);
}

bool RRRAgent::rerouteHistory(Connection &X)
{
    DEBUG("RRR: reroute " << X.name());
    unrouteHistory(X);
    bool rv = routeHistory(X);
    return rv;
}

void RRRAgent::decayHistoryCosts(float f)
{
    if (f == 1.0f)
        return;
    for (auto &nav : mPCB->getNavGrid().getPoints()) {
        uint16_t H = nav.getKOCounts()._User[1];
        nav.getKOCounts()._User[1] = std::ceil(H * f);
    }
}

uint RRRAgent::rasterize(const Connection &X, int8_t value, bool updateHistoryCost) const
{
    NavGrid &nav = mPCB->getNavGrid();
    Rasterizer<PathfinderROP> R(nav);
    R.OP.setTarget(nav);
    R.OP.WriteSeq = nav.nextRasterSeq();
    R.OP.Value = value;
    R.OP.HistCostIncrementSize = mHistoryCostIncrement;
    R.OP.HistCostNumIncrements = updateHistoryCost ? 1 : 0;
    R.OP.HistCostMaxIncrements = mHistoryCostMaxIncrements;
    R.setExpansion(nav.getSpacings().getExpansionForTracks(X.clearance()));
    for (const auto *T : X.getTracks())
        R.rasterizeFill(*T, RASTERIZE_MASK_ALL);
    return R.OP.OverlapCount;
}

void RRRAgent::updateSpacings(const Connection &X)
{
    NavGrid &nav = mPCB->getNavGrid();
    if (!nav.setSpacings(NavSpacings(X)))
        return;
    nav.resetUserKeepout(0);
    if (mPostrouteStage)
        return;
    for (const auto X : mConnections)
        rasterize(*X, 1, false);
}

bool RRRAgent::route(Connection &X)
{
    getStepLock().wait();
    countActions(1);
    updateSpacings(X);
    auto rv = mPCB->runPathFinding(X, 0, &mAStarCosts);
    if (!rv)
        return false;
    mPCB->rasterizeTracks(X);
    return true;
}
void RRRAgent::unroute(Connection &X)
{
    getStepLock().wait();
    WITH_WLOCK(mPCB, mPCB->eraseTracks(X));
}

bool RRRAgent::reroute(Connection &X)
{
    unroute(X);
    return route(X);
}

void RRRAgent::initParameters()
{
    mParameters["min_iterations"] = new Parameter("Min Iterations", [this](const Parameter &v){ setMinIterations(v.i()); });
    mParameters["max_iterations"] = new Parameter("Max Iterations", [this](const Parameter &v){ setMaxIterations(v.i()); });
    mParameters["max_iterations_stagnant"] = new Parameter("Max Stagnant Iterations", [this](const Parameter &v){ setMaxIterationsStagnant(v.i()); });
    mParameters["tidy_iterations"] = new Parameter("Tidying Iterations", [this](const Parameter &v){ setNumTidyIterations(v.i()); });
    mParameters["history_cost_decay"] = new Parameter("History Cost Decay", [this](const Parameter &v){ setHistoryCostDecay(v.d()); });
    mParameters["history_cost_increment"] = new Parameter("History Cost Increment", [this](const Parameter &v){ setHistoryCostIncrement(v.d()); });
    mParameters["history_cost_max"] = new Parameter("History Cost Maximum Increments", [this](const Parameter &v){ setHistoryCostMaxIncrements(v.i()); });
    mParameters["randomize_order"] = new Parameter("Randomize Order", [this](const Parameter &v){ setRandomizeOrder(v.b()); });

    mParameters["min_iterations"]->setLimits(int64_t(mMinIterations), 0, std::numeric_limits<int32_t>::max());
    mParameters["max_iterations"]->setLimits(int64_t(mMaxIterations), 0, std::numeric_limits<int32_t>::max());
    mParameters["max_iterations_stagnant"]->setLimits(int64_t(mMaxIterationsStagnant), 0, std::numeric_limits<int32_t>::max());
    mParameters["tidy_iterations"]->setLimits(int64_t(mNumTidyIterations), 0, 1024);
    mParameters["history_cost_decay"]->setLimits(mHistoryCostDecay, 0.0, 1.0);
    mParameters["history_cost_increment"]->setLimits(mHistoryCostIncrement, 0.0, 1024.0);
    mParameters["history_cost_max"]->setLimits(int64_t(mHistoryCostMaxIncrements), 0, 0xfffe);
    mParameters["randomize_order"]->init(false);
}

PyObject *RRRAgent::get_state(PyObject *py)
{
    py::Object req(py);

    py::GIL_guard GIL;
    const auto name = req.asStringView("RRR get_state: argument must be a string");
    if (name == "stats")
        return mStats.getPy();
    return py::ValueError("RRR get_state: unknown request");
}

void RRRAgent::reroutePolicy()
{
    if (!havePythonInterface())
        return;
    if (!**mConnectionsPy) {
        auto L = py::Object::new_List(mConnections.size());
        if (!L)
            return;
        for (uint i = 0; i < L.listSize(); ++i)
            L.setItem(i, mConnections.at(i)->getEndsPy());
        mConnectionsPy.reset(*L, 0);
    }
    auto order = py_P(mConnectionsPy.getRef());

    mConnectionOrder.clear();
    for (auto v : order)
        mConnectionOrder.push_back(static_cast<uint>(v));
    mRandomizeOrder = false;
}
