#ifndef GYM_PCB_RULES_H
#define GYM_PCB_RULES_H

#include "Defs.hpp"

struct DesignRules
{
    Real Clearance;
    Real TraceWidth;
    Real ViaDiameter;
    Real viaRadius() const { return ViaDiameter * 0.5; }

    DesignRules() { }
    DesignRules(Real c, Real w, Real d) : Clearance(c), TraceWidth(w), ViaDiameter(d) { }
    DesignRules max(const DesignRules&) const;
    std::string str() const;
};

inline DesignRules DesignRules::max(const DesignRules &rules) const
{
    DesignRules rv;
    rv.Clearance = std::max(Clearance, rules.Clearance);
    rv.TraceWidth = std::max(TraceWidth, rules.TraceWidth);
    rv.ViaDiameter = std::max(ViaDiameter, rules.ViaDiameter);
    return rv;
}

#endif // GYM_PCB_RULES_H
