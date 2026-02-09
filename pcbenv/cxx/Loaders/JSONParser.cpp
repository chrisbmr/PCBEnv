#ifndef GYM_PCB_LOADERS_JSONPARSER_H
#define GYM_PCB_LOADERS_JSONPARSER_H

#include "Loaders/DSN.hpp"
#include "Log.hpp"
#include "Util/Util.hpp"
#include "UserSettings.hpp"
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

class JSONParser
{
public:
    JSONParser(dsn::Data&);
    bool parse(const std::string&);
protected:
    dsn::Data &mData;
private:
    bool loadBoard(const rapidjson::Value&);
    bool loadComponent(const rapidjson::Value&, const std::string &name);
    bool loadPin(dsn::Image&, const rapidjson::Value&, const std::string &name, const dsn::Placement&);
    bool loadNet(const rapidjson::Value&, const std::string &name);
    void loadPath(dsn::Net&, const rapidjson::Value&);
    void loadConnection(dsn::Connection&, const rapidjson::Value&);
    void loadTrack(dsn::Track&, const rapidjson::Value&);
    bool loadShape(dsn::Shape&, const rapidjson::Value&);
    bool loadShape(dsn::Padstack&, const rapidjson::Value&);
    void moveToCenter(double &x, double &y, const dsn::Shape&, double anchor_x, double anchor_y);
    bool loadStructure(const rapidjson::Value&);
    void setBoundaryFromRect(double x0, double y0, double x1, double y1);
    bool loadBoundaryFromSize(const rapidjson::Value&);
    bool loadBoundaryFromArray(const rapidjson::Value&);
    void loadPoint(double &x, double &y, int &z, const rapidjson::Value&, uint index = 0);
    void setPointers();
    void loadSchema();
    std::string getParseError(const char *info, const std::string &data, const rapidjson::Document&);
    std::string getSchemaError(rapidjson::SchemaValidator&);
    std::unique_ptr<rapidjson::SchemaDocument> mSchemaDoc;
    dsn::Shape mUnitRect;
    bool mPinsRelative{false};
};

bool dsn::Data::parseJSON(dsn::Data &res, const std::string &s)
{
    JSONParser parser(res);
    return parser.parse(s);
}

void JSONParser::loadPoint(double &x, double &y, int &z, const rapidjson::Value &v, uint index)
{
    if (!v.IsArray())
        throw std::invalid_argument("expected an array");
    const auto &A = v.GetArray();
    if (A.Size() != 3)
        throw std::invalid_argument("expected an array with 3 items");
    x = A[index+0].GetDouble();
    y = A[index+1].GetDouble();
    z = A[index+2].GetInt();
    if (z == -1)
        z = mData.structure.layers.size() - 1;
}

std::string JSONParser::getParseError(const char *m, const std::string &s, const rapidjson::Document &d)
{
    const auto pos = d.GetErrorOffset();
    uint l = 0;
    uint c = 0;
    for (uint i = 0; i < pos; ++i, ++c) {
        if (s[i] == '\n') {
            l++;
            c = 0;
        }
    }
    return std::string(m) + " at L" + std::to_string(l) + "/C" + std::to_string(c) + ": " + rapidjson::GetParseError_En(d.GetParseError());
}

JSONParser::JSONParser(dsn::Data &data) : mData(data)
{
    loadSchema();

    mUnitRect.geometry = "rect";
    mUnitRect.values.push_back(-0.5);
    mUnitRect.values.push_back(-0.5);
    mUnitRect.values.push_back(0.5);
    mUnitRect.values.push_back(0.5);
    mUnitRect.z = mUnitRect.path.z = 0;
    mUnitRect.path.aperture_width = 0.0;
}

void JSONParser::loadSchema()
{
    rapidjson::Document doc;
    std::string s = util::loadFile(UserSettings::get().Paths.JSON + "pcb.schema.json");
    if (s.empty())
        throw std::runtime_error("Could not find board file JSON schema");
    if (doc.Parse(s.c_str()).HasParseError())
        throw std::runtime_error(getParseError("Board file JSON schema has errors", s, doc));
    mSchemaDoc = std::make_unique<rapidjson::SchemaDocument>(doc);
}

std::string JSONParser::getSchemaError(rapidjson::SchemaValidator &V)
{
    rapidjson::StringBuffer buf;
    V.GetInvalidSchemaPointer().StringifyUriFragment(buf);
    std::stringstream ss;
    ss << "Board JSON data has errors:" << std::endl;
    ss << "Invalid schema: " << buf.GetString() << std::endl;
    ss << "Invalid keyword: " << V.GetInvalidSchemaKeyword() << std::endl;
    buf.Clear();
    V.GetInvalidDocumentPointer().StringifyUriFragment(buf);
    ss << "Invalid document: " << buf.GetString();
    return ss.str();
}

bool JSONParser::parse(const std::string &s)
{
    rapidjson::Document doc;
    doc.Parse<rapidjson::kParseNanAndInfFlag>(s.c_str());
    if (doc.HasParseError())
        RETURN_INFO(false, getParseError("Board is not a valid JSON file", s, doc));
    rapidjson::SchemaValidator V(*mSchemaDoc);
    if (!doc.Accept(V))
        RETURN_ERROR(false, getSchemaError(V));
    return loadBoard(doc);
}

void JSONParser::setPointers()
{
    TRACE("Finalizing DSN");
    for (auto &L : mData.structure.layers)
        mData.structure.copper_layers.push_back(&L);
    for (auto &C : mData.placements)
        C.image = &mData.images.at(C.name);
    for (auto &I : mData.images) {
        for (auto &P : I.second.pins)
            P.padstack = &mData.padstacks.at(I.second.name + '-' + P.name);
    }
}

void JSONParser::moveToCenter(double &x, double &y, const dsn::Shape &shape, double anchor_x, double anchor_y)
{
    if (anchor_x == 0.5 && anchor_y == 0.5)
        return;
    if (shape.geometry == "circle") {
        x += shape.values[0] * (0.5 - anchor_x);
        y += shape.values[0] * (0.5 - anchor_y);
    } else if (shape.geometry == "rect") {
        x += (shape.values[2] - shape.values[0]) * (0.5 - anchor_x);
        y += (shape.values[3] - shape.values[1]) * (0.5 - anchor_y);
    } else if (shape.geometry == "path") {
        if (shape.path.xy.size() != 4)
            throw std::runtime_error("don't know how to center path that is not a single segment");
        x += (shape.path.xy[2] - shape.path.xy[0]) * (0.5 - anchor_x);
        y += (shape.path.xy[3] - shape.path.xy[1]) * (0.5 - anchor_y);
    } else if (shape.geometry == "polygon") {
        throw std::runtime_error("using non-central anchor point with polygons is not supported");
    }
}

bool JSONParser::loadShape(dsn::Shape &shape, const rapidjson::Value &v)
{
    const auto &A = v.GetArray();
    shape.geometry = A[0].GetString();
    shape.z = shape.path.z = 0;
    shape.path.aperture_width = 0.0;
    if (shape.geometry == "circle") {
        assert(A.Size() >= 2);
        shape.values.push_back(A[1].GetDouble() * 2.0); // radius -> diameter
    } else if (shape.geometry == "rect_iso") {
        double w2, h2;
        if (A.Size() == 3) {
            w2 = A[1].GetDouble() * 0.5;
            h2 = A[2].GetDouble() * 0.5;
        } else {
            assert(A.Size() == 5);
            w2 = (A[3].GetDouble() - A[1].GetDouble()) * 0.5;
            h2 = (A[4].GetDouble() - A[2].GetDouble()) * 0.5;
        }
        shape.values.push_back(-w2);
        shape.values.push_back(-h2);
        shape.values.push_back(w2);
        shape.values.push_back(h2);
        shape.geometry = "rect";
    } else if (shape.geometry == "wide_segment") {
        const auto x0 = A[1].GetDouble();
        const auto y0 = A[2].GetDouble();
        const auto x1 = A[3].GetDouble();
        const auto y1 = A[4].GetDouble();
        const auto cx = (x0 + x1) * 0.5;
        const auto cy = (y0 + y1) * 0.5;
        shape.path.xy.push_back(x0 - cx);
        shape.path.xy.push_back(y0 - cy);
        shape.path.xy.push_back(x1 - cx);
        shape.path.xy.push_back(y1 - cy);
        shape.path.z = A[5].GetInt();
        shape.path.aperture_width = A[6].GetDouble();
        shape.geometry = "path";
    } else {
        assert(shape.geometry == "polygon");
        if (!A[1].IsArray())
            throw std::runtime_error("expected [\"polygon\", [coordinates array]]");
        auto V = A[1].GetArray();
        if (V.Size() & 1)
            throw std::runtime_error("coordinates array has an odd number of values");
        for (uint i = 0; i < V.Size(); ++i) {
            shape.values.push_back(V[i].GetArray()[0].GetDouble());
            shape.values.push_back(V[i].GetArray()[1].GetDouble());
        }
        shape.recenter();
    }
    return true;
}
bool JSONParser::loadShape(dsn::Padstack &pad, const rapidjson::Value &v)
{
    dsn::Shape shape;
    if (!loadShape(shape, v))
        return false;
    pad.shapes.push_back(std::move(shape));
    return true;
}

bool JSONParser::loadPin(dsn::Image &I, const rapidjson::Value &v, const std::string &name, const dsn::Placement &C)
{
    dsn::Pin pin;
    dsn::Padstack padstack;
    pin.name = name;
    pin.index = I.pins.size();
    DEBUG("Loading pin " << pin.index << ": " << pin.name);

    const auto pos_key = v.HasMember("pos") ? "pos" : "center";
    pin.x = v[pos_key].GetArray()[0].GetDouble();
    pin.y = v[pos_key].GetArray()[1].GetDouble();
    if (!mPinsRelative) {
        pin.x -= C.pinref_x; // pin.x,y must be relative
        pin.y -= C.pinref_y;
    }
    if (v.HasMember("z")) {
        if (v["z"].IsArray()) {
            pin.z0 = v["z"].GetArray()[0].GetInt();
            pin.z1 = v["z"].GetArray()[1].GetInt();
            if (pin.z0 > pin.z1)
                std::swap(pin.z0, pin.z1);
        } else {
            pin.z0 = pin.z1 = v["z"].GetUint();
        }
    } else {
        pin.z0 = pin.z1 = C.z;
    }
    if (pin.z0 < 0 || pin.z1 >= (int)mData.structure.layers.size())
        RETURN_ERROR(false, "Pin " << I.name << '-' << pin.name << " has invalid layer range: " << pin.z0 << ',' << pin.z1);
    if (v.HasMember("clearance"))
        pin.clearance = v["clearance"].GetDouble();

    if (v.HasMember("compound") && v["compound"].IsArray() && v["compound"].GetArray().Size()) {
        auto A = v["compound"].GetArray();
        pin.compound = pin.name;
        for (uint i = 0; i < A.Size(); ++i) {
            std::string s = std::string(A[i].GetString());
            if (s < pin.compound.value())
                pin.compound = std::move(s);
        }
    }

    padstack.name = I.name + "-" + pin.name;
    padstack.type = (pin.z0 != pin.z1) ? "thru_hole" : "smd";
    padstack.attach = false;
    if (!v.HasMember("shape"))
        padstack.shapes.push_back(mUnitRect);
    else if (!loadShape(padstack, v["shape"]))
        return false;
    assert(!padstack.shapes.empty());
    for (auto &shape : padstack.shapes)
        shape.z = shape.path.z = pin.z0;
    const auto anchor_x = v.HasMember("pos") ? 0.0 : 0.5;
    const auto anchor_y = anchor_x;
    moveToCenter(pin.x, pin.y, padstack.shapes[0], anchor_x, anchor_y);
    mData.padstacks[padstack.name] = std::move(padstack);

    I.pins.push_back(std::move(pin));
    return true;
}

bool JSONParser::loadComponent(const rapidjson::Value &v, const std::string &name)
{
    dsn::Placement C;
    dsn::Image I;
    C.name = name;
    DEBUG("Loading component " << C.name);
    const auto pos_key = v.HasMember("pos") ? "pos" : "center";
    C.x = C.pinref_x = v[pos_key].GetArray()[0].GetDouble();
    C.y = C.pinref_y = v[pos_key].GetArray()[1].GetDouble();
    C.z = v.HasMember("z") ? v["z"].GetInt() : 0;
    if (v.HasMember("clearance"))
        C.clearance = v["clearance"].GetDouble();
    if (v.HasMember("via_ok"))
        C.via_ok = v["via_ok"].GetBool();
    if (v.HasMember("can_route"))
        C.can_route = v["can_route"].GetBool();
    if (C.z == -1)
        C.z = mData.structure.layers.size() - 1;
    if (C.z < 0 || C.z >= (int)mData.structure.layers.size())
        RETURN_ERROR(false, "Component placed on non-existent layer");
    if (v.HasMember("shape")) {
        if (!loadShape(I.footprint, v["shape"]))
            return false;
        const auto anchor_x = v.HasMember("pos") ? 0.0 : 0.5;
        const auto anchor_y = anchor_x;
        moveToCenter(C.x, C.y, I.footprint, anchor_x, anchor_y);
    }
    I.name = C.name;
    if (v.HasMember("pins"))
        for (auto m = v["pins"].MemberBegin(); m != v["pins"].MemberEnd(); ++m)
            if (!loadPin(I, m->value, m->name.GetString(), C))
                return false;
    mData.placements.push_back(std::move(C));
    mData.images[I.name] = std::move(I);
    return true;
}

void JSONParser::setBoundaryFromRect(double x0, double y0, double x1, double y1)
{
    mData.structure.boundary.xy.push_back(x0);
    mData.structure.boundary.xy.push_back(y0);
    mData.structure.boundary.xy.push_back(x0);
    mData.structure.boundary.xy.push_back(y1);
    mData.structure.boundary.xy.push_back(x1);
    mData.structure.boundary.xy.push_back(y1);
    mData.structure.boundary.xy.push_back(x1);
    mData.structure.boundary.xy.push_back(y0);
}
bool JSONParser::loadBoundaryFromSize(const rapidjson::Value &v)
{
    double x0 = v.HasMember("origin") ? v["origin"].GetArray()[0].GetDouble() : 0.0;
    double y0 = v.HasMember("origin") ? v["origin"].GetArray()[1].GetDouble() : 0.0;
    double x1 = x0 + v["size"].GetArray()[0].GetDouble();
    double y1 = y0 + v["size"].GetArray()[1].GetDouble();
    setBoundaryFromRect(x0, y0, x1, y1);
    return true;
}
bool JSONParser::loadBoundaryFromArray(const rapidjson::Value &v)
{
    auto A = v.GetArray();
    auto N = A.Size();
    if (N == 4) {
        setBoundaryFromRect(A[0].GetDouble(), A[1].GetDouble(), A[2].GetDouble(), A[3].GetDouble());
    } else if ((N % 2) == 0) {
        for (uint i = 0; i < N; ++i)
            mData.structure.boundary.xy.push_back(A[i].GetDouble());
    } else {
        return false;
    }
    return true;
}

bool JSONParser::loadStructure(const rapidjson::Value &v)
{
    auto &L = mData.structure.layers;
    if (v.HasMember("layers"))
        L.resize(v["layers"].IsArray() ? v["layers"].GetArray().Size() : v["layers"].GetUint());
    else
        L.resize(1);
    for (uint i = 0; i < L.size(); ++i) {
        L[i].name = std::to_string(i);
        L[i].type = "signal";
        L[i].copper_index = i;
        L[i].kicad_index = i;
        L[i].side = 'm';
    }
    if (v.HasMember("layers") && v["layers"].IsArray()) {
        for (uint i = 0; i < L.size(); ++i)
            L[i].type = v["layers"].GetArray()[i]["type"].GetString();
    }
    if (!L.empty()) {
        L.back().side = 'b';
        L.front().side = 't';
    }

    DEBUG("Loading board size");
    mData.structure.boundary.aperture_width = 0.0;
    mData.structure.boundary.z = 0;
    if (v.HasMember("layout_area"))
        return loadBoundaryFromArray(v["layout_area"]);
    return loadBoundaryFromSize(v);
}

void JSONParser::loadPath(dsn::Net &net, const rapidjson::Value &v)
{
    DEBUG("Loading path for net " << net.name);
    const auto &A = v.GetArray();
    assert(A.Size() >= 2);
    double x, y;
    int z;
    loadPoint(x, y, z, A[0]);
    for (uint i = 1; i < A.Size(); ++i) {
        dsn::Segment s;
        s.x0 = x;
        s.y0 = y;
        s.z = z;
        loadPoint(x, y, z, A[i]);
        s.x1 = x;
        s.y1 = y;
        s.w = net.width;
        if (std::min(z, s.z) < 0 || std::max(z, s.z) >= (int)mData.structure.layers.size())
            throw std::runtime_error("invalid layers for track segment");
        if (s.z != z) {
            dsn::Via via;
            via.dia = via.drill = net.via_diameter;
            via.x = s.x1;
            via.y = s.y1;
            via.z0 = s.z;
            via.z1 = z;
            net.vias.push_back(via);
        }
        if (s.x0 != s.x1 || s.y0 != s.y1)
            net.segments.push_back(s);
    }
}

void JSONParser::loadTrack(dsn::Track &T, const rapidjson::Value &v)
{
    loadPoint(T.start_x, T.start_y, T.start_z, v["start"]);
    loadPoint(T.end_x, T.end_y, T.end_z, v["end"]);

    if (v.HasMember("segments")) {
        const auto &A = v["segments"].GetArray();
        for (uint i = 0; i < A.Size(); ++i) {
            const auto &S = A[i];
            dsn::Segment s;
            s.x0 = S[0].GetDouble();
            s.y0 = S[1].GetDouble();
            s.x1 = S[2].GetDouble();
            s.y1 = S[3].GetDouble();
            s.z  = S[4].GetInt();
            s.w  = S[5].GetDouble();
            if (s.z == -1)
                s.z = mData.structure.layers.size() - 1;
            T.segments.push_back(s);
        }
    }

    if (v.HasMember("vias")) {
        const auto &A = v["vias"].GetArray();
        for (uint i = 0; i < A.Size(); ++i) {
            const auto &V = A[i];
            dsn::Via via;
            via.dia = via.drill = V["radius"].GetDouble() * 2.0;
            via.x = V["pos"].GetArray()[0].GetDouble();
            via.y = V["pos"].GetArray()[1].GetDouble();
            via.z0 = V["min_layer"].GetInt();
            via.z1 = V["max_layer"].GetInt();
            if (via.z1 == -1)
                via.z1 = mData.structure.layers.size() - 1;
            assert(via.z0 < via.z1);
            T.vias.push_back(via);
        }
    }

    T.default_width = v["width"].GetDouble();
    T.default_via_diameter = v["via_diam"].GetDouble();
}

void JSONParser::loadConnection(dsn::Connection &X, const rapidjson::Value &v)
{
    X.has_src_pin = v.HasMember("src_pin") && v["src_pin"].IsString();
    X.has_dst_pin = v.HasMember("dst_pin") && v["dst_pin"].IsString();
    if (X.has_src_pin)
        X.src_pin = v["src_pin"].GetString();
    if (X.has_dst_pin)
        X.dst_pin = v["dst_pin"].GetString();

    X.has_src = v.HasMember("src");
    X.has_dst = v.HasMember("dst");
    if (X.has_src)
        loadPoint(X.x0, X.y0, X.z0, v["src"]);
    if (X.has_dst)
        loadPoint(X.x1, X.y1, X.z1, v["dst"]);

    X.clearance = v.HasMember("clearance") ? v["clearance"].GetDouble() : -1.0;

    if (v.HasMember("tracks")) {
        const auto &A = v["tracks"].GetArray();
        for (uint t = 0; t < A.Size(); ++t) {
            dsn::Track track;
            loadTrack(track, A[t]);
            X.tracks.push_back(std::move(track));
        }
    }

    if (v.HasMember("min_len"))
        X.ref_len = v["min_len"].GetDouble();
}

bool JSONParser::loadNet(const rapidjson::Value &v, const std::string &name)
{
    dsn::Net net;
    net.name = name;
    DEBUG("Loading net " << net.name);
    net.width = v.HasMember("track_width") ? v["track_width"].GetDouble() : 1.0;
    net.via_diameter = v.HasMember("via_diameter") ? v["via_diameter"].GetDouble() : net.width;
    net.clearance = v.HasMember("clearance") ? v["clearance"].GetDouble() : net.width;
    if (mData.nets.find(net.name) != mData.nets.end())
        RETURN_ERROR(false, "Duplicate net name found: " << net.name);
    if (v.HasMember("pins"))
        for (uint i = 0; i < v["pins"].GetArray().Size(); ++i)
            net.pins.insert(v["pins"].GetArray()[i].GetString());
    if (v.HasMember("paths"))
        for (uint i = 0; i < v["paths"].GetArray().Size(); ++i)
            loadPath(net, v["paths"].GetArray()[i]);
    if (v.HasMember("connections")) {
        for (uint i = 0; i < v["connections"].GetArray().Size(); ++i) {
            dsn::Connection X;
            loadConnection(X, v["connections"].GetArray()[i]);
            if (X.clearance < 0.0)
                X.clearance = net.clearance;
            if (X.has_src_pin)
                net.pins.insert(X.src_pin);
            if (X.has_dst_pin)
                net.pins.insert(X.dst_pin);
            net.connections.push_back(std::move(X));
        }
    }
    mData.nets[net.name] = std::move(net);
    return true;
}

bool JSONParser::loadBoard(const rapidjson::Value &v)
{
    mData.pcb.name = v.HasMember("name") ? v["name"].GetString() : "Gym Board";
    if (v.HasMember("resolution")) {
        mData.pcb.resolution = v["resolution"].GetArray()[0].GetInt();
        mData.pcb.resolution_ex = v["resolution"].GetArray()[1].GetInt();
    } else if (v.HasMember("resolution_nm")) {
        mData.pcb.resolution = v["resolution_nm"].GetDouble();
        mData.pcb.resolution_ex = -9;
    } else {
        mData.pcb.resolution = v.HasMember("unit") ? v["unit"].GetArray()[0].GetInt() : 1;
        mData.pcb.resolution_ex = v.HasMember("unit") ? v["unit"].GetArray()[1].GetInt() : -6;
    }
    if (v.HasMember("unit")) {
        mData.pcb.unit = v["unit"].GetArray()[0].GetInt();
        mData.pcb.unit_ex = v["unit"].GetArray()[1].GetInt();
    } else if (v.HasMember("unit_nm")) {
        mData.pcb.unit = v["unit_nm"].GetDouble();
        mData.pcb.unit_ex = -9;
    } else {
        mData.pcb.unit = mData.pcb.resolution;
        mData.pcb.unit_ex = mData.pcb.resolution_ex;
    }
    mPinsRelative = false;
    if (v.HasMember("pin_positions"))
        mPinsRelative = std::string_view(v["pin_positions"].GetString()) == "relative";
    if (!loadStructure(v))
        return false;
    if (v.HasMember("components"))
        for (auto m = v["components"].MemberBegin(); m != v["components"].MemberEnd(); ++m)
            if (!loadComponent(m->value, m->name.GetString()))
                return false;
    if (v.HasMember("nets"))
        for (auto m = v["nets"].MemberBegin(); m != v["nets"].MemberEnd(); ++m)
            if (!loadNet(m->value, m->name.GetString()))
                return false;
    setPointers();
    return true;
}

#endif // GYM_PCB_LOADERS_JSONPARSER_H
