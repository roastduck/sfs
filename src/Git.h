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

/** Helper for creating smart pointer
 */
#define BUILD_PTR(ptrName, gitVarName) \
    struct Free##ptrName { void operator()(gitVarName *p) { if (p) gitVarName##_free(p); } }; \
    using ptrName = std::unique_ptr<gitVarName, Free##ptrName>;

/** Controller of a git repository
 */
class Git
{
public:
    static constexpr const char *GITKEEP_MAGIC = "....gitkeep....";

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
    /** Pointer classes that automatically free objects when an exception is threw */
    BUILD_PTR(IndexPtr, git_index);
    BUILD_PTR(TreePtr, git_tree);
    BUILD_PTR(SigPtr, git_signature);
    BUILD_PTR(CommitPtr, git_commit);
    BUILD_PTR(TreeEntryPtr, git_tree_entry);
    BUILD_PTR(ObjectPtr, git_object);

    TreePtr root(const char *spec = "HEAD^{tree}") const;
    CommitPtr head(const char *spec = "HEAD") const;
    TreeEntryPtr getEntry(const std::string &path) const;

    FileAttr getAttr(const git_tree_entry *entry) const;

    struct stat rootStat; /// Attributes of .git

    static int refCount; /// Reference count of Git objects

    static int checkErrorImpl(int error, const char *fn);

    /** Callback of git_tree_walk
     */
    static int treeWalkCallback(const char *root, const git_tree_entry *entry, void *_payload);

    void commit(const git_oid &blob_id, const std::string &path, const char *msg="commit", const bool executable = false);
    void commit(const IndexPtr &index, const CommitPtr &head, const char *msg);

public:
    git_repository *repo;

    /** Initialize from a .git directory
     *  @param path : Path to a .git directory
     */
    Git(const std::string &path);

    ~Git();

    void dump(const std::string &path, const std::string &out_path) const;
    void commit(const std::string &in_path, const std::string &path, const char *msg = "commit");
    void commit_remove(const std::string &path, const char *msg="commit");
    void truncate(const std::string &path, std::size_t size);
    void unlink(const std::string &path);
    std::vector<FileAttr> listDir(const std::string &path) const;
    FileAttr getAttr(const std::string &path) const;
    void chmod(const std::string &path, const mode_t mode, const bool executable);
};

#undef BUILD_PTR

#endif // GIT_H_

