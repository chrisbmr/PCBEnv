
#ifndef GYM_PCB_NET_H
#define GYM_PCB_NET_H

#include "Defs.hpp"
#include "Color.hpp"
#include "Enums.hpp"
#include "Connection.hpp"
#include "Rules.hpp"
#include <set>
#include <string>

class CloneEnv;
class PCBoard;
class Component;
class Pin;
class Track;

class Net
{
public:
    Net(const std::string &name);
    ~Net();
    Net *clone(CloneEnv&) const;

    PCBoard *getBoard() const { return mBoard; }

    std::string name() const { return mName; }
    uint id() const { return mId; }

    const std::set<Pin *>& getPins() const { return mPins; }
    uint numPins() const { return mPins.size(); }
    bool contains(const Pin *T) const { return mPins.find(const_cast<Pin *>(T)) != mPins.end(); }
    bool empty() const { return mPins.empty(); }

    uint numConnections() const { return mConnections.size(); }
    const std::vector<Connection *>& connections() const { return mConnections; }
    Connection *connection(uint i) const { return mConnections.at(i); }
    Connection *getConnectionBetween(const Pin&, const Pin&) const;

    /**
     * Check whether the track complies with the net's design rules and fits within the layout area.
     * @return 0x2 if rules-compliant | 0x1 if inside layout area
     */
    uint validateTrack(const Track&) const;

    const DesignRules& getRules() const { return mRules; }
    Real getMinTraceWidth() const { return mRules.TraceWidth; }
    Real getMinClearance() const { return mRules.Clearance; }
    Real getViaDiameter() const { return mRules.ViaDiameter; }
    uint32_t getLayerMask() const { return mLayerMask; }

    bool isGroundOrPower() const { return mSignalType & SignalType::POWER_GROUND; }
    bool isGround() const { return mSignalType == SignalType::GROUND; }
    SignalType signalType() const { return mSignalType; }

    Connection *addConnection(Pin *sourcePin, const Point_25& source, Pin *targetPin, const Point_25& target);
    void insert(Pin&);
    void remove(Pin&);

    /**
     * Create 2-pin connections to connect all pins with each other using Kruskal's algorithm.
     */
    void autocreateConnections();

    void setRules(const DesignRules&);
    void setRulesMin(const DesignRules&);
    void setMinTraceWidth(Real);
    void setMinClearance(Real c);
    void setViaDiameter(Real d);
    void setLayerMask(uint32_t m);
    void setLayerMaskFromSignalType();
    void setSignalType(SignalType t) { mSignalType = t; }

    static SignalType getLikelySignalTypeForName(const std::string&);

    void setBoard(PCBoard *board) { mBoard = board; }

    /**
     * Set the net's id and the connections' ids to (net ID << 16) | (connection index + 1).
     * @param id must be < 0x7fff
     */
    void setId(uint id);

    /**
     * Remove all connections connected to @Component and return whether the net is then empty.
     */
    bool extract(Component&);
    /**
     * Remove all connections not in @keep and return whether the net is then empty.
     */
    bool pruneConnections(std::set<Connection *> &keep);

    /**
     * Colorize according to signal type or indexing the palette by net ID.
     */
    void setColorAuto(bool usePalette = false);
    void setColor(const Color &c) { mColor = c; }

    const Color& getColor() const { return mColor; }

    std::string str() const;
    PyObject *getConnectionsPy(uint depth, bool asNumpy) const;
    PyObject *getPinsPy() const;
    PyObject *getPy(uint depth, bool asNumpy) const;

private:
    std::set<Pin *> mPins;
    std::vector<Connection *> mConnections; //!< These are owned by the net!
    const std::string mName;
    PCBoard *mBoard{0};
    uint mId;
    DesignRules mRules{0,0,0};
    uint32_t mLayerMask{0xffffffff};
    Color mColor{Color::WHITE};
    SignalType mSignalType{SignalType::ANY};

private:
    void removeConnectionsWithNonmemberPins();
    void rebuildPinSetFromConnections();
};

#endif // GYM_PCB_NET_H
