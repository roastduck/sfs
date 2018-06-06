#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <unistd.h>
#include <fstream>
#include <iostream>
#include <fuse.h>
#include "utils.h"
#include "Git.h"
#include "OpenContext.h"
#include "3rd-party/json.hpp"

using Json = nlohmann::json;

Json config;
Git *git;
bool commit_on_write = false, read_only = false;

#define CHECK_READONLY() \
    do { if (read_only) return -EROFS; } while (0)

std::string path_mangle(const std::string &path)
{
    // TODO(ltl)
    return path;
}

std::string path_demangle(const std::string &path)
{
    // TODO(ltl)
    return path;
}

static int sfs_readdir(
    const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi
)
{
    UNUSED(offset);
    UNUSED(fi);
    try
    {
        auto list = git->listDir(path_mangle(path));
        for (const auto &item : list)
            filler(buf, path_demangle(item.name).c_str(), &item.stat, 0 /* Offset disabled */);
        return 0;
    }
    catch (const Git::Error &e)
    {
        return e.unixError();
    }
}

static int sfs_getattr(const char *path, struct stat *st)
{
    try
    {
        *st = git->getAttr(path_mangle(path)).stat;
        return 0;
    }
    catch (const Git::Error &e)
    {
        return e.unixError();
    }
}

static int sfs_open(const char *path, struct fuse_file_info *fi)
{
    try
    {
        char tmp[] = "sfstemp.XXXXXX";
        if (!mktemp(tmp)) // FIXME(twd2): Never use this function.
        {
            perror("mktemp");
            exit(1);
        }
        bool executable;
        git->dump(path_mangle(path), tmp, &executable);
        printf("dumped %s\n", tmp);
        OpenContext *ctx = new OpenContext(path_mangle(path), tmp);
        fi->fh = (uint64_t)(void *)ctx;
        ctx->fd = open(tmp, O_RDWR);
        if (ctx->fd < 0)
        {
            perror("open");
            delete ctx;
            fi->fh = 0;
            return -EIO;
        }
        ctx->executable = executable;
        return 0;
    }
    catch (const Git::Error &e)
    {
        return e.unixError();
    }
}

static int sfs_release(const char *path, struct fuse_file_info *fi)
{
    RWlock mlock(git->rwlock);
    UNUSED(path);
    OpenContext *ctx = (OpenContext *)(void *)fi->fh;
    ctx->commit(*git, "close");
    delete ctx;
    return 0;
}

static int sfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    RWlock mlock(git->rwlock);
    UNUSED(path);
    OpenContext *ctx = (OpenContext *)(void *)fi->fh;
    int ret;
    if ((ret = lseek(ctx->fd, offset, SEEK_SET)) < 0) return ret;
    return read(ctx->fd, buf, size);
}

static int sfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    RWlock mlock(git->rwlock);
    UNUSED(path);
    CHECK_READONLY();
    OpenContext *ctx = (OpenContext *)(void *)fi->fh;
    int ret;
    if ((ret = lseek(ctx->fd, offset, SEEK_SET)) < 0) return ret;
    if ((ret = write(ctx->fd, buf, size)) < 0) return ret;
    ctx->dirty = true;
    if (commit_on_write) ctx->commit(*git, "write");
    return ret;
}

static int sfs_truncate(const char *path, off_t length)
{
    CHECK_READONLY();
    OpenContext::for_each(path_mangle(path), [=] (OpenContext *ctx) { ctx->truncate(length); });
    try
    {
        git->truncate(path_mangle(path), length);
        return 0;
    }
    catch (const Git::Error &e)
    {
        return e.unixError();
    }
}

static int sfs_unlink(const char *path)
{
    CHECK_READONLY();
    try
    {
        git->unlink(path_mangle(path));
        return 0;
    }
    catch (const Git::Error &e)
    {
        return e.unixError();
    }
}

static int sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    RWlock mlock(git->rwlock);
    CHECK_READONLY();
    try
    {
        char tmp[] = "sfstemp.XXXXXX";
        if (!mktemp(tmp)) // FIXME(twd2): Never use this function.
        {
            perror("mktemp");
            exit(1);
        }
        OpenContext *ctx = new OpenContext(path_mangle(path), tmp);
        fi->fh = (uint64_t)(void *)ctx;
        ctx->fd = open(tmp, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (ctx->fd < 0)
        {
            perror("open");
            delete ctx;
            fi->fh = 0;
            return -EIO;
        }
        ctx->dirty = true;
        ctx->executable = (mode & (S_IXUSR | S_IXGRP | S_IXOTH));
        //pthread_rwlock_wrlock(rwlock);
        ctx->commit(*git, ctx->executable ? "create executable": "create");
        //pthread_rwlock_unlock(rwlock);
        return 0;
    }
    catch (const Git::Error &e)
    {
        return e.unixError();
    }
}

static int sfs_mkdir(const char *path, mode_t mode)
{
    RWlock mlock(git->rwlock);
    UNUSED(mode);
    CHECK_READONLY();
    try
    {
        std::string gitKeep = path_mangle(path) + "/" + Git::GITKEEP_MAGIC;
        git->commit("", gitKeep, "mkdir");
        return 0;
    }
    catch (const Git::Error &e)
    {
        return e.unixError();
    }
}

static int sfs_rmdir(const char *path)
{
    CHECK_READONLY();
    try
    {
        if (!git->listDir(path_mangle(path)).empty())
            return -ENOTEMPTY;
        std::string gitKeep = path_mangle(path) + "/" + Git::GITKEEP_MAGIC;
        git->unlink(gitKeep, "rmdir");
        return 0;
    }
    catch (const Git::Error &e)
    {
        return e.unixError();
    }
}

static int sfs_opendir(const char *path, struct fuse_file_info *fi)
{
    UNUSED(path);
    UNUSED(fi);
    return 0;
}

static int sfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    UNUSED(path);
    UNUSED(fi);
    return 0;
}

static int sfs_chmod(const char *path, mode_t mode)
{
    CHECK_READONLY();
    bool executable = (mode & (S_IXUSR | S_IXGRP | S_IXOTH));
    OpenContext::for_each(path_mangle(path), [=] (OpenContext *ctx) { ctx->chmod(executable); });
    try
    {
        git->chmod(path_mangle(path), executable);
        return 0;
    }
    catch (const Git::Error &e)
    {
        return e.unixError();
    }
}

static int sfs_rename(const char *oldname, const char *newname)
{
    CHECK_READONLY();
    try
    {
        git->rename(path_mangle(oldname), path_mangle(newname),
                    [] (const std::string &oldname, const std::string &newname)
                    {
                        OpenContext::for_each(oldname, [&] (OpenContext *ctx) { ctx->rename(newname); });
                        printf("rename %s to %s\n", oldname.c_str(), newname.c_str());
                    });
        return 0;
    }
    catch (const Git::Error &e)
    {
        return e.unixError();
    }
}

static struct fuse_operations sfs_ops;
// CAUTIOUS: If you put `sfs_ops` in the stack, all the things will go wrong!

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return -1;
    }

    {
        std::ifstream configFile(argv[1]);
        configFile >> config;
    } // Here the file closes

    commit_on_write = config["commit_on_write"].get<bool>();
    read_only = config["read_only"].get<bool>();

    git = new Git(config["git_path"].get<std::string>()); // Will not be deleted
    git->checkSig();

    std::vector<std::string> fuseArgs = config["fuse_args"];
    fuseArgs.insert(fuseArgs.begin(), argv[0]);
    const int fuseArgc = fuseArgs.size();
    char *fuseArgv[fuseArgc];
    for (int i = 0; i < fuseArgc; i++)
        fuseArgv[i] = const_cast<char*>(fuseArgs[i].c_str()); // I bet FUSE won't change it

    // Named struct initializaion is only supported in plain C
    // So we are using assignments here
    sfs_ops.readdir = sfs_readdir;
    sfs_ops.getattr = sfs_getattr;
    sfs_ops.open = sfs_open;
    sfs_ops.release = sfs_release;
    sfs_ops.read = sfs_read;
    sfs_ops.write = sfs_write;
    sfs_ops.truncate = sfs_truncate;
    sfs_ops.unlink = sfs_unlink;
    sfs_ops.create = sfs_create;
    sfs_ops.mkdir = sfs_mkdir;
    sfs_ops.rmdir = sfs_rmdir;
    sfs_ops.opendir = sfs_opendir;
    sfs_ops.releasedir = sfs_releasedir;
    sfs_ops.chmod = sfs_chmod;
    sfs_ops.rename = sfs_rename;
    return fuse_main(fuseArgc, fuseArgv, &sfs_ops, NULL);
}

