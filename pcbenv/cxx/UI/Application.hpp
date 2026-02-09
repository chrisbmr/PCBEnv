
#ifndef GYM_PCB_UI_APPLICATION_H
#define GYM_PCB_UI_APPLICATION_H

#include <thread>
#include <mutex>
#include <semaphore>
#include <future>
#include <cassert>

// FIXME: We can't include the Log.hpp file here because of Qt. Why?

class Agent;
class PCBoard;

class IUIApplication
{
public:
    static IUIApplication *create();
public:
    virtual ~IUIApplication();
    virtual std::future<bool> runAsync() { return std::future<bool>(); }
    virtual void show() { }
    virtual void hide() { }
    virtual void wait(); // NOTE: This is on by default by reacts to the Lock setting in the UI.
    virtual void quit(int code) { } // This may terminate Python.
    virtual void setPCB(const std::shared_ptr<PCBoard>&, bool wait = true) { }
    virtual void setAutonomousAgent(const std::shared_ptr<Agent>&, const std::shared_ptr<PCBoard>&) { }
    void startExclusiveTask(const char *warn = 0);
    bool tryStartExclusiveTask(const char *name = 0);
    bool endExclusiveTask(bool rv = true, const char *reason = 0);
protected:
    std::shared_ptr<PCBoard> mPCB;
    std::unique_ptr<Agent> mUserAgent;
    std::shared_ptr<Agent> mAutoAgent;
private:
    std::binary_semaphore mTaskLock{1};
};

class UIApplication : public IUIApplication
{
public:
    /// These are not available outside the UI as it wouldn't be thread-safe.
    Agent *getUserAgent() const { return mUserAgent.get(); }
    Agent *getAutoAgent() const { return mAutoAgent.get(); }
    PCBoard *getPCB() const { return mPCB.get(); }
};

#endif // GYM_PCB_UI_APPLICATION_H
