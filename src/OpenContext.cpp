#include <cstdio>
#include <unistd.h>
#include "Git.h"
#include "OpenContext.h"

OpenContext::~OpenContext()
{
    if (fd >= 0)
    {
        printf("close %d\n", fd);
        close(fd);
    }
    if (tmpfile != "")
    {
        printf("unlink %s\n", tmpfile.c_str());
        unlink(tmpfile.c_str());
    }
}

void OpenContext::commit(Git &git, const char *msg)
{
    if (dirty)
    {
        git.commit(tmpfile, path, msg);
        dirty = false;
    }
    else
    {
        printf("not dirty\n");
    }
}

