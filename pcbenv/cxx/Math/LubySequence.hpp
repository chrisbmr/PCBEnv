
#ifndef GYM_PCB_MATH_LUBYSEQUENCE_H
#define GYM_PCB_MATH_LUBYSEQUENCE_H

#include <vector>

// 1, 1 2, 1 1 2 4, 1 1 2 1 1 2 4 8, ...

class LubySequence
{
public:
    LubySequence() { reset(); }
    void reset();
    uint next();
private:
    std::vector<uint> values;
    uint it;
};
inline void LubySequence::reset()
{
    values.resize(1);
    values[0] = 1;
    it = 0;
}
inline uint LubySequence::next()
{
    if (it < values.size())
        return values[it++];
    values.insert(values.end(), values.begin(), values.end());
    values.push_back(values.back() * 2);
    it = 1;
    return 1;
}

#endif // GYM_PCB_MATH_LUBYSEQUENCE_H
