
#ifndef GYM_PCB_CLONE_H
#define GYM_PCB_CLONE_H

#include <map>

class PCBoard;
class Component;
class Pin;
class Net;
class Connection;

/**
 * Helper for deep-copying a PCBoard to map old to new objects.
 */
class CloneEnv
{
public:
    CloneEnv(PCBoard &pcb) : origin(pcb) { }
    const PCBoard &origin;
    std::map<const Component *, Component *> C;
    std::map<const Pin *, Pin *> P;
    std::map<const std::vector<Pin *> *, std::vector<Pin *> *> PK;
    std::map<const Net *, Net *> N;
    std::map<const Connection *, Connection *> X;
    PCBoard *target{0};
};

#endif // GYM_PCB_CLONE_H
