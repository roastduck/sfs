# 增量型安全文件系统 sfs

## 引言

文件系统是计算机系统中为用户提供数据存储服务的一个很好的选择。然而，根据我们有限的调研，大部分通用文件系统在设计上没有特别考虑文件的永久性存储。这里的永久性指的是，文件及其每一个版本都在底层存储介质上永久保存。因此，“删库跑路”等事故时有发生。例如，一个对高层不满的系统管理员可能会通过运行`rm -rf /`命令来删除系统中的所有文件，从而表达他的不满。由于被上述命令删除的文件不难恢复，所以，稍有经验的系统管理员可能还会向文件系统写入垃圾数据来覆盖实际上未被清除的数据。此外，菜鸟系统管理员亦可能受到他人的教唆而运行这样的命令，为企业带来损失。可见，一个能够对文件永久性存储的文件系统是必要的。

为此，考虑到git [Git. 2018]是一个能够跟踪每一个文件版本的版本控制系统，我们在git的基础上提出了一种新型的增量型安全文件系统。

本文分为这样几个部分：“相关工作”介绍了前人的已有工作，“设计与实现”介绍了我们的设计与实现思路，“实验”部分给出了对于我们的方法的性能测试以及讨论。最后，“结论”一节对全文进行了总结。

## 相关工作

Git [Git. 2018]是一个能够跟踪每一个文件版本的版本控制系统。gitfs [presslabs. 2018]也是一个基于git的文件系统，不过其核心功能在于让用户能够很方便地以文件系统的方式来访问git仓库及其各个版本和分支。微软公司提出的GVFS [Microsoft. 2018]同样是基于git的文件系统，他们的关注点则在于访问大仓库时的性能，以及减少网络传输的数据量。

## 设计与实现

整体系统使用用户态文件系统libfuse实现。

根据安全性的不同，版本控制的策略也可以有不同的选择。对于文件，我们的设计目前支持两种版本控制策略：

1. `commit-on-close`：文件每次被关闭时，将缓冲区中的文件数据提交到git仓库。
2. `commit-on-write`：文件每次被写入后，将缓冲区中的文件数据提交到git仓库。

对于需要平衡安全性和性能的场景而言，`commit-on-close`是一个可能的选择；而对于安全性较高的场合，`commit-on-write`似乎更值得尝试。

对于其他写操作，我们把每一次操作都提交到git仓库。

## 实验

### 实验环境

* `libfuse`：版本2.9.4，使用`apt install libfuse-dev`安装
* `libgit2`：版本0.24.1，使用`apt install libgit2-dev`安装
* 其他必要的GCC工具链

### Micro-Benchmark



### Macro-Benchmark



## 结论

但我们的方法也有缺点：我们的文件系统对于底层存储设备空间的占用只增不减，在某些情况下这可能成为经济压力。

## 参考文献

* Git. 2018. *git*. https://git-scm.com/
* presslabs. 2018. *gitfs*. https://github.com/presslabs/gitfs
* Microsoft. 2018. *GVFS*. https://github.com/Microsoft/GVFS