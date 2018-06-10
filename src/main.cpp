#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <unistd.h>
#include <fstream>
#include <iostream>
#include <fuse.h>
#include <time.h>
#include "utils.h"
#include "Git.h"
#include "OpenContext.h"
#include "Timer.h"
#include "mangle.h"
#include "3rd-party/json.hpp"

using Json = nlohmann::json;

Json config;
Git *git;
bool commit_on_write = false, read_only = false;
int commit_interval = -1;
const char *time_format_str="%d-%d-%d %d:%d:%d";

static constexpr const char *GITKEEP_MAGIC = ".gitkeep";

time_t string2time(const std::string &str)
{
    // FIXME(twd2): use strptime
    struct tm tm1;
    int year,mon,mday,hour,min,sec;
    if( -1 == sscanf(str.c_str(),time_format_str,&year,&mon,&mday,&hour,&min,&sec)) return -1;
    tm1.tm_year=year-1900;
    tm1.tm_mon=mon-1;
    tm1.tm_mday=mday;
    tm1.tm_hour=hour;
    tm1.tm_min=min;
    tm1.tm_sec=sec;
    return mktime(&tm1);
}

#define CHECK_READONLY() \
    do { if (read_only) return -EROFS; } while (0)

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
        {
            std::string p = path_demangle(item.name);
            if (p != "")
            {
                filler(buf, p.c_str(), &item.stat, 0 /* Offset disabled */);
            }
        }
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
    UNUSED(path);
    RWlock mlock(git->rwlock, false);
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
    if (commit_on_write || ctx->commit_on_next_write)
    {
        ctx->commit(*git, ctx->commit_on_next_write ? "timed commit" : "write");
        ctx->commit_on_next_write = false;
    }
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
        {
            RWlock mlock(git->rwlock);
            ctx->commit(*git, ctx->executable ? "create executable": "create");
        }
        return 0;
    }
    catch (const Git::Error &e)
    {
        return e.unixError();
    }
}

static int sfs_mkdir(const char *path, mode_t mode)
{
    UNUSED(mode);
    CHECK_READONLY();
    try
    {
        std::string gitKeep = path_mangle(path) + "/" + GITKEEP_MAGIC;
        {
            RWlock mlock(git->rwlock);
            git->commit("", gitKeep, "mkdir");
        }
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
        if (git->listDir(path_mangle(path)).size() > 1) // .gitkeep is the last file
            return -ENOTEMPTY;
        std::string gitKeep = path_mangle(path) + "/" + GITKEEP_MAGIC;
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
    test_mangle();

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
    commit_interval = config["commit_interval"].get<int>();
    bool version_selection=config["version_selection"].get<bool>();
    git = new Git(config["git_path"].get<std::string>()); // Will not be deleted
    git->checkSig();
    if (version_selection)
        git->checkout_branch(string2time(config["version_time"].get<std::string>()));
    std::vector<std::string> fuseArgs = config["fuse_args"];
    fuseArgs.insert(fuseArgs.begin(), argv[0]);
    const int fuseArgc = fuseArgs.size();
    char *fuseArgv[fuseArgc];
    for (int i = 0; i < fuseArgc; i++)
        fuseArgv[i] = const_cast<char*>(fuseArgs[i].c_str()); // I bet FUSE won't change it

    Timer::start(commit_interval);

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

