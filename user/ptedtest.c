#include "kernel/types.h"
#include "kernel/riscv.h"
#include "user/user.h"

int
main(void)
{
  char *mem;
  uint64 dirty_pages[8];
  uint64 start;
  int n;

  printf("ptedtest: starting\n");

  mem = sbrk(3 * PGSIZE);
  if(mem == (char *)-1) {
    printf("ptedtest: sbrk failed\n");
    exit(1);
  }

  start = (uint64)mem;

  /*
   * sbrk分配页面时，内核可能通过memset初始化页面，
   * 因此先查询一次并清除已有的PTE_D。
   */
  n = dirty(dirty_pages, start, start + 3 * PGSIZE, 1);
  printf("initial dirty pages: %d\n", n);

  /*
   * 只修改第0页和第2页。
   */
  mem[0] = 1;
  mem[2 * PGSIZE] = 3;

  n = dirty(dirty_pages, start, start + 3 * PGSIZE, 1);

  printf("dirty pages after writes: %d\n", n);

  if(n != 2) {
    printf("ptedtest: expected 2 dirty pages, got %d\n", n);
    exit(1);
  }

  if(dirty_pages[0] != start ||
     dirty_pages[1] != start + 2 * PGSIZE) {
    printf("ptedtest: wrong dirty page addresses\n");
    printf("page 0: 0x%lx\n", dirty_pages[0]);
    printf("page 1: 0x%lx\n", dirty_pages[1]);
    exit(1);
  }

  /*
   * 上一次调用传入了clear=1，因此PTE_D应该已经被清除。
   * 期间没有再次写入，第二次查询应该返回0。
   */
  n = dirty(dirty_pages, start, start + 3 * PGSIZE, 1);

  if(n != 0) {
    printf("ptedtest: dirty bits were not cleared\n");
    exit(1);
  }

  printf("ptedtest: PTE_D test passed\n");
  exit(0);
}