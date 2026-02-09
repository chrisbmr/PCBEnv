#ifndef GYM_PCB_PIN_H
#define GYM_PCB_PIN_H

#include "Py.hpp"
#include "Component.hpp"
#include "Color.hpp"
#include <vector>

class CloneEnv;
class Component;
class Connection;
class Net;
class Pin;

using PinCompound = std::vector<Pin *>;

class Pin : public Object
{
public:
    Pin(Component *, const std::string &name);
    ~Pin();
    Object *clone(CloneEnv&) const override;

    Component *getParent() const;

    bool hasNet() const { return !!mNet; }
    Net *net() const { return mNet; }
    bool sameNetAs(const Pin &T) const { return mNet == T.net(); }
    void setNet(Net *net);

    /**
     * Get any layer of the pin that is allowed in the connection's layer mask.
     * If there is no overlap, returns minLayer().
     */
    uint getStartLayerFor(const Connection&) const;

    /**
     * Pin compounds are a hack to support pins with discontiguous shapes.
     */
    const PinCompound *compound() const { return mCompound; }
    bool inCompound(const Pin&) const;
    void setCompound(Pin *);

    uint numConnections() const { return mConnections.size(); }
    void addConnection(Connection *X) { mConnections.insert(X); }
    bool removeConnection(Connection *X, bool retainNet);
    const std::set<Connection *>& connections() const { return mConnections; }

    Color colorLine() const;
    Color colorFill() const;

    std::string str() const override;
    PyObject *getPy(uint depth) const override;

private:
    Net *mNet{0};
    std::set<Connection *> mConnections; // Owned by the net!
    PinCompound *mCompound{0};
};

inline bool Pin::inCompound(const Pin &P) const
{
    return mCompound && mCompound == P.mCompound;
}

inline Color Pin::colorLine() const
{
    return Palette::PinLine[hasNet() ? 1 : 0][minLayer() ? 1 : 0];
}
inline Color Pin::colorFill() const
{
    return Palette::PinFill[hasNet() ? 1 : 0][minLayer() ? 1 : 0];
}

inline Component *Pin::getParent() const
{
    return dynamic_cast<Component *>(mParent);
}

#endif // GYM_PCB_PIN_H
