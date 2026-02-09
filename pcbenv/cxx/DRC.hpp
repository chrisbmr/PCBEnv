#ifndef GYM_PCB_DRC_H
#define GYM_PCB_DRC_H

#include "Py.hpp"
#include "Geometry.hpp"

class Connection;
class Pin;

class DRCViolation
{
public:
    DRCViolation(Point_25, Connection *, Connection *, Pin *);
    const Point_25& getLocation() const { return mLocation; }
    Connection *getConnection(uint i) const { return mConnections[i ? 1 : 0]; }
    PyObject *getPy() const;
private:
    Point_25 mLocation;
    Connection *mConnections[2];
    Pin *mPin;
};
inline DRCViolation::DRCViolation(Point_25 x, Connection *A, Connection *B, Pin *P) : mLocation(x), mPin(P)
{
    mConnections[0] = A;
    mConnections[1] = B;
}

#endif // GYM_PCB_DRC_H
