#ifndef GIT_H_
#define GIT_H_

#include <string>
#include <stdexcept>
#include <git2.h>

/** Controller of a git repository
 */
class Git
{
private:
    static int refCount; /// Reference count of Git objects

    /** Check libgit2 error code
     *  !!!!! PLEASE WRAP ANY CALL TO libgit2 WITH THIS FUNCTION !!!!!
     */
    static int checkError(int error);

public:
    /** Git will throw this exception once `error < 0`
     */
    class Error : public std::runtime_error
    {
    public:
        Error(const std::string &what) :
            std::runtime_error("[ERROR] libgit2: " + what) {}
    };

    git_repository *repo;

    /** Initialize from a .git directory
     *  @param path : Path to a .git directory
     */
    Git(const std::string &path);

    ~Git();
};

#endif // GIT_H_

