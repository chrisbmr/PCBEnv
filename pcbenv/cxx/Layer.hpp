
#ifndef GYM_PCB_LAYER_H
#define GYM_PCB_LAYER_H

#include "Defs.hpp"
#include "Py.hpp"
#include "Enums.hpp"

class Layer
{
public:
    void setNumber(uint N) { mNumber = N; }
    void setType(SignalType t) { mType = t; }
    uint getNumber() const { return mNumber; }
    SignalType getType() const { return mType; }
    bool hasType(SignalType t) const { return (mType & t); }
    bool isSignal() const { return !!(mType & SignalType::SIGNAL); }
    bool isPower() const { return !!(mType & SignalType::POWER); }
    bool isGround() const { return !!(mType & SignalType::GROUND); }
    std::string str() const;
    PyObject *getPy() const;
public:
    static bool rangesOverlap(uint z0, uint z1, uint Z0, uint Z1);
private:
    uint mNumber;
    SignalType mType{SignalType::ANY};
};

/**
 * Adjust the range [zmin,zmax] assuming that layers [C0,C1] are cut.
 * If the return value is 0, zmin and zmax are not modified.
 *
 * @param C0 first cut layer
 * @param C1 last cut layer
 *
 * @return the number of layers remaining
 */
inline uint cutLayersFromRange(uint &zmin, uint &zmax, uint C0, uint C1)
{
    assert(zmin <= zmax && C0 <= C1);
    if (zmax < C0)
        return (zmax - zmin) + 1;
    if (C0 <= zmin && zmax <= C1)
        return 0;
    const uint dn = (C1 - C0) + 1;
    if (zmin >= C0) // zmin is above the cut range (then it drops by dn) or inside (then its new number is C0 as the first non-cut layer)
        zmin = (zmin > C1) ? (zmin - dn) : C0;
    zmax = (zmax > C1) ? (zmax - dn) : (C0 - 1); // if zmax is above the cut range it drops by dn, if it is inside it becomes the layer below the first cut one
    assert(zmin <= zmax);
    return (zmax - zmin) + 1;
}

/**
 * Adjust the layer index z assuming layers [C0,C1] are cut.
 * @return whether layer z still exists
 */
inline bool adjustIndexForCutLayers(uint &z, uint C0, uint C1)
{
    if (C0 <= z && z <= C1)
        return false;
    if (z > C1)
        z -= (C1 - C0) + 1;
    return true;
}

inline bool Layer::rangesOverlap(uint z0, uint z1, uint Z0, uint Z1)
{
    return (z1 >= Z0) && (z0 <= Z1);
}

#endif // GYM_PCB_LAYER_H
