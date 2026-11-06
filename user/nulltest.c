#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
    printf("about to deref null\n");
    *(volatile int *)0 = 1;
    printf("ERROR: survived\n");
    exit(0);
}
