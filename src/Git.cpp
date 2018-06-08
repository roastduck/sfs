#include <unistd.h>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include "Git.h"
#include "utils.h"
#include <vector>

int Git::refCount = 0;

int Git::checkErrorImpl(int error, const char *fn)
{
    if (error < 0)
    {
        const git_error *e = giterr_last();
        std::string msg =
            std::string(fn) + ": Error " + std::to_string(error) + "/" + std::to_string(e->klass) +
            ": " + e->message;
        printf("Git Error: %s\n", msg.c_str());
        throw Error(error, msg);
    }
    return error;
}

/** Check libgit2 error code
 *  !!!!! PLEASE WRAP ANY CALL TO libgit2 WITH THIS MACRO !!!!!
 */
#define CHECK_ERROR(fn) Git::checkErrorImpl((fn), #fn)

Git::Git(const std::string &path)
{
    if (++refCount == 1)
        CHECK_ERROR(git_libgit2_init());
    try
    {
        CHECK_ERROR(git_repository_open_bare(&repo, path.c_str()));
    } catch (const Error &e)
    {
        if (e.error() != GIT_ENOTFOUND)
            throw e;
        git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;
        opts.flags |= GIT_REPOSITORY_INIT_NO_REINIT;
        opts.flags |= GIT_REPOSITORY_INIT_NO_DOTGIT_DIR;
        opts.flags |= GIT_REPOSITORY_INIT_MKDIR;
        opts.flags |= GIT_REPOSITORY_INIT_MKPATH;
        CHECK_ERROR(git_repository_init_ext(&repo, path.c_str(), &opts));

        git_index *index_;
        CHECK_ERROR(git_repository_index(&index_, repo));
        IndexPtr index(index_);

        commit(index, nullptr, "Initial commit");
    }
    stat(path.c_str(), &rootStat);
    pthread_rwlock_init(&rwlock, nullptr);
}

Git::~Git()
{
    git_repository_free(repo);
    if (--refCount == 0)
        CHECK_ERROR(git_libgit2_shutdown());
    pthread_rwlock_destroy(&rwlock);
}

Git::TreePtr Git::root(const char *spec) const
{
    git_object *obj = nullptr;
    CHECK_ERROR(git_revparse_single(&obj, repo, spec));
    git_tree *root = (git_tree*)obj;
    return TreePtr(root);
}

Git::CommitPtr Git::head(const char *spec) const
{
    git_object *obj = nullptr;
    CHECK_ERROR(git_revparse_single(&obj, repo, spec));
    git_commit *root = (git_commit*)obj;
    return CommitPtr(root);
}

Git::TreeEntryPtr Git::getEntry(const std::string &path) const
{
    assert(path.length() > 0 && path[0] == '/');
    std::shared_ptr<git_tree> root = this->root();
    git_tree_entry *e = nullptr;
    CHECK_ERROR(git_tree_entry_bypath(&e, root.get(), path.c_str() + 1));
    return TreeEntryPtr(e);
}

void Git::checkSig() const
{
    git_signature *sig;
    if (git_signature_default(&sig, repo) < 0)
    {
        std::cerr << std::endl
            << "*** Please tell me who you are." << std::endl
            << std::endl
            << "Run" << std::endl
            << std::endl
            << "  git config --global user.email \"you@example.com\"" << std::endl
            << "  git config --global user.name \"Your Name\"" << std::endl
            << std::endl
            << "to set your account's default identity." << std::endl << std::endl;
        exit(1);
    }
    else
    {
        git_signature_free(sig);
    }
}

void Git::dump(const std::string &path, const std::string &out_path, bool *out_executable) const
{
    RWlock mlock(rwlock, false);
    // TODO(twd2): cache
    auto e = getEntry(path);
    const git_otype type = git_tree_entry_type(e.get());
    assert(type == GIT_OBJ_BLOB);
    git_filemode_t mode = git_tree_entry_filemode(e.get());
    git_object *obj_ = nullptr;
    CHECK_ERROR(git_tree_entry_to_object(&obj_, repo, e.get()));
    ObjectPtr obj(obj_);
    std::size_t size = git_blob_rawsize((git_blob*)(obj.get()));
    const void *data = git_blob_rawcontent((git_blob*)(obj.get()));

    std::ofstream fout(out_path.c_str());
    fout.write((const char *)data, size);
    fout.close();

    if (out_executable)
    {
        *out_executable = mode == GIT_FILEMODE_BLOB_EXECUTABLE;
    }
}

void Git::commit(const std::string &in_path, const std::string &path, const char *msg, bool executable)
{
    assert(path.length() > 0 && path[0] == '/');

    git_oid blob_id;
    if (in_path != "")
    {
        CHECK_ERROR(git_blob_create_fromdisk(&blob_id, repo, in_path.c_str()));
    }
    else
    {
        char c;
        CHECK_ERROR(git_blob_create_frombuffer(&blob_id, repo, &c, 0));
    }
    commit(blob_id, path, msg, executable);
}

void Git::commit(const git_oid &blob_id, const std::string &path, const char *msg, const bool executable)
{
    assert(path.length() > 0 && path[0] == '/');

    char idstr[256];
    memset(idstr, 0, sizeof(idstr));
    git_oid_fmt(idstr, &blob_id);
    printf("blob id %s\n", idstr);

    git_index_entry e;
    memset(&e, 0, sizeof(e));
    e.id = blob_id;
    if (executable)
        e.mode = GIT_FILEMODE_BLOB_EXECUTABLE;
    else
        e.mode = GIT_FILEMODE_BLOB;
    e.path = path.c_str() + 1;
    // TODO(twd2): more information

    git_index *index_;
    CHECK_ERROR(git_repository_index(&index_, repo));
    IndexPtr index(index_);
    CHECK_ERROR(git_index_add(index.get(), &e));

    commit(index, this->head(), (std::string(msg) + " " + path).c_str());
}

void Git::commit(const IndexPtr &index, const CommitPtr &head, const char *msg)
{
    char idstr[256];
    git_oid tree_id, commit_id;

    git_signature *sig_;
    CHECK_ERROR(git_signature_default(&sig_, repo));
    SigPtr sig(sig_);

    CHECK_ERROR(git_index_write(index.get()));
    CHECK_ERROR(git_index_write_tree(&tree_id, index.get()));

    memset(idstr, 0, sizeof(idstr));
    git_oid_fmt(idstr, &tree_id);
    printf("tree id %s\n", idstr);

    git_tree *tree_;
    CHECK_ERROR(git_tree_lookup(&tree_, repo, &tree_id));
    TreePtr tree(tree_);

    CHECK_ERROR(git_commit_create_v(
      &commit_id, repo, "HEAD", sig.get(), sig.get(),
      nullptr, msg, tree.get(), 1, head.get()
    ));

    memset(idstr, 0, sizeof(idstr));
    git_oid_fmt(idstr, &commit_id);
    printf("commit id %s\n", idstr);
}

void Git::commit_remove(const std::string &path, const char *msg)
{
    assert(path.length() > 0 && path[0] == '/');

    git_index *index_;
    CHECK_ERROR(git_repository_index(&index_, repo));
    IndexPtr index(index_);
    CHECK_ERROR(git_index_remove_bypath(index.get(), path.c_str() + 1));

    commit(index, this->head(), (std::string(msg) + " " + path).c_str());
}

void Git::truncate(const std::string &path, std::size_t size)
{
    RWlock mlock(rwlock);
    assert(path.length() > 0 && path[0] == '/');
    auto e = getEntry(path);
    const git_otype type = git_tree_entry_type(e.get());
    assert(type == GIT_OBJ_BLOB);

    git_object *obj_ = nullptr;
    CHECK_ERROR(git_tree_entry_to_object(&obj_, repo, e.get()));
    ObjectPtr obj(obj_);
    std::size_t oldsize = git_blob_rawsize((git_blob*)(obj.get()));
    const void *data = git_blob_rawcontent((git_blob*)(obj.get()));

    git_oid new_blob_id;
    if (size <= oldsize)
    {
        CHECK_ERROR(git_blob_create_frombuffer(&new_blob_id, repo, data, size));
    }
    else
    {
        std::unique_ptr<char []> new_data(new char[size]);
        memcpy(new_data.get(), data, oldsize);
        memset(new_data.get() + oldsize, 0, size - oldsize);
        CHECK_ERROR(git_blob_create_frombuffer(&new_blob_id, repo, new_data.get(), size));
    }

    commit(new_blob_id, path, "truncate");
}

void Git::unlink(const std::string &path, const char *msg)
{
    RWlock mlock(rwlock);
    assert(path.length() > 0 && path[0] == '/');
    auto e = getEntry(path);

    const git_otype type = git_tree_entry_type(e.get());

    assert(type == GIT_OBJ_BLOB);
    FileAttr attr = getAttr(e.get());
    if (attr.stat.st_nlink == 1)
    {
        // TODO: close file
        // TODO(twd2): what's this???
    }

    commit_remove(path, msg);
}

Git::FileAttr Git::getAttr(const git_tree_entry *entry) const
{
    FileAttr attr;

    const git_otype type = git_tree_entry_type(entry);
    assert(type == GIT_OBJ_TREE || type == GIT_OBJ_BLOB);
    bool isDir = (type == GIT_OBJ_TREE);
    git_filemode_t mode = git_tree_entry_filemode(entry);

    attr.name = git_tree_entry_name(entry);
    attr.stat.st_mode = mode | (isDir ? (S_IFDIR | 0755) : S_IFREG);
    attr.stat.st_uid = rootStat.st_uid;
    attr.stat.st_gid = rootStat.st_gid;
    attr.stat.st_nlink = 1;

    if (type == GIT_OBJ_BLOB)
    {
        git_object *obj_ = NULL;
        CHECK_ERROR(git_tree_entry_to_object(&obj_, repo, entry));
        ObjectPtr obj(obj_);
        attr.stat.st_size = git_blob_rawsize((git_blob*)(obj.get()));
    }
    else
    {
        attr.stat.st_size = 4096;
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
    if (attr.name != GITKEEP_MAGIC)
        payload->list.push_back(attr);

    return ((attr.stat.st_mode & S_IFMT) == S_IFDIR) ? SKIP : CONTINUE;
}

std::vector<Git::FileAttr> Git::listDir(const std::string &path) const
{
    
    TreePtr root = this->root(), tree = nullptr;

    assert(path.length() > 0 && path[0] == '/');
    if (path == "/")
        tree = std::move(root);
    else {
        git_tree *tree_ = NULL;
        RWlock mlock(rwlock, false);
        auto e = getEntry(path);
        assert(git_tree_entry_type(e.get()) == GIT_OBJ_TREE);
        CHECK_ERROR(git_tree_lookup(&tree_, repo, git_tree_entry_id(e.get())));
        tree = TreePtr(tree_);
    }

    WalkPayload payload;
    payload.git = this;
    CHECK_ERROR(git_tree_walk(tree.get(), GIT_TREEWALK_PRE, treeWalkCallback, &payload));
    return payload.list;
}

Git::FileAttr Git::getAttr(const std::string &path) const
{
    RWlock mlock(rwlock, false);
    FileAttr attr;

    assert(path.length() > 0 && path[0] == '/');
    if (path == "/")
    {
        // '/' is not an entry
        attr.stat = rootStat;
    } else {
        auto e = getEntry(path);
        attr = getAttr(e.get());
    }
    return attr;
}

void Git::chmod(const std::string &path, const bool executable)
{
    RWlock mlock(rwlock);
    auto e = getEntry(path);
    const git_otype type = git_tree_entry_type(e.get());
    if (type != GIT_OBJ_BLOB) return;
    git_object *obj_ = nullptr;
    CHECK_ERROR(git_tree_entry_to_object(&obj_, repo, e.get()));
    ObjectPtr obj(obj_);
    const git_oid *id = git_blob_id((git_blob *)(obj.get()));
    commit(*id, path, executable ? "chmod +x" : "chmod -x", executable);
}

void Git::rename(const std::string &oldname, const std::string &newname,
                 const std::function<void (const std::string &, const std::string &)> &cb)
{
    RWlock mlock(rwlock);
    assert(oldname.length() > 0 && oldname[0] == '/');
    assert(newname.length() > 0 && newname[0] == '/');
    auto e = getEntry(oldname);
    const git_otype type = git_tree_entry_type(e.get());
    assert(type == GIT_OBJ_BLOB || type == GIT_OBJ_TREE);
    git_index *index_;
    CHECK_ERROR(git_repository_index(&index_, repo));
    IndexPtr index(index_);
    size_t pos;
    if (type == GIT_OBJ_BLOB)
    {
        CHECK_ERROR(git_index_find_prefix(&pos, index.get(), oldname.c_str() + 1));
        const git_index_entry *entry = git_index_get_byindex(index.get(), pos);
        git_index_entry e;
        memcpy(&e, entry, sizeof(e));
        e.path = newname.c_str() + 1;
        CHECK_ERROR(git_index_add(index.get(), &e));
        CHECK_ERROR(git_index_remove_bypath(index.get(), entry->path));
        cb(oldname, newname);
    }
    else // type == GIT_OBJ_TREE
    {
        std::vector<git_index_entry> new_entry_list;
        std::string oldpath = oldname.substr(1) + '/';
        std::string newpath = newname.substr(1) + '/';
        CHECK_ERROR(git_index_find_prefix(&pos, index.get(), oldpath.c_str()));
        const git_index_entry *entry = git_index_get_byindex(index.get(), pos);
        std::string filename = entry->path;
        size_t find_pos = filename.find(oldpath);
        while (find_pos == 0)
        {
            git_index_entry e = *entry;
            std::string new_filename = filename;
            new_filename.replace(find_pos, oldpath.length(), newpath);
            char *tmp_cstr = new char[new_filename.length() + 1];
            strcpy(tmp_cstr, new_filename.c_str());
            e.path = tmp_cstr;
            new_entry_list.push_back(e);
            CHECK_ERROR(git_index_remove_bypath(index.get(), entry->path));
            // FIXME(twd2): If an exception occurs, previously removed paths could not
            //              be added into index in the following for-loop. Thus, they
            //              would be missing.
            cb("/" + filename, "/" + new_filename);

            // next entry
            entry = git_index_get_byindex(index.get(), pos);
            filename = entry->path;
            find_pos = filename.find(oldpath);
        }
        for (auto &e : new_entry_list) {
            CHECK_ERROR(git_index_add(index.get(), &e));
            delete e.path;
        }
    }

    commit(index, this->head(), ("rename " + oldname + " to " + newname).c_str());
}

#undef CHECK_ERROR
