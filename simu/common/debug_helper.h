#ifndef _PINT_DEBUG_HELPER_H__
#define _PINT_DEBUG_HELPER_H__

#include "pintdev.h"

#define __DEBUG 0
#define __pe_id (0)
#define __core_id (0)

#define __DebugBegin(level)                                                                        \
    if                                                                                             \
        constexpr(level == __DEBUG) {                                                              \
            if ((pint_pe_id() == __pe_id) && (pint_core_id_in_pe() == __core_id)) {
#define __DebugEnd                                                                                 \
    }                                                                                              \
    }

#define DBG_print_int(signed_int32_var)                                                            \
    pint_printf((char *)#signed_int32_var " = %d\n", (int)signed_int32_var);

#define DBG_print_float(f32_var) pint_printf((char *)#f32_var " = %f\n", pint_f2u(f32_var));

template <typename T> class TD; // for compile time debug.

#define __DEBUG_M 1

#define __DebugBeginM(level)                                                                       \
    if                                                                                             \
        constexpr(level == __DEBUG_M) {
#define __DebugEndM }

#define DBG_print_int_array(int32_array, cnt)                                                      \
    for (size_t i = 0; i < cnt; ++i) {                                                             \
        pint_printf((char *)#int32_array "[%d] = 0x%x.\n", i, int32_array[i]);                     \
    }

#endif
