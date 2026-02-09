
#ifndef GYM_PCB_USERPARAMETERS_H
#define GYM_PCB_USERPARAMETERS_H

#include "Defs.hpp"
#include <string>
#include <chrono>
#include <thread>

class UserSettings
{
public:
    static const UserSettings& get() { return sInstance; }

    uint32_t MaxGridCells{1u << 26};
    uint64_t AgentTimeoutUSecs{0};
    float AStarViaCostFactor{1.0f};
    struct {
        uint WindowSize[2]{1024, 768};
        float PointSize{1.0f};
        uint FPS{40};
        uint SidePaneWidth{256};
        uint LockSteppingGranularity{0};
        uint ActionDelayUSecs{0};
        bool EnableRendering{true};
        bool VSync{true};
        bool GridLinesVisible{false};
        bool GridPointsVisible{false};
        bool FootprintsVisible{false};
        bool RatsNestVisible{false};
        bool TriangulationVisible{false};
        bool RoutePreview{true};
    } UI;
    struct {
        float PerVia{0.0f};
        float PerDisconnect{-5.0f};
        float AnyDisconnect{0.0f};
    } Reward;
    struct {
        std::string GLSL;
        std::string JSON;
        std::string QTUI;
    } Paths;
    static void LoadFile(const std::string &path);
    static void LoadJSON(const std::string &json) { sInstance.loadJSON(json); }
    void AddPath(const std::string_view &s);

    void sleepForActionDelay(uint times = 1) const;

    std::string str() const;

    static UserSettings& edit() { return sInstance; }
private:
    UserSettings() { }
private:
    void loadJSON(const std::string &json);

    static UserSettings sInstance;
};

inline void UserSettings::sleepForActionDelay(uint times) const
{
#ifdef GYM_PCB_ENABLE_UI
    if (UI.ActionDelayUSecs)
        std::this_thread::sleep_for(std::chrono::microseconds(uint64_t(UI.ActionDelayUSecs) * times));
#endif
}

#endif // GYM_PCB_USERPARAMETERS_H
