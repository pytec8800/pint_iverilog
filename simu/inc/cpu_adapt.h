#ifndef __CPU_ADAPT_H__
#define __CPU_ADAPT_H__

#include <time.h>

#define pint_printf printf
#define Err_print printf

int pint_atomic_add(volatile int *addr, int val);
void pint_atomic_swap(volatile int *addr, int &val);
unsigned int pint_core_id();
unsigned int pint_core_num();
void pint_mcore_cycle_get(unsigned int *cycles);
void pint_ncore_cycle_get(unsigned int *cycles);
unsigned pint_enqueue_flag_set(unsigned int func_id);
unsigned pint_enqueue_flag_set_any(unsigned short *buf, unsigned int func_id);
void pint_enqueue_flag_clear(unsigned int func_id);
void pint_enqueue_flag_clear_any(unsigned short *buf, unsigned int func_id);
void printCHAR(unsigned int *addr, int c);
void printSTR(unsigned int *addr, char *s);
void printUINT(unsigned int *addr, unsigned int x);
void pint_msg_process(unsigned dev_id, unsigned char *data);
void parse_sys_args(int argc, char *argv[]);
void host_malloc_sys_args();
void host_free_sys_args();

extern char print_str[];
void pint_simu_log(unsigned dev_id, char *log, unsigned len);
void construct_printCHAR(int c);
void construct_printUINT(unsigned int x);
void construct_printSTR(char *s);

#define PRINT_LOCK() print_str[0] = NULL
#define PRINT_UNLOCK() // printf("%s\n",print_str)
#define PRINT_CHAR(addr, ch) construct_printCHAR(ch)
#define PRINT_UINT(addr, x) construct_printUINT(x)
#define PRINT_STR(addr, s) construct_printSTR(s)

#endif
