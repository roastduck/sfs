#include <cstdio>
#include <cassert>
#include <unistd.h>
#include "Timer.h"
#include "OpenContext.h"

std::thread Timer::timer_thread;

void Timer::timer_loop(int interval)
{
    if (interval <= 0) return;

    while (true)
    {
        printf("Timed out, setting commit flags...\n");
        OpenContext::openContextsLock.lock();
        for (auto &p : OpenContext::contexts())
        {
            for (OpenContext *ctx : p.second)
            {
                ctx->commit_on_next_write = true;
                printf("Find a context.\n");
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

