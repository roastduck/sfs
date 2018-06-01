#ifndef OPEN_CONTEXT_H_
#define OPEN_CONTEXT_H_

#include <string>
#include <unordered_map>

class Git;

class OpenContext
{
private:
    std::string path;
    std::string tmpfile;

    static std::unordered_map<std::string, OpenContext *> openContexts;

public:
    int fd = -1;
    bool dirty = false;

    explicit OpenContext(const std::string &path, const std::string &tmpfile);

    ~OpenContext();

    void commit(Git &git, const char *msg);

    static OpenContext *find(const std::string &path);
};

#endif // OPEN_CONTEXT_H_

