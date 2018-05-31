#include <unistd.h>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include "Git.h"
#include "utils.h"

int Git::refCount = 0;

int Git::checkErrorImpl(int error, const char *fn)
{
    if (error < 0)
    {
        const git_error *e = giterr_last();
        throw Error(
            error,
            std::string(fn) + ": Error " + std::to_string(error) + "/" + std::to_string(e->klass) +
            ": " + e->message
        );
    }
    return error;
}

Git::Git(const std::string &path)
{
    if (++refCount == 1)
        CHECK_ERROR(git_libgit2_init());
    stat(path.c_str(), &rootStat);
    CHECK_ERROR(git_repository_open_bare(&repo, path.c_str()));
}

Git::~Git()
{
    git_repository_free(repo);
    if (--refCount == 0)
        CHECK_ERROR(git_libgit2_shutdown());
}

std::shared_ptr<git_tree> Git::root(const char *spec) const
{
    git_object *obj = nullptr;
    CHECK_ERROR(git_revparse_single(&obj, repo, spec));
    git_tree *root = (git_tree*)obj;
    return std::shared_ptr<git_tree>(root, [](git_tree *p) { git_tree_free(p); });
}

std::shared_ptr<git_commit> Git::head(const char *spec) const
{
    git_object *obj = nullptr;
    CHECK_ERROR(git_revparse_single(&obj, repo, spec));
    git_commit *root = (git_commit*)obj;
    return std::shared_ptr<git_commit>(root, [](git_commit *p) { git_commit_free(p); });
}

std::shared_ptr<git_tree_entry> Git::getEntry(const std::string &path) const
{
    assert(path.length() > 0 && path[0] == '/');
    std::shared_ptr<git_tree> root = this->root();
    git_tree_entry *e = nullptr;
    CHECK_ERROR(git_tree_entry_bypath(&e, root.get(), path.c_str() + 1));
    return std::shared_ptr<git_tree_entry>(e, [](git_tree_entry *p) { git_tree_entry_free(p); });
}

void Git::dump(const std::string &path, const std::string &out_path) const
{
    // TODO(twd2): cache
    auto e = getEntry(path);
    const git_otype type = git_tree_entry_type(e.get());
    assert(type == GIT_OBJ_BLOB);
    git_object *obj_ = nullptr;
    CHECK_ERROR(git_tree_entry_to_object(&obj_, repo, e.get()));
    std::shared_ptr<git_blob> blob((git_blob *)obj_, [](git_blob *p) { git_blob_free(p); });
    std::size_t size = git_blob_rawsize(blob.get());
    const void *data = git_blob_rawcontent(blob.get());

    std::ofstream fout(out_path.c_str());
    fout.write((const char *)data, size);
    fout.close();
}

void Git::commit(const std::string &in_path, const std::string &path, const char *msg)
{
    // FIXME(twd2): memory leak if an exception is thrown.
    assert(path.length() > 0 && path[0] == '/');

    git_signature *sig;
    CHECK_ERROR(git_signature_default(&sig, repo));

    git_oid blob_id, tree_id, commit_id;

    CHECK_ERROR(git_blob_create_fromdisk(&blob_id, repo, in_path.c_str()));
    char idstr[256];
    memset(idstr, 0, sizeof(idstr));
    git_oid_fmt(idstr, &blob_id);
    printf("blob id %s\n", idstr);

    git_index_entry e;
    memset(&e, 0, sizeof(e));
    e.id = blob_id;
    e.mode = GIT_FILEMODE_BLOB;
    e.path = path.c_str() + 1;
    // TODO(twd2): more information

    git_index *index;
    CHECK_ERROR(git_repository_index(&index, repo));
    CHECK_ERROR(git_index_add(index, &e));
    CHECK_ERROR(git_index_write_tree(&tree_id, index));
    git_index_free(index);

    memset(idstr, 0, sizeof(idstr));
    git_oid_fmt(idstr, &tree_id);
    printf("tree id %s\n", idstr);

    git_tree *tree;
    CHECK_ERROR(git_tree_lookup(&tree, repo, &tree_id));

    auto head = this->head();
    CHECK_ERROR(git_commit_create_v(
      &commit_id, repo, "HEAD", sig, sig,
      nullptr, (std::string(msg) + " " + path).c_str(), tree, 1, head.get()
    ));

    git_tree_free(tree);
    git_signature_free(sig);

    memset(idstr, 0, sizeof(idstr));
    git_oid_fmt(idstr, &commit_id);
    printf("commit id %s\n", idstr);
}
// same as the commit(const std::string &in_path, const std::string &path, const char *msg)
void Git::commit(const git_oid &blob_id, const std::string &path, const char *msg)
{
    
    assert(path.length() > 0 && path[0] == '/');

    git_signature *sig;
    CHECK_ERROR(git_signature_default(&sig, repo));

    git_oid tree_id, commit_id;
    
    git_index_entry e;
    memset(&e, 0, sizeof(e));
    e.id = blob_id;
    e.mode = GIT_FILEMODE_BLOB;
    e.path = path.c_str() + 1;

    git_index *index;
    CHECK_ERROR(git_repository_index(&index, repo));
    CHECK_ERROR(git_index_add(index, &e));
    CHECK_ERROR(git_index_write_tree(&tree_id, index));
    git_index_free(index);

    git_tree *tree;
    CHECK_ERROR(git_tree_lookup(&tree, repo, &tree_id));

    auto head = this->head();
    CHECK_ERROR(git_commit_create_v(
      &commit_id, repo, "HEAD", sig, sig,
      nullptr, (std::string(msg) + " " + path).c_str(), tree, 1, head.get()
    ));

    git_tree_free(tree);
    git_signature_free(sig);

}
void Git::truncate(const std::string &path, std::size_t size)
{
    // TODO(twd2): this is a temporary implementation, rewrite it!
  /*  char tmp[256] = "sfstemp.XXXXXX";
    if (!mktemp(tmp)) // FIXME(twd2): Never use this function.
    {
        perror("mktemp");
        exit(1);
    }
    printf("%s %s\n", path.c_str(), tmp);
    dump(path, tmp);
    ::truncate(tmp, size);
    commit(tmp, path, "truncate");
    unlink(tmp);*/
    assert(path.length() > 0 && path[0] == '/');
    auto e = getEntry(path);
    const git_otype type = git_tree_entry_type(e.get());
    assert(type == GIT_OBJ_BLOB);
    
    git_object *obj_ = nullptr;
    CHECK_ERROR(git_tree_entry_to_object(&obj_, repo, e.get()));
    std::shared_ptr<git_blob> blob((git_blob *)obj_, [](git_blob *p) { git_blob_free(p); });
    std::size_t oldsize = git_blob_rawsize(blob.get());
    const void *data = git_blob_rawcontent(blob.get());
    
    git_oid new_blob_id;
    if (size<=oldsize) {
    	git_blob_create_frombuffer(&new_blob_id, repo, data, size);
    } else {
    	void *new_data = (void*)calloc(1,sizeof(char)*size);
	memcpy(new_data,data,oldsize);
	git_blob_create_frombuffer(&new_blob_id, repo, new_data, size);
    	free(new_data);
    }

    commit(new_blob_id, path, "truncate"); 
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
        git_object *obj = NULL;
        CHECK_ERROR(git_tree_entry_to_object(&obj, repo, entry));
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
    std::shared_ptr<git_tree> root = this->root(), tree;

    assert(path.length() > 0 && path[0] == '/');
    if (path == "/")
        tree = root;
    else {
        git_tree *tree_ = NULL;
        auto e = getEntry(path);
        assert(git_tree_entry_type(e.get()) == GIT_OBJ_TREE);
        CHECK_ERROR(git_tree_lookup(&tree_, repo, git_tree_entry_id(e.get())));
        tree = std::shared_ptr<git_tree>(tree_, [](git_tree *p) { git_tree_free(p); }); // TODO(twd2): refactor
    }

    WalkPayload payload;
    payload.git = this;
    CHECK_ERROR(git_tree_walk(tree.get(), GIT_TREEWALK_PRE, treeWalkCallback, &payload));
    return payload.list;
}

Git::FileAttr Git::getAttr(const std::string &path) const
{
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

