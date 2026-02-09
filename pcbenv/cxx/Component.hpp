
#ifndef GYM_PCB_COMPONENT_H
#define GYM_PCB_COMPONENT_H

#include "Py.hpp"
#include "Object.hpp"
#include "Color.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

class CloneEnv;
class Pin;
class Connection;

/**
 * A circuit component.
 * NOTE: Hyphens in component (but not pin) names are replaced by underscores.
 */
class Component : public Object
{
    static std::string sanitizeName(const std::string&);
public:
    Component(const std::string &name);
    ~Component();

    Object *clone(CloneEnv&) const override;

    /**
     * The location is for reference only, pins are not moved.
     */
    void setLocation(const Point_2 &refPoint, int layer = 0);

    bool isPlaced() const { return mLayerRange[0] >= 0; }
    const Point_2& getRefPoint() const { return mRefPoint; }

    const std::vector<Object *>& getPins() const { return getChildren(); }
    uint numPins() const { return numChildren(); }
    Pin *getPin(const std::string &name) const;
    Pin& getPin(uint i) const { return *mChildren.at(i)->as<Pin>(); }

    /**
     * Create a new pin with the given name.
     * The pin's layer is initialized to the layer specified in setLocation().
     * @param name must be unique or empty (empty names result in "§getNumPins()").
     */
    Pin *addPin(const std::string &name);

    /**
     * Remove a pin from this component and its net.
     * @return pointer to the pin
     */
    Pin *removeAndDisconnectPin(Pin&);

    /**
     * Add all connections this component's pins participate in to the set.
     */
    void gatherConnections(std::set<Connection *>&) const;

    /**
     * Add all pin pairs that are connected between the given components to the vector.
     */
    static uint connectivity(std::vector<std::pair<Pin *, Pin *>>&, const Component&, const Component&);

    /**
     * Mark the given object as occluded by this component.
     * At the moment this does only this->setCanRouteInside(true).
     */
    void addOccludedObject(Object&) override;

    std::string str() const override;
    PyObject *getPy(uint depth) const override;

    // UI
    void setColors(const Color &line, const Color &fill) { mColorLine = line; mColorFill = fill; }

    const Color& getLineColor() const { return mColorLine; }
    const Color& getFillColor() const { return mColorFill; }

private:
    Point_2 mRefPoint;
    std::map<std::string, Pin *> mPinMap;
    Color mColorLine;
    Color mColorFill;
};

inline Pin *Component::getPin(const std::string &name) const
{
    auto I = mPinMap.find(name);
    if (I == mPinMap.end())
        return 0;
    return I->second;
}

#endif // GYM_PCB_COMPONENT_H
