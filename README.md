# xv6-labs-2025

这是我用于学习 [Mit 6.1810(原 6.828) 2025 Fall](https://pdos.csail.mit.edu/6.828/2025/schedule.html) 操作系统课程的 xv6 实验仓库。项目基于 RISC-V 版本的 xv6，实验内容围绕进程、系统调用、文件系统、锁、虚拟内存和网络等操作系统核心机制展开。

## 环境搭建

我使用的环境是 Ubuntu 24.04，并按照[官方指引](https://pdos.csail.mit.edu/6.828/2025/tools.html) 安装了 RISC-V 工具链和 QEMU。

```sh
sudo apt-get install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

## 快速开始

可以通过以下命令编译并启动 xv6：

```sh
make qemu
```

进入 xv6 后可以使用 `ls`、`cat`、`echo`、`grep` 等用户程序。按 `Ctrl-a`，再按 `x` 可以退出 QEMU。

## 实验

所有实验的代码都在 `user/` 目录下。完成实验后，需要将新增的代码文件添加到 `Makefile` 中的 `UPROGS` 变量中，以便编译器将其编译为 xv6 的用户程序。

例如：

```makefile
UPROGS=\
    _user/_cat\
    _user/_sleep\
```

然后就可以运行评分器检查实验结果啦。

运行 util 实验的全部评分：

```sh
make grade
```

只运行某一组测试：

```sh
./grade-lab-util sleep
# 或者 make GRADEFLAGS=sleep grade
```

## 相关资源

- 课程主页: [https://pdos.csail.mit.edu/6.828/2025/schedule.html](https://pdos.csail.mit.edu/6.828/2025/schedule.html)

- xv6 官方仓库: [https://github.com/mit-pdos/xv6-riscv](https://github.com/mit-pdos/xv6-riscv)

- xv6 英文教材: [https://pdos.csail.mit.edu/6.828/2025/xv6/book-riscv-rev5.pdf](https://pdos.csail.mit.edu/6.828/2025/xv6/book-riscv-rev5.pdf)

- xv6 教材仓库：[https://github.com/mit-pdos/xv6-riscv-book](https://github.com/mit-pdos/xv6-riscv-book)

- xv6 中文教材: [https://obsidian-10.gitbook.io/xv6-book-zh](https://obsidian-10.gitbook.io/xv6-book-zh)

## 致谢

感谢 [MIT 6.1810](https://pdos.csail.mit.edu/6.828/2025/schedule.html) 提供的课程资源。

