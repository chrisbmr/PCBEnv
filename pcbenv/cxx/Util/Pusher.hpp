#ifndef GYM_PCB_UTIL_PUSHER_H
#define GYM_PCB_UTIL_PUSHER_H

// How it works ...
// ...
// 

class PCBoard;
class Connection;

class Pusher
{
public:
    Pusher(PCBoard&);
    ~Pusher();

    bool route(Connection&);

private:
    void init(Connection&);

private:
    PCBoard &mPCB;
    Connection *mConnection;
};

#endif // GYM_PCB_UTIL_PUSHER_H
