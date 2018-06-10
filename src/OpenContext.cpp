#include <cstdio>
#include <cassert>
#include <algorithm>
#include <unistd.h>
#include "Git.h"
#include "utils.h"
#include "OpenContext.h"

std::unordered_map<std::string, std::vector<OpenContext *> > OpenContext::openContexts;
std::mutex OpenContext::openContextsLock;

OpenContext::OpenContext(const std::string &path, const std::string &tmpfile)
    : path(path), tmpfile(tmpfile)
{
    emplaceMap();
}

OpenContext::~OpenContext()
{
    removeMap();
    if (fd >= 0)
    {
        LOG << "close " << fd << std::endl;
        close(fd);
    }
    if (tmpfile != "")
    {
        LOG << "unlink " << tmpfile.c_str() << std::endl;
        unlink(tmpfile.c_str());
    }
}

void OpenContext::emplaceMap()
{
    openContextsLock.lock();
    openContexts[path].push_back(this);
    openContextsLock.unlock();
}

void OpenContext::removeMap()
{
    openContextsLock.lock();
    auto &v = openContexts[path];
    auto iter = std::find(v.begin(), v.end(), this);
    assert(iter != v.end());
    v.erase(iter);
    if (v.empty())
    {
        openContexts.erase(path);
    }
    openContextsLock.unlock();
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
        LOG << "not dirty" << std::endl;
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
    if (path == newname) return;

    removeMap();
    path = newname;
    emplaceMap();
    // dirty = true; // TODO ?
}

void OpenContext::for_each(const std::string &path, const std::function<void (OpenContext *)> &f)
{
    auto iter = openContexts.find(path);
    if (iter != openContexts.end())
        for (OpenContext *ctx : iter->second)
            f(ctx);
}

const std::unordered_map<std::string, std::vector<OpenContext *> > &OpenContext::contexts()
{
    return openContexts;
}

