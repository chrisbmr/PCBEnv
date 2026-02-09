
#ifndef GYM_PCB_LOADERS_FACTORY_H
#define GYM_PCB_LOADERS_FACTORY_H

#include "Py.hpp"
#include "Units.hpp"
#include "Math/Misc.hpp"
#include "Loaders/Topology.hpp"
#include <cstdint>
#include <map>
#include <set>
#include <string>

class PCBoard;

class NetOverrides
{
public:
    void load(PyObject *);
    Micrometers traceWidth(const std::string &net, Micrometers) const;
    Micrometers viaDiameter(const std::string &net, Micrometers) const;
    Micrometers clearance(const std::string &net, Micrometers) const;
    std::string str() const;
private:
    std::map<Micrometers, Micrometers> mTraceWidths;
    std::map<Micrometers, Micrometers> mViaDiameters;
    std::map<Micrometers, Micrometers> mClearances;
    std::map<std::string, Micrometers> mNetTraceWidths;
    std::map<std::string, Micrometers> mNetViaDiameters;
    std::map<std::string, Micrometers> mNetClearances;
private:
    void set(std::map<std::string, Micrometers>&,
             std::map<Micrometers, Micrometers>&, PyObject *);
};

class PCBFactory
{
public:
    PCBFactory();
    const std::string& getData() const { return mData; }
    PCBoard *create(const std::string &data);
    PCBoard *loadAndCreate(const std::string &filePath);
    void setUnitLength_nm(Nanometers nm) { mUnitLength_nm = nm; }
    Nanometers getUnitLength_nm(Nanometers Default) const { return (mUnitLength_nm.value() > 0.0) ? mUnitLength_nm : Default; }
    void setForceDesignRes(bool b) { mForceDesignRes = b; }
    bool designResForced() const { return mForceDesignRes; }
    void setFixedTrackParams(bool b) { mFixedTrackParams = b; }
    Real hasFixedTrackParams() const { return mFixedTrackParams; }
    void setNetOverrides(PyObject *over) { mNetOverrides.load(over); }
    const NetOverrides& getNetOverrides() const { return mNetOverrides; }
    void setTopologies(PyObject *);
    const NetTopology *getTopology(const std::string &net) const;
    void setAutocreateConnections(bool b) { mAutocreateConnections = b; }
    bool autocreateConnections() const { return mAutocreateConnections; }
    void setLockRoutedConnections(bool b) { mLockRoutedConnections = b; }
    void setLoadFanout(bool b) { mLoadFanout = b; }
    bool getLoadFanout() const { return mLoadFanout; }
    void setLoadTracks(PyObject *);
    bool getLoadTracksForNet(const std::string &name) const { return mLoadTracksAll || mLoadTracksNets.contains(name); }
    void setRectifySegmentsDegrees(Real deg) { mRectifySegmentsRadians = math::Radians(deg); }
    Real getRectifySegmentsRadians() const { return mRectifySegmentsRadians; }
    void setLoadPolygonsAsBoxes(bool b) { mLoadPolygonsAsBoxes = b; }
    bool loadPolygonsAsBoxes() const { return mLoadPolygonsAsBoxes; }
    void setLayoutAreaMinMargin(float v) { mLayoutAreaMinMargin = v; } // relative if < 0, absolute if >= 0
    void setLayoutAreaMaxMargin(float v) { mLayoutAreaMaxMargin = v; } // relative if < 0, absolute if >= 0
    float getLayoutAreaMinMargin() const { return mLayoutAreaMinMargin; }
    float getLayoutAreaMaxMargin() const { return mLayoutAreaMaxMargin; }
    void setIgnoreLayoutBounds(bool b) { mIgnoreLayoutBounds = b; }
    bool ignoreLayoutBounds() const { return mIgnoreLayoutBounds; }
    bool checkPinIntersections() const { return mCheckPinIntersections; }
private:
    void loadOverrides();
    void lockRoutedConnections(PCBoard&);
    Nanometers mUnitLength_nm{0.0};
    Real mRectifySegmentsRadians{math::Radians(0.125)};
    bool mForceDesignRes{false};
    bool mFixedTrackParams{false};
    bool mLoadTracksAll{false};
    bool mLoadFanout{false};
    bool mLoadPolygonsAsBoxes{false};
    float mLayoutAreaMinMargin{0.0f};
    float mLayoutAreaMaxMargin{std::numeric_limits<float>::infinity()};
    bool mIgnoreLayoutBounds{true};
    bool mCheckPinIntersections{false};
    bool mAutocreateConnections{true};
    bool mLockRoutedConnections{false};
    bool loadFile(const std::string &path);
    std::string mData;
    std::string mFormat{"dsn"};
    std::set<std::string> mLoadTracksNets;
    std::map<std::string, NetTopology> mTopologies;
    NetOverrides mNetOverrides;
    PCBoard *create();
    PCBoard *createDSN();
};
inline const NetTopology *PCBFactory::getTopology(const std::string &net) const
{
    auto I = mTopologies.find(net);
    if (I != mTopologies.end())
        return &I->second;
    return 0;
}

#endif // GYM_PCB_LOADERS_FACTORY_H
