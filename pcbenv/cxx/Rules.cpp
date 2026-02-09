#include "Log.hpp"
#include "Rules.hpp"

std::string DesignRules::str() const
{
    return fmt::format("w={} d={} c={}", TraceWidth, ViaDiameter, Clearance);
}
