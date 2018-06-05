#include <cstdio>
#include <cassert>
#include <algorithm>
#include <unistd.h>
#include "Git.h"
#include "OpenContext.h"

std::unordered_map<std::string, std::vector<OpenContext *> > OpenContext::openContexts;

OpenContext::OpenContext(const std::string &path, const std::string &tmpfile)
    : path(path), tmpfile(tmpfile)
{
    openContexts[path].push_back(this);
}

OpenContext::~OpenContext()
{
    auto &v = openContexts[path];
    auto iter = std::find(v.begin(), v.end(), this);
    assert(iter != v.end());
    v.erase(iter);
    if (v.empty())
    {
        openContexts.erase(path);
    }
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
        if (path != "")
        {
            git.commit(tmpfile, path, msg, executable);
        }
        dirty = false;
    }
    else
    {
        printf("not dirty\n");
    }
}

void OpenContext::truncate(std::size_t len)
{
    if (ftruncate(fd, len) < 0)
    {
        perror("ftruncate");
    }
    // dirty = true; // TODO ?
}

void OpenContext::chmod(bool executable)
{
    this->executable = executable;
    // dirty = true; // TODO ?
}

void OpenContext::rename(const std::string &newname)
{
    path = newname;
    // dirty = true; // TODO ?
}

std::vector<OpenContext *> OpenContext::find(const std::string &path)
{
    auto iter = openContexts.find(path);
    if (iter == openContexts.end())
    {
        return std::vector<OpenContext *>();
    }
    else
    {
        return iter->second;
    }
}

void OpenContext::for_each(const std::string &path, const std::function<void (OpenContext *)> &f)
{
    std::vector<OpenContext *> v = find(path);
    for (OpenContext *ctx : v)
    {
        f(ctx);
    }
}

