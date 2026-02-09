#ifndef GYM_PCB_RL_STATE_DRCCHECK_H
#define GYM_PCB_RL_STATE_DRCCHECK_H

#include "RL/StateRepresentation.hpp"
#include "DRC.hpp"

struct PCBItemSets;

namespace sreps {

class ClearanceCheck : public StateRepresentation
{
public:
    const char *name() const override { return "clearance_check"; }
    PyObject *getPy(PyObject *) override;
    void set2D(bool b) { m2D = b; }
    void setLimit(uint m) { mLimit = m; }
    uint check(const PCBItemSets&);
    uint check(Connection&, Pin&);
    uint check(Connection&, Connection&);
    const std::vector<DRCViolation>& result() const { return mDRC; }
private:
    std::vector<DRCViolation> mDRC;
    uint mLimit{32768};
    bool m2D{false};
};

} // namespace sreps

#endif // GYM_PCB_RL_STATE_DRCCHECK_H
