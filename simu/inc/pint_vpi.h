#ifndef PINT_VPI_H
#define PINT_VPI_H
/*******************************************************************************
*                                                                              *
*  PINT SIMU VPI INTERFACE                                           *
*                                                                              *
*******************************************************************************/

#include <stdarg.h>
#ifdef CPU_MODE
#include <stdio.h>
#include "cpu_adapt.h"
#else
#include "pintdev.h"
#endif

/**
 * rtl simu vpi types
 */
typedef enum {
    PINT_PRINTF = 0,
    PINT_DISPLAY,
    PINT_DUMP_FILE,
    PINT_DUMP_VAR,
    PINT_WRITE,
    PINT_FDISPLAY,
    PINT_FWRITE,
    PINT_SEND_MSG,
    PINT_USER = 99
} vpi_type;

typedef enum {
    PINT_VALUE = 0,
    PINT_SIGNAL,
    PINT_STRING,
} val_type;

typedef enum {
    PINT_UNSIGNED = 0,
    PINT_SIGNED,
} vpi_signed_flag;
#define PINT_STATIC_ARRAY_SIZE 64

#ifndef CPU_MODE
#define PRINT_LOCK() asm volatile("uap.lkrt.1")
#define PRINT_UNLOCK() asm volatile("uap.lkrt.0")
#define PRINT_CHAR(addr, ch) printCHAR(addr, ch)
#define PRINT_UINT(addr, x) printUINT(addr, x)
#define PRINT_STR(addr, s) printSTR(addr, s)
#endif

extern unsigned random_num;
// extern unsigned int* gperf_calltimes_per_ncore;
// extern perf_event_s* gperf_total_event;
// extern int gperf_ncore_num_to_perf;

void pint_simu_vpi(vpi_type type, ...);
void longlong_decimal_to_string(char *out, unsigned long long *data);
void pint_vcddump_char(unsigned char sig, unsigned size, const char *symbol);
void pint_vcddump_int(unsigned int *sig, unsigned size, const char *symbol);
void pint_vcddump_char_without_check(unsigned char sig, unsigned size, const char *symbol);
void pint_vcddump_int_without_check(unsigned int *sig, unsigned size, const char *symbol);
void pint_vcddump_value(simu_time_t *cur_T);
#ifndef CPU_MODE
void pint_profdata_send();
#endif
int pint_random(void);
unsigned int pint_urandom(void);
void pint_vcddump_off(unsigned size, const char *symbol);

#endif // PINT_VPI_H
