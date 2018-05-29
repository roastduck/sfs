#include <cassert>
#include <iostream>
#include "Git.h"
#include "utils.h"

int Git::refCount = 0;

int Git::checkError(int error)
{
    if (error < 0)
    {
        const git_error *e = giterr_last();
        throw Error(
            error,
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
    stat(path.c_str(), &rootStat);
    checkError(git_repository_open_bare(&repo, path.c_str()));
}

Git::~Git()
{
    git_repository_free(repo);
    if (--refCount == 0)
        checkError(git_libgit2_shutdown());
}

Git::FileAttr Git::getAttr(const git_tree_entry *entry) const
{
    FileAttr attr;

    const git_otype type = git_tree_entry_type(entry);
    assert(type == GIT_OBJ_TREE || type == GIT_OBJ_BLOB);
    bool isDir = (type == GIT_OBJ_TREE);
    git_filemode_t mode = git_tree_entry_filemode(entry);

    attr.name = git_tree_entry_name(entry);
    attr.stat.st_mode = mode | (isDir ? S_IFDIR : S_IFREG);
    attr.stat.st_uid = rootStat.st_uid;
    attr.stat.st_gid = rootStat.st_gid;
    attr.stat.st_nlink = 1;

    if (type == GIT_OBJ_BLOB)
    {
        git_object *obj = NULL;
        checkError(git_tree_entry_to_object(&obj, repo, entry));
        git_blob *blob = (git_blob *)obj;
        attr.stat.st_size = git_blob_rawsize(blob);
        git_object_free(obj);
    }
    else
    {
        attr.stat.st_size = 4096; // TODO
    }

    return attr;
}

struct WalkPayload
{
    std::vector<Git::FileAttr> list;
    const Git *git;
};

int Git::treeWalkCallback(const char *root, const git_tree_entry *entry, void *_payload)
{
    UNUSED(root);
    WalkPayload *payload = (WalkPayload*)_payload;

    const int CONTINUE = 0, SKIP = 1, ABORT = -1;
    UNUSED(ABORT);

    auto attr = payload->git->getAttr(entry);
    payload->list.push_back(attr);

    return ((attr.stat.st_mode & S_IFMT) == S_IFDIR) ? SKIP : CONTINUE;
}

std::vector<Git::FileAttr> Git::listDir(const std::string &path) const
{
    git_object *obj = NULL;
    checkError(git_revparse_single(&obj, repo, "HEAD^{tree}"));
    git_tree *root = (git_tree*)obj, *tree = NULL;

    assert(path.length() > 0 && path[0] == '/');
    if (path == "/")
        tree = root;
    else {
        git_tree_entry *entry = NULL;
        checkError(git_tree_entry_bypath(&entry, root, path.c_str() + 1));
        assert(git_tree_entry_type(entry) == GIT_OBJ_TREE);
        checkError(git_tree_lookup(&tree, repo, git_tree_entry_id(entry)));
        // FIXME(twd2): entry would not be free if an exception is thrown.
        git_tree_entry_free(entry); // No error returned
    }

    WalkPayload payload;
    payload.git = this;
    checkError(git_tree_walk(tree, GIT_TREEWALK_PRE, treeWalkCallback, &payload));
    if (tree != root)
    {
        git_tree_free(tree);
    }
    git_object_free(obj);
    return payload.list;
}

Git::FileAttr Git::getAttr(const std::string &path) const
{
    git_object *obj = NULL;
    checkError(git_revparse_single(&obj, repo, "HEAD^{tree}"));
    git_tree *root = (git_tree*)obj;

    FileAttr attr;

    assert(path.length() > 0 && path[0] == '/');
    if (path == "/")
    {
        // '/' is not an entry
        attr.stat = rootStat;
    } else {
        git_tree_entry *entry = NULL;
        checkError(git_tree_entry_bypath(&entry, root, path.c_str() + 1));
        attr = getAttr(entry);
        // FIXME(twd2): ditto
        git_tree_entry_free(entry); // No error returned
    }
    git_object_free(obj);
    return attr;
}

