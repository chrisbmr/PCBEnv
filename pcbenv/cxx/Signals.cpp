
#include "Signals.hpp"
#include <csignal>

void SignalContext::install(void (*handler)(int))
{
    if (!handler)
        handler = [](int){ std::abort(); };
    mStack.push(std::signal(SIGINT, handler));
}

void SignalContext::restore()
{
    if (mStack.empty())
        throw std::runtime_error("no signal handler to restore");
    std::signal(SIGINT, mStack.top());
    mStack.pop();
}
