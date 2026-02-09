
#include "Net.hpp"
#include "Log.hpp"
#include "Component.hpp"
#include "Track.hpp"
#include "Clone.hpp"
#include "PCBoard.hpp"
#include "Util/Util.hpp"
#include <regex>
#include <sstream>

Net::Net(const std::string &name) : mName(name)
{
}

Net::~Net()
{
    DEBUG("~Net(" << this << '/' << name() << ')');
    for (auto P : mPins)
        P->setNet(0);
    for (auto X : mConnections)
        delete X;
}


SignalType Net::getLikelySignalTypeForName(const std::string &_name)
{
    std::string name = util::tolower(_name);
    if (name == "gnd" || name == "gnda")
        return SignalType::GROUND;
    if (name == "vin" || name.find("vcc") != std::string::npos)
        return SignalType::POWER;
    if (std::regex_match(name, std::regex(R"(\+\d+(\.\d+)?v.*)")))
        return SignalType::POWER;
    if (std::regex_match(name, std::regex(R"(/?\d+(\.\d+)?v_.*)")))
        return SignalType::POWER;
    return SignalType::SIGNAL;
}

void Net::setId(uint id)
{
    if (id > 0x7fff)
        throw std::runtime_error("Net ID must be <= 0x7fff");
    mId = id;
    int xid = (id << 16) | 1;
    for (auto X : mConnections)
        X->setId(xid++);
}

void Net::setLayerMaskFromSignalType()
{
    assert(mBoard);
    mLayerMask = 0;
    for (auto &L : mBoard->getLayers())
        if (L.hasType(mSignalType))
            mLayerMask |= 1 << L.getNumber();
    setLayerMask(mLayerMask);
}

void Net::insert(Pin &pin)
{
    pin.setNet(this);
    mPins.insert(&pin);
}

std::string Net::str() const
{
    std::stringstream ss;
    ss << "Net " << id() << '/' << name() << " = {";
    for (const auto pin : mPins)
        ss << " " << pin->getFullName();
    ss << " }";
    return ss.str();
}

void Net::setLayerMask(uint32_t m)
{
    mLayerMask = m;
    for (auto X : mConnections)
        X->setLayerMask(m);
}

void Net::setViaDiameter(Real d)
{
    mRules.ViaDiameter = d;
    for (auto X : mConnections)
        X->setDefaultViaDiameter(d);
}

void Net::setMinTraceWidth(Real w)
{
    mRules.TraceWidth = w;
    for (auto X : mConnections)
        X->setDefaultTraceWidth(std::max(w, X->defaultTraceWidth()));
}

void Net::setMinClearance(Real c)
{
    mRules.Clearance = c;
    for (auto X : mConnections)
        X->setClearance(std::max(c, X->clearance()));
}

void Net::setRules(const DesignRules &rules)
{
    mRules = rules;
    for (auto X : mConnections)
        X->setRulesMin(rules);
}

void Net::setRulesMin(const DesignRules &rules)
{
    if (rules.ViaDiameter > mRules.ViaDiameter)
        setViaDiameter(rules.ViaDiameter);
    if (rules.Clearance > mRules.Clearance)
        setMinClearance(rules.Clearance);
    if (rules.TraceWidth > mRules.TraceWidth)
        setMinTraceWidth(rules.TraceWidth);
}

Connection *Net::addConnection(Pin *sourcePin, const Point_25& source,
                               Pin *targetPin, const Point_25& target)
{
    if (sourcePin && mPins.find(sourcePin) == mPins.end())
        throw std::runtime_error("pin for new connection not part of the net");
    if (targetPin && mPins.find(targetPin) == mPins.end())
        throw std::runtime_error("pin for new connection not part of the net");
    auto X = new Connection(this, source, sourcePin, target, targetPin);
    if (!X)
        return 0;
    if (getMinClearance() < 0.0 ||
        getViaDiameter() <= 0.0 ||
        getMinTraceWidth() <= 0.0)
        throw std::runtime_error(fmt::format("connection must have positive track dimensions: {}", mRules.str()));
    mConnections.push_back(X);
    X->setId((mId << 16) | mConnections.size());
    X->setLayerMask(mLayerMask);
    X->setRulesDefault(mRules);
    if (sourcePin) {
        sourcePin->addConnection(X);
        insert(*sourcePin);
    }
    if (targetPin) {
        targetPin->addConnection(X);
        insert(*targetPin);
    }
    return X;
}

Connection *Net::getConnectionBetween(const Pin &P0, const Pin &P1) const
{
    for (auto X : P0.connections()) {
        if (X->net() != this)
            throw std::runtime_error("pin has a connection that is not part of this net");
        if (X->sourcePin() == &P0 && X->targetPin() == &P1)
            return X;
        if (X->sourcePin() == &P1 && X->targetPin() == &P0)
            return X;
    }
    TRACE("Connection between " << P0.getFullName() << " & " << P1.getFullName() << " is not part of net topology.");
    return 0;
}

void Net::setColorAuto(bool usePalette)
{
    switch (signalType()) {
    default:
        setColor(usePalette ? Palette::Nets[id()] : Color::WHITE);
        break;
    case SignalType::POWER:
        if (Palette::PowerNet != Color::TRANSPARENT)
            setColor(Palette::PowerNet);
        else
            setColor(usePalette ? Palette::Nets[id()] : Color::WHITE);
        break;
    case SignalType::GROUND:
        if (Palette::GroundNet != Color::TRANSPARENT)
            setColor(Palette::GroundNet);
        else
            setColor(usePalette ? Palette::Nets[id()] : Color::WHITE);
        break;
    }
}

void Net::remove(Pin &P)
{
    assert(P.net() == this);
    // NOTE: This can remove a connection which will remove itself from the pin which will remove itself from its net so we have to set the net 0 here first.
    P.setNet(0);
    mPins.erase(&P);
    removeConnectionsWithNonmemberPins();
}

bool Net::extract(Component &C)
{
    DEBUG("Net::extract " << C.name());

    for (auto o : C.getPins()) {
        auto P = o->as<Pin>();
        assert(P);
        if (P->net() != this)
            continue;
        P->setNet(0);
        mPins.erase(P);
    }
    removeConnectionsWithNonmemberPins();
    return empty();
}

void Net::removeConnectionsWithNonmemberPins()
{
    for (uint i = 0; i < mConnections.size(); ++i) {
        auto X = mConnections[i];
        if (X->arePinsOnNet(this))
            continue;
        mConnections[i] = mConnections.back();
        mConnections.pop_back();
        delete X;
        --i;        
    }
}

void Net::rebuildPinSetFromConnections()
{
    // Clear out everything.
    for (auto P : mPins)
        P->setNet(0);
    mPins.clear();
    // Add back pins.
    for (const auto X : mConnections) {
        auto sPin = X->sourcePin();
        auto tPin = X->targetPin();
        if (sPin) {
            this->insert(*sPin);
            sPin->addConnection(X);
        }
        if (tPin) {
            this->insert(*tPin);
            tPin->addConnection(X);
        }
    }
}

bool Net::pruneConnections(std::set<Connection *> &keep)
{
    for (uint i = 0; i < mConnections.size(); ++i) {
        auto X = mConnections[i];
        if (keep.erase(X))
            continue;
        mConnections[i--] = mConnections.back();
        mConnections.pop_back();
        delete X;
    }
    // We don't know which pins were shared between multiple connections.
    rebuildPinSetFromConnections();
    return empty();
}

uint Net::validateTrack(const Track &T) const
{
    if (!mBoard)
        throw std::runtime_error("net must be associated with a board to validate tracks");

    const auto &LA = mBoard->getLayoutArea();
    uint mask = 0x3;

    for (const auto &s : T.getSegments()) {
        if (mask & 0x1)
            if (!LA.isInside(s.base()))
                mask &= 0x2;
        if (mask & 0x2) {
            if (!(mLayerMask & (1 << s.z())))
                mask &= 0x1;
            if (s.width() < getMinTraceWidth())
                mask &= 0x1;
        }
        if (!mask)
            return mask;
    }
    for (const auto &V : T.getVias()) {
        if (mask & 0x1)
            if (!LA.isInside(V))
                mask &= 0x2;
        if (mask & 0x2) {
            if (!(mLayerMask & (1 << V.zmin())) ||
                !(mLayerMask & (1 << V.zmax())))
                mask &= 0x1;
            if (V.diameter() < getViaDiameter())
                mask &= 0x1;
        }
        if (!mask)
            return mask;
    }
    return mask;
}

Net *Net::clone(CloneEnv &env) const
{
    auto N = new Net(mName);
    env.N[this] = N;
    N->mId = mId;
    N->mRules = mRules;
    N->mLayerMask = mLayerMask;
    N->mColor = mColor;
    N->mSignalType = mSignalType;
    if (N->getBoard() == &env.origin)
        N->setBoard(env.target);
    for (auto P : mPins)
        N->insert(*env.P[P]);
    for (auto X : mConnections)
        N->mConnections.push_back(X->clone(env));
    return N;
}

/// Conversion from Python

PyObject *Net::getConnectionsPy(uint depth, bool asNumpy) const
{
    auto py = PyList_New(mConnections.size());
    if (!py)
        return 0;
    for (uint i = 0; i < mConnections.size(); ++i)
        PyList_SetItem(py, i, depth ? mConnections[i]->getPy(depth - 1, asNumpy) : mConnections[i]->getEndsPy());
    return py;
}
PyObject *Net::getPinsPy() const
{
    auto py = PyList_New(mPins.size());
    if (!py)
        return 0;
    uint i = 0;
    for (auto P : mPins)
        PyList_SetItem(py, i++, py::String(P->getFullName()));
    return py;
}
PyObject *Net::getPy(uint depth, bool asNumpy) const
{
    if (depth == 0)
        return py::String(name());
    auto py = PyDict_New();
    if (!py)
        return 0;
    py::Dict_StealItemString(py, "type", py::String(to_string(signalType())));
    py::Dict_StealItemString(py, "track_width", PyFloat_FromDouble(mRules.TraceWidth));
    py::Dict_StealItemString(py, "via_diameter", PyFloat_FromDouble(mRules.ViaDiameter));
    py::Dict_StealItemString(py, "clearance", PyFloat_FromDouble(mRules.Clearance));
    py::Dict_StealItemString(py, "pins", getPinsPy());
    py::Dict_StealItemString(py, "connections", getConnectionsPy(depth, asNumpy));
    return py;
}


///
/// Kruskal's algorithm for minimum spanning tree.
///

class PinPair
{
public:
    PinPair(Pin *A, Pin *B) : P1(A), P2(B) { }
    Pin *P1;
    Pin *P2;
    Real distance2() const;
private:
    mutable Real mDistance2{-1.0};
};
inline Real PinPair::distance2() const
{
    if (mDistance2 >= 0.0)
        return mDistance2;
    mDistance2 = P1->getShape()->squared_distance(*P2->getShape());
    if (!P1->sharesLayer(*P2))
        mDistance2 = math::squared(std::sqrt(mDistance2) + 4.0 * P1->net()->getViaDiameter());
    return mDistance2;
}

/**
 * Kruskal's algorithm to create a minimum spanning tree between disconnected pins of a net.
 */
class ConnectNets
{
public:
    ConnectNets(Net &net) : mNet(net) { }
    bool connect();
private:
    Net &mNet;
    std::vector<PinPair> mPairs;
    std::map<Point_25, Pin *> mPoint2Pin;
    std::map<Point_25, uint> mPoint2Group;
    std::map<Pin *, uint> mPin2Group;
    std::vector<std::set<Point_25>> mPointGroups;
    std::vector<std::set<Pin *>> mPinGroups;
    std::vector<const Via *> mVias;
    std::vector<std::vector<const WideSegment_25 *>> mSegmentsZ;
    void initPinPoints();
    void associate(const Point_25&, Pin *);
    void connectPointGroups(const Connection&);
    void connectPointGroupsForSegments(uint z);
    void connectPointGroups(const Point_25&, const Point_25&);
    void createPinGroupsFromPointGroups();
    void collectPinPairs();
    void collectSegmentsAndVias();
    void connectPinGroups();
};
void ConnectNets::associate(const Point_25 &v, Pin *P)
{
    assert(mPin2Group.find(P) != mPin2Group.end());
    const uint g = mPin2Group[P];
    mPoint2Pin[v] = P;
    mPointGroups[g].insert(v);
    mPoint2Group[v] = g;
    DEBUG("Pin " << P->getFullName() << " <-> Point " << v << " in Group " << g);
}
void ConnectNets::initPinPoints()
{
    mPointGroups.resize(mNet.numPins());
    uint i = 0;
    for (auto P : mNet.getPins())
        mPin2Group[P] = i++;
    for (auto P : mNet.getPins())
        for (auto z = P->minLayer(); z <= P->maxLayer(); ++z)
            associate(Point_25(P->getCenter(), z), P);
    for (const auto X : mNet.connections()) {
        const auto Ps = X->sourcePin();
        const auto Pd = X->targetPin();
        if (Ps)
            for (auto z = Ps->minLayer(); z <= Ps->maxLayer(); ++z)
                associate(Point_25(X->source().xy(), z), Ps);
        if (Pd)
            for (auto z = Pd->minLayer(); z <= Pd->maxLayer(); ++z)
                associate(Point_25(X->target().xy(), z), Pd);
    }
    mPin2Group.clear();
}
void ConnectNets::connectPointGroups(const Connection &X)
{
    connectPointGroups(X.source(), X.target());

    for (const auto *T : X.getTracks()) {
        const std::vector<WideSegment_25> &segs = T->getSegments();
        for (uint i = 0; i < segs.size(); ++i) {
            const auto &s = segs[i];
            if (i > 0 && s.z() != segs[i-1].z())
                connectPointGroups(segs[i-1].target(), s.source());
            connectPointGroups(s.source(), s.target());
        }
        for (const auto &v : T->getVias())
            for (uint z = v.zmin() + 1; z <= v.zmax(); ++z)
                connectPointGroups(Point_25(v.location(), z - 1), Point_25(v.location(), z));
    }
}
void ConnectNets::connectPointGroupsForSegments(uint z)
{
    const auto &segs = mSegmentsZ.at(z);
    const uint N = segs.size();
    if (N > 20)
        DEBUG("PERFORMANCE: checking " << N << " segments against each other on net " << mNet.name() << " layer " << z << " is expensive and needs optimization");
    for (uint i = 1; i < N; ++i) {
    for (uint k = 0; k < i; ++k) {
        const auto *s1 = segs[i];
        const auto *s2 = segs[k];
        if (s1->intersects(*s2))
            connectPointGroups(s1->source(), s2->target()); // doesn't matter which ends
    }}
    for (const auto *s : segs) {
        for (const auto *v : mVias)
            if (CircleEx(v->getCircle()).intersects(*s))
                connectPointGroups(s->source(), Point_25(v->location(), s->z()));
        for (const auto *P : mNet.getPins())
            if (P->isOnLayer(z))
                if (P->getShape()->intersects(*s))
                    connectPointGroups(s->source(), Point_25(P->getCenter(), z));
    }
}
void ConnectNets::connectPointGroups(const Point_25 &v0, const Point_25 &v1)
{
    const auto G0 = mPoint2Group.find(v0);
    const auto G1 = mPoint2Group.find(v1);
    const uint g0 = G0 != mPoint2Group.end() ? G0->second : -1;
    const uint g1 = G1 != mPoint2Group.end() ? G1->second : -1;
    DEBUG("Connecting " << v0 << " <-> " << v1 << " in groups " << g0 << " and " << g1);
    if (G0 != mPoint2Group.end() && G1 == mPoint2Group.end()) {
        mPoint2Group[v1] = g0;
        mPointGroups[g0].insert(v1);
    } else if (G0 == mPoint2Group.end() && G1 != mPoint2Group.end()) {
        mPoint2Group[v0] = g1;
        mPointGroups[g1].insert(v0);
    } else if (G0 == mPoint2Group.end() && G1 == mPoint2Group.end()) {
        const uint g = mPointGroups.size();
        mPointGroups.resize(mPointGroups.size() + 1);
        mPointGroups.back().insert(v0);
        mPointGroups.back().insert(v1);
        mPoint2Group[v0] = g;
        mPoint2Group[v1] = g;
    } else if (g0 != g1) {
        for (auto &v : mPointGroups[g1]) {
            mPointGroups[g0].insert(v);
            mPoint2Group[v] = g0;
        }
        assert(mPoint2Group[v1] == g0);
        mPointGroups[g1].clear();
    }
}
void ConnectNets::createPinGroupsFromPointGroups()
{
    mPinGroups.reserve(mPointGroups.size());
    mPinGroups.resize(1);
    for (const auto &G : mPointGroups) {
        if (!mPinGroups.back().empty())
            mPinGroups.resize(mPinGroups.size() + 1);
        const uint g = mPinGroups.size() - 1;
        for (const auto &v : G) {
            auto P = mPoint2Pin.find(v);
            if (P == mPoint2Pin.end())
                continue;
            mPinGroups.back().insert(P->second);
            mPin2Group[P->second] = g;
        }
    }
}
void ConnectNets::collectSegmentsAndVias()
{
    mSegmentsZ.resize(mNet.getBoard()->getNumLayers());
    for (const auto *const X : mNet.connections()) {
        for (const auto *T : X->getTracks()) {
            for (const Via &v : T->getVias())
                mVias.push_back(&v);
            for (const auto &s : T->getSegments())
                mSegmentsZ.at(s.z()).push_back(&s);
        }
    }
}
void ConnectNets::collectPinPairs()
{
    // Collect all pairs.
    for (auto P1 : mNet.getPins())
        for (auto P2 : mNet.getPins())
            if (P1 != P2 && P1->getFullName() < P2->getFullName() && mPin2Group[P1] != mPin2Group[P2])
                mPairs.emplace_back(P1, P2);

    // Sort pairs by distance.
    std::sort(mPairs.begin(), mPairs.end(), [](const PinPair &lhs, const PinPair &rhs){
        return lhs.distance2() < rhs.distance2();
    });
}
void ConnectNets::connectPinGroups()
{
    for (const auto &PP : mPairs) {
        auto P1 = PP.P1;
        auto P2 = PP.P2;
        const uint g1 = mPin2Group[P1];
        const uint g2 = mPin2Group[P2];
        if (g1 == g2)
            continue;
        if (!P1->intersects(*P2)) {
            Connection *X = mNet.addConnection(P1, P1->getCenter25(), P2, P2->getCenter25());
            if (!X->isOrderedXMajor())
                X->reverse();
        }
        for (auto P : mPinGroups[g2]) {
            mPinGroups[g1].insert(P);
            mPin2Group[P] = g1;
        }
        mPinGroups[g2].clear();
        if (mPinGroups[g1].size() == mNet.numPins()) // if all pins are in one group, we're done
            break;
    }
}
bool ConnectNets::connect()
{
    initPinPoints();
    for (const auto *const X : mNet.connections())
        connectPointGroups(*X);
    collectSegmentsAndVias();
    for (uint z = 0; z < mNet.getBoard()->getNumLayers(); ++z)
        connectPointGroupsForSegments(z);
    createPinGroupsFromPointGroups();
    collectPinPairs();
    connectPinGroups();
    return true;
}

/// Create connections for all pins of the net by connecting
void Net::autocreateConnections()
{
    DEBUG("Autoconnecting net " << name() << " with " << numPins() << " pins.");
    ConnectNets X(*this);
    X.connect();
}
