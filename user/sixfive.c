#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: sixfive [FILE ...]\n");
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        char *file_name = argv[i];
        
        int fd = open(file_name, O_RDONLY);
        if (fd < 0) {
            fprintf(2, "Error: Cannot open file %s\n", file_name);
            continue;
        }
        
        char c;
        int check = 1;
        int cur = 0;
        while (read(fd, &c, 1) > 0) {
            if (c == '-' || c == '\r' || c == '\t' || c == '\n' || c == '.' || c == '/' || c == ',') {
                if (check) {
                    if (cur % 6 == 0 || cur % 5 == 0) {
                        printf("%d\n", cur);
                    }
                }
                check = 1;
                cur = 0;
            } else if (c >= '0' && c <= '9') {
                cur = cur * 10 + (c - '0');
            } else {
                check = 0;
            }
        }
        if (check) {
            if (cur % 6 == 0 || cur % 5 == 0) {
                printf("%d\n", cur);
            }
        }
        close(fd);
    }
}