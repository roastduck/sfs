#ifndef OPEN_CONTEXT_H_
#define OPEN_CONTEXT_H_

#include <string>

class Git;

class OpenContext
{
private:
    std::string path;
    std::string tmpfile;

public:
    int fd = -1;
    bool dirty = false;

    explicit OpenContext(const std::string &path, const std::string &tmpfile) :
        path(path), tmpfile(tmpfile) { }

    ~OpenContext();

    void commit(Git &git, const char *msg);
};

#endif // OPEN_CONTEXT_H_

