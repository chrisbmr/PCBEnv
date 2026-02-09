
#include "Metrics.hpp"
#include "PCBoard.hpp"
#include "Net.hpp"
#include "Connection.hpp"

namespace metrics {

Real meanDistanceP2P(const Segment_2 &s1, const Segment_2 &s2, int log2StepSize, Real ex)
{
    if (log2StepSize > 0)
        throw std::invalid_argument("log2(step size) must be <= 0");
    const Real dt = std::exp2(log2StepSize);
    Real d12[4];
    d12[0] = CGAL::squared_distance(s1.source(), s2.source());
    d12[1] = CGAL::squared_distance(s1.target(), s2.target());
    d12[2] = CGAL::squared_distance(s1.source(), s2.target());
    d12[3] = CGAL::squared_distance(s1.target(), s2.source());
    uint n = 0;
    for (uint i = 1; i < 4; ++i)
        if (d12[i] < d12[n])
            n = i;
    const auto P1 = s1.source();
    const auto P2 = (n <= 1) ? s2.source() : s2.target();
    const auto v1 = s1.to_vector();
    const auto v2 = (n <= 1) ? s2.to_vector() : -s2.to_vector();
    Real d = 0.0;
    uint m = 0;
    for (Real t = 0.0; t <= 1.0; t += dt, ++m)
        d += std::pow(CGAL::squared_distance(P1 + t * v1, P2 + t * v2), ex * 0.5);
    return d / m;
}
Real meanDistanceP2S(const Segment_2 &s1, const Segment_2 &s2, int log2StepSize, Real ex)
{
    if (log2StepSize > 0)
        throw std::invalid_argument("log2(step size) must be <= 0");
    const Real dt = std::exp2(log2StepSize);
    const auto P1 = s1.source();
    const auto P2 = s2.source();
    const auto v1 = s1.to_vector();
    const auto v2 = s2.to_vector();
    Real d1 = 0.0;
    Real d2 = 0.0;
    uint m = 0;
    for (Real t = 0.0; t <= 1.0; t += dt, ++m) {
        d1 += std::pow(CGAL::squared_distance(P1 + t * v1, s2), ex * 0.5);
        d2 += std::pow(CGAL::squared_distance(P2 + t * v2, s1), ex * 0.5);
    }
    return (d1 + d2) / (2 * m);
}

Real ConnectionLinkage(const Connection &A, const Connection &B, bool cross, Real ex, int log2StepSize)
{
    const Real d2_rat = CGAL::squared_distance(A.segment2(), B.segment2());
    const Real d2_mean = meanDistanceP2S(A.segment2(), B.segment2(), log2StepSize, ex);
    if (cross || d2_rat == 0.0)
        return d2_mean * 0.5;
    return d2_mean;
}
Real ConnectionLinkage(const PCBoard &PCB, Real ex, int log2StepSize)
{
    std::vector<const Connection *> conns;
    std::vector<Real> L;
    for (const auto *N : PCB.getNets())
        conns.insert(conns.end(), N->connections().begin(), N->connections().end());
    L.reserve(conns.size());
    const double M = conns.size() * (conns.size() - 1) / 2;
    for (uint i = 0;     i < conns.size(); ++i)
    for (uint k = i + 1; k < conns.size(); ++k)
        L.push_back(ConnectionLinkage(*conns[i], *conns[k], false, ex, log2StepSize));
    std::sort(L.begin(), L.end());
    return std::accumulate(L.begin(), L.end(), 0.0) / M;
}

uint RatsNestCrossings(const PCBoard &PCB, const std::set<std::string> *nets)
{
    std::vector<const Connection *> conns;
    for (const auto *N : PCB.getNets())
        if (!nets || nets->contains(N->name()))
            conns.insert(conns.end(), N->connections().begin(), N->connections().end());
    uint N = 0;
    for (uint i = 0;     i < conns.size(); ++i) {
    for (uint k = i + 1; k < conns.size(); ++k) {
        auto x = CGAL::intersection(conns[i]->segment2(), conns[k]->segment2());
        if (!x)
            continue;
        if (const Point_2 *v = std_get_for_cgal<Point_2>(&*x))
            if (*v != conns[i]->source().xy() &&
                *v != conns[i]->target().xy() &&
                *v != conns[k]->source().xy() &&
                *v != conns[k]->target().xy())
                N++;
    }}
    return N;
}


void SegmentParallelity(Real &cosine, Real &weight, const WideSegment_25 &s, const WideSegment_25 &r)
{
    const Real ls = s.base().length();
    const Real lr = r.base().length();
    const Real ds1 = std::sqrt(CGAL::squared_distance(s.source_2(), r.base().s2()));
    const Real dt1 = std::sqrt(CGAL::squared_distance(s.target_2(), r.base().s2()));
    const Real ds2 = std::sqrt(CGAL::squared_distance(r.source_2(), s.base().s2()));
    const Real dt2 = std::sqrt(CGAL::squared_distance(r.target_2(), s.base().s2()));
    const Real w = s.halfWidth() + r.halfWidth();
    const Real dAvg = std::max(std::min(ds1 + dt1, ds2 + dt2) * 0.5 - w, 0.0);

    cosine = std::abs(CGAL::scalar_product(s.base().getDirection(), r.base().getDirection()));
    weight = std::exp(-0.125 * dAvg / w) * std::min(lr/ls, 1.0);
}

} // namespace metrics
