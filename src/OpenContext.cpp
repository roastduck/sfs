#include <cstdio>
#include <unistd.h>
#include "Git.h"
#include "OpenContext.h"

std::unordered_map<std::string, OpenContext *> OpenContext::openContexts;

OpenContext::OpenContext(const std::string &path, const std::string &tmpfile)
    : path(path), tmpfile(tmpfile)
{
    openContexts[path] = this;
}

OpenContext::~OpenContext()
{
    openContexts.erase(path);
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

OpenContext *OpenContext::find(const std::string &path)
{
    auto iter = openContexts.find(path);
    if (iter == openContexts.end())
    {
        return nullptr;
    }
    else
    {
        return iter->second;
    }
}

