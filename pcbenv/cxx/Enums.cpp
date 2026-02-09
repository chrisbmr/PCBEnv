
#include "Enums.hpp"

SignalType signalType(const std::string_view s)
{
    uint rv = SignalType::USER;
    if (s.find("signal") != s.npos) rv |= SignalType::SIGNAL;
    if (s.find("power") != s.npos) rv |= SignalType::POWER;
    if (s.find("ground") != s.npos) rv |= SignalType::GROUND;
    if (s.find("any") != s.npos ||
        s.find("all") != s.npos)
        return SignalType::ANY;
    return SignalType(rv);
}

std::string to_string(SignalType t)
{
    static const char *_s[8] =
    {
        "",
        "signal",
        "power",
        "signal|power",
        "ground",
        "signal|ground",
        "ground|power",
        "any"
    };
    if (uint(t) >= 8)
        throw std::invalid_argument("invalid SignalType value");
    return _s[uint(t)];
}
