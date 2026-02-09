
#ifndef GYM_PCB_UTIL_SUBSETS
#define GYM_PCB_UTIL_SUBSETS

#include "Py.hpp"
#include <set>
class Component;
class Pin;
class Connection;
class Net;
class PCBoard;

namespace util
{

void populate(std::set<Connection *>&, PCBoard&, PyObject *);
void populate(std::set<Net *>&, PCBoard&, PyObject *);
void populate(std::set<Component *>&, PCBoard&, PyObject *);
void populate(std::set<Pin *>&, PCBoard&, PyObject *);

} // namespace util

struct PCBItemSets
{
    PCBItemSets(std::set<Component *> *com = 0,
                std::set<Pin *> *pin = 0,
                std::set<Net *> *net = 0,
                std::set<Connection *> *con = 0) : coms(com), pins(pin), nets(net), cons(con)
    {
        if (!com) coms = &_coms;
        if (!pin) pins = &_pins;
        if (!net) nets = &_nets;
        if (!con) cons = &_cons;
    }
    std::set<Component *> *coms;
    std::set<Component *> _coms;
    std::set<Pin *> *pins;
    std::set<Pin *> _pins;
    std::set<Net *> *nets;
    std::set<Net *> _nets;
    std::set<Connection *> *cons;
    std::set<Connection *> _cons;
    void populate(PCBoard&, PyObject *);
    void clear();
    void complement(PCBoard&);
    void populateComsFromPins();
    void populateComsFromNets();
    void populateComsFromConnections();
    void populatePinsFromComs();
    void populatePinsFromNets();
    void populatePinsFromConnections();
    void populateNetsFromComs();
    void populateNetsFromPins();
    void populateNetsFromConnections();
    void populateConnectionsFromComs();
    void populateConnectionsFromPins();
    void populateConnectionsFromNets();
    void prune(PCBoard&);
};

#endif // GYM_PCB_UTIL_SUBSETS
