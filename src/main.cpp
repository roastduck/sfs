#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

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
    auto list = git->listDir(path);
    for (const auto &item : list)
        filler(buf, item.name.c_str(), &item.stat, 0 /* Offset disabled */);
    return 0;
}

static int sfs_getattr(const char *path, struct stat* st)
{
    *st = git->getAttr(path).stat;
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

    return fuse_main(fuseArgc, fuseArgv, &sfs_ops, NULL);
}

