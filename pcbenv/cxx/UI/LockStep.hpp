
#ifndef GYM_PCB_UI_LOCKSTEP_H
#define GYM_PCB_UI_LOCKSTEP_H

#include <mutex>
#include <condition_variable>

class StepLock
{
public:
    StepLock(uint granularity = 0) : mGranularity(granularity) { }
    void setGranularity(uint n) { if (n == 0 && mGranularity != 0) signal(); mGranularity = n; }
    uint getGranularity() const { return mGranularity; }
    void wait(uint granularity = 1);
    void signal();
private:
    std::mutex mLock;
    std::condition_variable mCV;
    uint mSeq{0};
    uint mGranularity{0};
};

inline void StepLock::wait(uint granularity)
{
    if (granularity <= mGranularity) {
        std::unique_lock lock(mLock);
        mCV.wait(lock, [this, curr = mSeq]{ return mSeq != curr; });
    }
}
inline void StepLock::signal()
{
    if (mGranularity) {
        {
        std::lock_guard lock(mLock);
        mSeq++;
        }
        mCV.notify_one();
    }
}

#endif // GYM_PCB_UI_LOCKSTEP_H
