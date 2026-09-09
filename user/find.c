#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[]) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if (argc < 3) {
        fprintf(2, "Usage: find [path] [filename]\n");
        exit(1);
    }

    if ((fd = open(argv[1], O_RDONLY)) < 0) {
        fprintf(2, "find: cannot open %s\n", argv[1]);
        exit(1);
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", argv[1]);
        close(fd);
        exit(1);
    }

    switch (st.type) {
        case T_DEVICE: // 设备文件
        case T_FILE: {
            for(p = argv[1] + strlen(argv[1]); p >= argv[1] && *p != '/'; p--);
            p++;
            memmove(buf, p, strlen(p));
            buf[strlen(p)] = '\0';
            if (strcmp(buf, argv[2]) == 0) {
                printf("%s\n", argv[1]);
            }
            close(fd);
            break;
        }
        case T_DIR: {
            if(strlen(argv[1]) + 1 + DIRSIZ + 1 > sizeof(buf)){
                printf("find: path too long\n");
                close(fd);
                break;
            }
            strcpy(buf, argv[1]);
            p = buf + strlen(buf);
            *p++ = '/';
            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                if (de.inum == 0) {
                    continue;
                }
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = '\0';
                
                if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0) {
                    continue;
                }

                if (stat(buf, &st) < 0) {
                    fprintf(2, "find: cannot stat %s\n", buf);
                    continue;
                }

                char *new_argv[] = {argv[0], buf, argv[2]};
                main(3, new_argv);
            }
            close(fd);
            break;
        }
    }
}