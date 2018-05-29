#ifndef GIT_H_
#define GIT_H_

#include <cstring>
#include <vector>
#include <string>
#include <stdexcept>
#include <git2.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <memory>

/** Check libgit2 error code
 *  !!!!! PLEASE WRAP ANY CALL TO libgit2 WITH THIS MACRO !!!!!
 */
#define CHECK_ERROR(fn) Git::checkErrorImpl((fn), #fn)

/** Controller of a git repository
 */
class Git
{
public:
    /** Git will throw this exception once `error < 0`
     */
    class Error : public std::runtime_error
    {
    private:
        int _error;

    public:
        Error(int error, const std::string &what) :
            std::runtime_error("[ERROR] libgit2: " + what), _error(error) {}

        int error() const
        {
            return _error;
        }

        int unixError() const
        {
            if (_error == GIT_ENOTFOUND) return -ENOENT;
            if (_error == GIT_EEXISTS) return -EEXIST;
            return -EPERM; // TODO
        }
    };

    /** Attributes of a file
     */
    struct FileAttr
    {
        std::string name;
        struct stat stat;
        FileAttr() { memset(&stat, 0, sizeof stat); }
    };

private:
    FileAttr getAttr(const git_tree_entry *entry) const;

    struct stat rootStat; /// Attributes of .git

    static int refCount; /// Reference count of Git objects

    static int checkErrorImpl(int error, const char *fn);

    /** Callback of git_tree_walk
     */
    static int treeWalkCallback(const char *root, const git_tree_entry *entry, void *_payload);

public:
    git_repository *repo;

    /** Initialize from a .git directory
     *  @param path : Path to a .git directory
     */
    Git(const std::string &path);

    ~Git();

    std::shared_ptr<git_tree> root(const char *spec = "HEAD^{tree}") const;
    std::shared_ptr<git_commit> head(const char *spec = "HEAD") const;
    std::shared_ptr<git_tree_entry> getEntry(const std::string &path) const;

    void dump(const std::string &path, const std::string &out_path) const;
    void commit(const std::string &in_path, const std::string &path, const char *msg = "commit");
    void truncate(const std::string &path, std::size_t size);

    std::vector<FileAttr> listDir(const std::string &path) const;
    FileAttr getAttr(const std::string &path) const;
};

#endif // GIT_H_

