#include <cstdio>
#include <cassert>
#include <unistd.h>
#include "Timer.h"
#include "utils.h"
#include "OpenContext.h"

std::thread Timer::timer_thread;

void Timer::timer_loop(int interval)
{
    if (interval <= 0) return;

    while (true)
    {
        LOG << "Timed out, setting commit flags..." << std::endl;
        OpenContext::openContextsLock.lock();
        for (auto &p : OpenContext::contexts())
        {
            for (OpenContext *ctx : p.second)
            {
                ctx->commit_on_next_write = true;
                LOG << "Find a context." << std::endl;
            }
        }
        OpenContext::openContextsLock.unlock();

        sleep(interval);
    }
}

void Timer::start(int commit_interval)
{
    timer_thread = std::thread(timer_loop, commit_interval);
    timer_thread.detach();
}

