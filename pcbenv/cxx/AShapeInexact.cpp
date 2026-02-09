#define CGAL_NO_PRECONDITIONS
#include "AShape.hpp"
#include "Log.hpp"
#include <CGAL/approximated_offset_2.h>

#ifndef GYM_PCB_NO_CGAL_POLYGON_OFFSET
#define GYM_PCB_NO_CGAL_POLYGON_OFFSET 1
#endif

Polygon_2 PolygonEx::grow(const Polygon_2 &poly, Real ds, Real eps)
{
    if (GYM_PCB_NO_CGAL_POLYGON_OFFSET)
        return _grow_stupid(poly, ds);

    try {
        return _grow_offset(poly, ds, eps);
    } catch (const std::exception &e) {
        WARN("CGAL is unhappy growing this polygon");
    }
    return _grow_stupid(poly, ds);
}

Polygon_2 PolygonEx::_grow_offset(const Polygon_2 &poly, Real ds, Real eps)
{
    auto H = CGAL::approximated_offset_2(poly, ds, ds * eps);
    if (H.has_holes())
        throw std::runtime_error("grown polygon has holes");
    auto G = H.outer_boundary();
    Polygon_2 P;
    for (auto c = G.curves_begin(); c != G.curves_end(); ++c)
        P.push_back(::Point_2(CGAL::to_double(c->source().x()),
                              CGAL::to_double(c->source().y())));
    return P;
}

/**
 * Primitive method to grow a polygon by offseting each vertex
 * outward in the direction of the average of the adjoining edges.
 */
Polygon_2 PolygonEx::_grow_stupid(const Polygon_2 &poly, Real ds)
{
    if (poly.is_clockwise_oriented())
        ds = -ds;
    const uint size = poly.size();
    Polygon_2 res;
    for (uint i = 0; i < size; ++i) {
        const auto R = poly[(i - 1) % size];
        const auto M = poly[i];
        const auto L = poly[(i + 1) % size];
        const auto vL = L - M;
        const auto vR = R - M;
        auto nL = Vector_2(vL.y(), -vL.x());
        auto nR = Vector_2(-vR.y(), vR.x());
        const auto sL = nL.squared_length();
        const auto sR = nR.squared_length();
        if (sL)
            nL *= 1.0 / std::sqrt(sL);
        if (sR)
            nR *= 1.0 / std::sqrt(sR);
        res.push_back(M + (nL + nR) * (0.5 * ds));
    }
    return res;
}

