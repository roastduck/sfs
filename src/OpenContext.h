#ifndef OPEN_CONTEXT_H_
#define OPEN_CONTEXT_H_

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

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

    explicit OpenContext(const std::string &path, const std::string &tmpfile);

    ~OpenContext();

    void commit(Git &git, const char *msg);
    void truncate(std::size_t len);
    void chmod(bool executable);
    void rename(const std::string &newname);

    static std::vector<OpenContext *> find(const std::string &path);
    static void for_each(const std::string &path, const std::function<void (OpenContext *)> &f);
};

#endif // OPEN_CONTEXT_H_

