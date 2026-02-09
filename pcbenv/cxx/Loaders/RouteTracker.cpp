
#include "Log.hpp"
#include "Loaders/RouteTracker.hpp"
#include "Component.hpp"
#include "Net.hpp"
#include "Track.hpp"

RouteTracker::RouteTracker(Net &net, const std::vector<WideSegment_25> &segs, const std::vector<Via> &vias) : mNet(net), mSegs(segs), mVias(vias)
{
    DEBUG("<-> Tracking routes for net " << net.name() << " with " << mSegs.size() << " segments and " << mVias.size() << " vias.");
    for (const auto &seg : mSegs) {
        const auto source = seg.source();
        const auto target = seg.target();
        for (auto pin : net.getPins()) {
            if (!pin->isOnLayer(seg.z()))
                continue;
            const bool s = pin->contains2D(source.xy());
            const bool t = pin->contains2D(target.xy());
            if (s)
                mPt2Pin[source] = pin;
            if (t)
                mPt2Pin[target] = pin;
            if (s || t)
                continue;
            //const auto w2 = seg.halfWidth();
            //const auto ds = pin->distanceTo(source.xy());
            //const auto dt = pin->distanceTo(target.xy());
            //if (dt < w2 || ds < w2)
            if (pin->intersects(seg))
                WARN("FIXME: segment " << seg << " touches pin " << pin->getFullName() << " but is not regarded as connected");
        }
        for (auto &via : mVias) {
            if (via.contains(source))
                mPoint2Vias[source].insert(&via);
            if (via.contains(target))
                mPoint2Vias[target].insert(&via);
        }
    }
}
bool RouteTracker::run()
{
    stitchTracks();
    makeConnections();
    return true;
}
/// See if we have a via connecting the point @v with the layer @z.
bool RouteTracker::canAddVia(const Point_25 &v, int z) const
{
    if (v.z() == z)
        return true;
    bool found = false;

    const auto V = mPoint2Vias.find(v);
    if (V != mPoint2Vias.end()) {
        for (const auto via : V->second) {
            assert(via->onLayer(v.z()));
            if (via->onLayer(v.z()) && via->onLayer(z))
                found = true;
            if (found)
                break;
        }
    }
    if (!found) {
        // Pins serve as "vias" as well.
        const auto P = mPt2Pin.find(v);
        if (P != mPt2Pin.end())
            found = P->second->isOnLayer(z);
    }
    return found;
}
/// See if we can add the segment @s to any of the tracks in @T other than @except (where it has already been added).
uint RouteTracker::tryAppend(std::set<uint> &T, const WideSegment_25 &s, uint except)
{
    for (auto t : T)
        if (t != except && tryAppend(t, s))
            return t;
    return std::numeric_limits<uint>::max();
}
/// See if we can append the segment @s to the track with index @t at either end.
bool RouteTracker::tryAppend(uint t, const WideSegment_25 &s)
{
    std::list<WideSegment_25> &T = mTracks.at(t);
    assert(!T.empty());
    const auto v0 = s.source();
    const auto v1 = s.target();
    const auto vF = T.front().source_2();
    const auto vB = T.back().target_2();
    const int JF = s.joins2D(vF); // returns -1/1 for an exact match and -2/2 for an overlap
    const int JB = s.joins2D(vB);
    if (JF == (JF < 0 ? -1 : 1) && canAddVia(JF < 0 ? s.source() : s.target(), T.front().z())) // JF<0: (B-A)+(B-C)-... / JF>0: (A-B)+(B-C)-...
        T.push_front(JF < 0 ? s.opposite() : s);
    else
    if (JB == (JB < 0 ? -1 : 1) && canAddVia(JB < 0 ? s.source() : s.target(), T.back().z())) // JB<0: ...-(C-B)+(B-A) / JB>0: ...-(C-B)+(B-A)
        T.push_back(JB < 0 ? s : s.opposite());
    else
    if (JF && canAddVia(JF < 0 ? s.source() : s.target(), T.front().z())) // JF<0: (B-A)+(B-C)-... / JF>0: (A-B)+(B-C)-...
        T.push_front(JF < 0 ? s.opposite() : s);
    else
    if (JB && canAddVia(JB < 0 ? s.source() : s.target(), T.back().z())) // JB<0: ...-(C-B)+(B-A) / JB>0: ...-(C-B)+(B-A)
        T.push_back(JB < 0 ? s : s.opposite());
    else
        return false;
    mPoint2Tracks[v0.xy()].insert(t);
    mPoint2Tracks[v1.xy()].insert(t);
    return true;
}
bool RouteTracker::tryConnectTracks(uint t, uint u)
{
    auto &T1 = mTracks[t];
    auto &T2 = mTracks[u];

    // First and last segments of the tracks.
    const auto s1A = T1.front();
    const auto s2A = T2.front();
    const auto s1B = T1.back();
    const auto s2B = T2.back();

    // Endpoints of the tracks.
    const auto v1A = s1A.source_2();
    const auto v2A = s2A.source_2();
    const auto v1B = s1B.target_2();
    const auto v2B = s2B.target_2();

    // Find the closest pair of endpoints.
    const auto dAA = CGAL::squared_distance(v1A, v2A); // f2f = 0
    const auto dAB = CGAL::squared_distance(v1A, v2B); // f2b = 1
    const auto dBA = CGAL::squared_distance(v1B, v2A); // b2f = 2
    const auto dBB = CGAL::squared_distance(v1B, v2B); // b2b = 3
    const Real d[4] = { dAA, dAB, dBA, dBB };
    uint e = 0;
    for (uint i = 1; i < 4; ++i)
        if (d[i] < d[e])
            e = i;

    // Check that the segments of the closest endpoint pair intersect.
    switch (e) {
    case 0: if (!s1A.intersects(s2A)) return false; break;
    case 1: if (!s1A.intersects(s2B)) return false; break;
    case 2: if (!s1B.intersects(s2A)) return false; break;
    case 3: if (!s1B.intersects(s2B)) return false; break;
    }

    // Reverse segments if necessary.
    if (e == 0 || e == 3)
        for (auto &s : T2)
            s._setBase(s.base().opposite());

    // Insert T2 into T1.
    if (e == 2)
        T1.splice(T1.end(), T2, T2.begin(), T2.end());
    else if (e == 3)
        T1.insert(T1.end(), T2.rbegin(), T2.rend());
    else if (e == 1)
        T1.splice(T1.begin(), T2, T2.begin(), T2.end());
    else if (e == 0)
        T1.insert(T1.begin(), T2.rbegin(), T2.rend());
    else
        return false;
    T2.clear();
    return true;
}
void RouteTracker::mergeTracks(uint t, uint u)
{
    assert(t != u);
    const auto td = std::min(t, u);
    const auto ts = std::max(t, u);
    auto &Td = mTracks[td];
    auto &Ts = mTracks[ts];

    const int b2f = Td.back().base().equals(Ts.front().base()); // ...-A-B + A-B-...
    const int b2b = Td.back().base().equals(Ts.back().base()); // ...-A-B + ...-B-A
    const int f2b = Td.front().base().equals(Ts.back().base()); // A-B-... + ...-A-B
    const int f2f = Td.front().base().equals(Ts.front().base()); // A-B-... + B-A-...

    // In the T case we cannot join:
    // A-B-C-D     A-B-C-D
    // A-B      ->   |
    //   |           E
    //   E
    const bool T_join_f = b2f < 0 || f2f > 0;
    const bool T_join_b = b2b > 0 || f2b < 0;
    if (T_join_f)
        eraseFront(ts);
    if (T_join_b)
        eraseBack(ts);
    if (T_join_f || T_join_b)
        return;

    for (const auto &s : Ts) {
        auto &s2ts = mPoint2Tracks[s.source_2()];
        auto &t2ts = mPoint2Tracks[s.target_2()];
        s2ts.erase(ts);
        t2ts.erase(ts);
        s2ts.insert(td);
        t2ts.insert(td);
    }
    if (b2b < 0 || f2f < 0)
        for (auto &s : Ts)
            s._setBase(s.base().opposite());

    if (b2f > 0)
        Td.splice(Td.end(), Ts, ++Ts.begin(), Ts.end());
    else if (b2b < 0)
        Td.insert(Td.end(), ++Ts.rbegin(), Ts.rend());
    else if (f2b > 0)
        Td.splice(Td.begin(), Ts, Ts.begin(), --Ts.end());
    else if (f2f < 0)
        Td.insert(Td.begin(), Ts.rbegin(), --Ts.rend());
    else
        throw std::runtime_error("Tried to merge tracks that do not fit!");

    if (ts == mTracks.size() - 1)
        mTracks.pop_back();
    else
        mTracks[ts].clear();
}
void RouteTracker::erasedFromTrack(const WideSegment_25 &r, uint t)
{
    bool haveSource = false;
    bool haveTarget = false;
    for (const auto &s : mTracks[t]) {
        haveSource = haveSource || (s.source_2() == r.source_2()) || (s.target_2() == r.source_2());
        haveTarget = haveTarget || (s.source_2() == r.target_2()) || (s.target_2() == r.target_2());
    }
    if (!haveSource)
        mPoint2Tracks[r.source_2()].erase(t);
    if (!haveTarget)
        mPoint2Tracks[r.target_2()].erase(t);
}
void RouteTracker::eraseFront(uint t)
{
    auto &T = mTracks[t];
    const auto r = T.front();
    T.pop_front();
    erasedFromTrack(r, t);
}
void RouteTracker::eraseBack(uint t)
{
    auto &T = mTracks[t];
    const auto r = T.back();
    T.pop_back();
    erasedFromTrack(r, t);
}

void RouteTracker::stitchTracks()
{
    // Stitch segments together if their endpoints match.
    for (const WideSegment_25 &s : mSegs) {
        const auto v0 = s.source();
        const auto v1 = s.target();
        const uint t0 = tryAppend(mPoint2Tracks[v0.xy()], s);
        const uint t1 = tryAppend(mPoint2Tracks[v1.xy()], s, t0);
        const bool a0 = t0 < mTracks.size();
        const bool a1 = t1 < mTracks.size();
        if (a0 != a1)
            continue;
        if (a0 && a1) {
            mergeTracks(t0, t1);
        } else {
            const uint t = mTracks.size();
            mTracks.resize(t + 1);
            mTracks.back().push_back(s);
            mPoint2Tracks[v0.xy()].insert(t);
            mPoint2Tracks[v1.xy()].insert(t);
            DEBUG("Created track " << t << " with " << v0 << " - " << v1);
        }
    }
    // Stitch tracks together if their segments overlap.
    for (uint numStitched = 1; numStitched;) {
        numStitched = 0;
        for (uint t1 = 0; t1 < mTracks.size(); ++t1) {
            if (mTracks[t1].empty())
                continue;
            for (uint t2 = t1 + 1; t2 < mTracks.size(); ++t2) {
                if (!mTracks[t2].empty())
                    numStitched += int(tryConnectTracks(t1, t2));
            }
        }
        if (numStitched)
            INFO("Stitched " << numStitched << " tracks");
    }
}
void RouteTracker::addConnection(Pin *srcPin, std::vector<WideSegment_25> &segs, std::vector<Via> &vias, Pin *dstPin)
{
    DEBUG("Adding connection " << segs.front().source() << " '" << (srcPin ? srcPin->getFullName() : "") << "' <-> " << segs.back().target() << " '" << (dstPin ? dstPin->getFullName() : "") << "' with " << segs.size() << " segments and " << vias.size() << " vias.");
    const bool rev = !dstPin && srcPin && !srcPin->contains3D(segs.front().source());
    if (rev)
        Track::_reverseSegments(segs);
    if (segs.front().source() == segs.back().target()) {
        WARN("FIXME: circular tracks should not be added as connections");
        segs.back().nudge(0, -1); // FIXME: I don't want to rewrite nets to have tracks outside of connections now.
    }
    Connection *X = mNet.addConnection(srcPin, segs.front().source(), dstPin, segs.back().target());
    if (!X)
        std::runtime_error("Could not create connection.");
    Track *T = X->newTrack(segs.front().source());
    T->setSegments(std::move(segs));
    T->setVias(std::move(vias));
    assert(segs.empty());
    assert(vias.empty());
    X->setRouted(true);
}
/// Walk the tracks, collecting segments and vias.
/// Create a new connection whenever we have passed through a new pair of pins.
void RouteTracker::makeConnections()
{
    std::set<const Via *> viasLeft;
    for (const auto &v : mVias)
        viasLeft.insert(&v);
    for (const auto &T : mTracks) {
        DEBUG("Following track with " << T.size() << " points.");
        if (T.empty())
            continue;
        Pin *srcPin = getPin(T.front().source());
        Pin *dstPin = 0;
        std::vector<WideSegment_25> track;
        std::vector<Via> vias;
        auto z = T.front().z();
        for (const auto &s : T) {
            Pin *P = getPin(s.target());
            DEBUG("Segment " << s << " srcPin=" << (srcPin ? srcPin->getFullName() : "") << " dstPin=" << (dstPin ? dstPin->getFullName() : "") << " P=" << (P ? P->getFullName() : ""));
            if (dstPin && dstPin != P) {
                addConnection(srcPin, track, vias, dstPin);
                srcPin = getPin(s.source());
                dstPin = P;
            } else if (P != srcPin) {
                dstPin = P;
            }
            if (s.z() != z) {
                const auto &V = mPoint2Vias[s.source()];
                if (!V.empty()) {
                    const auto v = *V.begin();
                    viasLeft.erase(v);
                    vias.push_back(*v);
                    if (V.size() > 1)
                        WARN("More than one via for point " << s.source() << " found!");
                }
                z = s.z();
            }
            track.emplace_back(s);
        }
        if (!track.empty())
            addConnection(srcPin, track, vias, dstPin);
    }
    for (auto v : viasLeft) {
        bool added = false;
        // Attach remaining vias to the endpoints of connections.
        for (auto X : mNet.connections()) {
            if (v->contains(X->target())) {
                try {
                    X->getTrack(0).append(*v);
                } catch (const std::runtime_error &e) {
                    if (std::strncmp(e.what(), "the other end", 13))
                        throw;
                    // The track has no clear endpoint but we don't care.
                }
            } else if (v->contains(X->source())) {
                X->getTrack(0).prepend(*v);
            } else
                continue;
            added = true;
            break;
        }
        if (!added) {
            auto X = mNet.addConnection(0, Point_25(v->location(), v->zmin()), 0, Point_25(v->location(), v->zmax()));
            X->newTrack(X->source())->append(*v);
            X->setRouted(true);
            INFO("Could not append via " << v->str() << " to any tracks.");
        }
    }
}
