#ifndef OPEN_CONTEXT_H_
#define OPEN_CONTEXT_H_

#include <mutex>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

class Git;

class OpenContext
{
private:
    std::string path;
    std::string tmpfile;

    static std::unordered_map<std::string, std::vector<OpenContext *> > openContexts;

public:
    int fd = -1;
    bool dirty = false;
    bool executable = false;
    bool commit_on_next_write = false;
    static std::mutex openContextsLock;

    explicit OpenContext(const std::string &path, const std::string &tmpfile);

    ~OpenContext();

    void commit(Git &git, const char *msg);
    void truncate(std::size_t len);
    void chmod(bool executable);
    void rename(const std::string &newname);

    static void for_each(const std::string &path, const std::function<void (OpenContext *)> &f);
    static const std::unordered_map<std::string, std::vector<OpenContext *> > &contexts();

private:
    void emplaceMap();
    void removeMap();
};

#endif // OPEN_CONTEXT_H_

