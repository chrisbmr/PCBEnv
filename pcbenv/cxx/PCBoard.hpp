
#ifndef GYM_PCB_PCBOARD_H
#define GYM_PCB_PCBOARD_H

#include "Units.hpp"
#include "Geometry.hpp"
#include <map>
#include <vector>
#include <future>
#include <shared_mutex>
#include <atomic>
#include "Rules.hpp"
#include "LayoutArea.hpp"
#include "Layer.hpp"
#include "NavGrid.hpp"
#include "BVH.hpp"

class CloneEnv;
class Component;
class Pin;
class Net;
class Connection;
class NavTriangulation;

///
/// For synchronization with the UI.
///
#define PCB_CHANGED_NAV_GRID   (1 << 0)
#define PCB_CHANGED_NAV_TRIS   (1 << 1)
#define PCB_CHANGED_ROUTES     (1 << 2)
#define PCB_CHANGED_COMPONENTS (1 << 3)
#define PCB_CHANGED_PINS       (1 << 4)
#define PCB_CHANGED_NET_COLORS (1 << 5)
#define PCB_CHANGED_OBJECTS    (1 << 6) // use after add/remove(Component)
#define PCB_CHANGED_NEW_BOARD  (1 << 15)

#define PCB_CHANGED_OBJECTS_SET (PCB_CHANGED_OBJECTS | PCB_CHANGED_COMPONENTS | PCB_CHANGED_PINS | PCB_CHANGED_ROUTES)

/**
 * This class represents the printed circuit board.
 */
class PCBoard
{
public:
    PCBoard(const Nanometers UnitLength = Nanometers(100000.0));
    PCBoard *clone(CloneEnv&) const;
    ~PCBoard();

    const std::string& name() const { return mName; }
    const std::string& getSourceFilePath() const { return mSourceFilePath; }

    uint getNumLayers() const { return mLayers.size(); }
    const Layer& getLayer(uint i) const { return mLayers.at(i); }

    LayoutArea& getLayoutArea() { return mLayoutArea; }
    const LayoutArea& getLayoutArea() const { return mLayoutArea; }

    PCBoard& operator=(const PCBoard&) = delete;

    Net *getNet(const std::string &name) const;
    Net *getNet(uint) const;
    Net *getNet(PyObject *) const; //!< param must be a name string
    Component *getComponent(const std::string &name) const;
    Component *getComponent(uint) const;
    Component *getComponent(PyObject *) const; //!< param must be a name string
    Component *getComponentAt(const Point_2&, int z = -1) const; //!< -1 for all layers
    Pin *getPinAt(const Point_2&, int z = -1) const; //!< -1 for all layers
    Pin *getPin(const std::string &name) const; //!< {com}-{pin}
    Pin *getPin(PyObject *) const; //!< "{com}-{pin}" or (com,pin)
    Connection *getConnection(PyObject *) const; // (net,connection-index) or (pin,pin)

    uint getNumNets() const { return mNets.size(); }
    const std::vector<Net *>& getNets() const { return mNets; }

    uint getNumComponents() const { return mParts.size(); }
    const std::vector<Component *>& getComponents() const { return mParts; }
    void renumberComs();
    void renumberPins();
    void renumberNets();

    const ObjectsBVH *getComBVH() const { return mComBVH; }
    const ObjectsBVH *getPinBVH() const { return mPinBVH; }

    const NavGrid& getNavGrid() const { return mNavGrid; }
    NavGrid& getNavGrid() { return mNavGrid; }

    NavTriangulation *getTNG() const { return mTNG; }
    bool rebuildTNG();

    /**
     * Move all connection endpoints to the nearest grid cell center.
     */
    void forceConnectionsToGrid();

    /**
     * These unblock/restore the grid points of pins and tracks associated with this connection.
     */
    void initPathfindingForNet(const Connection&);
    void finiPathfindingForNet(const Connection&);
    void initPathfindingFor(const Connection&);
    void finiPathfindingFor(const Connection&);

    void add(Component&);
    void add(Net&);
    Component *remove(Component&);
    Net *remove(Net&);

    /**
     * Remove all objects not contained in set.
     */
    void pruneLayers(std::set<uint> &keep);
    void pruneConnections(std::set<Connection *> &keep, bool deleteNets = true);
    void pruneNets(std::set<Net *> &keep);
    void prunePins(std::set<Pin *> &keep);
    void pruneComponents(std::set<Component *> &keep);

    void removeEmptyNets();

    Real metersToUnits(Nanometers nm) const { return nm.value() / mUnitLength_nm.value(); }
    Nanometers unitsToNanometers(Real r) const { return mUnitLength_nm * r; }
    Micrometers unitsToMicrometers(Real r) const { return Micrometers(mUnitLength_nm * r); }

    uint getLayerForSide(char t_or_b) const;
    const std::vector<Layer>& getLayers() const { return mLayers; }
    Layer& getLayer(uint i) { return mLayers[i]; }

    /**
     * Set the number of layers and, if less than the current number, delete objects on no-longer existing ones.
     */
    void setNumLayers(uint N);

    void setName(const std::string &name) { mName = name; }
    void setSourceFilePath(const std::string &path) { mSourceFilePath = path; }

    /**
     * Adjust the layout area around the component area bbox.
     *
     * Negative values are interpreted as fractions of the component bbox's maximum dimension.
     *
     * @param min Minimum margin around component area bbox.
     * @param max Maximum margin around component area bbox (may be infinite).
     */
    void adjustLayoutAreaMargins(Real min, Real max = std::numeric_limits<Real>::infinity());

    void setMinClearancePins(Micrometers);
    Real getMinClearancePins() const { return mTech.pins.minClearance; }
    void setMinViaDiameter(Micrometers);
    Real getMinViaDiameter() const { return mTech.nets.ViaDiameter; }
    void setMinClearanceNets(Micrometers);
    Real getMinClearanceNets() const { return mTech.nets.Clearance; }
    void setMinTraceWidth(Micrometers);
    Real getMinTraceWidth() const { return mTech.nets.TraceWidth; }

    Bbox_2 getComponentAreaBbox() const { return mComponentAreaBbox; }
    void recomputeComponentAreaBbox();

    /**
     * This is an RL-specific working area.
     */
    Bbox_2 getActiveAreaBbox() const { return mActiveAreaBbox; }
    void setActiveAreaBbox(const Bbox_2 &box) { mActiveAreaBbox = box; }

    /**
     * Run A-star.
     * NOTE: Locks the PCB while modifying the Connection.
     * @param ref Set this if there is a different connection we're allowed to cross.
     */
    bool runPathFinding(Connection&, Connection *ref = 0, const AStarCosts * = 0);

    /**
     * Rasterize the tracks of the connection to the grid.
     * @param count This affects only the tracks' internal rasterization counter.
     */
    void rasterizeTracks(const Connection&, int8_t count = 1);
    void unrasterizeTracks(const Connection&, int8_t count = -1);
    void eraseTracks(Connection&); //!< delete and unrasterize the connection's tracks
    void wipe(); //!< delete and unrasterize all tracks

    /**
     * Count the number of unavailable grid cells that the tracks of the connection would use.
     * This sums over the whole area rather than just the center lines.
     * NOTE: The connection should be unrasterized for this.
     */
    Real sumViolationArea(Connection&, Connection *ref = 0);

    PyObject *getPy(PyObject *dict, uint depth = 3, bool asNumpy = true) const;
    PyObject *getLayersPy() const;

    /**
     * Hold this lock to synchronize with the GUI while modifying the board.
     * Only runPathFinding() does this on its own.
     * This does not prevent two pathfinding operations from running in parallel and producing conflicting tracks!
     */
    std::mutex& getLock() const { return mLock; }

    bool hasChanged() const { return mHasChanged; }
    void setChanged(uint32_t mask) { mHasChanged |= mask; }
    uint32_t ackChanges() const { return mHasChanged.exchange(0); }

private:
    LayoutArea mLayoutArea;
    std::string mName;
    std::string mSourceFilePath;
    std::vector<Component *> mParts;
    Bbox_2 mComponentAreaBbox;
    Bbox_2 mActiveAreaBbox;
    std::vector<Net *> mNets;
    std::vector<Layer> mLayers;
    std::map<std::string, Component *> mPartsByName;
    std::map<std::string, Net *> mNetsByName;
    Point_2 mLayoutAreaOrigin;
    Bbox_2 mLayoutAreaBbox;
    NavGrid mNavGrid;
    NavTriangulation *mTNG{0};
    ObjectsBVH *mComBVH{0};
    ObjectsBVH *mPinBVH{0};
    struct {
        struct {
            Real minClearance{0.0};
        } pins;
        DesignRules nets{0,0,0};
    } mTech;
    const Nanometers mUnitLength_nm;
    mutable std::atomic<uint32_t> mHasChanged{0};
    mutable std::mutex mLock;

private:
    void rebuildBVH();
    void removedLayersUpdate(uint removedZMin, uint removedZMax);
};

inline uint PCBoard::getLayerForSide(char side) const
{
    if (side == 't')
        return 0;
    assert(side == 'b');
    return mLayers.size() - 1;
}

#endif // GYM_PCB_PCBOARD_H
