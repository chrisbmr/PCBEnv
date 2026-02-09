#ifndef GYM_PCB_UTIL_FANCYLOCK_H
#define GYM_PCB_UTIL_FANCYLOCK_H

#include <mutex>

// I really want to be able to write WITH_LOCK(...) { }

struct fancy_lock_guard
{
    fancy_lock_guard(std::mutex &mutex) : _lock(mutex) { }
    std::lock_guard<std::mutex> _lock;
    operator bool() const { return true; }
};

#define WITH_LOCKGUARD(m) if (auto __lock = fancy_lock_guard(m))

#endif // GYM_PCB_UTIL_FANCYLOCK_H
