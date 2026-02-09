#ifndef GYM_PCB_RL_STATE_TRACKS_H
#define GYM_PCB_RL_STATE_TRACKS_H

#include "RL/StateRepresentation.hpp"

namespace sreps {

class TrackRasterization : public StateRepresentation
{
public:
    const char *name() const override { return "raster"; }
    PyObject *getPy(PyObject *track_or_track_list) override;
private:
    PyObject *getPy1(PyObject *) const;
    PyObject *getPy1(const Connection&) const;
};

class TrackSegments : public StateRepresentation
{
public:
    const char *name() const override { return "track"; }
    TrackSegments(bool asNumpy) : mFormatNumpy(asNumpy) { }
    void setFormatNumpy(bool b) { mFormatNumpy = b; }
    PyObject *getPy(PyObject *track_or_track_list) override;
private:
    bool mFormatNumpy{true};
};

} // namespace sreps

#endif // GYM_PCB_RL_STATE_TRACKS_H
