#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // Your code here.
  char *base = sbrk(1 << 20);   // 申请 1MB 内存
  char *help = "This may help.";
  int len = 15; 
  for (int i = 0; i + 16 + 8 < (1 << 20); i++) {
    int found = 1;
    // 判断是否匹配
    for (int j = 0; j < len; j++) {
      if (base[i + j] != help[j]) {
        found = 0;
        break;
      }
    }
    if (found) {
      // 提取 secret
      char secret[9];
      for (int j = 0; j < 8; j++) secret[j] = base[i + 16 + j];
      secret[8] = '\0';
      printf("%s\n", secret);
      exit(0);
    }
  }
  exit(1);
}
