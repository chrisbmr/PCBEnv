
#ifndef GYM_PCB_METRICS_H
#define GYM_PCB_METRICS_H

#include <set>

class Connection;
class PCBoard;
class WideSegment_25;

namespace metrics {

Real ConnectionLinkage(const Connection&, const Connection&, bool cross, Real ex = 2.0, int log2StepSize = -3);
Real ConnectionLinkage(const PCBoard&, Real ex, int log2StepSize);

uint RatsNestCrossings(const PCBoard&, const std::set<std::string> *nets = 0);

void SegmentParallelity(Real &cosine, Real &weight, const WideSegment_25&, const WideSegment_25&);

} // namespace metrics

#endif // GYM_PCB_METRICS_H
