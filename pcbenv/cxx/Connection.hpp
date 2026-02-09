#ifndef GYM_PCB_CONNECTION_H
#define GYM_PCB_CONNECTION_H

#include "Py.hpp"
#include "Defs.hpp"
#include "Color.hpp"
#include "Pin.hpp"
#include "Rules.hpp"

class CloneEnv;
class Component;
class Net;
class Track;
class UniformGrid25;
class NavGrid;

class Connection
{
    constexpr static const bool EndpointsOnMaskedLayersOK = true;
public:
    Connection *clone(CloneEnv&) const;
    Connection(Net *, const Point_25 &source, Pin *sourcePin, const Point_25 &target, Pin *targetPin);
    Connection(const Connection&, const Point_25 &source, const Point_25 &target);
    ~Connection();

    void setId(int id) { mId = id; }
    int id() const { return mId; }
    std::string name() const;
    bool hasNet() const { return !!mNet; }
    Net *net() const { return mNet; }

    const Point_25& source() const { return mSource; }
    const Point_25& target() const { return mTarget; }
    Pin *sourcePin() const { return mSourcePin; }
    Pin *targetPin() const { return mTargetPin; }
    Pin *otherPin(const Pin *) const;

    Component *sourceComponent() const { return mSourcePin ? mSourcePin->getParent() : 0; }
    Component *targetComponent() const { return mTargetPin ? mTargetPin->getParent() : 0; }

    Vector_2 vector2() const { return target().xy() - source().xy(); }
    Segment_2 segment2() const { return Segment_2(source().xy(), target().xy()); }

    void detachSourcePin();
    void detachTargetPin();

    void forceEndpointsToGrid(const UniformGrid25&);

    bool hasTracks() const { return !mTracks.empty(); }
    uint numTracks() const { return mTracks.size(); }
    const std::vector<Track *>& getTracks() const { return mTracks; }
    Track& getTrack(uint i) const { return *mTracks.at(i); }
    Track *getTrackAt(const Point_25&) const;
    Track *getTrackEndingNear(const Point_25&, Real tolerance) const;
    Track *newTrack(const Point_25 &start);
    Track *popTrack(uint index);
    void clearTracks();

    bool isRouted() const { return mIsRouted; }
    bool _isRouted() const;
    void setRouted(bool b) { mIsRouted = b; }
    bool checkRouted() { setRouted(_isRouted()); return isRouted(); }

    /**
     * Force the connection's track's endpoints to the connection endpoints.
     */
    void forceRouted();

    bool isLocked() const { return mLocked; }
    void setLocked(bool b) { mLocked = b; }

    /**
     * Replace all tracks with the one provided.
     * The track is either added by reference, moved, or copied.
     */
    void setTrack(Track *);
    void setTrack(Track&&);
    void setTrack(const Track&);

    /**
     * Append the track to this connection.
     * WARNING: The pointer may no longer be valid after this call!
     */
    void appendTrack(Track *T);
    void appendTrack(const Track &T);

    void reverse(); /**< Reverse the endpoints and the tracks. */

    /**
     * Create the most direct track for this connection without considering obstacles.
     * The connection will have up to 2 intermediate points (source --, / start, / end, -- target).
     * @param minSquaredLen Don't create segments shorter than this.
     * @param viaLocation The index of the point where to put the via.
     * @param bendLocation 0 to start with the 45° segment, 1 to start with the 0°/90° one, values in between for 3 segments.
     */
    void makeDirectTrack45(Real minSquaredLen, uint viaLocation, float bendLocation);
    void makeDirectTrack(Real minSquaredLen, uint viaLocation);

    /**
     * Adjust the connection after removing a range of layers.
     * Pins must be adjusted first!
     * If and endpoint's layer is removed and there is no pin, this will return false and the connection will become invalid.
     * Tracks with segments on the removed layers will be deleted entirely.
     */
    bool updateForRemovedLayers(uint removedZMin, uint removedZMax);

    /**
     * Returns a set of pairs of disconnected track endpoints such that each point occurs exactly once.
     * Points are paired in order from lowest to highest pair distance over all possible pairings.
     */
    std::vector<std::pair<Point_25, Point_25>> getRatsNest() const;

    bool arePinsOnNet(const Net *) const; /**< sanity **/

    const DesignRules& getRules() const { return mRules; }
    Real clearance() const { return mRules.Clearance; }
    Real defaultTraceWidth() const { return mRules.TraceWidth; }
    Real defaultViaDiameter() const { return mRules.ViaDiameter; }
    Real defaultViaRadius() const { return mRules.viaRadius(); }

    void setRulesMin(const DesignRules &rules) { setRulesDefault(mRules.max(rules)); }
    void setRulesDefault(const DesignRules &rules) { mRules = rules; }
    void setClearance(Real c) { mRules.Clearance = c; }
    void setDefaultTraceWidth(float w) { mRules.TraceWidth = w; }
    void setDefaultViaDiameter(Real d) { mRules.ViaDiameter = d; }

    bool canRouteOnLayer(uint n) const { return mLayerMask & (1 << n); }
    uint32_t getLayerMask() const { return mLayerMask; }

    void setLayerMask(uint32_t);

    void setParametersFrom(const Connection&);

    float referenceLen() const { return mReferenceLen; }
    void setReferenceLen(float v) { assert(mReferenceLen == 0.0f && v > 0.0f); mReferenceLen = v; }

    Bbox_2 tracksBbox() const;
    Bbox_2 bbox() const;
    Bbox_2 bboxAroundComps() const;

    Real squared_distance() const { return CGAL::squared_distance(mSource.xy(), mTarget.xy()); }
    Real distance() const { return std::sqrt(squared_distance()); }
    Real distance45() const { return geo::distance45(mSource.xy(), mTarget.xy()); }

    bool isOrderedXMajor() const; /**< source.x <= target.x etc. **/

    uint numNecessaryVias() const; /**< 0 if the layers of the endpoints or pins overlap, 1 otherwise **/

    template<class Container> static void sort_name(Container&);

    /**
     * Returns whether this connection's tracks come within the maximum of the default clearances of each other.
     * If a connection has no tracks, the rat's nest line (this->segment2()) is used instead.
     */
    bool intersects(const Connection&) const;

    /**
     * Check whether the connection's tracks are marked as rasterized.
     * \throws std::runtime_error If the tracks' status is inconsistent.
     */
    bool isRasterized_allOrNone() const;

    /**
     * Check whether the endpoints' grid points are marked as having tracks.
     */
    bool checkRasterization(const NavGrid&) const;

    void setColor(Color c) { mColor = c; } /**< use Color::TRANSPARENT to use net's color **/
    Color getColor() const;

    std::string str() const;

    /**
     * A tuple (source, target) where source and target are pin names or, if there is no pin, coordinates (x,y,z).
     */
    PyObject *getEndsPy() const;
    /**
     * A list of tracks as per Track::getPy(asNumpy).
     */
    PyObject *getTracksPy(bool asNumpy) const;
    PyObject *getPy(uint depth, bool asNumpy) const; /**< see pcb.schema.json */
private:
    Point_25 mSource;
    Point_25 mTarget;
    Pin *mSourcePin;
    Pin *mTargetPin;
    Net *const mNet;
    std::vector<Track *> mTracks;
    DesignRules mRules;
    uint32_t mLayerMask{0xffffffff};
    int mId{-1};
    bool mIsRouted{false};
    bool mLocked{false};
    float mReferenceLen{0.0f};
    Color mColor{Color::TRANSPARENT};

private:
    void mergeTrack(Track *);
    void deleteEmptyTracks();
    void updateEndpointForLayerMask(bool target);
};

inline void Connection::setParametersFrom(const Connection &X)
{
    setClearance(X.clearance());
    setDefaultTraceWidth(X.defaultTraceWidth());
    setDefaultViaDiameter(X.defaultViaDiameter());
    setLayerMask(X.getLayerMask());
}
inline Pin *Connection::otherPin(const Pin *P) const
{
    assert(mSourcePin == P || mTargetPin == P);
    return (mSourcePin == P) ? mTargetPin : mSourcePin;
}

inline Track *Connection::popTrack(uint index)
{
    setRouted(false);
    const auto T = mTracks.at(index);
    mTracks.erase(mTracks.begin() + index);
    return T;
}

template<class Container> void Connection::sort_name(Container &A)
{
    std::sort(A.begin(),
              A.end(),
              [](const Connection *const L, const Connection *const R) -> bool { return L->name() < R->name(); });
}

#endif // GYM_PCB_CONNECTION_H
