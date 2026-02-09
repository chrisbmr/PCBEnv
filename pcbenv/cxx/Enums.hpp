#ifndef GYM_PCB_ENUMS_H
#define GYM_PCB_ENUMS_H

enum SignalType
{
    USER   = 0x0,
    SIGNAL = 0x1,
    POWER  = 0x2,
    GROUND = 0x4,
    SIGNAL_POWER  = 0x3,
    SIGNAL_GROUND = 0x5,
    POWER_GROUND  = 0x6,
    ANY = 0x7
};
SignalType signalType(const std::string_view);

std::string to_string(SignalType);

#endif // GYM_PCB_ENUMS_H
