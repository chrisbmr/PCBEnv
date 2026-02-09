#include "Log.hpp"
#include "PCBoard.hpp"
#include "Net.hpp"
#include "Connection.hpp"
#include "Util/Pusher.hpp"

Pusher::Pusher(PCBoard &PCB) : mPCB(PCB)
{
}

Pusher::~Pusher()
{
}

void Pusher::init(Connection &X)
{
    mConnection = &X;
}

void Pusher::route(Connection &X)
{
    init(X);
}
