#ifndef GYM_PCB_TOPOLOGY_H
#define GYM_PCB_TOPOLOGY_H

#include "Geometry.hpp"
#include <string>
#include <variant>
#include <vector>

class Net;

struct NetTopology
{
public:
    std::vector<std::vector<std::variant<Point_25, std::string>>> Connections;
    void create(Net&) const;
};

#endif // GYM_PCB_TOPOLOGY_H
