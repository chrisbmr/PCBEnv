
#ifndef GYM_PCB_DEFS_H
#define GYM_PCB_DEFS_H

#include <cstdint>

#ifdef GYM_PCB_NO_STDFORMAT
#include <fmt/format.h>
#else
#include <format>
namespace fmt { using std::format; }
#endif

using Real = double;
using uint = unsigned int;

#define std_08x std::hex << std::setw(8) << std::setfill('0')

#define PEDANTIC(...) args

#define WITH_LOCK(a, b) do { std::lock_guard _lock((a)->getLock()); b; } while(0)

#define WITH_RLOCK(a, b) do { std::shared_lock _lock((a)->getLock()); b; } while(0)
#define WITH_WLOCK(a, b) do { std::unique_lock _lock((a)->getLock()); b; } while(0)

#endif // GYM_PCB_DEFS_H
