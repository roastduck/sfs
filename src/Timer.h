#ifndef TIMER_H_
#define TIMER_H_

#include <thread>

class Timer
{
private:
    static void timer_loop(int interval);
    static std::thread timer_thread;

public:
    static void start(int commit_interval);
};

#endif // TIMER_H_

