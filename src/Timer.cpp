#include <cstdio>
#include <cassert>
#include <unistd.h>
#include "Timer.h"
#include "OpenContext.h"

bool is_running = true;

void timer_loop(int interval)
{
    if (interval <= 0) return;

    while (is_running)
    {
        printf("Timed out, setting commit flags...\n");
        for (auto &p : OpenContext::contexts())
        {
            for (OpenContext *ctx : p.second)
            {
                ctx->commit_on_next_write = true;
                // TODO(twd2): fence?
                printf("Find a context.\n");
            }
        }

        // FIXME(twd2)!
        for (int i = 0; is_running && i < interval; ++i)
        {
            sleep(1);
        }
    }
}
