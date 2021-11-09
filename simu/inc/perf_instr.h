#ifndef __PERF_INSTR__H_
#define __PERF_INSTR__H_

//#include "pintdev.h"

#define NO_INSTRUMENT __attribute__((no_instrument_function))

#define MY_DUMP(func, call)                                                                        \
    pint_printf((char *)"\n%s: func=%x, called by=%x\n", __FUNCTION__, (unsigned int)func,         \
                (unsigned int)call);

void NO_INSTRUMENT inline __cyg_profile_func_enter(void *this_func, void *call_site) {
    MY_DUMP(this_func, call_site);
}

void NO_INSTRUMENT inline __cyg_profile_func_exit(void *this_func, void *call_site) {
    MY_DUMP(this_func, call_site);
}

#endif
