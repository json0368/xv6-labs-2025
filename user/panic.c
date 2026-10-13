#include "kernel/types.h"                                                                                                                          
#include "kernel/stat.h"                                                                                                                           
#include "user/user.h"                                                                                                                             
                                                                                                                                                    
int main(void) {                                                                                                                                   
    char *cur = 0;                                                                                                                                   
    // 反复 lazy sbrk，把 p->sz 抬到 256GB(MAXVA) 以上，不分配任何物理页                                                                             
    for (int i = 0; i < 300; i++) {                                                                                                                  
        cur = sbrklazy(0x7fffffff);                                                                                                                    
        if (cur == SBRK_ERROR) { 
            printf("sbrklazy failed at %d\n", i); 
            exit(1); 
        }                                                                      
    }                                                                                                                                                
    printf("sz grown past MAXVA, old break=%lx\n", (unsigned long)cur);                                                                              
                                                                                                                                                    
    // 访问一个非规范地址：>= MAXVA 但仍然 < p->sz                                                                                                   
    volatile char c = *(volatile char *)0x4000000000UL;                                                                                              
    printf("read %d (kernel did not panic)\n", c);                                                                                                   
    exit(0);                                                                                                                                         
}  