
#include "Log.hpp"
#include "Connection.hpp"
#include "Net.hpp"
#include "Component.hpp"
#include "Pin.hpp"
#include "Track.hpp"
#include "Clone.hpp"
#include "UniformGrid25.hpp"

// Connections are removed from a pin's connection list when a pin is removed from the net, or when the net is deleted.

// Temporary connections are never added to a pin's connection list.

Connection::Connection(Net *net,
                       const Point_25 &source, Pin *sourcePin,
                       const Point_25 &target, Pin *targetPin)
    : mNet(net),
      mSourcePin(sourcePin),
      mTargetPin(targetPin)
{
    if (source == target || (mSourcePin && mSourcePin == mTargetPin))
        WARN("connection endpoints are the same");
    mSource = source;
    mTarget = target;
    if (mSourcePin && !mSourcePin->contains3D(mSource))
        throw std::runtime_error("connection source point outside of source pin");
    if (mTargetPin && !mTargetPin->contains3D(mTarget))
        throw std::runtime_error("connection target point outside of target pin");
}

Connection::Connection(const Connection &X, const Point_25 &source, const Point_25 &target)
    : mNet(X.net()),
      mSourcePin(0),
      mTargetPin(0),
      mSource(source),
      mTarget(target)
{
    if (source == target)
        throw std::runtime_error("connection endpoints are the same");
    if (X.sourcePin()) {
        if (X.sourcePin()->contains3D(source))
            mSourcePin = X.sourcePin();
        else if (X.sourcePin()->contains3D(target))
            mTargetPin = X.sourcePin();
    }
    if (X.targetPin()) {
        if (!mTargetPin && X.targetPin()->contains3D(target))
            mTargetPin = X.targetPin();
        else if (!mSourcePin && X.targetPin()->contains3D(source))
            mSourcePin = X.targetPin();
    }
    setParametersFrom(X);
}

Connection::~Connection()
{
    clearTracks();
    if (mNet) {
        detachSourcePin();
        detachTargetPin();
    }
}

bool Connection::isRasterized_allOrNone() const
{
    if (!hasTracks())
        return false;
    const auto r = getTrack(0).isRasterized();
    for (uint i = 1; i < numTracks(); ++i)
        if (getTrack(i).isRasterized() != r)
            throw std::runtime_error("expected all or none of the tracks to be rasterized");
    return r;
}

void Connection::clearTracks()
{
    setRouted(false);
    for (auto T : mTracks)
        delete T;
    mTracks.clear();
}

Bbox_2 Connection::tracksBbox() const
{
    Bbox_2 bbox;
    for (const auto T : getTracks())
        bbox += T->bbox(0.0, -1);
    return bbox;
}

void Connection::detachSourcePin()
{
    // NOTE: mSourcePin must be set 0 before removeConnection, otherwise the connection is removed from the net along with the pin
    auto P = mSourcePin;
    if (mSourcePin) {
        mSourcePin = 0;
        P->removeConnection(this, false);
    }
}
void Connection::detachTargetPin()
{
    // NOTE: mTargetPin must be set 0 before removeConnection, otherwise the connection is removed from the net along with the pin
    auto P = mTargetPin;
    if (mTargetPin) {
        mTargetPin = 0;
        P->removeConnection(this, false);
    }
}

void Connection::forceEndpointsToGrid(const UniformGrid25 &G)
{
    if (hasTracks())
        throw std::runtime_error("cannot change endpoints when tracks are present");
    mSource = G.snapToMidPoint(mSource);
    mTarget = G.snapToMidPoint(mTarget);
}

Track *Connection::newTrack(const Point_25 &start)
{
    if (getTrackAt(start))
        throw std::runtime_error("tried to create a new track when one can be extended");
    mTracks.push_back(new Track(start));
    auto T = mTracks.back();
    T->setDefaultWidth(defaultTraceWidth());
    T->setDefaultViaDiameter(defaultViaDiameter());
    return T;
}

Track *Connection::getTrackAt(const Point_25 &v) const
{
    for (auto T : mTracks)
        if (T->start() == v || T->end() == v)
            return T;
    return 0;
}

Track *Connection::getTrackEndingNear(const Point_25 &v, Real tolerance) const
{
    Real d_min = std::numeric_limits<Real>::infinity();
    Track *R = 0;
    for (auto T : mTracks) {
        if (T->end().z() != v.z())
            continue;
        const Real d = std::max(std::abs(v.x() - T->end().x()),
                                std::abs(v.y() - T->end().y()));
        if (d < d_min) {
            d_min = d;
            R = T;
        }
    }
    return (d_min <= tolerance) ? R : 0;
}

bool Connection::arePinsOnNet(const Net *net) const
{
    if (mNet != net)
        throw std::runtime_error("connection is not on the net queried");
    if (sourcePin() && sourcePin()->net() != mNet)
        return false;
    if (targetPin() && targetPin()->net() != mNet)
        return false;
    return true;
}

bool Connection::isOrderedXMajor() const
{
    return
        (mSource.x() <  mTarget.x()) ||
        (mSource.x() == mTarget.x() && mSource.y() <  mTarget.y()) ||
        (mSource.x() == mTarget.x() && mSource.y() == mTarget.y() && mSource.z() < mTarget.z());
}

void Connection::reverse()
{
    std::swap(mSource, mTarget);
    std::swap(mSourcePin, mTargetPin);
    for (auto T : mTracks) {
        if (T->start() == mTarget)
            T->reverse();
        else if (T->end() == mSource)
            T->reverse();
    }
}

bool Connection::_isRouted() const
{
    if (mTracks.size() != 1)
        return false;
    const auto &A = mTracks[0]->start();
    const auto &B = mTracks[0]->end();
    if (B.xy() != mTarget.xy() ||
        A.xy() != mSource.xy())
        return false;
    return
        (A.z() == mSource.z() || (mSourcePin && mSourcePin->isOnLayer(A.z()))) &&
        (B.z() == mTarget.z() || (mTargetPin && mTargetPin->isOnLayer(B.z())));
}

Color Connection::getColor() const
{
    return (mColor == Color::TRANSPARENT) ? (mNet ? mNet->getColor() : Color::BLACK) : mColor;
}

Bbox_2 Connection::bbox() const
{
    return Bbox_2(std::min(mSource.x(), mTarget.x()),
                  std::min(mSource.y(), mTarget.y()),
                  std::max(mSource.x(), mTarget.x()),
                  std::max(mSource.y(), mTarget.y()));
}
Bbox_2 Connection::bboxAroundComps() const
{
    auto box = bbox();
    if (sourceComponent())
        box += sourceComponent()->getBbox();
    if (targetComponent())
        box += targetComponent()->getBbox();
    return box;
}

void Connection::setLayerMask(const uint32_t mask)
{
    mLayerMask = mask;
    updateEndpointForLayerMask(0);
    updateEndpointForLayerMask(1);
}
void Connection::updateEndpointForLayerMask(bool target)
{
    Point_25 &v = target ? mTarget : mSource;
    if (canRouteOnLayer(v.z()))
        return;
    const Pin *P = target ? mTargetPin : mSourcePin;
    if (P) {
        const uint z0 = v.z();
        const uint z1 = P->getStartLayerFor(*this);
        if (z0 != z1) {
            DEBUG("layer mask: moved connection point for " << P->getFullName() << " from layer " << z0 << " to layer " << z1);
            v = Point_25(v.xy(), z1);
            if (canRouteOnLayer(v.z()))
                return;
        }
    }
    const auto err = fmt::format("layer mask {} for {}/{} excludes {} point at z={}", Logger::formatMask(mLayerMask), mNet->name(), name(), target ? "target" : "source", v.z());
    if (hasTracks() || EndpointsOnMaskedLayersOK)
        DEBUG(err);
    else
        throw std::runtime_error(err);
}

std::string Connection::name() const
{
    std::stringstream ss;
    ss << '(';
    if (mSourcePin)
        ss << mSourcePin->getFullName();
    else
        ss << mSource;
    ss << ',';
    if (mTargetPin)
        ss << mTargetPin->getFullName();
    else
        ss << mTarget;
    ss << ')';
    return ss.str();
}
std::string Connection::str() const
{
    std::stringstream ss;
    ss << '{' << name();
    if (mNet)
        ss << " #: " << mNet->name();
    ss << '}';
    return ss.str();
}

PyObject *Connection::getEndsPy() const
{
    auto py = PyTuple_New(2);
    if (!py)
        return 0;
    PyTuple_SetItem(py, 0, mSourcePin ? py::String(mSourcePin->getFullName()) : *py::Object(mSource));
    PyTuple_SetItem(py, 1, mTargetPin ? py::String(mTargetPin->getFullName()) : *py::Object(mTarget));
    return py;
}
PyObject *Connection::getTracksPy(bool asNumpy) const
{
    auto py = PyList_New(mTracks.size());
    for (uint i = 0; i < mTracks.size(); ++i)
        PyList_SetItem(py, i, mTracks[i]->getPy(asNumpy));
    return py;
}
PyObject *Connection::getPy(uint depth, bool asNumpy) const
{
    if (depth == 0)
        return getEndsPy();
    auto py = py::Object(PyDict_New());
    if (!*py)
        return 0;
    py.setItem("net", mNet ? py::String(mNet->name()) : py::None());
    py.setItem("src", mSource);
    py.setItem("dst", mTarget);
    py.setItem("src_pin", mSourcePin ? py::String(mSourcePin->getFullName()) : py::None());
    py.setItem("dst_pin", mTargetPin ? py::String(mTargetPin->getFullName()) : py::None());
    if (depth > 1)
        py.setItem("tracks", getTracksPy(asNumpy));
    py.setItem("clearance", mRules.Clearance);
    return *py;
}

void Connection::makeDirectTrack(Real minSquaredLen, uint viaLocation)
{
    if (source().z() == target().z())
        viaLocation = 2;
    if (hasTracks())
        throw std::runtime_error("requested a direct track for connection but a track already exists");
    Segment_25 s(source().xy(), target().xy(), viaLocation ? source().z() : target().z());
    auto T = newTrack(source());
    if (viaLocation == 0)
        T->appendVia(source().xy(), source().z(), target().z(), defaultViaRadius());
    if (s.squared_length() >= minSquaredLen)
        T->append(WideSegment_25(s, 0.5 * defaultTraceWidth()));
    if (viaLocation == 1)
        T->appendVia(target().xy(), source().z(), target().z(), defaultViaRadius());
}
void Connection::makeDirectTrack45(Real minSquaredLen, uint viaLocation, float bendLocation)
{
    if (bendLocation < 0.0f || bendLocation > 1.0f)
        throw std::runtime_error("must have 0 <= bendLocation <= 1");
    if (viaLocation > 3)
        throw std::runtime_error("viaLocation must be <= 3");
    if (hasTracks())
        throw std::runtime_error("requested a direct track for connection but a track already exists");

    const auto dx = vector2().x();
    const auto dy = vector2().y();
    const auto lx = std::abs(dx);
    const auto ly = std::abs(dy);
    const auto mx = dx < 0.0 ? -1.0 : 1.0;
    const auto my = dy < 0.0 ? -1.0 : 1.0;
    const auto ds = Vector_2((lx > ly) ? (lx - ly) * mx : 0.0,
                             (lx > ly) ? 0.0 : (ly - lx) * my);
    const auto dd = Vector_2(std::min(lx, ly) * mx,
                             std::min(lx, ly) * my);
    Point_2 v = source().xy();
    int z = source().z();
    Segment_25 s[3];
    // 1. Move straight up to the bend (caller must choose a sensible bendLocation).
    v += ds * bendLocation;
    if (viaLocation == 0)
        z = target().z();
    s[0] = Segment_25(source().xy(), v, z);
    // 2. Move diagonally.
    v += dd;
    if (viaLocation == 1)
        z = target().z();
    s[1] = Segment_25(s[0].target_2(), v, z);
    // 3. Move straight to the target.
    if (viaLocation == 2)
        z = target().z();
    s[2] = Segment_25(s[1].target_2(), target().xy(), z);

    // Stop when we encounter a tiny segment.
    auto T = newTrack(source());
    for (uint i = 0; i < 3 && (s[i].squared_length() >= minSquaredLen); ++i)
        if (s[i].squared_length() > 0.0)
            T->append(WideSegment_25(s[i], 0.5 * defaultTraceWidth()));
    // Caller must be sure there are no pins at source and target.
    T->autocreateVias(target());
}

void Connection::deleteEmptyTracks()
{
    uint i, n;
    for (i = 0, n = 0; i < mTracks.size(); ++i) {
        if (!mTracks[i]->empty())
            mTracks[n++] = mTracks[i];
        else
            delete mTracks[i];
    }
    mTracks.resize(n);
}

void Connection::mergeTrack(Track *T1)
{
    if (!T1)
        return;
    DEBUG("mergeTrack " << T1->str() << " with " << numTracks() << " tracks");

    if (T1->end() == source())
        T1->reverse();
    bool concatenatedTracks;
    do {
        concatenatedTracks = false;
        assert(!T1->empty());
        for (auto T2 : mTracks) {
            if (T2 == T1 || T2->empty())
                continue;
            auto A = T2->canAttach(*T1);
            if (A == ContactType::NONE)
                continue;
            if (A == ContactType::END_TO_START) {
                T2->appendMove(T1);
                T1 = T2;
            } else if (A == ContactType::START_TO_END) {
                T1->appendMove(T2);
            } else if (A == ContactType::END_TO_END) {
                if (T1->start() != source())
                    std::swap(T1, T2);
                T2->reverse();
                T1->appendMove(T2);
            } else {
                throw std::runtime_error("did not expect tracks to have same starting point");
            }
            assert(T1->hasValidEnds());
            concatenatedTracks = true;
        }
    } while (concatenatedTracks);
    deleteEmptyTracks();
    checkRouted();

    DEBUG("mergeTrack: now have " << numTracks() << " and isRouted=" << isRouted());
}

void Connection::setTrack(Track *T)
{
    clearTracks();
    mTracks.push_back(T);
    checkRouted();
}
void Connection::setTrack(Track &&T)
{
    clearTracks();
    mTracks.push_back(new Track(std::move(T)));
    checkRouted();
}
void Connection::setTrack(const Track &T)
{
    clearTracks();
    mTracks.push_back(new Track(T));
    checkRouted();
}

// Use cases:
// * We adjust the track endpoint to the connection endpoint.
// * If the track ended on a different layer because of thru-hole pins, we use the endpoint on that layer.
// * If the track ends with a via: only check that we don't actually need to move it.
void Connection::forceRouted()
{
    if (numTracks() != 1)
        throw std::runtime_error("forceRouted() requires a single track");
    auto &T = getTrack(0);
    auto zs = source().z();
    auto zt = target().z();
    if (T.start().z() != zs && mSourcePin && mSourcePin->isOnLayer(T.start().z()))
        zs = T.start().z();
    if (T.end().z() != zt && mTargetPin && mTargetPin->isOnLayer(T.end().z()))
        zt = T.end().z();
    if (CGAL::squared_distance(T.end().xy(), target().xy()) > 1.0)
        throw std::runtime_error("track end is too far from connection target");
    if (T.start() != source())
        T.moveStartTo(source().withZ(zs));
    if (T.end() != target())
        T.moveEndTo(target().withZ(zt));
    setRouted(true);
}

void Connection::appendTrack(Track *T)
{
    if (isRouted())
        throw std::runtime_error("cannot append track to routed connection");
    mTracks.push_back(T);
    mergeTrack(T);
}
void Connection::appendTrack(const Track &T)
{
    if (isRouted())
        throw std::runtime_error("cannot append track to routed connection");
    mTracks.push_back(new Track(T));
    mergeTrack(mTracks.back());
}

uint Connection::numNecessaryVias() const
{
    if (source().z() == target().z())
        return 0;
    if (sourcePin() && targetPin())
        return sourcePin()->sharesLayer(*targetPin()) ? 0 : 1;
    if (sourcePin())
        return sourcePin()->isOnLayer(target().z()) ? 0 : 1;
    if (targetPin())
        return targetPin()->isOnLayer(source().z()) ? 0 : 1;
    return 1;
}

bool Connection::updateForRemovedLayers(uint zmin, uint zmax)
{
    const uint dn = (zmax - zmin) + 1;
    const uint sz = mSource.z();
    const uint tz = mTarget.z();
    if (zmin <= sz && sz <= zmax) {
        if (!mSourcePin)
            return false;
        mSource = Point_25(mSource.xy(), mSourcePin->getParent()->getSingleLayer());
    } else if (sz > zmax) {
        mSource = Point_25(mSource.xy(), sz - dn);
    }
    if (zmin <= tz && tz <= zmax) {
        if (!mTargetPin)
            return false;
        mTarget = Point_25(mTarget.xy(), mTargetPin->getParent()->getSingleLayer());
    } else if (tz > zmax) {
        mTarget = Point_25(mTarget.xy(), tz - dn);
    }

    for (uint i = 0; i < mTracks.size(); ++i) {
        if (mTracks[i]->updateForRemovedLayers(zmin, zmax))
            continue;
        WARN("Track cleared due to removing some of its layers");
        delete popTrack(i--);
    }

    return true;
}

std::vector<std::pair<Point_25, Point_25>> Connection::getRatsNest() const
{
    struct Rat {
        Point_25 A;
        Point_25 B;
        Real d2;
        Rat(const Point_25 &P, const Point_25 &Q) : A(P), B(Q), d2(CGAL::squared_distance(P.xy(), Q.xy()))
        {
            assert(d2 > 0.0 || A.z() != B.z());
            if (A.xy() == B.xy())
                WARN("rat's nest found only vertical gap");
        }
        bool operator<(const Rat &that) const { return this->d2 < that.d2; }
    };
    std::vector<std::pair<Point_25, Point_25>> ratsNest;
    if (!hasTracks()) {
        ratsNest.push_back(std::make_pair(source(), target()));
        return ratsNest;
    }
    std::list<Rat> conns;
    bool looseSource = true;
    bool looseTarget = true;
    for (const auto *T1 : mTracks) {
        if (T1->start().xy() == source().xy() || T1->end().xy() == source().xy()) // even if the 2nd is not actually permitted
            looseSource = false;
        if (T1->start().xy() == target().xy() || T1->end().xy() == target().xy()) // both are permitted
            looseTarget = false;
        for (const auto *T2 : mTracks) {
            if (T1 != T2) {
                conns.emplace_back(T1->start(), T2->start());
                conns.emplace_back(T1->start(), T2->end());
                conns.emplace_back(T2->start(), T1->end());
                conns.emplace_back(T1->end(), T2->end());
            }
        }
    }
    if (looseSource || looseTarget) {
        for (const auto *T : mTracks) {
            if (looseSource) {
                conns.emplace_back(source(), T->start());
                conns.emplace_back(source(), T->end());
            }
            if (looseTarget) {
                conns.emplace_back(target(), T->start());
                conns.emplace_back(target(), T->end());
            }
        }
    }
    conns.sort();
    std::set<Point_25> connected;
    for (auto &rat : conns)
        if (connected.insert(rat.A).second && connected.insert(rat.B).second)
            if (rat.d2 > 0.0)
                ratsNest.push_back(std::make_pair(rat.A, rat.B));
    assert(!ratsNest.empty());
    return ratsNest;
}

bool Connection::intersects(const Connection &X) const
{
    const auto c = std::max(clearance(), X.clearance());
    if (!hasTracks()) {
        const auto sY = segment2();
        if (!X.hasTracks())
            return CGAL::do_intersect(sY, X.segment2());
        for (auto TX : X.getTracks())
        for (const auto &sX : TX->getSegments())
            if (sX.squared_distance(sY) < (c * c))
                return true;
        return false;
    }
    for (const auto TY : getTracks()) {
    for (const auto &sY : TY->getSegments()) {
        if (!X.hasTracks()) {
            if (sY.squared_distance(X.segment2()) < (c * c))
                return true;
        } else {
            for (const auto TX : X.getTracks())
            for (const auto &sX : TX->getSegments())
                if (sY.violatesClearance2D(sX, c))
                    return true;
        }
    }}
    return false;
}

Connection *Connection::clone(CloneEnv &env) const
{
    auto X = new Connection(env.N[mNet], mSource, mSourcePin, mTarget, mTargetPin);
    env.X[this] = X;
    for (auto T : mTracks)
        X->mTracks.push_back(new Track(*T));
    X->setParametersFrom(*this);
    X->mId = mId;
    X->mIsRouted = mIsRouted;
    X->mColor = mColor;
    return X;
}


bool Connection::checkRasterization(const NavGrid &nav) const
{
    for (auto T : mTracks)
        if (!T->checkRasterization(nav))
            return false;
    return true;
}
