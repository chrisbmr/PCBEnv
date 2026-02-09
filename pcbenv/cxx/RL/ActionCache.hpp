#ifndef GYM_PCB_RL_ACTIONCACHE_H
#define GYM_PCB_RL_ACTIONCACHE_H

#include "RL/Action.hpp"
#include <list>
#include <map>

template<class Data> class ActionCache
{
public:
    ActionCache(uint size, uint maxOversize) : mSize(size), mTriggerSize(size + maxOversize) { }
    using Ref = Action::CacheRef;
    using List = std::list<std::pair<Ref, Data>>;
    Data *load(Ref);
    void store(Ref, const Data&);
    void clear();
    void shrink();
private:
    std::map<Ref, typename List::iterator> mLUT;
    List mLRU;
    uint mSize;
    uint mTriggerSize;
};

template<class Data> Data *ActionCache<Data>::load(ActionCache::Ref ref)
{
    auto M = mLUT.find(ref);
    if (M == mLUT.end())
        return 0;
    mLRU.splice(mLRU.end(), mLRU, M->second);
    return &M->second->second;
}

template<class Data> void ActionCache<Data>::store(ActionCache::Ref ref, const Data &data)
{
    auto M = mLUT.find(ref);
    if (M != mLUT.end()) {
        M->second->second = data;
    } else {
        mLRU.emplace_back(ref, data);
        mLUT[ref] = --mLRU.end();
    }
    if (mLRU.size() >= mTriggerSize)
        shrink();
}

template<class Data> void ActionCache<Data>::shrink()
{
    if (mLRU.size() <= mSize)
        return;
    uint N = mLRU.size() - mSize;
    for (auto I = mLRU.begin(); I != mLRU.end() && N; --N) {
        mLUT.erase(I->first);
        I = mLRU.erase(I);
    }
}

template<class Data> void ActionCache<Data>::clear()
{
    mLUT.clear();
    mLRU.clear();
}

#endif // GYM_PCB_RL_ACTIONCACHE_H
