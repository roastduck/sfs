#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <unistd.h>
#include <fstream>
#include <iostream>
#include <fuse.h>
#include "Git.h"
#include "3rd-party/json.hpp"

using Json = nlohmann::json;

Git *git;

static int sfs_readdir(
    const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi
)
{
    try
    {
        auto list = git->listDir(path);
        for (const auto &item : list)
            filler(buf, item.name.c_str(), &item.stat, 0 /* Offset disabled */);
        return 0;
    }
    catch (Git::Error e)
    {
        return e.unixError();
    }
}

static int sfs_getattr(const char *path, struct stat *st)
{
    try
    {
        *st = git->getAttr(path).stat;
        return 0;
    }
    catch (Git::Error e)
    {
        return e.unixError();
    }
}

struct OpenContext
{
    std::string tmpfile;
    int fd = -1;

    explicit OpenContext(const std::string &tmpfile) : tmpfile(tmpfile) { }

    ~OpenContext()
    {
        if (fd >= 0)
        {
            printf("close %d\n", fd);
            close(fd);
        }
        if (tmpfile != "")
        {
            printf("unlink %s\n", tmpfile.c_str());
            unlink(tmpfile.c_str());
        }
    }
};

static int sfs_open(const char *path, struct fuse_file_info *fi)
{
    try
    {
        char tmp[256] = "sfstemp.XXXXXX";
        if (!mktemp(tmp)) // FIXME(twd2): Never use this function.
        {
            perror("mktemp");
            exit(1);
        }
        git->dump(path, tmp);
        printf("dumped %s\n", tmp);
        OpenContext *ctx = new OpenContext(tmp);
        fi->fh = (uint64_t)(void *)ctx;
        ctx->fd = open(tmp, O_RDWR);
        if (ctx->fd < 0)
        {
            perror("open");
            delete ctx;
            fi->fh = 0;
            return -EIO;
        }
        return 0;
    }
    catch (Git::Error e)
    {
        return e.unixError();
    }
}

static int sfs_release(const char *path, struct fuse_file_info *fi)
{
    OpenContext *ctx = (OpenContext *)(void *)fi->fh;
    // TODO(twd2): commit
    delete ctx;
    return 0;
}

static int sfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    OpenContext *ctx = (OpenContext *)(void *)fi->fh;
    int ret;
    if ((ret = lseek(ctx->fd, offset, SEEK_SET)) < 0) return ret;
    return read(ctx->fd, buf, size);
}

static int sfs_write(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    OpenContext *ctx = (OpenContext *)(void *)fi->fh;
    int ret;
    if ((ret = lseek(ctx->fd, offset, SEEK_SET)) < 0) return ret;
    if ((ret = write(ctx->fd, buf, size)) < 0) return ret;
    // TODO(twd2): commit
    return 0;
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

    Json config;
    {
        std::ifstream configFile(argv[1]);
        configFile >> config;
    } // Here the file closes

    git = new Git(config["git_path"].get<std::string>()); // Will not be deleted

    std::vector<std::string> fuseArgs = config["fuse_args"];
    fuseArgs.insert(fuseArgs.begin(), ""); // argv[0] is command name
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

    return fuse_main(fuseArgc, fuseArgv, &sfs_ops, NULL);
}

