
#include "Loaders/DSNParserBase.hpp"
#include "Log.hpp"

namespace dsn {
struct Parser
{
    char string_quote{'"'};
    bool space_in_quote_tokens{true};
    std::string host_cad;
    std::string host_version;
};
} // namespace dsn

class DSNParser : public DSNParserBase
{
public:
    DSNParser(dsn::Data&);
    bool parse(const std::string&) override;
private:
    dsn::Parser mParser;
    std::map<std::string, uint> mLayerNameToZ;
    std::string fixQuotes(const std::string&);
    void readParser(const std::string&);
    void readStructure(const std::string&);
    void readLayer(const std::string&);
    void readPCB(const std::string&);
    void readNet(const std::string&);
    void readClass(const std::string&);
    void readImage(const std::string&);
    void readPlacement(dsn::Image *, const std::string&);
    void readPadstack(const std::string&);
    dsn::Path readPath(const std::string&);
    dsn::Pin readPin(const std::string&);
    dsn::Shape readShape(const std::string&);
};

void DSNParser::readParser(const std::string &s)
{
    std::vector<std::string> v = gather(s, "string_quote");
    if (!v.empty())
        mParser.string_quote = v[0][0];
    v = gather(s, "host_cad");
    if (!v.empty())
        mParser.host_cad = unquote(v[0]);
    v = gather(s, "host_version");
    if (!v.empty())
        mParser.host_version = unquote(v[0]);
}
void DSNParser::readLayer(const std::string &s)
{
    auto t = tokenize(s);
    if (t.empty())
        throw std::runtime_error("Expected layer name");
    dsn::Layer layer;
    layer.name = unquote(t[0].String());
    layer.side = parseSide(layer.name);
    auto v = gather(s, "type");
    if (!v.empty())
        layer.type = v[0];
    v = gather(s, "property");
    for (const auto &p : v) {
        auto pi = gather(p, "index");
        if (!pi.empty())
            layer.kicad_index = Token(pi[0]).Int64();
    }
    if (layer.kicad_index < 0)
        throw std::runtime_error("Layer index < 0");
    if (mData.structure.layers.size() <= uint(layer.kicad_index))
        mData.structure.layers.resize(layer.kicad_index + 1);
    mData.structure.layers[layer.kicad_index] = layer;
}
dsn::Path DSNParser::readPath(const std::string &s)
{
    dsn::Path path;
    auto t = tokenize(s);
    if (t.size() < 6)
        throw std::runtime_error("Expect >= 2 vertices in path");
    path.z = mLayerNameToZ[unquote(t[0].String())];
    path.aperture_width = t[1].Float64();
    for (uint i = 2; (i + 1) < t.size(); i += 2) {
        path.xy.push_back(t[i].Float64());
        path.xy.push_back(t[i+1].Float64());
    }
    return path;
}
void DSNParser::readStructure(const std::string &s)
{
    auto v = gather(s, "layer");
    for (auto &s : v)
        readLayer(s);
    for (auto &L : mData.structure.layers) {
        if (L.type != "signal" && L.type != "power" && L.type != "ground" && L.type != "mixed")
            continue;
        L.copper_index = mData.structure.copper_layers.size();
        mData.structure.copper_layers.push_back(&L);
        mLayerNameToZ[L.name] = L.copper_index;
    }
    v = gather(s, "boundary");
    if (!v.empty()) {
        v = gather(v[0], "path");
        if (!v.empty())
            mData.structure.boundary = readPath(v[0]);
    }
    for (const auto &vi : gather(s, "rule")) {
        for (const auto &wi : gather(vi, "width"))
            mData.structure.width[""] = Token(wi).Float64();
        for (const auto &ci : gather(vi, "clearance")) {
            auto t = gather(ci, "type");
            mData.structure.clearance[t.empty() ? "" : t[0]] = Token(ci).Float64();
        }
    }
    v = gather(s, "via");
    if (!v.empty())
        mData.structure.via[""] = unquote(v[0]);
}
dsn::Pin DSNParser::readPin(const std::string &s)
{
    // (pin <padstack> [(rotate <degrees>)] <name> <x> <y>)
    dsn::Pin pin;
    auto t = tokenize(s);
    auto v = gather(s, "rotate");
    if (!v.empty()) {
        pin.angle = Token(v[0]).AngleDeg();
        auto next = s.find(")", s.find("(rotate")) + 1;
        for (auto ti : tokenize(s.substr(next)))
            t.push_back(ti);
    }
    if (t.size() != 4)
        throw std::runtime_error("Expected 4 tokens for (pin ...)");
    const auto I = mData.padstacks.find(unquote(t[0].String()));
    if (I == mData.padstacks.end())
        throw std::runtime_error("pin with unknown padstack");
    pin.padstack = &I->second;
    pin.name = unquote(t[1].String());
    pin.x = t[2].Float64();
    pin.y = t[3].Float64();
    return pin;
}
void DSNParser::readImage(const std::string &s)
{
    // (image <name> {(pin ...)})
    dsn::Image image;
    image.name = unquote(firstToken(s).String());
    assert(image.name.size() > 1);
    for (const auto &vi : gather(s, "pin"))
        image.pins.push_back(readPin(vi));
    for (uint i = 0; i < image.pins.size(); ++i)
        image.pins[i].index = i;
    mData.images[image.name] = image;
}
dsn::Shape DSNParser::readShape(const std::string &s)
{
    // (shape (<geometry> <layer> {<x> <y>}))
    dsn::Shape shape;
    const auto t = tokenize(s.substr(1, s.size() - 1));
    if (t.size() < 3)
        throw std::runtime_error("Expected >= 3 shape tokens");
    shape.geometry = t[0].String();
    if (shape.geometry == "path") {
        auto v = gather(s, "path");
        assert(v.size() == 1);
        shape.path = readPath(v[0]);
        shape.z = shape.path.z;
    } else if (shape.geometry == "polygon") {
        shape.z = mLayerNameToZ[unquote(t[1].String())];
        shape.path.aperture_width = t[2].Float64();
        for (uint i = 3; i < t.size(); ++i)
            shape.values.push_back(t[i].Float64());
    } else {
        shape.z = mLayerNameToZ[unquote(t[1].String())];
        for (uint i = 2; i < t.size(); ++i)
            shape.values.push_back(t[i].Float64());
    }
    return shape;
}
void DSNParser::readPadstack(const std::string &s)
{
    // (padstack <name> (shape ...) (attach <bool>))
    dsn::Padstack pad;
    pad.name = unquote(firstToken(s).String());
    auto v = gather(s, "attach");
    if (!v.empty())
        pad.attach = Token(v[0]).Bool();
    for (const auto &vi : gather(s, "shape"))
        pad.shapes.push_back(readShape(vi));
    pad.type = (pad.shapes.size() > 1) ? "thru_hole" : "smd";
    mData.padstacks[pad.name] = pad;
}
void DSNParser::readNet(const std::string &s)
{
    dsn::Net net;
    net.name = unquote(firstToken(s).String());
    net.clearance = mData.structure.clearance[""]; // FIXME
    net.via_diameter = mData.structure.width[""]; // FIXME
    net.width = mData.structure.width[""]; // FIXME
    for (const auto &vi : gather(s, "pins"))
        for (const auto &name : tokenize(vi))
            net.pins.insert(unquotePin(name.String()));
    mData.nets[net.name] = net;
}
void DSNParser::readClass(const std::string &s)
{
    const auto nets = tokenize(s);
    const auto rule = gather(s, "rule");
    const auto circ = gather(s, "circuit");
    double w = 0.0;
    double c = 0.0;
    double d = 0.0;
    if (!rule.empty()) {
        const auto width = gather(rule[0], "width");
        const auto clear = gather(rule[0], "clearance");
        if (!width.empty())
            w = firstToken(width[0]).Float64();
        if (!clear.empty())
            c = firstToken(clear[0]).Float64();
    }
    if (!circ.empty()) {
        const auto use_via = gather(circ[0], "use_via");
        if (!use_via.empty())
            d = mData.padstacks.at(unquote(firstToken(use_via[0]).String())).shapes.at(0).values.at(0);
    }
    for (uint i = 1; i < nets.size(); ++i) {
        if (unquote(nets[i].String()).empty()) // (class kicad_default "") alread set in structure?
            continue;
        dsn::Net &net = mData.nets.at(unquote(nets[i].String()));
        net.klass = nets[0].String();
        if (w > 0.0)
            net.width = w;
        if (c > 0.0)
            net.clearance = c;
        if (d > 0.0)
            net.via_diameter = d;
    }
}
void DSNParser::readPlacement(dsn::Image *image, const std::string &s)
{
    // (place <name> <x> <y> <z> <angle>)
    dsn::Placement place;
    const auto t = tokenize(s);
    if (t.size() != 5)
        throw std::runtime_error("Expect 5 placement tokens");
    assert(image);
    place.image = image;
    place.name = unquote(t[0].String());
    place.x = place.pinref_x = t[1].Float64();
    place.y = place.pinref_y = t[2].Float64();
    place.z = mLayerNameToZ[unquote(t[3].String())];
    place.angle = t[4].AngleDeg();
    mData.placements.push_back(place);
}
std::string DSNParser::fixQuotes(const std::string &s)
{
    // The string quote specification confuses the parser as it looks like an open quote, delete it.
    std::string r = s;
    const auto pos = s.find(fmt::format("(string_quote {})", mStringQuote));
    if (pos != std::string::npos)
        r.replace(pos + 14, 1, " ");
    return r;
}
void DSNParser::readPCB(const std::string &_s)
{
    auto v = gather(_s, "parser");
    if (!v.empty())
        readParser(v[0]);
    mStringQuote = mParser.string_quote;

    const std::string s = fixQuotes(_s);

    mData.pcb.name = unquote(firstToken(s).String());

    v = gather(s, "unit");
    if (!v.empty())
        mData.pcb.unit_ex = unitToExp10(v[0]);

    v = gather(s, "resolution");
    if (!v.empty()) {
        auto t = tokenize(v[0]);
        if (t.size() != 2)
            throw std::runtime_error("Expected resolution(<unit> <amount>)");
        mData.pcb.resolution = t[1].Int64();
        mData.pcb.resolution_ex = unitToExp10(t[0].String());
    }

    v = gather(s, "structure");
    if (!v.empty())
        readStructure(v[0]);

    v = gather(s, "library");
    if (!v.empty()) {
        for (const auto &vi : gather(v[0], "padstack"))
            readPadstack(vi);
        for (const auto &vi : gather(v[0], "image"))
            readImage(vi);
    }

    v = gather(s, "network");
    if (!v.empty()) {
        for (const auto &vi : gather(v[0], "net"))
            readNet(vi);
        for (const auto &vi : gather(v[0], "class"))
            readClass(vi);
    }

    for (const auto &vi : gather(s, "placement")) {
        for (const auto &c : gather(vi, "component")) {
            dsn::Image &I = mData.images.at(unquote(firstToken(c).String()));
            for (const auto &p : gather(c, "place"))
                readPlacement(&I, p);
        }
    }
}

DSNParser::DSNParser(dsn::Data &data) : DSNParserBase(data)
{
    mLayerNameToZ["pcb"] = 0;
}

bool DSNParser::parse(const std::string &text)
{
    const auto v = gather(text, "pcb");
    if (v.empty()) {
        INFO("Data is not a Specctra design");
        return false;
    }
    INFO("Parsing Specctra design.");
    readPCB(v[0]);
    return true;
}

bool dsn::Data::parseDSN(dsn::Data &res, const std::string &s)
{
    DSNParser dsn(res);
    return dsn.parse(s);
}
