
#include "RL/CommonStateReprs.hpp"

StateRepresentation *StateRepresentation::create(PyObject *py)
{
    py::Object args(py);
    if (!args)
        return new StateRepresentation();
    const auto name = args.isDict() ? args.item("class").asStringView() : args.asStringView();
    if (name.empty()) return new StateRepresentation();
    if (name.starts_with("image")) return sreps::Image::create(py);
    if (name == "board") return new sreps::WholeBoard();
    if (name.starts_with("end")) return new sreps::ConnectionEndpoints();
    if (name == "features") return new sreps::CustomFeatures();
    if (name == "grid") return new sreps::GridData();
    if (name.starts_with("raster")) return new sreps::TrackRasterization();
    if (name == "track" || name == "track_segments") return new sreps::TrackSegments(name.ends_with("_np") || name.ends_with("numpy"));
    throw std::invalid_argument(fmt::format("invalid state representation specifier: {}", name));
    return 0;
}
