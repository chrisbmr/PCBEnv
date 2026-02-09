#ifndef GYM_PCB_SIGNALS_H
#define GYM_PCB_SIGNALS_H

#include <stack>

//! Save signal handlers so we can intercept Ctrl-C while executing C++ code.
//! Otherwise signals would only be delivered upon return to Python.
class SignalContext
{
public:
    void install(void (*handler_fptr)(int));
    void restore();
private:
    std::stack<void(*)(int)> mStack;
};

#endif // GYM_PCB_SIGNALS_H
