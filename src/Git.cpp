#include <iostream>
#include "Git.h"

int Git::refCount = 0;

int Git::checkError(int error)
{
    if (error < 0)
    {
        const git_error *e = giterr_last();
        throw Error(
            "Error " + std::to_string(error) + "/" + std::to_string(e->klass) +
            ": " + e->message
        );
    }
    return error;
}

Git::Git(const std::string &path)
{
    if (++refCount == 1)
        checkError(git_libgit2_init());
    checkError(git_repository_open_bare(&repo, path.c_str()));
}

Git::~Git()
{
    if (--refCount == 0)
        checkError(git_libgit2_shutdown());
}

