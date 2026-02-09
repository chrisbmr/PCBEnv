
#include "Loaders/DSNParserBase.hpp"
#include "Log.hpp"

namespace dsn {
struct Parser
{
    std::string kicad_version;
};
} // namespace dsn

class KiCADParser : public DSNParserBase
{
public:
    KiCADParser(dsn::Data&);
    bool parse(const std::string&) override;
private:
    dsn::Parser mParser;
    std::map<std::string, uint> mLayerNameToZ;
    std::map<std::string, uint> mLayerNameToKiCADIndex;
    std::map<std::string, uint> mPlacementNameCounts;
    std::vector<dsn::Net *> mNets;
    double mYInvert{-1.0};
    bool mPadIncludesModuleRotation{true};
    void readParser(const std::string&);
    void readLayers(const std::string&);
    void readLayer(const std::string&);
    void readLayerRange(int &z0, int &z1, const std::vector<std::string>&, bool thru);
    void readPCB(const std::string&);
    void readNetNames(const std::string&);
    void readClass(const std::string&);
    void readModule(const std::string&);
    void readPad(dsn::Image&, const std::string&, int rotate);
    void readSegment(const std::string&);
    void readVia(const std::string&);
    void readBoundary(const std::vector<std::string>&);
    void readZone(const std::string&);
    void fixPlacementName(std::string&);
    void sortLayers();
};

void KiCADParser::readParser(const std::string &s)
{
    std::vector<std::string> v = gather(s, "version");
    if (!v.empty())
        mParser.kicad_version = v[0];
}
void KiCADParser::sortLayers()
{
    std::sort(mData.structure.layers.begin(), mData.structure.layers.end());
}
void KiCADParser::readLayers(const std::string &s)
{
    auto v = gather(s, "layers");
    assert(v.size() == 1);
    if (!v.empty()) {
        for (const auto &l : gatherArray(v[0]))
            readLayer(l);
    }
    sortLayers();

    int zT = -1, zB = -1;
    for (auto &L : mData.structure.layers) {
        if (L.type != "signal" && L.type != "power" && L.type != "ground" && L.type != "mixed")
            continue;
        L.copper_index = mData.structure.copper_layers.size();
        mData.structure.copper_layers.push_back(&L);
        mLayerNameToZ[L.name] = L.copper_index;
        if (L.side == 't')
            zT = L.copper_index;
        else if (L.side == 'b')
            zB = L.copper_index;
    }
    if (mData.structure.copper_layers.empty())
        throw std::runtime_error("no copper layers found");
    if (zB < 0 || zT < 0) {
        mData.structure.copper_layers.back()->side = 'b';
        mData.structure.copper_layers.front()->side = 't';
    }
}
void KiCADParser::readLayer(const std::string &s)
{
    auto t = tokenize(s);
    if (t.empty())
        throw std::runtime_error("Expected layer info");
    dsn::Layer layer;
    layer.kicad_index = t[0].Int64();
    if (layer.kicad_index < 0)
        throw std::runtime_error("Expected layer index >= 0");
    layer.name = unquote(t[1].String());
    layer.side = parseSide(layer.name);
    layer.type = t[2].String();
    if (mData.structure.layers.size() <= uint(layer.kicad_index))
        mData.structure.layers.resize(layer.kicad_index + 1);
    mData.structure.layers[layer.kicad_index] = layer;
    mLayerNameToKiCADIndex[layer.name] = layer.kicad_index;
}
void KiCADParser::readNetNames(const std::string &s)
{
    auto v = gather(s, "net"); // FIXME: top level only
    int indexMax = -1;
    for (auto &vi : v) {
        const auto t = tokenize(vi);
        dsn::Net net;
        net.name = unquote(t[1].String());
        net.index = t[0].Int64();
        indexMax = std::max(indexMax, net.index);
        mData.nets[net.name] = net;
    }
    // Don't add nets after populating this!
    mNets.resize(indexMax + 1, 0);
    for (auto &net : mData.nets)
        mNets[net.second.index] = &net.second;
}
void KiCADParser::readClass(const std::string &s)
{
    const std::string type = firstToken(s).String();

    auto clearance = gather(s, "clearance");
    auto trace_width = gather(s, "trace_width");
    auto via_dia = gather(s, "via_dia");

    double c = clearance.empty() ? 0.0 : firstToken(clearance[0]).Float64();
    double w = trace_width.empty() ? 0.0 : firstToken(trace_width[0]).Float64();
    double d = via_dia.empty() ? 0.0 : firstToken(via_dia[0]).Float64();

    if (type == "Default") {
        mData.structure.clearance[""] = c;
        mData.structure.width[""] = w;
        mData.structure.via[""] = d;
    }
    for (const auto &name : gather(s, "add_net")) {
        dsn::Net &net = mData.nets.at(unquote(name));
        if (c != 0.0)
            net.clearance = c;
        if (w != 0.0)
            net.width = w;
        if (d != 0.0)
            net.via_diameter = d;
    }
}

void KiCADParser::readLayerRange(int &z0, int &z1, const std::vector<std::string> &s, bool thru)
{
    if (s.empty()) {
        if (!thru)
            throw std::runtime_error("Expected layers(...)");
        z0 = 0;
        z1 = mData.structure.copper_layers.size() - 1;
        return;
    }
    z0 = std::numeric_limits<int>::max();
    z1 = -1;
    for (const auto &_name : tokenize(s[0])) {
        const auto name = unquote(_name.String());
        if (name.find("*.") == 0 || name == "F&B.Cu") { // FIXME: Is F&B.Cu actually the same?
            if (name == "*.Cu" || name == "F&B.Cu") {
                z0 = 0;
                z1 = mData.structure.copper_layers.size() - 1;
            } else {
                DEBUG("Ignoring layer range " << name);
            }
        } else if (mLayerNameToZ.find(name) != mLayerNameToZ.end()) {
            const int z = mLayerNameToZ.at(name);
            z0 = std::min(z0, z);
            z1 = std::max(z1, z);
        } else if (mLayerNameToKiCADIndex.find(name) == mLayerNameToKiCADIndex.end()) {
            throw std::runtime_error(fmt::format("Encountered unknown layer name: {}", name));
        }
    }
}

void KiCADParser::fixPlacementName(std::string &name)
{
    const uint n = ++mPlacementNameCounts[name];
    if (n >= 2) {
        name += "§";
        name += std::to_string(n - 1);
    }
}

void KiCADParser::readPad(dsn::Image &image, const std::string &s, int rotate)
{
    dsn::Pin pin;
    dsn::Padstack pad;
    const auto main = tokenize(s);
    if (main.size() < 3)
        throw std::runtime_error("Expected (pad <index> <type> <shape> ...)");
    pin.name = unquote(main[0].String());
    pad.name = std::to_string(mData.padstacks.size());
    if (pin.name.empty())
        pin.name = std::string("§") + pad.name;
    pad.type = main[1].String();
    pad.attach = false;

    const auto net = gather(s, "net");
    if (!net.empty()) {
        if (net.size() != 1)
            throw std::runtime_error("Expected unique (pad (net ...))");
        auto t = tokenize(net[0]);
        if (t.size() != 2)
            throw std::runtime_error("Expected unique (pad (net <index> <name>))");
        pin.net = unquote(t[1].String());
        pin.has_net = true;
    }

    auto at = gather(s, "at");
    if (at.size() != 1)
        throw std::runtime_error("Expected unique (pad (at ...))");
    auto coord = tokenize(at[0]);
    if (coord.size() != 2 && coord.size() != 3)
        throw std::runtime_error("Expected (pad (at <x> <y> [<angle>]))");
    pin.x = coord[0].Float64();
    pin.y = coord[1].Float64() * mYInvert;
    pin.angle = rotate;
    if (coord.size() == 3)
        pin.angle += coord[2].AngleDeg();
    readLayerRange(pin.z0, pin.z1, gather(s, "layers"), pad.type == "np_thru_hole");

    auto clearance = gather(s, "clearance");
    if (clearance.size() > 1)
        throw std::runtime_error("Did not expect multiple (clearance ) entries");
    if (clearance.size() == 1)
        pin.clearance = firstToken(clearance[0]).Float64();

    dsn::Shape shape;
    shape.geometry = main[2].String();
    const auto size = gather(s, "size");
    if (size.size() != 1)
        throw std::runtime_error("Expected (pad (size ...))");
    const auto size_xy = tokenize(size[0]);
    if (size_xy.size() != 2)
        throw std::runtime_error("Expected (pad (size <x> <y>))");
    const double w = size_xy[0].Float64();
    const double h = size_xy[1].Float64();
    shape.z = pin.z0;
    if (shape.geometry == "roundrect")
        shape.geometry = "rect"; // FIXME: roundrect_rratio
    else if (shape.geometry == "custom") {
        const auto prims = gather(s, "primitives");
        if (prims.size() == 0)
            WARN("No or empty primitives specified for KiCAD custom shape, assuming rectangle");
        else if (prims.size() == 1)
            WARN("KiCAD custom shape will be read as rectangle");
        else
            throw std::runtime_error("More than one (primitives ...) specifier found");
        shape.geometry = "rect";
    }
    if (shape.geometry == "circle") {
        shape.values.push_back(w);
    } else if (shape.geometry == "oval") {
        shape.geometry = "path";
        const bool horiz = w > h;
        shape.path.aperture_width = horiz ? h : w;
        shape.path.xy.push_back(horiz ? -(w - h) * 0.5 : 0.0);
        shape.path.xy.push_back(horiz ? 0.0 : -(h - w) * 0.5);
        shape.path.xy.push_back(horiz ? +(w - h) * 0.5 : 0.0);
        shape.path.xy.push_back(horiz ? 0.0 : +(h - w) * 0.5);
    } else if (shape.geometry == "rect") {
        shape.values.push_back(-w * 0.5);
        shape.values.push_back(-h * 0.5);
        shape.values.push_back(w * 0.5);
        shape.values.push_back(h * 0.5);
    }
    pad.shapes.push_back(shape);

    pin.padstack = &mData.padstacks[pad.name];
    *pin.padstack = pad;
    pin.index = image.pins.size();

    for (auto &base : image.pins) {
        if (pin.x != base.x || pin.y != base.y || (pin.z1 < base.z0 || pin.z0 > base.z1) || pin.angle != base.angle)
            continue;
        //assert(pin.name == base.name); // even this is legal apparently
        if (pin.z0 != base.z0 || pin.z1 != base.z1)
            continue;
        base.clearance = std::max(base.clearance, pin.clearance);
        WARN("discarding duplicate pin on " << image.name << ": " << pin.str() << " == " << base.str());
        assert(base.padstack->shapes[0] == pad.shapes[0]);
        return;
    }
    image.pins.push_back(pin);
    if (pin.has_net)
        mData.nets.at(pin.net).pins.insert(image.name + '-' + pin.name);
}
void KiCADParser::readModule(const std::string &s)
{
    dsn::Placement place;
    dsn::Image image;

    bool hasReference = false;
    for (auto &text : gather(s, "fp_text")) {
        if (hasReference)
            break;
        auto t = tokenize(text);
        hasReference = t.size() >= 2 && t[0].String() == "reference";
        if (hasReference)
            place.name = noHyphen(unquote(t[1].String()));
    }
    for (auto &text : gather(s, "property")) {
        if (hasReference)
            break;
        auto t = tokenize(text);
        hasReference = t.size() >= 2 && unquote(t[0].String()) == "Reference";
        if (hasReference)
            place.name = noHyphen(unquote(t[1].String()));
    }
    if (!hasReference)
        throw std::runtime_error("Expected (module (fp_text reference <name>))");
    if (place.name.empty()) {
        WARN("KiCAD: ignoring placement with empty reference");
        return;
    }
    fixPlacementName(place.name);
    image.name = place.name;

    auto layer = gather(s, "layer");
    auto at = gather(s, "at");
    if (layer.size() != 1 || at.size() != 1)
        throw std::runtime_error("Expected unique (module (at ...) (layer ...))");
    auto coord = tokenize(at[0]);
    auto z = tokenize(layer[0]);
    if ((coord.size() != 2 && coord.size() != 3) || z.size() != 1)
        throw std::runtime_error("Expected (module (at <x> <y> [<angle>]) (layer <name>))");
    place.x = place.pinref_x = coord[0].Float64();
    place.y = place.pinref_y = coord[1].Float64() * mYInvert;
    if (coord.size() == 3)
        place.angle = coord[2].AngleDeg();
    place.z = mLayerNameToZ.at(unquote(z[0].String()));
    // v = gather(s, "fp_poly");
    // if (!v.empty())
    //    readPoly(v[0]);
    place.image = &mData.images[image.name];
    mData.placements.push_back(place);

    for (const auto &pad : gather(s, "pad"))
        readPad(image, pad, mPadIncludesModuleRotation ? -place.angle : 0);
    *place.image = image;
}
void KiCADParser::readSegment(const std::string &s)
{
    auto start = gather(s, "start");
    auto end = gather(s, "end");
    auto layer = gather(s, "layer");
    auto net = gather(s, "net");
    auto width = gather(s, "width");
    if (start.size() != 1 ||
        end.size() != 1 ||
        layer.size() != 1 ||
        net.size() != 1 ||
        width.size() != 1)
        throw std::runtime_error("Expected (segment (start ...) (end ...) (layer ...) (net ...) (width ...))");
    const auto L = mLayerNameToZ.find(unquote(firstToken(layer[0]).String()));
    if (L == mLayerNameToZ.end())
        return;
    dsn::Segment segment;
    auto v0 = tokenize(start[0]);
    auto v1 = tokenize(end[0]);
    if (v0.size() != 2 || v1.size() != 2)
        throw std::runtime_error("Expected (segment (start/end <x> <y>))");
    segment.z = L->second;
    segment.x0 = v0[0].Float64();
    segment.y0 = v0[1].Float64() * mYInvert;
    segment.x1 = v1[0].Float64();
    segment.y1 = v1[1].Float64() * mYInvert;
    segment.w = firstToken(width[0]).Float64();
    mNets.at(firstToken(net[0]).Int64())->segments.push_back(segment);
}
void KiCADParser::readVia(const std::string &s)
{
    auto at = gather(s, "at");
    auto layers = gather(s, "layers");
    auto size = gather(s, "size");
    auto drill = gather(s, "drill");
    auto net = gather(s, "net");
    if (at.size() != 1 ||
        layers.size() != 1 ||
        size.size() != 1 ||
        drill.size() > 1 ||
        net.size() != 1)
        throw std::runtime_error("Expected (via (at ...) (size ...) [(drill ...)] (layers ...) (net ...))");
    auto v = tokenize(at[0]);
    if (v.size() != 2)
        throw std::runtime_error("Expected (via (at <x> <y>))");
    dsn::Via via;
    readLayerRange(via.z0, via.z1, layers, false);
    via.x = v[0].Float64();
    via.y = v[1].Float64() * mYInvert;
    via.dia = firstToken(size[0]).Float64();
    via.drill = drill.empty() ? 0.0 : firstToken(drill[0]).Float64();
    mNets.at(firstToken(net[0]).Int64())->vias.push_back(via);
}

void KiCADParser::readZone(const std::string &s)
{
    auto net_id = gather(s, "net");
    auto min_thickness = gather(s, "min_thickness");
    if (net_id.size() != 1 ||
        min_thickness.size() != 1)
        throw std::runtime_error("expected single net and min_thickness");
    dsn::Net *net = mNets.at(firstToken(net_id[0]).Int64());
    const auto w = firstToken(min_thickness[0]).Float64();
    if (net->width <= 0.0)
        net->width = w;
}

void KiCADParser::readPCB(const std::string &s)
{
    mData.pcb.unit_ex = -3;
    mData.pcb.resolution = 1;
    mData.pcb.resolution_ex = -6;
    readParser(s);
    readLayers(s);
    readNetNames(s);
    for (const auto &v : gather(s, "net_class"))
        readClass(v);
    for (const auto &v : gather(s, "module"))
        readModule(v);
    for (const auto &v : gather(s, "footprint"))
        readModule(v);
    for (const auto &v : gather(s, "segment"))
        readSegment(v);
    for (const auto &v : gather(s, "via"))
        readVia(v);
    for (const auto &v : gather(s, "zone"))
        readZone(v);
    mData.structure.boundary.z = 0;
    readBoundary(gather(s, "gr_line"));
    readBoundary(gather(s, "gr_arc"));
}
void KiCADParser::readBoundary(const std::vector<std::string> &grs)
{
    for (const auto &gr : grs) {
        auto start = gather(gr, "start");
        if (start.size() != 1)
            throw std::runtime_error("Expected (gr_line/arc (start ...))");
        auto coord = tokenize(start[0]);
        mData.structure.boundary.xy.push_back(coord[0].Float64());
        mData.structure.boundary.xy.push_back(coord[1].Float64() * mYInvert);
    }
}

KiCADParser::KiCADParser(dsn::Data &data) : DSNParserBase(data)
{
    mStringQuote = '"';
    mEscapeChar = '\\';
}
bool KiCADParser::parse(const std::string &text)
{
    const auto v = gather(text, "kicad_pcb");
    if (v.empty()) {
        INFO("Data is not a KiCAD design.");
        return false;
    }
    INFO("Parsing KiCAD design");
    readPCB(v[0]);
    mData.renamePinsWithSharedNames();
    return true;
}
bool dsn::Data::parseKiCAD(dsn::Data &res, const std::string &s)
{
    KiCADParser parser(res);
    return parser.parse(s);
}
