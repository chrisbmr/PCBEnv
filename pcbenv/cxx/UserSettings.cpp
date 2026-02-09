
#include "UserSettings.hpp"
#include "Color.hpp"
#include "Log.hpp"
#include "Util/Util.hpp"
#include "NavPoint.hpp"
#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

UserSettings UserSettings::sInstance;

void UserSettings::LoadFile(const std::string &filePath)
{
    INFO("Loading user settings from " << filePath);
    try {
        sInstance.LoadJSON(util::loadFile(filePath));
    } catch (std::exception &e) {
        WARN(e.what());
    }
}

void UserSettings::loadJSON(const std::string &data)
{
    if (data.empty())
        throw std::runtime_error("User settings JSON is empty.");

    std::string schemaData = util::loadFile(Paths.JSON + "settings.schema.json");
    if (schemaData.empty())
        throw std::runtime_error("User settings JSON schema is empty.");
    rapidjson::Document doc;
    if (doc.Parse(schemaData.c_str()).HasParseError())
        throw std::runtime_error("User settings JSON schema has errors.");
    rapidjson::SchemaDocument schema(doc);

    if (doc.Parse(data.c_str()).HasParseError())
        throw std::runtime_error("User settings JSON has errors.");

    rapidjson::SchemaValidator validator(schema);
    if (!doc.Accept(validator)) {
        rapidjson::StringBuffer buf;
        validator.GetInvalidSchemaPointer().StringifyUriFragment(buf);
        ERROR("Invalid schema: " << buf.GetString());
        ERROR("Invalid keyword: " << validator.GetInvalidSchemaKeyword());
        buf.Clear();
        validator.GetInvalidDocumentPointer().StringifyUriFragment(buf);
        ERROR("Invalid document: " << buf.GetString());
        throw std::runtime_error("Settings JSON violates schema.");
    }

    if (doc.HasMember("Paths"))
        for (auto &dir : doc["Paths"].GetArray())
            AddPath(dir.GetString());

    uint64_t maxCells = std::numeric_limits<uint64_t>::max();
    if (doc.HasMember("MaxGridSize_Cells"))
        maxCells = doc["MaxGridSize_Cells"].GetUint64();
    if (doc.HasMember("MaxGridSize_MiB"))
        maxCells = std::min(maxCells, (doc["MaxGridSize_MiB"].GetUint64() << 20) / sizeof(NavPoint));
    if (maxCells <= std::numeric_limits<int32_t>::max())
        MaxGridCells = maxCells;

    if (doc.HasMember("AStarViaCostFactor"))
        AStarViaCostFactor = doc["AStarViaCostFactor"].GetFloat();

    if (doc.HasMember("AgentTimeoutSeconds"))
        AgentTimeoutUSecs = doc["AgentTimeoutSeconds"].GetUint64() * 1000000;

    if (doc.HasMember("Reward"))
    {
        const auto &R = doc["Reward"];
        if (R.HasMember("PerDisconnect"))
            Reward.PerDisconnect = R["PerDisconnect"].GetFloat();
        if (R.HasMember("AnyDisconnect"))
            Reward.AnyDisconnect = R["AnyDisconnect"].GetFloat();
        if (R.HasMember("PerVia"))
            Reward.PerVia = R["PerVia"].GetFloat();
    }

#ifdef GYM_PCB_ENABLE_UI

    if (doc.HasMember("UserInterface"))
    {
        const auto &J = doc["UserInterface"];
        if (J.HasMember("Enable"))
            UI.EnableRendering = J["Enable"].GetBool();
        if (J.HasMember("WindowSize")) {
            const auto &A = J["WindowSize"].GetArray();
            UI.WindowSize[0] = A[0].GetUint();
            UI.WindowSize[1] = A[1].GetUint();
        }
        if (J.HasMember("VSync"))
            UI.VSync = J["VSync"].GetBool();
        if (J.HasMember("FPS"))
            UI.FPS = J["FPS"].GetUint();
        if (J.HasMember("PaneWidth"))
            UI.SidePaneWidth = J["PaneWidth"].GetUint();
        if (J.HasMember("PointSize"))
            UI.PointSize = J["PointSize"].GetFloat();
        if (J.HasMember("Slowdown"))
            UI.ActionDelayUSecs = J["Slowdown"].GetUint();
        if (J.HasMember("LockSteppingGranularity"))
            UI.LockSteppingGranularity = J["LockSteppingGranularity"].GetUint();
        if (J.HasMember("VisibleElements")) {
            for (auto &E : J["VisibleElements"].GetArray()) {
                std::string_view s(E.GetString());
                assert(!s.empty());
                const auto b = s[0] != '!';
                if (!b)
                    s = s.substr(1);
                if (s == "GridLines") UI.GridLinesVisible = b; else
                if (s == "GridPoints") UI.GridPointsVisible = b; else
                if (s == "Triangulation") UI.TriangulationVisible = b; else
                if (s == "RatsNest") UI.RatsNestVisible = b; else
                if (s == "Footprints") UI.FootprintsVisible = b; else
                if (s == "RoutePreview") UI.RoutePreview = b;
            }
        }
    }

    if (doc.HasMember("Colors"))
    {
        const auto &J = doc["Colors"];
        if (J.HasMember("Components")) {
            Palette::FootprintLine[0] = Color(J["Components"][0]["line"].GetString());
            Palette::FootprintLine[1] = Color(J["Components"][1]["line"].GetString());
            Palette::FootprintFill[0] = Color(J["Components"][0]["fill"].GetString());
            Palette::FootprintFill[1] = Color(J["Components"][1]["fill"].GetString());
        }
        if (J.HasMember("Pins-Net")) {
            Palette::PinLine[0][0] = Color(J["Pins-Net"][0]["line"].GetString());
            Palette::PinLine[0][1] = Color(J["Pins-Net"][1]["line"].GetString());
            Palette::PinFill[0][0] = Color(J["Pins-Net"][0]["fill"].GetString());
            Palette::PinFill[0][1] = Color(J["Pins-Net"][1]["fill"].GetString());
        }
        if (J.HasMember("Pins+Net")) {
            Palette::PinLine[1][0] = Color(J["Pins+Net"][0]["line"].GetString());
            Palette::PinLine[1][1] = Color(J["Pins+Net"][1]["line"].GetString());
            Palette::PinFill[1][0] = Color(J["Pins+Net"][0]["fill"].GetString());
            Palette::PinFill[1][1] = Color(J["Pins+Net"][1]["fill"].GetString());
        }
        if (J.HasMember("PowerNets"))
            Palette::PowerNet = Color(J["PowerNets"].GetString());
        if (J.HasMember("GroundNets"))
            Palette::GroundNet = Color(J["GroundNets"].GetString());
        if (J.HasMember("Nets"))
            for (uint i = 0; i < J["Nets"].GetArray().Size(); ++i)
                Palette::Nets.setColor(i, Color(J["Nets"][i].GetString()));
        if (J.HasMember("Tracks"))
            for (uint i = 0; i < J["Tracks"].GetArray().Size(); ++i)
                Palette::Tracks.setColor(i, Color(J["Tracks"][i].GetString()));
    }

#else // !GYM_PCB_ENABLE_UI

    UI.EnableRendering = false;

#endif // GYM_PCB_ENABLE_UI

    if (doc.HasMember("LogFiles")) {
        for (uint i = 0; i < Logger::NumLevels; ++i) {
            std::string c(1, Logger::levelChar(LogLevel(i)));
            if (doc["LogFiles"].HasMember(c.c_str()))
                Logger::get().setTarget(LogLevel(i), doc["LogFiles"][c.c_str()].GetString());
        }
    }
    if (doc.HasMember("LogLevel"))
        Logger::get().setDefaultLevel(doc["LogLevel"].GetString());

    INFO("User settings loaded:" << std::endl << str());
}

void UserSettings::AddPath(const std::string_view &s)
{
    INFO("Added to paths: " << s);
    if (s.ends_with("*.json")) Paths.JSON = s.substr(0, s.size() - 6); else
    if (s.ends_with("*.glsl")) Paths.GLSL = s.substr(0, s.size() - 6); else
    if (s.ends_with("*.ui")) Paths.QTUI = s.substr(0, s.size() - 4);
}

std::string UserSettings::str() const
{
    std::stringstream ss;
    ss << "* Max grid cells: " << MaxGridCells << std::endl;
    ss << "* Agent timeout: " << Logger::formatDurationUS(AgentTimeoutUSecs) << std::endl;
    ss << "* A-star via cost factor: " << AStarViaCostFactor << std::endl;
    ss << "* Window size: " << UI.WindowSize[0] << 'x' << UI.WindowSize[1] << std::endl;
    ss << "* Point size: " << UI.PointSize << std::endl;
    ss << "* Side pane width: " << UI.SidePaneWidth << std::endl;
    ss << "* Action delay: " << Logger::formatDurationUS(UI.ActionDelayUSecs) << std::endl;
    ss << "* Visible elements:";
    if (UI.GridLinesVisible)
        ss << " grid-lines";
    if (UI.GridPointsVisible)
        ss << " grid-points";
    if (UI.FootprintsVisible)
        ss << " footprints";
    if (UI.RatsNestVisible)
        ss << " rat'snest";
    if (UI.TriangulationVisible)
        ss << " triangles";
    ss << std::endl;
    ss << "* VSync: " << UI.VSync << std::endl;
    ss << "* Reward: PerDisconnect(" << Reward.PerDisconnect << ") + Via(" << Reward.PerVia << ") + AnyDisconnect(" << Reward.AnyDisconnect << ')';
    return ss.str();
}
