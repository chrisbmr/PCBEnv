
#include "UI/Application.hpp"
#include "Log.hpp"
#include "RL/Agent.hpp"

IUIApplication::~IUIApplication()
{
}

void IUIApplication::wait()
{
    if (mUserAgent)
        mUserAgent->getStepLock().wait();
}

void IUIApplication::startExclusiveTask(const char *warn)
{
    if (warn) {
        if (mTaskLock.try_acquire())
            return;
        WARN(warn);
    }
    mTaskLock.acquire();
}

bool IUIApplication::tryStartExclusiveTask(const char *name)
{
    auto rv = mTaskLock.try_acquire();
    if (rv) {
        if (name)
            INFO("Started task: " << name);
    } else {
        INFO("Worker is busy.");
    }
    return rv;
}
bool IUIApplication::endExclusiveTask(bool rv, const char *reason)
{
    mTaskLock.release();
    INFO("UIApplication: Task finished: " << rv);
    if (!rv && reason)
        INFO(reason);
    return rv;
}
