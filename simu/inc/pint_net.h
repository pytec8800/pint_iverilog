#ifndef PINT_NET_H
#define PINT_NET_H

#ifdef USE_CXX11
#define __constexpr
#else
#define __constexpr constexpr
#endif

#include <stdbool.h>
#include <array>

#ifdef CPU_MODE
#include <stdio.h>
#include <assert.h>
#include <cstring>
#else
#include "pintdev.h"
#include "../common/debug_helper.h"
#endif

#include "../inc/pint_simu.h"
#include "../common/perf.h"
#include "pint_code_macro.h"

//#define _DBG_INFO_
//#define _DBG_ASSERT_

typedef enum {
    DEBUG_LEVEL_INFO = 1,
    DEBUG_LEVEL_ASSERT = 2,
} DEBUG_LEVEL;

typedef enum {
    VALUE_MAX = (int)0x7fffffff,
    VALUE_MIN = (int)0x80000001,
    VALUE_XZ = (int)0x80000000,
} VALUE_SPECIAL;

#define LOG_LEVEL DEBUG_LEVEL_INFO | DEBUG_LEVEL_ASSERT

#ifdef _DBG_INFO_
#define pint_info(format, args...)                                                                 \
    do {                                                                                           \
        if (DEBUG_LEVEL_INFO & (LOG_LEVEL)) {                                                      \
            printf("[Log]@%s:%d: " format " \n", __FUNCTION__, __LINE__, ##args);                  \
        }                                                                                          \
    } while (0)
#else
#define pint_info(format, args...)
#endif

#ifdef _DBG_ASSERT_
#define pint_assert(state, format, args...)                                                        \
    do {                                                                                           \
        if ((!(state)) && (DEBUG_LEVEL_ASSERT & (LOG_LEVEL))) {                                    \
            printf("[Assert fail(%s)]@%s:%d: " format " \n", #state, __FUNCTION__, __LINE__,       \
                   ##args);                                                                        \
        }                                                                                          \
    } while (0)
#else
#define pint_assert(state, format, args...)
#endif

#define pint_print_line() printf("@@ %s into line %d\n", __FUNCTION__, __LINE__)

#define PINT_CHAR_OFFSET 4
#define PINT_INT_SIZE 32
#define PINT_INT_MASK 0XFFFFFFFF
#define PINT_CHAR_MASK 0XFF
#define PINT_COMPARE_BIGGER 0
#define PINT_COMPARE_EQUAL 1
#define PINT_COMPARE_LESS 2
#define PINT_COMPARE_X 3
#define BIT_0 0
#define BIT_1 1
#define BIT_X 3
#define BIT_Z 2
#define BIT_0_CHAR 0x00
#define BIT_1_CHAR 0x01
#define BIT_X_CHAR 0x11
#define BIT_Z_CHAR 0x10

#ifndef USE_CXX11
#define COPY_USE_TPL
#endif

#define USE_INLINE

#ifndef USE_INLINE
#ifdef __cplusplus
extern "C" {
#endif
#endif

/*
 * value||L_bits|H_bits|
 * ======================
 * | 0  ||  0   |   0  |
 * | 1  ||  1   |   0  |
 * | x  ||  1   |   1  |
 * | z  ||  0   |   1  |
 *
 * width||type    |H/L
 * ===========================================================
 * 1-4  ||char    |L_bits:3-0
 *      ||        |H_bits:7-4
 * -----------------------------------------------------------
 * >4   ||unsigned|L_bits:(size-1)/32 unsigned int
 *      ||int*a   |H_bits:the next (size-1)/32 unsigned int
 * -----------------------------------------------------------
 * for width>5, we use unsigned int pairs to store the value
 * for example, for width = 18,the L_bits and H_bits for
 * unsigned int *a in the memory is showed below:
 *                   a+1                               a
 * ----------------------------------------------------------------------
 *    |             32bits             |             32bits             |
 * ----------------------------------------------------------------------
 *    | 14bits zeros |  18bits H_bits  | 14bits zeros |  18bits L_bits  |
 *
 * for width = 36,the L_bits and H_bits for
 * unsigned int *a in the memory is showed below:
 *                   a+1                               a
 * ----------------------------------------------------------------------
 *    |             32bits             |             32bits             |
 * ----------------------------------------------------------------------
 *    | 28bits zeros |   4bits L_bits  |          32bits L_bits         |
 *
 *                   a+3                               a+2    s
 * ----------------------------------------------------------------------
 *    |             32bits             |             32bits             |
 * ----------------------------------------------------------------------
 *    | 28bits zeros |   4bits H_bits  |          32bits H_bits         |
 */

template <bool _nxz = false>
void pint_mask_copy_char(unsigned char *out, const unsigned char *in0, const unsigned char *mask);
template <bool _nxz = false>
void pint_mask_copy_int(unsigned int *out, const unsigned int *in0, const unsigned int *mask,
                        unsigned int width);
template <bool _nxz = false>
bool pint_mask_case_equality_int(unsigned int *in0, const unsigned int *in1,
                                 const unsigned int *mask, unsigned int width);
void pint_set_mask(unsigned int *out, unsigned int updt_base, unsigned int updt_size,
                   unsigned int width);
void pint_set_mult_arr_updt_word(unsigned int *updt, unsigned int word);
void pint_set_sig_x(void *buf, unsigned int width);
void pint_set_sig_z(void *buf, unsigned int width);
//  Function:   out[base+size-1, base] = in;
//  Character:	size(The width of signal in0), size_out(The width of signal out)
template <bool _nxz = false>
void pint_set_subarray_char(unsigned char *out, const unsigned char *in0, unsigned int base,
                            unsigned int size, unsigned int size_out);
template <bool _nxz = false>
void pint_set_subarray_char_value(unsigned char *out, unsigned char in0, unsigned int base,
                                  unsigned int size);
template <bool _nxz = false>
void pint_set_subarray_int_char(unsigned int *out, const unsigned char *in0, unsigned int base,
                                unsigned int size, unsigned int size_out);
template <bool _nxz = false>
void pint_set_subarray_int_char_value(unsigned int *out, unsigned char in0, unsigned int base,
                                      unsigned int size, unsigned int size_out);
template <bool _nxz = false>
void pint_set_subarray_int(unsigned int *out, const unsigned int *in0, unsigned int base,
                           unsigned int size, unsigned int size_out);
template <bool _nxz = false>
void pint_set_subarray_int_value(unsigned int *out, unsigned int in0, unsigned int base,
                                 unsigned int size, unsigned int size_out);

template <bool _nxz = false>
void pint_get_subarray_char_char(unsigned char *out, const unsigned char *in0, int base, int size,
                                 int size_in);
template <bool _nxz = false>
void pint_get_subarray_char_int(unsigned int *out, const unsigned char *in0, int base, int size,
                                int size_in);
template <bool _nxz = false>
void pint_get_subarray_int_char(unsigned char *out, const unsigned int *in0, int base, int size,
                                int size_in);
template <bool _nxz = false>
void pint_get_subarray_int_int(unsigned int *out, const unsigned int *in0, int base, int size,
                               int size_in);
void pint_copy_bits_common(void *out, const void *in0, int base, int size, int size_in);
template <bool _nxz = false> void pint_set_sig_zero(void *buf, unsigned width);
void pint_merge_multi_drive(void *pint_out, unsigned *muldrv_table, int idx, int base, int size,
                            int sig_type);
void pint_finish_multi_drive(unsigned *muldrv_table, int mul_drv_num);

//  Function:   get_value(exclude x, z);
//  Character:	3'b01z => 0, 3'b01x => 0;
template <bool _nxz = false> int pint_get_value_char_s(const unsigned char *in, unsigned char size);
template <bool _nxz = false> int pint_get_value_char_u(const unsigned char *in, unsigned char size);
template <bool _nxz = false> int pint_get_value_int_s(const unsigned int *in, unsigned int size);
template <bool _nxz = false> int pint_get_value_int_u(const unsigned int *in, unsigned int size);

//  Function:   copy
void pint_copy_char(unsigned char *out, const unsigned char *in0, unsigned int size);
template <bool _nxz = false>
void pint_copy_int(unsigned int *out, const unsigned int *in0, unsigned int size);
template <bool _nxz = false> // _nxz = true
void pint_copy_value(unsigned int *out, unsigned int in0, unsigned int size);

//  Function: these functions are used to set each bit of a signal; set to 0/1/x/z
/*
 * #define BIT_0 0
 * #define BIT_1 1
 * #define BIT_X 3
 * #define BIT_Z 2
 */
void pint_set_char(unsigned char *out, unsigned int size, unsigned int val);
template <bool _nxz = false> // _nxz = true
void pint_set_int(unsigned int *out, unsigned int size, unsigned int val);

//  Function: these functions are used to set only 1 bit of a signal
void pint_set_bit_char(unsigned char *out, unsigned int place, unsigned int val); // place,
void pint_set_bit_int(unsigned int *out, unsigned int place, unsigned int size, unsigned int val);

//  Function: pad, broaden the width of a signal
template <bool _nxz = false>
void pint_pad_char_to_char_s(unsigned char *out, const unsigned char *in0, unsigned int size_in,
                             unsigned int size_out);
template <bool _nxz = false>
void pint_pad_char_to_int_s(unsigned int *out, const unsigned char *in0, unsigned int size_in,
                            unsigned int size_out);
template <bool _nxz = false>
void pint_pad_int_to_int_s(unsigned int *out, const unsigned int *in0, unsigned int size_in,
                           unsigned int size_out);
template <bool _nxz = false>
void pint_pad_char_to_int_u(unsigned int *out, const unsigned char *in0, unsigned int size_in,
                            unsigned int size_out);
template <bool _nxz = false>
void pint_pad_int_to_int_u(unsigned int *out, const unsigned int *in0, unsigned int size_in,
                           unsigned int size_out);

//  Function: reduce the width of a signal
template <bool _nxz = false>
void pint_reduce_char_to_char(unsigned char *out, const unsigned char *in0, unsigned int size_in,
                              unsigned int size_out);
template <bool _nxz = false>
void pint_reduce_int_to_char(unsigned char *out, const unsigned int *in0, unsigned int size_in,
                             unsigned int size_out);
template <bool _nxz = false>
void pint_reduce_int_to_int(unsigned int *out, const unsigned int *in0, unsigned int size_in,
                            unsigned int size_out);

//  Function: these functions are used in "if()" in verilog.
//            if *in0 is not 0,x,z,return true, else return false.
template <bool _nxz = false> bool pint_is_true_char(const unsigned char *in0, unsigned int size);
template <bool _nxz = false> bool pint_is_true_int(const unsigned int *in0, unsigned int size);
bool pint_is_xz_char(unsigned char *in0, unsigned int size);
bool pint_is_xz_int(unsigned int *in0, unsigned int size);

/************************************************** <Arithmetic Operator>
 * **************************************************/

unsigned int add_with_carry(unsigned int a, unsigned int b, unsigned int *carry);
unsigned int multiply_with_carry(unsigned int a, unsigned int b, unsigned int *carry);

//  Function: out = in0 + in1
template <bool _nxz = false>
void pint_add_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                     unsigned int size);
template <bool _nxz = false>
void pint_add_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                     unsigned int size);
template <bool _nxz = false>
void pint_add_int_u(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                    unsigned int size);
template <bool _nxz = false>
void pint_add_int_s(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                    unsigned int size);

//  Function: out = in0 - in1
template <bool _nxz = false>
void pint_sub_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                     unsigned int size);
template <bool _nxz = false>
void pint_sub_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                     unsigned int size);
template <bool _nxz = false>
void pint_sub_int_u(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                    unsigned int size);
template <bool _nxz = false>
void pint_sub_int_s(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                    unsigned int size);

// Function: ino-=x
void pint_decrement_char_u(unsigned char *in0, unsigned char decrement_val, unsigned int size);
void pint_decrement_int_u(unsigned int *in0, unsigned decrement_val, unsigned int size);

//  Function: out = in0 * in1
template <bool _nxz = false>
void pint_mul_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                     unsigned int size);
template <bool _nxz = false>
void pint_mul_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                     unsigned int size);
template <bool _nxz = false>
void pint_mul_int_u(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                    unsigned int size);
template <bool _nxz = false>
void pint_mul_int_s(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                    unsigned int size);

//  Function: out = in0 / in1
template <bool _nxz = false>
void pint_divided_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                         unsigned int size);
template <bool _nxz = false>
void pint_divided_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                         unsigned int size);
template <bool _nxz = false>
void pint_divided_int_u(unsigned int *quotient, const unsigned int *dividend,
                        const unsigned int *divisor, unsigned int size);
template <bool _nxz = false>
void pint_divided_int_s(unsigned int *quotient, const unsigned int *dividend,
                        const unsigned int *divisor, unsigned int size);

//  Function: out = in0 % in1
template <bool _nxz = false>
void pint_modulob_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                         unsigned int size);
template <bool _nxz = false>
void pint_modulob_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                         unsigned int size);
template <bool _nxz = false>
void pint_modulob_int_u(unsigned int *remainder, const unsigned int *dividend,
                        const unsigned int *divisor, unsigned int size);
template <bool _nxz = false>
void pint_modulob_int_s(unsigned int *remainder, const unsigned int *dividend,
                        const unsigned int *divisor, unsigned int size);

//  Function: out = in0 ** in1
void pint_power_int_u(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                      unsigned int size);
// void pint_power_int_s   (unsigned int  *out, const unsigned int  *in0, const unsigned int  *in1,
// unsigned int size);
void pint_power_int_s(unsigned int *out, int in0, int in1, unsigned int size);
template <bool _nxz = false>
void pint_power_u_common(void *out, unsigned out_size, const void *in0, unsigned in0_size,
                         const void *in1, unsigned in1_size);
template <bool _nxz = false>
void pint_power_s_common(void *out, unsigned out_size, const void *in0, unsigned in0_size,
                         unsigned in0_signed, const void *in1, unsigned in1_size,
                         unsigned in1_signed);

// Function: out = -in0
template <bool _nxz = false>
void pint_minus_char(unsigned char *out, const unsigned char *in0, unsigned int size);
template <bool _nxz = false>
void pint_minus_int(unsigned int *out, const unsigned int *in0, unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a>b",
 * We assume the width of out, in0, in1 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_greater_than_char_s(unsigned char *out, const unsigned char *in0,
                              const unsigned char *in1, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_greater_than_int_s(unsigned char *out, const unsigned int *in0, const unsigned int *in1,
                             unsigned int size);

/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_greater_than_char_u(unsigned char *out, const unsigned char *in0,
                              const unsigned char *in1, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_greater_than_int_u(unsigned char *out, const unsigned int *in0, const unsigned int *in1,
                             unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a>=b",
 * We assume the width of out, in0, in1 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_greater_than_oreq_char_s(unsigned char *out, const unsigned char *in0,
                                   const unsigned char *in1, unsigned int size);
/*9 <= size <=16.*/
template <bool _nxz = false>
void pint_greater_than_oreq_int_s(unsigned char *out, const unsigned int *in0,
                                  const unsigned int *in1, unsigned int size);

/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_greater_than_oreq_char_u(unsigned char *out, const unsigned char *in0,
                                   const unsigned char *in1, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_greater_than_oreq_int_u(unsigned char *out, const unsigned int *in0,
                                  const unsigned int *in1, unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a<b",
 * We assume the width of out, in0, in1 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_less_than_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                           unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_less_than_int_s(unsigned char *out, const unsigned int *in0, const unsigned int *in1,
                          unsigned int size);

/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_less_than_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                           unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_less_than_int_u(unsigned char *out, const unsigned int *in0, const unsigned int *in1,
                          unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a<=b",
 * We assume the width of out, in0, in1 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_less_than_oreq_char_s(unsigned char *out, const unsigned char *in0,
                                const unsigned char *in1, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_less_than_oreq_int_s(unsigned char *out, const unsigned int *in0, const unsigned int *in1,
                               unsigned int size);

/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_less_than_oreq_char_u(unsigned char *out, const unsigned char *in0,
                                const unsigned char *in1, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_less_than_oreq_int_u(unsigned char *out, const unsigned int *in0, const unsigned int *in1,
                               unsigned int size);

/*
 * these functions are used for expressions in verilog such as "!a",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_logical_invert_char(unsigned char *out, const unsigned char *in0, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_logical_invert_int(unsigned char *out, const unsigned int *in0, unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a&&b",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_logical_and_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                           unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_logical_and_int(unsigned char *out, const unsigned int *in0, const unsigned int *in1,
                          unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a||b",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_logical_or_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                          unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_logical_or_int(unsigned char *out, const unsigned int *in0, const unsigned int *in1,
                         unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a==b",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_logical_equality_char(unsigned char *out, const unsigned char *in0,
                                const unsigned char *in1, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_logical_equality_int(unsigned char *out, const unsigned int *in0, const unsigned int *in1,
                               unsigned int size);
template <bool _nxz = false>
bool pint_logical_equality_int_ret(unsigned int *in0, const unsigned int *in1, unsigned int size);
template <bool _nxz = false>
bool pint_logical_equality_int_value_ret(unsigned int *in0, unsigned int in1, unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a===b",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */

template <bool _nxz = false>
void pint_case_equality_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                             unsigned int size);
template <bool _nxz = false>
void pint_case_equality_int(unsigned char *out, const unsigned int *in0, const unsigned int *in1,
                            unsigned int size);
template <bool _nxz = false>
bool pint_case_equality_int_ret(const unsigned int *in0, const unsigned int *in1,
                                unsigned int size);
template <bool _nxz = false>
bool pint_case_equality_int_value_ret(const unsigned int *in0, unsigned int in1, unsigned int size);
bool pint_equality_ignore_z_char_ret(const unsigned char *in0, const unsigned char *in1,
                                     unsigned int size);
bool pint_equality_ignore_z_int_ret(const unsigned int *in0, const unsigned int *in1,
                                    unsigned int size);
bool pint_equality_ignore_xz_char_ret(const unsigned char *in0, const unsigned char *in1,
                                      unsigned int size);
bool pint_equality_ignore_xz_int_ret(const unsigned int *in0, const unsigned int *in1,
                                     unsigned int size);

//  Function: "a!=b"
template <bool _nxz = false>
void pint_logical_inquality_char(unsigned char *out, const unsigned char *in0,
                                 const unsigned char *in1, unsigned int size);
template <bool _nxz = false>
void pint_logical_inquality_int(unsigned char *out, const unsigned int *in0,
                                const unsigned int *in1, unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a!==b",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_case_inquality_char(unsigned char *out, const unsigned char *in0,
                              const unsigned char *in1, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_case_inquality_int(unsigned char *out, const unsigned int *in0, const unsigned int *in1,
                             unsigned int size);

//  Function: "out = sel ? in1 : in0"
template <bool _nxz = false>
void pint_mux_char_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                        const unsigned char *sel, unsigned int size_d, unsigned int size_s);
template <bool _nxz = false>
void pint_mux_char_int(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                       const unsigned int *sel, unsigned int size_d, unsigned int size_s);
template <bool _nxz = false>
void pint_mux_int_char(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                       const unsigned char *sel, unsigned int size_d, unsigned int size_s);
template <bool _nxz = false>
void pint_mux_int_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                      const unsigned int *sel, unsigned int size_d, unsigned int size_s);

/*
 * these functions are used for relation operates in verilog,
 * We assume the width of out, in0, in1 are the same,which is the
 * size input of these functions.
 * we use different return value to indecate the relation result,
 * return PINT_COMPARE_BIGGER (0):*in0>*in1 is true;
 * return PINT_COMPARE_EQUAL  (1):*in0==*in1 is true;
 * return PINT_COMPARE_LESS   (2):*in0<*in1 is true;
 * return PINT_COMPARE_X      (3):the result is x;
 *
 */
/*1 <= size <= 4.*/
unsigned int pint_compare_char_s(const unsigned char *in0, const unsigned char *in1,
                                 unsigned int size);
/*4 < size .*/
unsigned int pint_compare_int_s(const unsigned int *in0, const unsigned int *in1,
                                unsigned int size);

/*1 <= size <= 4.*/
unsigned int pint_compare_char_u(const unsigned char *in0, const unsigned char *in1,
                                 unsigned int size);
/*4 < size .*/
unsigned int pint_compare_int_u(const unsigned int *in0, const unsigned int *in1,
                                unsigned int size);

/*
 * these functions are used for expressions in verilog such as "~a",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_bitw_invert_char(unsigned char *out, const unsigned char *in0, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_bitw_invert_int(unsigned int *out, const unsigned int *in0, unsigned int size);
template <bool _nxz = false>
unsigned char pint_bitw_invert_char_ret(const unsigned char *in0, unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a&b",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_bitw_and_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                        unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_bitw_and_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                       unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a|b",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_bitw_or_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                       unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_bitw_or_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                      unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a^b",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_bitw_exclusive_or_char(unsigned char *out, const unsigned char *in0,
                                 const unsigned char *in1, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_bitw_exclusive_or_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                                unsigned int size);

//  Function: a ~& b
template <bool _nxz = false>
void pint_bitw_nand_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                         unsigned int size);
template <bool _nxz = false>
void pint_bitw_nand_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                        unsigned int size);

//  Function: a ~| b
template <bool _nxz = false>
void pint_bitw_nor_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1,
                        unsigned int size);
template <bool _nxz = false>
void pint_bitw_nor_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                       unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a~^b",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_bitw_equivalence_char(unsigned char *out, const unsigned char *in0,
                                const unsigned char *in1, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_bitw_equivalence_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                               unsigned int size);

/*
 * these functions are used for expressions in verilog such as "&a",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_reduction_and_char(unsigned char *out, const unsigned char *in0, unsigned int size);
/*16 < size.*/
template <bool _nxz = false>
void pint_reduction_and_int(unsigned char *out, const unsigned int *in0, unsigned int size);

/*
 * these functions are used for expressions in verilog such as "~&a",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_reduction_nand_char(unsigned char *out, const unsigned char *in0, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_reduction_nand_int(unsigned char *out, const unsigned int *in0, unsigned int size);

/*
 * these functions are used for expressions in verilog such as "|a",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_reduction_or_char(unsigned char *out, const unsigned char *in0, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_reduction_or_int(unsigned char *out, const unsigned int *in0, unsigned int size);

/*
 * these functions are used for expressions in verilog such as "~|a",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_reduction_nor_char(unsigned char *out, const unsigned char *in0, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_reduction_nor_int(unsigned char *out, const unsigned int *in0, unsigned int size);

/*
 * these functions are used for expressions in verilog such as "^a",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_reduction_xor_char(unsigned char *out, const unsigned char *in0, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_reduction_xor_int(unsigned char *out, const unsigned int *in0, unsigned int size);

/*
 * these functions are used for expressions in verilog such as "~^a",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_reduction_xnor_char(unsigned char *out, const unsigned char *in0, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_reduction_xnor_int(unsigned char *out, const unsigned int *in0, unsigned int size);

template <bool _nxz = false> // _nxz = true
void pint_reduction_and_int_value(unsigned char *out, unsigned int in0, unsigned int size);
template <bool _nxz = false> // _nxz = true
void pint_reduction_nand_int_value(unsigned char *out, unsigned int in0, unsigned int size);
template <bool _nxz = false> // _nxz = true
void pint_reduction_or_int_value(unsigned char *out, unsigned int in0, unsigned int size);
template <bool _nxz = false> // _nxz = true
void pint_reduction_nor_int_value(unsigned char *out, unsigned int in0, unsigned int size);
template <bool _nxz = false> // _nxz = true
void pint_reduction_xor_int_value(unsigned char *out, unsigned int in0, unsigned int size);
template <bool _nxz = false> // _nxz = true
void pint_reduction_xnor_int_value(unsigned char *out, unsigned int in0, unsigned int size);

// Function: "a << b" or "a <<< b"
template <bool _nxz = false>
void pint_lshift_char(unsigned char *out, const unsigned char *in0, unsigned int shift,
                      unsigned int size);
template <bool _nxz = false>
void pint_lshift_int(unsigned int *out, const unsigned int *in0, unsigned int shift,
                     unsigned int size);

// Function: "a >> b" or "a >>> b"
template <bool _nxz = false>
void pint_rshift_char_u(unsigned char *out, const unsigned char *in0, unsigned int shift,
                        unsigned int size);
template <bool _nxz = false>
void pint_rshift_int_u(unsigned int *out, const unsigned int *in0, unsigned int shift,
                       unsigned int size);
template <bool _nxz = false>
void pint_rshift_char_s(unsigned char *out, const unsigned char *in0, unsigned int shift,
                        unsigned int size);
template <bool _nxz = false>
void pint_rshift_int_s(unsigned int *out, const unsigned int *in0, unsigned int shift,
                       unsigned int size);

template <bool _nxz = false>
void pint_c_logic_lshift_char(unsigned char *out, const unsigned char *in0, unsigned int in1,
                              unsigned int size);
template <bool _nxz = false>
void pint_c_logic_lshift_int(unsigned int *out, const unsigned int *in0, unsigned int in1,
                             unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a>>b",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 * *in0 should be a unsigned value.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_logic_rshift_char(unsigned char *out, const unsigned char *in0, const unsigned int *in1,
                            unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_logic_rshift_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                           unsigned int size);

/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_c_logic_rshift_char(unsigned char *out, const unsigned char *in0, unsigned int in1,
                              unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_c_logic_rshift_int(unsigned int *out, const unsigned int *in0, unsigned int in1,
                             unsigned int size);
/*
 * these functions are used for expressions in verilog such as "a<<<b",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 * *in0 should be a unsigned value.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_arithmetic_lshift_char(unsigned char *out, const unsigned char *in0,
                                 const unsigned int *in1, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_arithmetic_lshift_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                                unsigned int size);

/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_c_arithmetic_lshift_char(unsigned char *out, const unsigned char *in0, unsigned int in1,
                                   unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_c_arithmetic_lshift_int(unsigned int *out, const unsigned int *in0, unsigned int in1,
                                  unsigned int size);

/*
 * these functions are used for expressions in verilog such as "a>>>b",
 * We assume the width of out, in0 are the same,which is the
 * size input of these functions.
 * *in0 should be a unsigned value.
 */
/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_arithmetic_rshift_char_s(unsigned char *out, const unsigned char *in0,
                                   const unsigned int *in1, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_arithmetic_rshift_int_s(unsigned int *out, const unsigned int *in0,
                                  const unsigned int *in1, unsigned int size);

/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_arithmetic_rshift_char_u(unsigned char *out, const unsigned char *in0,
                                   const unsigned int *in1, unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_arithmetic_rshift_int_u(unsigned int *out, const unsigned int *in0,
                                  const unsigned int *in1, unsigned int size);

/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_c_arithmetic_rshift_char_s(unsigned char *out, const unsigned char *in0, unsigned int in1,
                                     unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_c_arithmetic_rshift_int_s(unsigned int *out, const unsigned int *in0, unsigned int in1,
                                    unsigned int size);

/*1 <= size <= 4.*/
template <bool _nxz = false>
void pint_c_arithmetic_rshift_char_u(unsigned char *out, const unsigned char *in0, unsigned int in1,
                                     unsigned int size);
/*4 < size .*/
template <bool _nxz = false>
void pint_c_arithmetic_rshift_int_u(unsigned int *out, const unsigned int *in0, unsigned int in1,
                                    unsigned int size);

//{a,b,c,..}  repeat{a}
template <bool _nxz = false>
void pint_concat(void *out, const void *in0, unsigned size_out, unsigned size_in, unsigned offset);
template <bool _nxz = false>
void pint_concat_repeat(void *out, const void *in0, unsigned size_out, unsigned size_in,
                        unsigned offset, unsigned repeat);
template <bool _nxz = false>
void pint_concat_value(void *out, unsigned int in0, unsigned size_out, unsigned size_in,
                       unsigned offset);
template <bool _nxz = false>
void pint_concat_value_repeat(void *out, unsigned int in0, unsigned size_out, unsigned size_in,
                              unsigned offset, unsigned repeat);
template <bool _nxz = false>
void pint_sig_to_real_char(float *out, const unsigned char *in, unsigned size);
template <bool _nxz = false>
void pint_sig_to_real_int(float *out, const unsigned int *in, unsigned size);
template <bool _nxz = false> // _nxz = true
void pint_sig_to_real_value(float *out, unsigned int in, unsigned size);
template <bool _nxz = false>
void pint_real_to_sig_char(unsigned char *out, const float *in, unsigned size);
template <bool _nxz = false>
void pint_real_to_sig_int(unsigned int *out, const float *in, unsigned size);
int pint_copy_signal_common(const void *src, unsigned src_width, unsigned src_base, void *dst,
                            unsigned dst_width, unsigned dst_base, unsigned count);
template <bool _nxz = false> int pint_copy_char_char_same_edge(const void *src, void *dst);
template <bool _nxz = false>
int pint_copy_char_char_edge(const void *src, unsigned src_base, void *dst, unsigned dst_base,
                             unsigned count);
template <bool _nxz = false>
void pint_copy_int_int_same(const void *src, void *dst, unsigned width);
template <bool _nxz = false>
int pint_copy_int_int_same_edge(const void *src, void *dst, unsigned width);
template <bool _nxz = false>
int pint_copy_int_int_edge(const void *src, unsigned src_width, unsigned src_base, void *dst,
                           unsigned dst_width, unsigned dst_base, unsigned count);
template <bool _nxz = false>
int pint_copy_int_char_edge(const void *src, unsigned src_width, unsigned src_base, void *dst,
                            unsigned dst_base, unsigned count);
template <bool _nxz = false>
int pint_copy_char_int_edge(const void *src, unsigned src_base, void *dst, unsigned dst_width,
                            unsigned dst_base, unsigned count);

template <bool _nxz = false>
void pint_copy_char_char(const void *src, unsigned src_base, void *dst, unsigned dst_base,
                         unsigned count);
template <bool _nxz = false>
void pint_copy_int_int(const void *src, unsigned src_width, unsigned src_base, void *dst,
                       unsigned dst_width, unsigned dst_base, unsigned count);
template <bool _nxz = false>
void pint_copy_int_char(const void *src, unsigned src_width, unsigned src_base, void *dst,
                        unsigned dst_base, unsigned count);
template <bool _nxz = false>
void pint_copy_char_int(const void *src, unsigned src_base, void *dst, unsigned dst_width,
                        unsigned dst_base, unsigned count);

template <bool _nxz = false>
pint_edge_t pint_copy_sig_out_range_edge(const void *src, int src_width, int src_base, void *dst,
                                         int dst_width, int dst_base, int count);
template <bool _nxz = false>
void pint_copy_sig_out_range_noedge(const void *src, int src_width, int src_base, void *dst,
                                    int dst_width, int dst_base, int count);

template <bool _nxz = false> unsigned int pint_concat_ptr_to_int(const unsigned int *in);

//  Function:   sig to string
template <bool _nxz = false>
void pint_sig_to_string_char(char *out, unsigned char in0, unsigned int size);
template <bool _nxz = false>
void pint_sig_to_string_int(char *out, unsigned int *in0, unsigned int size);
int pint_get_string_len(unsigned char *in);

#ifndef USE_INLINE
#ifdef __cplusplus
}
#endif
#endif

#ifdef USE_INLINE
#define forceinline __inline__ __attribute__((always_inline))
#include "pint_net.inc"
#endif // USE_INLINE

#ifdef COPY_USE_TPL

//================================ begin ================================
#define USE_SHM_INT_UNROLL
#define IS_CONSTEXPR(x) (__builtin_constant_p(x))
#define __TO_CONST(x) (IS_CONSTEXPR(x) ? (x) : PINT_INT_MASK)

using signal_type_char = unsigned char;

template <unsigned int width>
using signal_type_int =
    std::array<unsigned int, (((width + 31) >> 5) << 1)>; // ((width + (32 - 1)) / 32) * 2

template <unsigned int width, unsigned int base, unsigned int count>
using signal_type =
    typename std::conditional<(width <= 4), signal_type_char, signal_type_int<width>>::type;

template <unsigned int width, unsigned int base, unsigned int count>
using signal_type_valid = typename std::conditional<
    (width <= 4), signal_type_char,
    typename std::conditional<(width <= 32), signal_type_int<width>,
                              signal_type_int<(base & 0x1f) + count>>::type>::type;

template <unsigned int base, unsigned int count, bool _nxz = false>
__constexpr unsigned char get_mask_char() {
    if
        __constexpr(!_nxz) {
            __constexpr unsigned char char_mask_l = ((0x01 << count) - 1) << base;
            __constexpr unsigned char char_mask = (char_mask_l << 4) | char_mask_l;
            return char_mask;
        }
    else {
        __constexpr unsigned char char_mask = ((0x01 << count) - 1) << base;
        return char_mask;
    }
}

template <unsigned int width, unsigned int base, unsigned int count, typename T,
          typename RT = std::array<T, ((width + 31) >> 5)>>
__constexpr RT get_mask_int_half() {
    RT mask = {0};

    if
        __constexpr(width <= 32) { mask[0] = (((unsigned long long)0x01 << count) - 1) << base; }
    else {
        // __constexpr unsigned int start = base;
        // __constexpr unsigned int start_idx = start >> 5;  // start / 32
        // __constexpr unsigned int start_mod = start & 0x1f;  //start % 32
        __constexpr unsigned int start_idx = base >> 5;   // start / 32
        __constexpr unsigned int start_mod = base & 0x1f; // start % 32
        __constexpr unsigned int start_mask = ~(((unsigned int)0x01 << start_mod) - 1);

        __constexpr unsigned int end = base + count - 1;
        __constexpr unsigned int end_idx = end >> 5;
        __constexpr unsigned int end_mod = end & 0x1f;
        __constexpr unsigned int end_mask = ((unsigned long long)0x01 << (end_mod + 1)) - 1;

        if
            __constexpr(end_idx == start_idx) { mask[end_idx] = end_mask & start_mask; }
        else {
            mask[start_idx] = start_mask;
            mask[end_idx] = end_mask;
            for (int i = start_idx + 1; i <= end_idx - 1; i++) {
                mask[i] = PINT_INT_MASK;
            }
        }
    }

    return mask;
}

template <bool dst_whole, bool src_whole, unsigned char mask>
inline void shm_char(unsigned char *dst, unsigned char src) {
    if
        __constexpr(dst_whole) {
            if
                __constexpr(src_whole) { dst[0] = src; }
            else {
                dst[0] = src & mask;
            }
        }
    else {
#ifdef CPU_MODE
        *dst = (*dst & ~mask) | (src & mask);
#else
        unsigned int rs2;
        if ((unsigned int)dst & 0x01) {
            rs2 = (mask << 24) | (src << 8);
        } else {
            rs2 = (mask << 16) | src;
        }
        asm volatile("uap.shm\t%1, %0" : "+m"(*dst), "+r"(rs2));
#endif
    }
}

template <bool dst_whole, bool src_whole>
inline void shm_char_common(unsigned char mask, unsigned char *dst, unsigned char src) {
    if
        __constexpr(dst_whole) {
            if
                __constexpr(src_whole) { dst[0] = src; }
            else {
                dst[0] = src & mask;
            }
        }
    else {
#ifdef CPU_MODE
        dst[0] = (dst[0] & ~mask) | (src & mask);
#else
        unsigned int rs2;
        if ((unsigned int)dst & 0x01) {
            rs2 = (mask << 24) | (src << 8);
        } else {
            rs2 = (mask << 16) | src;
        }
        asm volatile("uap.shm\t%1, %0" : "+m"(*dst), "+r"(rs2));
#endif
    }
}

template <bool dst_whole, bool src_whole, unsigned int mask>
inline void shm_int(unsigned int *dst, unsigned int src) {
    if
        __constexpr(dst_whole) {
            if
                __constexpr(src_whole) { dst[0] = src; }
            else {
                dst[0] = src & mask;
            }
        }
    else if
        __constexpr(mask == PINT_INT_MASK) { dst[0] = src; }
    else {
#ifdef CPU_MODE
        dst[0] = (dst[0] & ~mask) | (src & mask);
#else
        __constexpr unsigned int mask_l = mask << 16;
        __constexpr unsigned int mask_h = mask & 0xffff0000;
        unsigned short *dst_short = (unsigned short *)dst;
        unsigned int rs2;

        if
            __constexpr(mask_l == 0xffff0000) { dst_short[0] = (unsigned short)src; }
        else {
            rs2 = mask_l | (src & 0xffff);
            asm volatile("uap.shm\t%1, %0" : "+m"(*dst_short), "+r"(rs2));
        }

        if
            __constexpr(mask_h == 0xffff0000) { dst_short[1] = (unsigned short)(src >> 16); }
        else {
            rs2 = mask_h | (src >> 16);
            asm volatile("uap.shm\t%1, %0" : "+m"(*(dst_short + 1)), "+r"(rs2));
        }
#endif
    }
}

template <bool dst_whole, bool src_whole>
inline void shm_int_common(unsigned int mask, unsigned int *dst, unsigned int src) {
    if
        __constexpr(dst_whole) {
            if
                __constexpr(src_whole) { dst[0] = src; }
            else {
                dst[0] = src & mask;
            }
        }
    else {
        if (mask == PINT_INT_MASK) {
            dst[0] = src;
        } else {
#ifdef CPU_MODE
            dst[0] = (dst[0] & ~mask) | (src & mask);
#else
            unsigned int mask_l = mask << 16;
            unsigned int mask_h = mask & 0xffff0000;
            unsigned short *dst_short = (unsigned short *)dst;
            unsigned int rs2;

            if (mask_l == 0xffff0000) {
                dst_short[0] = (unsigned short)src;
            } else {
                rs2 = mask_l | (src & 0xffff);
                asm volatile("uap.shm\t%1, %0" : "+m"(*dst_short), "+r"(rs2));
            }

            if (mask_h == 0xffff0000) {
                dst_short[1] = (unsigned short)(src >> 16);
            } else {
                rs2 = mask_h | (src >> 16);
                asm volatile("uap.shm\t%1, %0" : "+m"(*(dst_short + 1)), "+r"(rs2));
            }
#endif
        }
    }
}

template <unsigned int width, unsigned int base, bool _nxz = false>
inline unsigned char get_edge_bit(const void *signal) {
    unsigned char edge_bit;

    if
        __constexpr(!_nxz) {
            if
                __constexpr(width <= 4) {
                    unsigned char *signal_char = (unsigned char *)signal;

                    if
                        __constexpr(base == 0) { edge_bit = signal_char[0] & 0x11; }
                    else {
                        edge_bit = (signal_char[0] >> base) & 0x11;
                    }
                }
            else {
                unsigned int *signal_int = (unsigned int *)signal;

                if
                    __constexpr(width <= 32) {
                        if
                            __constexpr(base == 0) {
                                edge_bit = (signal_int[0] & 0x01) | ((signal_int[1] & 0x01) << 4);
                            }
                        else {
                            edge_bit = ((signal_int[0] >> base) & 0x01) |
                                       (((signal_int[1] >> base) & 0x01) << 4);
                        }
                    }
                else {                                                     // width > 32
                    __constexpr unsigned int signal_cnt = (width + 31) >> 5; // (width + (32-1)) / 32

                    __constexpr unsigned int start_idx = base >> 5;   // start / 32
                    __constexpr unsigned int start_mod = base & 0x1f; // start % 32

                    if
                        __constexpr(start_mod == 0) {
                            edge_bit = (signal_int[start_idx] & 0x01) |
                                       ((signal_int[start_idx + signal_cnt] & 0x01) << 4);
                        }
                    else {
                        edge_bit =
                            ((signal_int[start_idx] >> start_mod) & 0x01) |
                            (((signal_int[start_idx + signal_cnt] >> start_mod) & 0x01) << 4);
                    }
                }
            }
        }
    else {
        if
            __constexpr(width <= 4) {
                unsigned char *signal_char = (unsigned char *)signal;

                if
                    __constexpr(base == 0) { edge_bit = signal_char[0] & 0x01; }
                else {
                    edge_bit = (signal_char[0] >> base) & 0x01;
                }
            }
        else {
            unsigned int *signal_int = (unsigned int *)signal;

            if
                __constexpr(width <= 32) {
                    if
                        __constexpr(base == 0) { edge_bit = signal_int[0] & 0x01; }
                    else {
                        edge_bit = (signal_int[0] >> base) & 0x01;
                    }
                }
            else {                                                     // width > 32
                __constexpr unsigned int signal_cnt = (width + 31) >> 5; // (width + (32-1)) / 32

                __constexpr unsigned int start_idx = base >> 5;   // start / 32
                __constexpr unsigned int start_mod = base & 0x1f; // start % 32

                if
                    __constexpr(start_mod == 0) { edge_bit = signal_int[start_idx] & 0x01; }
                else {
                    edge_bit = (signal_int[start_idx] >> start_mod) & 0x01;
                }
            }
        }
    }

    return edge_bit;
}

template <bool _whole = false, unsigned int _base = 0, bool _nxz = false>
inline pint_edge_t get_edge(unsigned char from, unsigned char to) {
    if (from == to) {
        return pint_nonedge;
    } else {
        if
            __constexpr(!_nxz) {
                if
                    __constexpr(_whole) {
                        if
                            __constexpr(_base > 0) {
                                from = (from >> _base) & 0x11;
                                to = (to >> _base) & 0x11;
                            }
                        else {
                            from &= 0x11;
                            to &= 0x11;
                        }
                    }
                if ((from == 0) && (to != 0)) {
                    return pint_posedge;
                } else if ((from == 1) && (to != 1)) {
                    return pint_negedge;
                } else if ((from != 1) && (to == 1)) {
                    return pint_posedge;
                } else if ((from != 0) && (to == 0)) {
                    return pint_negedge;
                } else {
                    return pint_otherchange;
                }
            }
        else {
            if
                __constexpr(_whole) {
                    if
                        __constexpr(_base > 0) {
                            from = (from >> _base) & 0x01;
                            to = (to >> _base) & 0x01;
                        }
                    else {
                        from &= 0x01;
                        to &= 0x01;
                    }
                }
            if ((from == 0) && (to != 0)) {
                return pint_posedge;
            } else if ((from == 1) && (to != 1)) {
                return pint_negedge;
            } else {
                return pint_otherchange;
            }
        }
    }
}

template <
    bool _whole = false, // true: whole, false: valid
    unsigned int to_width, unsigned int to_base, unsigned int to_count, typename T,
    unsigned int from_cnt, // unsigned int from_cnt = (std::tuple_size<T>::value >> 1);  // size / 2
    bool _nxz = false>
inline pint_edge_t get_edge_other(T from, const void *to) {
    pint_edge_t edge = pint_nonedge;

    if
        __constexpr(to_width <= 4) {
            unsigned char *to_char = (unsigned char *)to;
            __constexpr unsigned char to_char_mask = get_mask_char<to_base, to_count, _nxz>();

            if
                __constexpr(_whole) {
                    if (to_char[0] != from) {
                        edge = pint_otherchange;
                    }
                }
            else {
                if ((to_char[0] & to_char_mask) != from) {
                    edge = pint_otherchange;
                }
            }
        }
    else {
        unsigned int *to_int = (unsigned int *)to;
        __constexpr auto to_int_mask_half =
            get_mask_int_half<to_width, to_base, to_count, unsigned int>();

        __constexpr unsigned int to_cnt = (to_width + 31) >> 5; // (width + (32-1)) / 32

        if
            __constexpr(!_nxz) { // nxz

                if
                    __constexpr(to_cnt == from_cnt) {
                        if
                            __constexpr(to_width <= 32) {
                                if
                                    __constexpr(_whole) {
                                        if ((to_int[0] != from[0]) || (to_int[1] != from[1])) {
                                            edge = pint_otherchange;
                                        }
                                    }
                                else {
                                    if (((to_int[0] & to_int_mask_half[0]) != from[0]) ||
                                        ((to_int[1] & to_int_mask_half[0]) != from[1])) {
                                        edge = pint_otherchange;
                                    }
                                }
                            }
                        else { // width > 32
                            if
                                __constexpr(_whole) {
                                    for (int i = 0; i < to_cnt; i++) {
                                        if ((to_int[i] != from[i]) ||
                                            (to_int[i + to_cnt] != from[i + to_cnt])) {
                                            edge = pint_otherchange;
                                            break;
                                        }
                                    }
                                }
                            else {
                                for (int i = 0; i < to_cnt; i++) {
                                    if (((to_int[i] & to_int_mask_half[i]) != from[i]) ||
                                        ((to_int[i + to_cnt] & to_int_mask_half[i]) !=
                                         from[i + to_cnt])) {
                                        edge = pint_otherchange;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                else {
                    if
                        __constexpr(to_width <= 32) {
                            if
                                __constexpr(_whole) {
                                    if ((to_int[0] != from[0]) || (to_int[1] != from[from_cnt])) {
                                        edge = pint_otherchange;
                                    }
                                }
                            else {
                                if (((to_int[0] & to_int_mask_half[0]) != from[0]) ||
                                    ((to_int[1] & to_int_mask_half[0]) != from[from_cnt])) {
                                    edge = pint_otherchange;
                                }
                            }
                        }
                    else {                                                  // width > 32
                        __constexpr unsigned int to_start_idx = to_base >> 5; // start / 32
                        __constexpr unsigned int to_end_idx = (to_base + to_count - 1) >> 5;

                        if
                            __constexpr(_whole) {
                                for (int i = 0, j = to_start_idx;
                                     (i < from_cnt) && (j <= to_end_idx); i++, j++) {
                                    if ((to_int[j] != from[i]) ||
                                        (to_int[j + to_cnt] != from[i + from_cnt])) {
                                        edge = pint_otherchange;
                                        break;
                                    }
                                }
                            }
                        else {
                            for (int i = 0, j = to_start_idx; (i < from_cnt) && (j <= to_end_idx);
                                 i++, j++) {
                                if (((to_int[j] & to_int_mask_half[j]) != from[i]) ||
                                    ((to_int[j + to_cnt] & to_int_mask_half[j]) !=
                                     from[i + from_cnt])) {
                                    edge = pint_otherchange;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        else { // nxz

            if
                __constexpr(to_cnt == from_cnt) {
                    if
                        __constexpr(to_width <= 32) {
                            if
                                __constexpr(_whole) {
                                    if ((to_int[0] != from[0])) {
                                        edge = pint_otherchange;
                                    }
                                }
                            else {
                                if (((to_int[0] & to_int_mask_half[0]) != from[0])) {
                                    edge = pint_otherchange;
                                }
                            }
                        }
                    else { // width > 32
                        if
                            __constexpr(_whole) {
                                for (int i = 0; i < to_cnt; i++) {
                                    if ((to_int[i] != from[i])) {
                                        edge = pint_otherchange;
                                        break;
                                    }
                                }
                            }
                        else {
                            for (int i = 0; i < to_cnt; i++) {
                                if (((to_int[i] & to_int_mask_half[i]) != from[i])) {
                                    edge = pint_otherchange;
                                    break;
                                }
                            }
                        }
                    }
                }
            else {
                if
                    __constexpr(to_width <= 32) {
                        if
                            __constexpr(_whole) {
                                if ((to_int[0] != from[0])) {
                                    edge = pint_otherchange;
                                }
                            }
                        else {
                            if (((to_int[0] & to_int_mask_half[0]) != from[0])) {
                                edge = pint_otherchange;
                            }
                        }
                    }
                else {                                                  // width > 32
                    __constexpr unsigned int to_start_idx = to_base >> 5; // start / 32
                    __constexpr unsigned int to_end_idx = (to_base + to_count - 1) >> 5;

                    if
                        __constexpr(_whole) {
                            for (int i = 0, j = to_start_idx; (i < from_cnt) && (j <= to_end_idx);
                                 i++, j++) {
                                if ((to_int[j] != from[i])) {
                                    edge = pint_otherchange;
                                    break;
                                }
                            }
                        }
                    else {
                        for (int i = 0, j = to_start_idx; (i < from_cnt) && (j <= to_end_idx);
                             i++, j++) {
                            if (((to_int[j] & to_int_mask_half[j]) != from[i])) {
                                edge = pint_otherchange;
                                break;
                            }
                        }
                    }
                }
            }

        } // nxz
    }

    return edge;
}

template <bool _total = true,  // true: total word, false: valid word
          bool _whole = false, // true: whole bit , false: valid bit
          unsigned int width, unsigned int base, unsigned int count, typename RT, bool _nxz = false>
inline RT get_signal(void *signal) {
    if
        __constexpr(width <= 4) {
            unsigned char *signal_char = (unsigned char *)signal;
            __constexpr unsigned char signal_char_mask = get_mask_char<base, count, _nxz>();

            if
                __constexpr(_whole) { return signal_char[0]; }
            else {
                return (signal_char[0] & signal_char_mask);
            }
        }
    else {
        RT signal_out = {0};

        unsigned int *signal_int = (unsigned int *)signal;
        __constexpr auto signal_int_mask_half = get_mask_int_half<width, base, count, unsigned int>();

        if
            __constexpr(!_nxz) { // nxz

                if
                    __constexpr(width <= 32) {
                        if
                            __constexpr(_whole) {
                                signal_out[0] = signal_int[0];
                                signal_out[1] = signal_int[1];
                            }
                        else {
                            signal_out[0] = signal_int[0] & signal_int_mask_half[0];
                            signal_out[1] = signal_int[1] & signal_int_mask_half[0];
                        }
                    }
                else {
                    __constexpr unsigned int signal_cnt = (width + 31) >> 5; // (width + (32-1)) / 32

                    if
                        __constexpr(_total) {
                            if
                                __constexpr(_whole) {
                                    for (int i = 0; i < signal_cnt; i++) {
                                        signal_out[i] = signal_int[i];
                                        signal_out[i + signal_cnt] = signal_int[i + signal_cnt];
                                    }
                                }
                            else {
                                for (int i = 0; i < signal_cnt; i++) {
                                    signal_out[i] = signal_int[i] & signal_int_mask_half[i];
                                    signal_out[i + signal_cnt] =
                                        signal_int[i + signal_cnt] & signal_int_mask_half[i];
                                }
                            }
                        }
                    else {
                        __constexpr unsigned int signal_start_idx = base >> 5; // start / 32
                        __constexpr unsigned int signal_end_idx = (base + count - 1) >> 5;

                        __constexpr unsigned int signal_out_cnt = signal_out.size() >> 1; // size / 2

                        if
                            __constexpr(_whole) {
                                for (int i = signal_start_idx, j = 0; i <= signal_end_idx;
                                     i++, j++) {
                                    signal_out[j] = signal_int[i];
                                    signal_out[j + signal_out_cnt] = signal_int[i + signal_cnt];
                                }
                            }
                        else {
                            for (int i = signal_start_idx, j = 0; i <= signal_end_idx; i++, j++) {
                                signal_out[j] = signal_int[i] & signal_int_mask_half[i];
                                signal_out[j + signal_out_cnt] =
                                    signal_int[i + signal_cnt] & signal_int_mask_half[i];
                            }
                        }
                    }
                }
            }
        else { // nxz

            if
                __constexpr(width <= 32) {
                    if
                        __constexpr(_whole) { signal_out[0] = signal_int[0]; }
                    else {
                        signal_out[0] = signal_int[0] & signal_int_mask_half[0];
                    }
                }
            else {
                __constexpr unsigned int signal_cnt = (width + 31) >> 5; // (width + (32-1)) / 32

                if
                    __constexpr(_total) {
                        if
                            __constexpr(_whole) {
                                for (int i = 0; i < signal_cnt; i++) {
                                    signal_out[i] = signal_int[i];
                                }
                            }
                        else {
                            for (int i = 0; i < signal_cnt; i++) {
                                signal_out[i] = signal_int[i] & signal_int_mask_half[i];
                            }
                        }
                    }
                else {
                    __constexpr unsigned int signal_start_idx = base >> 5; // start / 32
                    __constexpr unsigned int signal_end_idx = (base + count - 1) >> 5;

                    __constexpr unsigned int signal_out_cnt = signal_out.size() >> 1; // size / 2

                    if
                        __constexpr(_whole) {
                            for (int i = signal_start_idx, j = 0; i <= signal_end_idx; i++, j++) {
                                signal_out[j] = signal_int[i];
                            }
                        }
                    else {
                        for (int i = signal_start_idx, j = 0; i <= signal_end_idx; i++, j++) {
                            signal_out[j] = signal_int[i] & signal_int_mask_half[i];
                        }
                    }
                }
            }

        } // nxz

        return signal_out;
    }
}

#ifdef USE_SHM_INT_UNROLL
template <unsigned int dst_width, unsigned int dst_base, unsigned int src_width,
          unsigned int src_base, unsigned int count,

          bool dst_whole, bool src_whole, unsigned int dst_cnt, unsigned int dst_start_mod,
          unsigned int src_cnt, unsigned int src_start_mod, unsigned int src_end_idx, int dst_idx_t,
          int src_idx_t, bool _nxz = false>
inline void shm_int_unroll_func(unsigned int *dst_int, unsigned int *src_int) {
    __constexpr auto dst_int_mask_half =
        get_mask_int_half<dst_width, dst_base, count, unsigned int>();
    __constexpr auto src_int_mask_half =
        get_mask_int_half<src_width, src_base, count, unsigned int>();

    if
        __constexpr(!_nxz) { // nxz

            if
                __constexpr(dst_start_mod == src_start_mod) {
                    shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>(
                        (dst_int + dst_idx_t), src_int[src_idx_t]);
                    shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>(
                        (dst_int + dst_idx_t + dst_cnt), src_int[src_idx_t + src_cnt]);
                }
            else if
                __constexpr(dst_start_mod > src_start_mod) {
                    __constexpr unsigned int lshift = dst_start_mod - src_start_mod;
                    __constexpr unsigned int rshift = 32 - lshift;

                    if
                        __constexpr(src_idx_t <= src_end_idx) {
                            shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>(
                                (dst_int + dst_idx_t), ((src_int[src_idx_t - 1] >> rshift) |
                                                        (src_int[src_idx_t] << lshift)));
                            shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>(
                                (dst_int + dst_idx_t + dst_cnt),
                                ((src_int[src_idx_t - 1 + src_cnt] >> rshift) |
                                 (src_int[src_idx_t + src_cnt] << lshift)));
                        }
                    else {
                        shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>(
                            (dst_int + dst_idx_t), (src_int[src_idx_t - 1] >> rshift));
                        shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>(
                            (dst_int + dst_idx_t + dst_cnt),
                            (src_int[src_idx_t - 1 + src_cnt] >> rshift));
                    }
                }
            else { // dst_start_mod < src_start_mod
                __constexpr unsigned int rshift = src_start_mod - dst_start_mod;
                __constexpr unsigned int lshift = 32 - rshift;

                if
                    __constexpr(src_idx_t + 1 <= src_end_idx) {
                        shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>(
                            (dst_int + dst_idx_t),
                            ((src_int[src_idx_t] >> rshift) | (src_int[src_idx_t + 1] << lshift)));
                        shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>(
                            (dst_int + dst_idx_t + dst_cnt),
                            ((src_int[src_idx_t + src_cnt] >> rshift) |
                             (src_int[src_idx_t + 1 + src_cnt] << lshift)));
                    }
                else {
                    shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>(
                        (dst_int + dst_idx_t), (src_int[src_idx_t] >> rshift));
                    shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>(
                        (dst_int + dst_idx_t + dst_cnt), (src_int[src_idx_t + src_cnt] >> rshift));
                }
            }
        }
    else { // nxz

        if
            __constexpr(dst_start_mod == src_start_mod) {
                shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>((dst_int + dst_idx_t),
                                                                            src_int[src_idx_t]);
            }
        else if
            __constexpr(dst_start_mod > src_start_mod) {
                __constexpr unsigned int lshift = dst_start_mod - src_start_mod;
                __constexpr unsigned int rshift = 32 - lshift;

                if
                    __constexpr(src_idx_t <= src_end_idx) {
                        shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>(
                            (dst_int + dst_idx_t),
                            ((src_int[src_idx_t - 1] >> rshift) | (src_int[src_idx_t] << lshift)));
                    }
                else {
                    shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>(
                        (dst_int + dst_idx_t), (src_int[src_idx_t - 1] >> rshift));
                }
            }
        else { // dst_start_mod < src_start_mod
            __constexpr unsigned int rshift = src_start_mod - dst_start_mod;
            __constexpr unsigned int lshift = 32 - rshift;

            if
                __constexpr(src_idx_t + 1 <= src_end_idx) {
                    shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>(
                        (dst_int + dst_idx_t),
                        ((src_int[src_idx_t] >> rshift) | (src_int[src_idx_t + 1] << lshift)));
                }
            else {
                shm_int<dst_whole, src_whole, dst_int_mask_half[dst_idx_t]>(
                    (dst_int + dst_idx_t), (src_int[src_idx_t] >> rshift));
            }
        }

    } // nxz
}

template <unsigned int dst_width, unsigned int dst_base, unsigned int src_width,
          unsigned int src_base, unsigned int count,

          bool dst_whole, bool src_whole, unsigned int dst_cnt, unsigned int dst_start_mod,
          unsigned int src_cnt, unsigned int src_start_mod, unsigned int src_end_idx,
          int dst_end_idx_t, int src_end_idx_t, bool _nxz,
          int N> // (dst_end_idx_t - dst_start_idx_t) ... 0
class Loop {
  public:
    static inline void shm_int_unroll(unsigned int *dst_int, unsigned int *src_int) {
        __constexpr int dst_idx_t = dst_end_idx_t - N;
        __constexpr int src_idx_t = src_end_idx_t - N;

        shm_int_unroll_func<dst_width, dst_base, src_width, src_base, count, dst_whole, src_whole,
                            dst_cnt, dst_start_mod, src_cnt, src_start_mod, src_end_idx, dst_idx_t,
                            src_idx_t, _nxz>(dst_int, src_int);

        Loop<dst_width, dst_base, src_width, src_base, count, dst_whole, src_whole, dst_cnt,
             dst_start_mod, src_cnt, src_start_mod, src_end_idx, dst_end_idx_t, src_end_idx_t, _nxz,
             (N - 1)>::shm_int_unroll(dst_int, src_int);
    }
};

template <unsigned int dst_width, unsigned int dst_base, unsigned int src_width,
          unsigned int src_base, unsigned int count,

          bool dst_whole, bool src_whole, unsigned int dst_cnt, unsigned int dst_start_mod,
          unsigned int src_cnt, unsigned int src_start_mod, unsigned int src_end_idx,
          int dst_end_idx_t, int src_end_idx_t, bool _nxz>
class Loop<dst_width, dst_base, src_width, src_base, count, dst_whole, src_whole, dst_cnt,
           dst_start_mod, src_cnt, src_start_mod, src_end_idx, dst_end_idx_t, src_end_idx_t, _nxz,
           0> {
  public:
    static inline void shm_int_unroll(unsigned int *dst_int, unsigned int *src_int) {
        __constexpr int dst_idx_t = dst_end_idx_t;
        __constexpr int src_idx_t = src_end_idx_t;

        shm_int_unroll_func<dst_width, dst_base, src_width, src_base, count, dst_whole, src_whole,
                            dst_cnt, dst_start_mod, src_cnt, src_start_mod, src_end_idx, dst_idx_t,
                            src_idx_t, _nxz>(dst_int, src_int);
    }
};
#endif

struct NormalVersionTag {};
struct PartialVersionTag {};

template <unsigned int dst_base> struct TagDispatchTrait { using Tag = NormalVersionTag; };

template <> struct TagDispatchTrait<PINT_INT_MASK> { using Tag = PartialVersionTag; };

template <unsigned int src_width, unsigned int src_base, unsigned int dst_width,
          unsigned int dst_base, unsigned int count, bool _edge = true, bool _nxz = false>
/*inline*/ pint_edge_t copy_signal_common_edge(const void *src, void *dst, unsigned int dst_base_,
                                               PartialVersionTag) {
    NCORE_PERF_PINT_NET_SUMMARY(184);
    if
        __constexpr((dst_width <= 4) && (src_width <= 4)) {
            //// at (count == dst_width), dst_base must be 0
            //// at (count == src_width), src_base must be 0
            if
                __constexpr(dst_width == src_width) {
                    if
                        __constexpr(count == dst_width) {
                            if
                                __constexpr(!_edge) {
                                    ((unsigned char *)dst)[0] = ((unsigned char *)src)[0];
                                    return pint_nonedge;
                                }
                            else {
                                return (pint_edge_t)pint_copy_char_char_same_edge<_nxz>(src, dst);
                            }
                        }
                }
            if
                __constexpr(!_edge) {
                    pint_copy_char_char<_nxz>(src, src_base, dst, dst_base_, count);
                    return pint_nonedge;
                }
            else {
                return (pint_edge_t)pint_copy_char_char_edge<_nxz>(src, src_base, dst, dst_base_,
                                                                   count);
            }
        }
    else if
        __constexpr((dst_width <= 4) && (src_width > 4)) {
            if
                __constexpr(!_edge) {
                    pint_copy_int_char<_nxz>(src, src_width, src_base, dst, dst_base_, count);
                    return pint_nonedge;
                }
            else {
                return (pint_edge_t)pint_copy_int_char_edge<_nxz>(src, src_width, src_base, dst,
                                                                  dst_base_, count);
            }
        }
    else if
        __constexpr((dst_width > 4) && (src_width <= 4)) {
            if
                __constexpr(!_edge) {
                    pint_copy_char_int<_nxz>(src, src_base, dst, dst_width, dst_base_, count);
                    return pint_nonedge;
                }
            else {
                return (pint_edge_t)pint_copy_char_int_edge<_nxz>(src, src_base, dst, dst_width,
                                                                  dst_base_, count);
            }
        }
    else {
        //// at (count == dst_width), dst_base must be 0
        //// at (count == src_width), src_base must be 0
        if
            __constexpr(dst_width == src_width) {
                if
                    __constexpr(count == dst_width) {
                        if
                            __constexpr(!_edge) {
                                for (int i = 0; i < (dst_width + 31) / 32 * 2; i++) {
                                    ((unsigned int *)dst)[i] = ((unsigned int *)src)[i];
                                }
                                return pint_nonedge;
                            }
                        else {
                            return (pint_edge_t)pint_copy_int_int_same_edge<_nxz>(src, dst,
                                                                                  dst_width);
                        }
                    }
            }
        if
            __constexpr(!_edge) {
                pint_copy_int_int<_nxz>(src, src_width, src_base, dst, dst_width, dst_base_, count);
                return pint_nonedge;
            }
        else {
            return (pint_edge_t)pint_copy_int_int_edge<_nxz>(src, src_width, src_base, dst,
                                                             dst_width, dst_base_, count);
        }
    }
}

#pragma GCC push_options
#pragma GCC optimize("-funroll-loops")
/*
    The invalid bit of src must be "0"
    The invalid bit of dst must be "0"
    (src_width >= src_base + count) always
    (dst_width >= dst_base + count) always
*/
template <unsigned int src_width, unsigned int src_base, unsigned int dst_width,
          unsigned int dst_base, unsigned int count, bool _edge = true, bool _nxz = false>
/*inline*/ pint_edge_t copy_signal_common_edge(const void *src, void *dst, unsigned int dst_base_,
                                               NormalVersionTag) {
    NCORE_PERF_PINT_NET_SUMMARY(185);
    static_assert(src_width >= src_base + count, "check (src_width >= src_base + count) err!");
    // static_assert(dst_width >= dst_base + count, "check (dst_width >= dst_base + count) err!");
    pint_edge_t edge = pint_nonedge;
    if (dst_width < dst_base + count) {
        return edge;
    }

    __constexpr bool src_whole = (count == src_width);
    __constexpr bool dst_whole = (count == dst_width);

    signal_type<dst_width, dst_base, count> from_signal = {0};
    signal_type_valid<dst_width, dst_base, count> from_valid = {0};

    if
        __constexpr(!_nxz) { // nxz

            if
                __constexpr(dst_width <= 4) {
                    unsigned char *dst_char = (unsigned char *)dst;
                    __constexpr unsigned char dst_char_mask = get_mask_char<dst_base, count, _nxz>();

                    if
                        __constexpr(src_width <= 4) {
                            unsigned char *src_char = (unsigned char *)src;
                            __constexpr unsigned char src_char_mask =
                                get_mask_char<src_base, count, _nxz>();

                            if
                                __constexpr(_edge) {
                                    //// at (dst_whole == true), dst_base must be 0
                                    //// at (src_whole == true), src_base must be 0
                                    if
                                        __constexpr(dst_whole && src_whole) {
                                            edge =
                                                get_edge<true, 0, _nxz>(dst_char[0], src_char[0]);
                                        }
                                    else {
                                        from_signal =
                                            get_signal<true, true, dst_width, dst_base, count,
                                                       decltype(from_signal), _nxz>(dst);
                                    }
                                }

                            if
                                __constexpr(dst_base == src_base) {
                                    shm_char<dst_whole, src_whole, dst_char_mask>(dst_char,
                                                                                  src_char[0]);
                                }
                            else if
                                __constexpr(dst_base > src_base) {
                                    __constexpr unsigned int lshift =
                                        dst_base - src_base; // (lshift <= 3)

                                    shm_char<dst_whole, src_whole, dst_char_mask>(
                                        dst_char, (src_char[0] << lshift));
                                }
                            else { // dst_base < src_base
                                __constexpr unsigned int rshift =
                                    src_base - dst_base; // (rshift <= 3)

                                shm_char<dst_whole, src_whole, dst_char_mask>(
                                    dst_char, (src_char[0] >> rshift));
                            }

                            if
                                __constexpr(_edge) {
                                    if
                                        __constexpr(!(dst_whole && src_whole)) {
                                            edge = get_edge<true, dst_base, _nxz>(
                                                from_signal, dst_char[0]); // ??? get_edge2
                                        }
                                }
                        }
                    else { // src_width > 4
                        unsigned int *src_int = (unsigned int *)src;
                        __constexpr auto src_int_mask_half =
                            get_mask_int_half<src_width, src_base, count, unsigned int>();

                        if
                            __constexpr(_edge) {
                                from_signal = get_signal<true, true, dst_width, dst_base, count,
                                                         decltype(from_signal), _nxz>(dst);
                            }

                        if
                            __constexpr(src_width <= 32) {
                                if
                                    __constexpr(dst_base == src_base) {
                                        shm_char<dst_whole, true, dst_char_mask>(
                                            dst_char, ((src_int[0] & src_int_mask_half[0]) |
                                                       (src_int[1] << 4)));
                                    }
                                else if
                                    __constexpr(dst_base > src_base) {
                                        __constexpr unsigned int lshift = dst_base - src_base;

                                        shm_char<dst_whole, true, dst_char_mask>(
                                            dst_char,
                                            (((src_int[0] & src_int_mask_half[0]) << lshift) |
                                             ((src_int[1]) << (lshift + 4))));
                                    }
                                else { // dst_base < src_base
                                    __constexpr unsigned int rshift = src_base - dst_base;

                                    shm_char<dst_whole, true, dst_char_mask>(
                                        dst_char, (((src_int[0] & src_int_mask_half[0]) >> rshift) |
                                                   (((src_int[1]) >> rshift) << 4)));
                                }
                            }
                        else { // src_width > 32
                            __constexpr unsigned int src_cnt =
                                (src_width + 31) >> 5; // (width + (32-1)) / 32

                            __constexpr unsigned int src_start_idx = src_base >> 5;   // start / 32
                            __constexpr unsigned int src_start_mod = src_base & 0x1f; // start % 32
                            __constexpr unsigned int src_end_idx = (src_base + count - 1) >> 5;

                            if
                                __constexpr(src_end_idx == src_start_idx) {
                                    if
                                        __constexpr(dst_base == src_start_mod) {
                                            shm_char<dst_whole, true, dst_char_mask>(
                                                dst_char,
                                                ((src_int[src_start_idx] &
                                                  src_int_mask_half[src_start_idx]) |
                                                 (src_int[src_start_idx + src_cnt] << 4)));
                                        }
                                    else if
                                        __constexpr(dst_base > src_start_mod) {
                                            __constexpr unsigned int lshift =
                                                dst_base - src_start_mod;

                                            shm_char<dst_whole, true, dst_char_mask>(
                                                dst_char, (((src_int[src_start_idx] &
                                                             src_int_mask_half[src_start_idx])
                                                            << lshift) |
                                                           ((src_int[src_start_idx + src_cnt])
                                                            << (lshift + 4))));
                                        }
                                    else { // dst_base < src_start_mod
                                        __constexpr unsigned int rshift = src_start_mod - dst_base;

                                        shm_char<dst_whole, true, dst_char_mask>(
                                            dst_char,
                                            (((src_int[src_start_idx] &
                                               src_int_mask_half[src_start_idx]) >>
                                              rshift) |
                                             (((src_int[src_start_idx + src_cnt]) >> rshift)
                                              << 4)));
                                    }
                                }
                            else {
                                // (dst_base < src_start_mod) always
                                __constexpr unsigned int rshift = src_start_mod - dst_base;
                                __constexpr unsigned int lshift = 32 - rshift;

                                shm_char<dst_whole, true, dst_char_mask>(
                                    dst_char, ((((src_int[src_start_idx]) >> rshift) |
                                                ((src_int[src_start_idx + 1] &
                                                  src_int_mask_half[src_start_idx + 1])
                                                 << lshift)) |
                                               ((((src_int[src_start_idx + src_cnt]) >> rshift) |
                                                 ((src_int[src_start_idx + 1 + src_cnt]) << lshift))
                                                << 4)));
                            }
                        }

                        if
                            __constexpr(_edge) {
                                edge = get_edge<true, dst_base, _nxz>(from_signal, dst_char[0]);
                            }
                    }
                }
            else { // dst_width > 4
                unsigned int *dst_int = (unsigned int *)dst;
                __constexpr auto dst_int_mask_half =
                    get_mask_int_half<dst_width, dst_base, count, unsigned int>();

                if
                    __constexpr(dst_width <= 32) {
                        if
                            __constexpr(src_width <= 4) {
                                unsigned char *src_char = (unsigned char *)src;
                                __constexpr unsigned char src_char_mask =
                                    get_mask_char<src_base, count, _nxz>();

                                if
                                    __constexpr(_edge) {
                                        unsigned char from_edge_bit =
                                            get_edge_bit<dst_width, dst_base, _nxz>(dst);
                                        unsigned char to_edge_bit =
                                            get_edge_bit<src_width, src_base, _nxz>(src);
                                        edge = get_edge<false, 0, _nxz>(from_edge_bit, to_edge_bit);

                                        if
                                            __constexpr(count > 1) {
                                                if (edge == pint_nonedge) {
                                                    from_signal =
                                                        get_signal<true, true, dst_width, dst_base,
                                                                   count, decltype(from_signal),
                                                                   _nxz>(dst);
                                                }
                                            }
                                    }

                                if
                                    __constexpr(dst_base == src_base) {
                                        shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                            dst_int, (src_char[0]));
                                        shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                            (dst_int + 1), (src_char[0] >> 4));
                                    }
                                else if
                                    __constexpr(dst_base > src_base) {
                                        __constexpr unsigned int lshift = dst_base - src_base;

                                        shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                            dst_int, (src_char[0] << lshift));
                                        shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                            (dst_int + 1), ((src_char[0] >> 4) << lshift));
                                    }
                                else { // dst_base < src_base
                                    __constexpr unsigned int rshift = src_base - dst_base;

                                    shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                        dst_int, (src_char[0] >> rshift));
                                    shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                        (dst_int + 1), (src_char[0] >> (4 + rshift)));
                                }

                                if
                                    __constexpr(_edge) {
                                        if
                                            __constexpr(count > 1) {
                                                if (edge == pint_nonedge) {
                                                    edge =
                                                        get_edge_other<true, dst_width, dst_base,
                                                                       count, decltype(from_signal),
                                                                       (from_signal.size() >> 1),
                                                                       _nxz>(from_signal, dst);
                                                }
                                            }
                                    }
                            }
                        else { // src_width > 4
                            unsigned int *src_int = (unsigned int *)src;
                            __constexpr auto src_int_mask_half =
                                get_mask_int_half<src_width, src_base, count, unsigned int>();

                            if
                                __constexpr(src_width <= 32) {
                                    if
                                        __constexpr(_edge) {
                                            unsigned char from_edge_bit =
                                                get_edge_bit<dst_width, dst_base, _nxz>(dst);
                                            unsigned char to_edge_bit =
                                                get_edge_bit<src_width, src_base, _nxz>(src);
                                            edge = get_edge<false, 0, _nxz>(from_edge_bit,
                                                                            to_edge_bit);

                                            if
                                                __constexpr(count > 1) {
                                                    if (edge == pint_nonedge) {
                                                        //// at (dst_whole == true), dst_base must
                                                        /// be 0
                                                        //// at (src_whole == true), src_base must
                                                        /// be 0
                                                        if
                                                            __constexpr(dst_whole && src_whole) {
                                                                edge = get_edge_other<
                                                                    true, src_width, src_base,
                                                                    count, unsigned int *,
                                                                    (from_signal.size() >> 1),
                                                                    _nxz>(dst_int, src);
                                                            }
                                                        else {
                                                            from_signal = get_signal<
                                                                true, true, dst_width, dst_base,
                                                                count, decltype(from_signal), _nxz>(
                                                                dst);
                                                        }
                                                    }
                                                }
                                        }

                                    if
                                        __constexpr(dst_base == src_base) {
                                            shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                                dst_int, src_int[0]);
                                            shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                                (dst_int + 1), src_int[1]);
                                        }
                                    else if
                                        __constexpr(dst_base > src_base) {
                                            __constexpr unsigned int lshift =
                                                dst_base - src_base; // (lshift <= 31)

                                            shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                                dst_int, (src_int[0] << lshift));
                                            shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                                (dst_int + 1), (src_int[1] << lshift));
                                        }
                                    else { // dst_base < src_base
                                        __constexpr unsigned int rshift =
                                            src_base - dst_base; // (rshift <= 31)

                                        shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                            dst_int, (src_int[0] >> rshift));
                                        shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                            (dst_int + 1), (src_int[1] >> rshift));
                                    }

                                    if
                                        __constexpr(_edge) {
                                            if
                                                __constexpr(count > 1) {
                                                    if
                                                        __constexpr(!(dst_whole && src_whole)) {
                                                            if (edge == pint_nonedge) {
                                                                edge = get_edge_other<
                                                                    true, dst_width, dst_base,
                                                                    count, decltype(from_signal),
                                                                    (from_signal.size() >> 1),
                                                                    _nxz>(from_signal, dst);
                                                            }
                                                        }
                                                }
                                        }
                                }
                            else { // src_width > 32
                                __constexpr unsigned int src_cnt =
                                    (src_width + 31) >> 5; // (width + (32-1)) / 32

                                __constexpr unsigned int src_start_idx = src_base >> 5; // start / 32
                                __constexpr unsigned int src_start_mod = src_base & 0x1f; // start %
                                                                                        // 32
                                __constexpr unsigned int src_end_idx = (src_base + count - 1) >> 5;

                                __constexpr bool aligned =
                                    dst_whole &&
                                    ((src_start_mod == 0) &&
                                     ((src_base + count == src_width) || ((count & 0x1f) == 0)));
                                if
                                    __constexpr(_edge) {
                                        unsigned char from_edge_bit =
                                            get_edge_bit<dst_width, dst_base, _nxz>(dst);
                                        unsigned char to_edge_bit =
                                            get_edge_bit<src_width, src_base, _nxz>(src);
                                        edge = get_edge<false, 0, _nxz>(from_edge_bit, to_edge_bit);

                                        if
                                            __constexpr(count > 1) {
                                                if (edge == pint_nonedge) {
                                                    if
                                                        __constexpr(aligned) {
                                                            edge = get_edge_other<
                                                                true, src_width, src_base, count,
                                                                unsigned int *,
                                                                (from_signal.size() >> 1), _nxz>(
                                                                dst_int, (void *)src_int);
                                                        }
                                                    else {
                                                        from_signal =
                                                            get_signal<true, true, dst_width,
                                                                       dst_base, count,
                                                                       decltype(from_signal), _nxz>(
                                                                dst);
                                                    }
                                                }
                                            }
                                    }

                                if
                                    __constexpr(src_end_idx == src_start_idx) {
                                        if
                                            __constexpr(dst_base == src_start_mod) {
                                                shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                                    dst_int, src_int[src_start_idx]);
                                                shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                                    (dst_int + 1),
                                                    src_int[src_start_idx + src_cnt]);
                                            }
                                        else if
                                            __constexpr(dst_base > src_start_mod) {
                                                __constexpr unsigned int lshift =
                                                    dst_base - src_start_mod;

                                                shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                                    dst_int, (src_int[src_start_idx] << lshift));
                                                shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                                    (dst_int + 1),
                                                    (src_int[src_start_idx + src_cnt] << lshift));
                                            }
                                        else { // dst_base < src_start_mod
                                            __constexpr unsigned int rshift =
                                                src_start_mod - dst_base;

                                            shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                                dst_int, (src_int[src_start_idx] >> rshift));
                                            shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                                (dst_int + 1),
                                                (src_int[src_start_idx + src_cnt] >> rshift));
                                        }
                                    }
                                else {
                                    // (dst_base < src_start_mod) always
                                    __constexpr unsigned int rshift = src_start_mod - dst_base;
                                    __constexpr unsigned int lshift = 32 - rshift;

                                    shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                        dst_int, ((src_int[src_start_idx] >> rshift) |
                                                  (src_int[src_start_idx + 1] << lshift)));
                                    shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                        (dst_int + 1),
                                        ((src_int[src_start_idx + src_cnt] >> rshift) |
                                         (src_int[src_start_idx + 1 + src_cnt] << lshift)));
                                }

                                if
                                    __constexpr(_edge) {
                                        if
                                            __constexpr(count > 1) {
                                                if
                                                    __constexpr(!aligned) {
                                                        if (edge == pint_nonedge) {
                                                            edge = get_edge_other<
                                                                true, dst_width, dst_base, count,
                                                                decltype(from_signal),
                                                                (from_signal.size() >> 1), _nxz>(
                                                                from_signal, dst);
                                                        }
                                                    }
                                            }
                                    }
                            }
                        }
                    }
                else {                                                      // dst_width > 32
                    __constexpr unsigned int dst_cnt = (dst_width + 31) >> 5; // (width + (32-1)) / 32

                    __constexpr unsigned int dst_start_idx = dst_base >> 5;   // start / 32
                    __constexpr unsigned int dst_start_mod = dst_base & 0x1f; // start % 32
                    __constexpr unsigned int dst_end_idx = (dst_base + count - 1) >> 5;

                    if
                        __constexpr(src_width <= 4) {
                            unsigned char *src_char = (unsigned char *)src;
                            __constexpr unsigned char src_char_mask =
                                get_mask_char<src_base, count, _nxz>();

                            if
                                __constexpr(_edge) {
                                    unsigned char from_edge_bit =
                                        get_edge_bit<dst_width, dst_base, _nxz>(dst);
                                    unsigned char to_edge_bit =
                                        get_edge_bit<src_width, src_base, _nxz>(src);
                                    edge = get_edge<false, 0, _nxz>(from_edge_bit, to_edge_bit);

                                    if
                                        __constexpr(count > 1) {
                                            if (edge == pint_nonedge) {
                                                from_valid =
                                                    get_signal<false, true, dst_width, dst_base,
                                                               count, decltype(from_valid), _nxz>(
                                                        dst);
                                            }
                                        }
                                }

                            if
                                __constexpr(dst_end_idx == dst_start_idx) {
                                    if
                                        __constexpr(dst_start_mod == src_base) {
                                            shm_int<dst_whole, src_whole,
                                                    dst_int_mask_half[dst_start_idx]>(
                                                (dst_int + dst_start_idx), (src_char[0]));
                                            shm_int<dst_whole, src_whole,
                                                    dst_int_mask_half[dst_start_idx]>(
                                                (dst_int + dst_start_idx + dst_cnt),
                                                (src_char[0] >> 4));
                                        }
                                    else if
                                        __constexpr(dst_start_mod > src_base) {
                                            __constexpr unsigned int lshift =
                                                dst_start_mod - src_base;

                                            shm_int<dst_whole, src_whole,
                                                    dst_int_mask_half[dst_start_idx]>(
                                                (dst_int + dst_start_idx), (src_char[0] << lshift));
                                            shm_int<dst_whole, src_whole,
                                                    dst_int_mask_half[dst_start_idx]>(
                                                (dst_int + dst_start_idx + dst_cnt),
                                                ((src_char[0] >> 4) << lshift));
                                        }
                                    else { // dst_start_mod < src_base
                                        __constexpr unsigned int rshift = src_base - dst_start_mod;

                                        shm_int<dst_whole, src_whole,
                                                dst_int_mask_half[dst_start_idx]>(
                                            (dst_int + dst_start_idx), (src_char[0] >> rshift));
                                        shm_int<dst_whole, src_whole,
                                                dst_int_mask_half[dst_start_idx]>(
                                            (dst_int + dst_start_idx + dst_cnt),
                                            (src_char[0] >> (4 + rshift)));
                                    }
                                }
                            else {
                                // (dst_start_mod > src_base) always
                                __constexpr unsigned int lshift = dst_start_mod - src_base;
                                __constexpr unsigned int rshift = 32 - lshift;

                                shm_int<dst_whole, src_whole, dst_int_mask_half[dst_start_idx]>(
                                    (dst_int + dst_start_idx), (src_char[0] << lshift));
                                shm_int<dst_whole, src_whole, dst_int_mask_half[dst_start_idx + 1]>(
                                    (dst_int + dst_start_idx + 1), (src_char[0] >> rshift));
                                shm_int<dst_whole, src_whole, dst_int_mask_half[dst_start_idx]>(
                                    (dst_int + dst_start_idx + dst_cnt),
                                    ((src_char[0] >> 4) << lshift));
                                shm_int<dst_whole, src_whole, dst_int_mask_half[dst_start_idx + 1]>(
                                    (dst_int + dst_start_idx + 1 + dst_cnt),
                                    (src_char[0] >> (4 + rshift)));
                            }

                            if
                                __constexpr(_edge) {
                                    if
                                        __constexpr(count > 1) {
                                            if (edge == pint_nonedge) {
                                                edge =
                                                    get_edge_other<true, dst_width, dst_base, count,
                                                                   decltype(from_valid),
                                                                   (from_valid.size() >> 1), _nxz>(
                                                        from_valid, dst);
                                            }
                                        }
                                }
                        }
                    else { // src_width > 4
                        unsigned int *src_int = (unsigned int *)src;
                        __constexpr auto src_int_mask_half =
                            get_mask_int_half<src_width, src_base, count, unsigned int>();

                        if
                            __constexpr(src_width <= 32) {
                                __constexpr bool aligned =
                                    ((dst_start_mod == 0) &&
                                     ((dst_base + count == dst_width) || ((count & 0x1f) == 0))) &&
                                    src_whole;
                                if
                                    __constexpr(_edge) {
                                        unsigned char from_edge_bit =
                                            get_edge_bit<dst_width, dst_base, _nxz>(dst);
                                        unsigned char to_edge_bit =
                                            get_edge_bit<src_width, src_base, _nxz>(src);
                                        edge = get_edge<false, 0, _nxz>(from_edge_bit, to_edge_bit);

                                        if
                                            __constexpr(count > 1) {
                                                if (edge == pint_nonedge) {
                                                    if
                                                        __constexpr(aligned) {
                                                            edge = get_edge_other<
                                                                true, src_width, src_base, count,
                                                                unsigned int *,
                                                                (from_signal.size() >> 1), _nxz>(
                                                                dst_int + dst_start_idx, src);
                                                        }
                                                    else {
                                                        from_valid =
                                                            get_signal<false, true, dst_width,
                                                                       dst_base, count,
                                                                       decltype(from_valid), _nxz>(
                                                                dst);
                                                    }
                                                }
                                            }
                                    }

                                if
                                    __constexpr(dst_end_idx == dst_start_idx) {
                                        if
                                            __constexpr(dst_start_mod == src_base) {
                                                shm_int<dst_whole, src_whole,
                                                        dst_int_mask_half[dst_start_idx]>(
                                                    (dst_int + dst_start_idx), src_int[0]);
                                                shm_int<dst_whole, src_whole,
                                                        dst_int_mask_half[dst_start_idx]>(
                                                    (dst_int + dst_start_idx + dst_cnt),
                                                    src_int[1]);
                                            }
                                        else if
                                            __constexpr(dst_start_mod > src_base) {
                                                __constexpr unsigned int lshift =
                                                    dst_start_mod - src_base; // (lshift <= 31)

                                                shm_int<dst_whole, src_whole,
                                                        dst_int_mask_half[dst_start_idx]>(
                                                    (dst_int + dst_start_idx),
                                                    (src_int[0] << lshift));
                                                shm_int<dst_whole, src_whole,
                                                        dst_int_mask_half[dst_start_idx]>(
                                                    (dst_int + dst_start_idx + dst_cnt),
                                                    (src_int[1] << lshift));
                                            }
                                        else { // dst_start_mod < src_base
                                            __constexpr unsigned int rshift =
                                                src_base - dst_start_mod; // (rshift <= 31)

                                            shm_int<dst_whole, src_whole,
                                                    dst_int_mask_half[dst_start_idx]>(
                                                (dst_int + dst_start_idx), (src_int[0] >> rshift));
                                            shm_int<dst_whole, src_whole,
                                                    dst_int_mask_half[dst_start_idx]>(
                                                (dst_int + dst_start_idx + dst_cnt),
                                                (src_int[1] >> rshift));
                                        }
                                    }
                                else {
                                    // (dst_start_mod > src_base) always
                                    __constexpr unsigned int lshift = dst_start_mod - src_base;
                                    __constexpr unsigned int rshift = 32 - lshift;

                                    shm_int<dst_whole, src_whole, dst_int_mask_half[dst_start_idx]>(
                                        (dst_int + dst_start_idx), (src_int[0] << lshift));
                                    shm_int<dst_whole, src_whole,
                                            dst_int_mask_half[dst_start_idx + 1]>(
                                        (dst_int + dst_start_idx + 1), (src_int[0] >> rshift));
                                    shm_int<dst_whole, src_whole, dst_int_mask_half[dst_start_idx]>(
                                        (dst_int + dst_start_idx + dst_cnt),
                                        (src_int[1] << lshift));
                                    shm_int<dst_whole, src_whole,
                                            dst_int_mask_half[dst_start_idx + 1]>(
                                        (dst_int + dst_start_idx + 1 + dst_cnt),
                                        (src_int[1] >> rshift));
                                }

                                if
                                    __constexpr(_edge) {
                                        if
                                            __constexpr(count > 1) {
                                                if
                                                    __constexpr(!aligned) {
                                                        if (edge == pint_nonedge) {
                                                            edge = get_edge_other<
                                                                true, dst_width, dst_base, count,
                                                                decltype(from_valid),
                                                                (from_valid.size() >> 1), _nxz>(
                                                                from_valid, dst);
                                                        }
                                                    }
                                            }
                                    }
                            }
                        else {
                            __constexpr unsigned int src_cnt =
                                (src_width + 31) >> 5; // (width + (32-1)) / 32

                            __constexpr unsigned int src_start_idx = src_base >> 5;   // start / 32
                            __constexpr unsigned int src_start_mod = src_base & 0x1f; // start % 32
                            __constexpr unsigned int src_end_idx = (src_base + count - 1) >> 5;

                            __constexpr bool aligned =
                                ((dst_start_mod == 0) && (src_start_mod == 0)) &&
                                (((dst_base + count == dst_width) &&
                                  (src_base + count == src_width)) ||
                                 ((count & 0x1f) == 0));
                            if
                                __constexpr(_edge) {
                                    unsigned char from_edge_bit =
                                        get_edge_bit<dst_width, dst_base, _nxz>(dst);
                                    unsigned char to_edge_bit =
                                        get_edge_bit<src_width, src_base, _nxz>(src);
                                    edge = get_edge<false, 0, _nxz>(from_edge_bit, to_edge_bit);

                                    if
                                        __constexpr(count > 1) {
                                            if (edge == pint_nonedge) {
                                                if
                                                    __constexpr(aligned) {
                                                        edge = get_edge_other<
                                                            true, src_width, src_base, count,
                                                            unsigned int *,
                                                            (from_signal.size() >> 1), _nxz>(
                                                            dst_int + dst_start_idx, src);
                                                    }
                                                else {
                                                    from_valid =
                                                        get_signal<false, true, dst_width, dst_base,
                                                                   count, decltype(from_valid),
                                                                   _nxz>(dst);
                                                }
                                            }
                                        }
                                }

                            if
                                __constexpr(dst_start_mod == src_start_mod) {
#ifndef USE_SHM_INT_UNROLL
                                    for (int i = src_start_idx, j = dst_start_idx; j <= dst_end_idx;
                                         i++, j++) {
                                        shm_int_common<dst_whole, src_whole>(
                                            dst_int_mask_half[j], (dst_int + j), src_int[i]);
                                        shm_int_common<dst_whole, src_whole>(
                                            dst_int_mask_half[j], (dst_int + j + dst_cnt),
                                            src_int[i + src_cnt]);
                                    }
#else
                                    __constexpr int N = dst_end_idx - dst_start_idx; // (N >= 0)
                                    __constexpr int dst_end_idx_t = dst_end_idx;
                                    __constexpr int src_end_idx_t = src_start_idx + N;
                                    Loop<dst_width, dst_base, src_width, src_base, count, dst_whole,
                                         src_whole, dst_cnt, dst_start_mod, src_cnt, src_start_mod,
                                         src_end_idx, dst_end_idx_t, src_end_idx_t, _nxz,
                                         N>::shm_int_unroll(dst_int, src_int);
#endif
                                }
                            else if
                                __constexpr(dst_start_mod > src_start_mod) {
                                    __constexpr unsigned int lshift = dst_start_mod - src_start_mod;
                                    __constexpr unsigned int rshift = 32 - lshift;

#ifndef USE_SHM_INT_UNROLL
                                    shm_int_common<dst_whole, src_whole>(
                                        dst_int_mask_half[dst_start_idx], (dst_int + dst_start_idx),
                                        (src_int[src_start_idx] << lshift));
                                    shm_int_common<dst_whole, src_whole>(
                                        dst_int_mask_half[dst_start_idx],
                                        (dst_int + dst_start_idx + dst_cnt),
                                        (src_int[src_start_idx + src_cnt] << lshift));

                                    for (int i = src_start_idx + 1, j = dst_start_idx + 1;
                                         j <= dst_end_idx; i++, j++) {
                                        if (i <= src_end_idx) {
                                            shm_int_common<dst_whole, src_whole>(
                                                dst_int_mask_half[j], (dst_int + j),
                                                ((src_int[i - 1] >> rshift) |
                                                 (src_int[i] << lshift)));
                                            shm_int_common<dst_whole, src_whole>(
                                                dst_int_mask_half[j], (dst_int + j + dst_cnt),
                                                ((src_int[i - 1 + src_cnt] >> rshift) |
                                                 (src_int[i + src_cnt] << lshift)));
                                        } else {
                                            shm_int_common<dst_whole, src_whole>(
                                                dst_int_mask_half[j], (dst_int + j),
                                                (src_int[i - 1] >> rshift));
                                            shm_int_common<dst_whole, src_whole>(
                                                dst_int_mask_half[j], (dst_int + j + dst_cnt),
                                                (src_int[i - 1 + src_cnt] >> rshift));
                                        }
                                    }
#else
                                    shm_int<dst_whole, src_whole, dst_int_mask_half[dst_start_idx]>(
                                        (dst_int + dst_start_idx),
                                        (src_int[src_start_idx] << lshift));
                                    shm_int<dst_whole, src_whole, dst_int_mask_half[dst_start_idx]>(
                                        (dst_int + dst_start_idx + dst_cnt),
                                        (src_int[src_start_idx + src_cnt] << lshift));

                                    if
                                        __constexpr(dst_end_idx > dst_start_idx) {
                                            __constexpr int N =
                                                dst_end_idx - (dst_start_idx + 1); // (N >= 0)
                                            __constexpr int dst_end_idx_t = dst_end_idx;
                                            __constexpr int src_end_idx_t = (src_start_idx + 1) + N;
                                            Loop<dst_width, dst_base, src_width, src_base, count,
                                                 dst_whole, src_whole, dst_cnt, dst_start_mod,
                                                 src_cnt, src_start_mod, src_end_idx, dst_end_idx_t,
                                                 src_end_idx_t, _nxz, N>::shm_int_unroll(dst_int,
                                                                                         src_int);
                                        }
#endif
                                }
                            else { // dst_start_mod < src_start_mod
                                __constexpr unsigned int rshift = src_start_mod - dst_start_mod;
                                __constexpr unsigned int lshift = 32 - rshift;

#ifndef USE_SHM_INT_UNROLL
                                for (int i = src_start_idx, j = dst_start_idx; j <= dst_end_idx;
                                     i++, j++) {
                                    if (i + 1 <= src_end_idx) {
                                        shm_int_common<dst_whole, src_whole>(
                                            dst_int_mask_half[j], (dst_int + j),
                                            ((src_int[i] >> rshift) | (src_int[i + 1] << lshift)));
                                        shm_int_common<dst_whole, src_whole>(
                                            dst_int_mask_half[j], (dst_int + j + dst_cnt),
                                            ((src_int[i + src_cnt] >> rshift) |
                                             (src_int[i + 1 + src_cnt] << lshift)));
                                    } else {
                                        shm_int_common<dst_whole, src_whole>(
                                            dst_int_mask_half[j], (dst_int + j),
                                            (src_int[i] >> rshift));
                                        shm_int_common<dst_whole, src_whole>(
                                            dst_int_mask_half[j], (dst_int + j + dst_cnt),
                                            (src_int[i + src_cnt] >> rshift));
                                    }
                                }
#else
                                __constexpr int N = dst_end_idx - dst_start_idx; // (N >= 0)
                                __constexpr int dst_end_idx_t = dst_end_idx;
                                __constexpr int src_end_idx_t = src_start_idx + N;
                                Loop<dst_width, dst_base, src_width, src_base, count, dst_whole,
                                     src_whole, dst_cnt, dst_start_mod, src_cnt, src_start_mod,
                                     src_end_idx, dst_end_idx_t, src_end_idx_t, _nxz,
                                     N>::shm_int_unroll(dst_int, src_int);
#endif
                            }

                            if
                                __constexpr(_edge) {
                                    if
                                        __constexpr(count > 1) {
                                            if
                                                __constexpr(!aligned) {
                                                    if (edge == pint_nonedge) {
                                                        edge = get_edge_other<
                                                            true, dst_width, dst_base, count,
                                                            decltype(from_valid),
                                                            (from_valid.size() >> 1), _nxz>(
                                                            from_valid, dst);
                                                    }
                                                }
                                        }
                                }
                        }
                    }
                }
            }
        }
    else { // nxz

        if
            __constexpr(dst_width <= 4) {
                unsigned char *dst_char = (unsigned char *)dst;
                __constexpr unsigned char dst_char_mask = get_mask_char<dst_base, count, _nxz>();

                if
                    __constexpr(src_width <= 4) {
                        unsigned char *src_char = (unsigned char *)src;
                        __constexpr unsigned char src_char_mask =
                            get_mask_char<src_base, count, _nxz>();

                        if
                            __constexpr(_edge) {
                                //// at (dst_whole == true), dst_base must be 0
                                //// at (src_whole == true), src_base must be 0
                                if
                                    __constexpr(dst_whole && src_whole) {
                                        edge = get_edge<true, 0, _nxz>(dst_char[0], src_char[0]);
                                    }
                                else {
                                    from_signal = get_signal<true, true, dst_width, dst_base, count,
                                                             decltype(from_signal), _nxz>(dst);
                                }
                            }

                        if
                            __constexpr(dst_base == src_base) {
                                shm_char<dst_whole, src_whole, dst_char_mask>(dst_char,
                                                                              src_char[0]);
                            }
                        else if
                            __constexpr(dst_base > src_base) {
                                __constexpr unsigned int lshift =
                                    dst_base - src_base; // (lshift <= 3)

                                shm_char<dst_whole, src_whole, dst_char_mask>(
                                    dst_char, (src_char[0] << lshift));
                            }
                        else { // dst_base < src_base
                            __constexpr unsigned int rshift = src_base - dst_base; // (rshift <= 3)

                            shm_char<dst_whole, src_whole, dst_char_mask>(dst_char,
                                                                          (src_char[0] >> rshift));
                        }

                        if
                            __constexpr(_edge) {
                                if
                                    __constexpr(!(dst_whole && src_whole)) {
                                        edge = get_edge<true, dst_base, _nxz>(
                                            from_signal, dst_char[0]); // ??? get_edge2
                                    }
                            }
                    }
                else { // src_width > 4
                    unsigned int *src_int = (unsigned int *)src;
                    __constexpr auto src_int_mask_half =
                        get_mask_int_half<src_width, src_base, count, unsigned int>();

                    if
                        __constexpr(_edge) {
                            from_signal = get_signal<true, true, dst_width, dst_base, count,
                                                     decltype(from_signal), _nxz>(dst);
                        }

                    if
                        __constexpr(src_width <= 32) {
                            if
                                __constexpr(dst_base == src_base) {
                                    shm_char<dst_whole, true, dst_char_mask>(
                                        dst_char, ((src_int[0] & src_int_mask_half[0])));
                                }
                            else if
                                __constexpr(dst_base > src_base) {
                                    __constexpr unsigned int lshift = dst_base - src_base;

                                    shm_char<dst_whole, true, dst_char_mask>(
                                        dst_char,
                                        (((src_int[0] & src_int_mask_half[0]) << lshift)));
                                }
                            else { // dst_base < src_base
                                __constexpr unsigned int rshift = src_base - dst_base;

                                shm_char<dst_whole, true, dst_char_mask>(
                                    dst_char, (((src_int[0] & src_int_mask_half[0]) >> rshift)));
                            }
                        }
                    else { // src_width > 32
                        __constexpr unsigned int src_cnt =
                            (src_width + 31) >> 5; // (width + (32-1)) / 32

                        __constexpr unsigned int src_start_idx = src_base >> 5;   // start / 32
                        __constexpr unsigned int src_start_mod = src_base & 0x1f; // start % 32
                        __constexpr unsigned int src_end_idx = (src_base + count - 1) >> 5;

                        if
                            __constexpr(src_end_idx == src_start_idx) {
                                if
                                    __constexpr(dst_base == src_start_mod) {
                                        shm_char<dst_whole, true, dst_char_mask>(
                                            dst_char, ((src_int[src_start_idx] &
                                                        src_int_mask_half[src_start_idx])));
                                    }
                                else if
                                    __constexpr(dst_base > src_start_mod) {
                                        __constexpr unsigned int lshift = dst_base - src_start_mod;

                                        shm_char<dst_whole, true, dst_char_mask>(
                                            dst_char, (((src_int[src_start_idx] &
                                                         src_int_mask_half[src_start_idx])
                                                        << lshift)));
                                    }
                                else { // dst_base < src_start_mod
                                    __constexpr unsigned int rshift = src_start_mod - dst_base;

                                    shm_char<dst_whole, true, dst_char_mask>(
                                        dst_char, (((src_int[src_start_idx] &
                                                     src_int_mask_half[src_start_idx]) >>
                                                    rshift)));
                                }
                            }
                        else {
                            // (dst_base < src_start_mod) always
                            __constexpr unsigned int rshift = src_start_mod - dst_base;
                            __constexpr unsigned int lshift = 32 - rshift;

                            shm_char<dst_whole, true, dst_char_mask>(
                                dst_char, ((((src_int[src_start_idx]) >> rshift) |
                                            ((src_int[src_start_idx + 1] &
                                              src_int_mask_half[src_start_idx + 1])
                                             << lshift))));
                        }
                    }

                    if
                        __constexpr(_edge) {
                            edge = get_edge<true, dst_base, _nxz>(from_signal, dst_char[0]);
                        }
                }
            }
        else { // dst_width > 4
            unsigned int *dst_int = (unsigned int *)dst;
            __constexpr auto dst_int_mask_half =
                get_mask_int_half<dst_width, dst_base, count, unsigned int>();

            if
                __constexpr(dst_width <= 32) {
                    if
                        __constexpr(src_width <= 4) {
                            unsigned char *src_char = (unsigned char *)src;
                            __constexpr unsigned char src_char_mask =
                                get_mask_char<src_base, count, _nxz>();

                            if
                                __constexpr(_edge) {
                                    unsigned char from_edge_bit =
                                        get_edge_bit<dst_width, dst_base, _nxz>(dst);
                                    unsigned char to_edge_bit =
                                        get_edge_bit<src_width, src_base, _nxz>(src);
                                    edge = get_edge<false, 0, _nxz>(from_edge_bit, to_edge_bit);

                                    if
                                        __constexpr(count > 1) {
                                            if (edge == pint_nonedge) {
                                                from_signal =
                                                    get_signal<true, true, dst_width, dst_base,
                                                               count, decltype(from_signal), _nxz>(
                                                        dst);
                                            }
                                        }
                                }

                            if
                                __constexpr(dst_base == src_base) {
                                    shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                        dst_int, (src_char[0]));
                                }
                            else if
                                __constexpr(dst_base > src_base) {
                                    __constexpr unsigned int lshift = dst_base - src_base;

                                    shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                        dst_int, (src_char[0] << lshift));
                                }
                            else { // dst_base < src_base
                                __constexpr unsigned int rshift = src_base - dst_base;

                                shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                    dst_int, (src_char[0] >> rshift));
                            }

                            if
                                __constexpr(_edge) {
                                    if
                                        __constexpr(count > 1) {
                                            if (edge == pint_nonedge) {
                                                edge =
                                                    get_edge_other<true, dst_width, dst_base, count,
                                                                   decltype(from_signal),
                                                                   (from_signal.size() >> 1), _nxz>(
                                                        from_signal, dst);
                                            }
                                        }
                                }
                        }
                    else { // src_width > 4
                        unsigned int *src_int = (unsigned int *)src;
                        __constexpr auto src_int_mask_half =
                            get_mask_int_half<src_width, src_base, count, unsigned int>();

                        if
                            __constexpr(src_width <= 32) {
                                if
                                    __constexpr(_edge) {
                                        unsigned char from_edge_bit =
                                            get_edge_bit<dst_width, dst_base, _nxz>(dst);
                                        unsigned char to_edge_bit =
                                            get_edge_bit<src_width, src_base, _nxz>(src);
                                        edge = get_edge<false, 0, _nxz>(from_edge_bit, to_edge_bit);

                                        if
                                            __constexpr(count > 1) {
                                                if (edge == pint_nonedge) {
                                                    //// at (dst_whole == true), dst_base must be 0
                                                    //// at (src_whole == true), src_base must be 0
                                                    if
                                                        __constexpr(dst_whole && src_whole) {
                                                            edge = get_edge_other<
                                                                true, src_width, src_base, count,
                                                                unsigned int *,
                                                                (from_signal.size() >> 1), _nxz>(
                                                                dst_int, src);
                                                        }
                                                    else {
                                                        from_signal =
                                                            get_signal<true, true, dst_width,
                                                                       dst_base, count,
                                                                       decltype(from_signal), _nxz>(
                                                                dst);
                                                    }
                                                }
                                            }
                                    }

                                if
                                    __constexpr(dst_base == src_base) {
                                        shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                            dst_int, src_int[0]);
                                    }
                                else if
                                    __constexpr(dst_base > src_base) {
                                        __constexpr unsigned int lshift =
                                            dst_base - src_base; // (lshift <= 31)

                                        shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                            dst_int, (src_int[0] << lshift));
                                    }
                                else { // dst_base < src_base
                                    __constexpr unsigned int rshift =
                                        src_base - dst_base; // (rshift <= 31)

                                    shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                        dst_int, (src_int[0] >> rshift));
                                }

                                if
                                    __constexpr(_edge) {
                                        if
                                            __constexpr(count > 1) {
                                                if
                                                    __constexpr(!(dst_whole && src_whole)) {
                                                        if (edge == pint_nonedge) {
                                                            edge = get_edge_other<
                                                                true, dst_width, dst_base, count,
                                                                decltype(from_signal),
                                                                (from_signal.size() >> 1), _nxz>(
                                                                from_signal, dst);
                                                        }
                                                    }
                                            }
                                    }
                            }
                        else { // src_width > 32
                            __constexpr unsigned int src_cnt =
                                (src_width + 31) >> 5; // (width + (32-1)) / 32

                            __constexpr unsigned int src_start_idx = src_base >> 5;   // start / 32
                            __constexpr unsigned int src_start_mod = src_base & 0x1f; // start % 32
                            __constexpr unsigned int src_end_idx = (src_base + count - 1) >> 5;

                            __constexpr bool aligned =
                                dst_whole &&
                                ((src_start_mod == 0) &&
                                 ((src_base + count == src_width) || ((count & 0x1f) == 0)));
                            if
                                __constexpr(_edge) {
                                    unsigned char from_edge_bit =
                                        get_edge_bit<dst_width, dst_base, _nxz>(dst);
                                    unsigned char to_edge_bit =
                                        get_edge_bit<src_width, src_base, _nxz>(src);
                                    edge = get_edge<false, 0, _nxz>(from_edge_bit, to_edge_bit);

                                    if
                                        __constexpr(count > 1) {
                                            if (edge == pint_nonedge) {
                                                if
                                                    __constexpr(aligned) {
                                                        edge = get_edge_other<
                                                            true, src_width, src_base, count,
                                                            unsigned int *,
                                                            (from_signal.size() >> 1), _nxz>(
                                                            dst_int, (void *)src_int);
                                                    }
                                                else {
                                                    from_signal =
                                                        get_signal<true, true, dst_width, dst_base,
                                                                   count, decltype(from_signal),
                                                                   _nxz>(dst);
                                                }
                                            }
                                        }
                                }

                            if
                                __constexpr(src_end_idx == src_start_idx) {
                                    if
                                        __constexpr(dst_base == src_start_mod) {
                                            shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                                dst_int, src_int[src_start_idx]);
                                        }
                                    else if
                                        __constexpr(dst_base > src_start_mod) {
                                            __constexpr unsigned int lshift =
                                                dst_base - src_start_mod;

                                            shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                                dst_int, (src_int[src_start_idx] << lshift));
                                        }
                                    else { // dst_base < src_start_mod
                                        __constexpr unsigned int rshift = src_start_mod - dst_base;

                                        shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                            dst_int, (src_int[src_start_idx] >> rshift));
                                    }
                                }
                            else {
                                // (dst_base < src_start_mod) always
                                __constexpr unsigned int rshift = src_start_mod - dst_base;
                                __constexpr unsigned int lshift = 32 - rshift;

                                shm_int<dst_whole, src_whole, dst_int_mask_half[0]>(
                                    dst_int, ((src_int[src_start_idx] >> rshift) |
                                              (src_int[src_start_idx + 1] << lshift)));
                            }

                            if
                                __constexpr(_edge) {
                                    if
                                        __constexpr(count > 1) {
                                            if
                                                __constexpr(!aligned) {
                                                    if (edge == pint_nonedge) {
                                                        edge = get_edge_other<
                                                            true, dst_width, dst_base, count,
                                                            decltype(from_signal),
                                                            (from_signal.size() >> 1), _nxz>(
                                                            from_signal, dst);
                                                    }
                                                }
                                        }
                                }
                        }
                    }
                }
            else {                                                      // dst_width > 32
                __constexpr unsigned int dst_cnt = (dst_width + 31) >> 5; // (width + (32-1)) / 32

                __constexpr unsigned int dst_start_idx = dst_base >> 5;   // start / 32
                __constexpr unsigned int dst_start_mod = dst_base & 0x1f; // start % 32
                __constexpr unsigned int dst_end_idx = (dst_base + count - 1) >> 5;

                if
                    __constexpr(src_width <= 4) {
                        unsigned char *src_char = (unsigned char *)src;
                        __constexpr unsigned char src_char_mask =
                            get_mask_char<src_base, count, _nxz>();

                        if
                            __constexpr(_edge) {
                                unsigned char from_edge_bit =
                                    get_edge_bit<dst_width, dst_base, _nxz>(dst);
                                unsigned char to_edge_bit =
                                    get_edge_bit<src_width, src_base, _nxz>(src);
                                edge = get_edge<false, 0, _nxz>(from_edge_bit, to_edge_bit);

                                if
                                    __constexpr(count > 1) {
                                        if (edge == pint_nonedge) {
                                            from_valid =
                                                get_signal<false, true, dst_width, dst_base, count,
                                                           decltype(from_valid), _nxz>(dst);
                                        }
                                    }
                            }

                        if
                            __constexpr(dst_end_idx == dst_start_idx) {
                                if
                                    __constexpr(dst_start_mod == src_base) {
                                        shm_int<dst_whole, src_whole,
                                                dst_int_mask_half[dst_start_idx]>(
                                            (dst_int + dst_start_idx), (src_char[0]));
                                    }
                                else if
                                    __constexpr(dst_start_mod > src_base) {
                                        __constexpr unsigned int lshift = dst_start_mod - src_base;

                                        shm_int<dst_whole, src_whole,
                                                dst_int_mask_half[dst_start_idx]>(
                                            (dst_int + dst_start_idx), (src_char[0] << lshift));
                                    }
                                else { // dst_start_mod < src_base
                                    __constexpr unsigned int rshift = src_base - dst_start_mod;

                                    shm_int<dst_whole, src_whole, dst_int_mask_half[dst_start_idx]>(
                                        (dst_int + dst_start_idx), (src_char[0] >> rshift));
                                }
                            }
                        else {
                            // (dst_start_mod > src_base) always
                            __constexpr unsigned int lshift = dst_start_mod - src_base;
                            __constexpr unsigned int rshift = 32 - lshift;

                            shm_int<dst_whole, src_whole, dst_int_mask_half[dst_start_idx]>(
                                (dst_int + dst_start_idx), (src_char[0] << lshift));
                            shm_int<dst_whole, src_whole, dst_int_mask_half[dst_start_idx + 1]>(
                                (dst_int + dst_start_idx + 1), (src_char[0] >> rshift));
                        }

                        if
                            __constexpr(_edge) {
                                if
                                    __constexpr(count > 1) {
                                        if (edge == pint_nonedge) {
                                            edge = get_edge_other<true, dst_width, dst_base, count,
                                                                  decltype(from_valid),
                                                                  (from_valid.size() >> 1), _nxz>(
                                                from_valid, dst);
                                        }
                                    }
                            }
                    }
                else { // src_width > 4
                    unsigned int *src_int = (unsigned int *)src;
                    __constexpr auto src_int_mask_half =
                        get_mask_int_half<src_width, src_base, count, unsigned int>();

                    if
                        __constexpr(src_width <= 32) {
                            __constexpr bool aligned =
                                ((dst_start_mod == 0) &&
                                 ((dst_base + count == dst_width) || ((count & 0x1f) == 0))) &&
                                src_whole;
                            if
                                __constexpr(_edge) {
                                    unsigned char from_edge_bit =
                                        get_edge_bit<dst_width, dst_base, _nxz>(dst);
                                    unsigned char to_edge_bit =
                                        get_edge_bit<src_width, src_base, _nxz>(src);
                                    edge = get_edge<false, 0, _nxz>(from_edge_bit, to_edge_bit);

                                    if
                                        __constexpr(count > 1) {
                                            if (edge == pint_nonedge) {
                                                if
                                                    __constexpr(aligned) {
                                                        edge = get_edge_other<
                                                            true, src_width, src_base, count,
                                                            unsigned int *,
                                                            (from_signal.size() >> 1), _nxz>(
                                                            dst_int + dst_start_idx, src);
                                                    }
                                                else {
                                                    from_valid =
                                                        get_signal<false, true, dst_width, dst_base,
                                                                   count, decltype(from_valid),
                                                                   _nxz>(dst);
                                                }
                                            }
                                        }
                                }

                            if
                                __constexpr(dst_end_idx == dst_start_idx) {
                                    if
                                        __constexpr(dst_start_mod == src_base) {
                                            shm_int<dst_whole, src_whole,
                                                    dst_int_mask_half[dst_start_idx]>(
                                                (dst_int + dst_start_idx), src_int[0]);
                                        }
                                    else if
                                        __constexpr(dst_start_mod > src_base) {
                                            __constexpr unsigned int lshift =
                                                dst_start_mod - src_base; // (lshift <= 31)

                                            shm_int<dst_whole, src_whole,
                                                    dst_int_mask_half[dst_start_idx]>(
                                                (dst_int + dst_start_idx), (src_int[0] << lshift));
                                        }
                                    else { // dst_start_mod < src_base
                                        __constexpr unsigned int rshift =
                                            src_base - dst_start_mod; // (rshift <= 31)

                                        shm_int<dst_whole, src_whole,
                                                dst_int_mask_half[dst_start_idx]>(
                                            (dst_int + dst_start_idx), (src_int[0] >> rshift));
                                    }
                                }
                            else {
                                // (dst_start_mod > src_base) always
                                __constexpr unsigned int lshift = dst_start_mod - src_base;
                                __constexpr unsigned int rshift = 32 - lshift;

                                shm_int<dst_whole, src_whole, dst_int_mask_half[dst_start_idx]>(
                                    (dst_int + dst_start_idx), (src_int[0] << lshift));
                                shm_int<dst_whole, src_whole, dst_int_mask_half[dst_start_idx + 1]>(
                                    (dst_int + dst_start_idx + 1), (src_int[0] >> rshift));
                            }

                            if
                                __constexpr(_edge) {
                                    if
                                        __constexpr(count > 1) {
                                            if
                                                __constexpr(!aligned) {
                                                    if (edge == pint_nonedge) {
                                                        edge = get_edge_other<
                                                            true, dst_width, dst_base, count,
                                                            decltype(from_valid),
                                                            (from_valid.size() >> 1), _nxz>(
                                                            from_valid, dst);
                                                    }
                                                }
                                        }
                                }
                        }
                    else {
                        __constexpr unsigned int src_cnt =
                            (src_width + 31) >> 5; // (width + (32-1)) / 32

                        __constexpr unsigned int src_start_idx = src_base >> 5;   // start / 32
                        __constexpr unsigned int src_start_mod = src_base & 0x1f; // start % 32
                        __constexpr unsigned int src_end_idx = (src_base + count - 1) >> 5;

                        __constexpr bool aligned =
                            ((dst_start_mod == 0) && (src_start_mod == 0)) &&
                            (((dst_base + count == dst_width) && (src_base + count == src_width)) ||
                             ((count & 0x1f) == 0));
                        if
                            __constexpr(_edge) {
                                unsigned char from_edge_bit =
                                    get_edge_bit<dst_width, dst_base, _nxz>(dst);
                                unsigned char to_edge_bit =
                                    get_edge_bit<src_width, src_base, _nxz>(src);
                                edge = get_edge<false, 0, _nxz>(from_edge_bit, to_edge_bit);

                                if
                                    __constexpr(count > 1) {
                                        if (edge == pint_nonedge) {
                                            if
                                                __constexpr(aligned) {
                                                    edge = get_edge_other<true, src_width, src_base,
                                                                          count, unsigned int *,
                                                                          (from_signal.size() >> 1),
                                                                          _nxz>(
                                                        dst_int + dst_start_idx, src);
                                                }
                                            else {
                                                from_valid =
                                                    get_signal<false, true, dst_width, dst_base,
                                                               count, decltype(from_valid), _nxz>(
                                                        dst);
                                            }
                                        }
                                    }
                            }

                        if
                            __constexpr(dst_start_mod == src_start_mod) {
#ifndef USE_SHM_INT_UNROLL
                                for (int i = src_start_idx, j = dst_start_idx; j <= dst_end_idx;
                                     i++, j++) {
                                    shm_int_common<dst_whole, src_whole>(dst_int_mask_half[j],
                                                                         (dst_int + j), src_int[i]);
                                }
#else
                                __constexpr int N = dst_end_idx - dst_start_idx; // (N >= 0)
                                __constexpr int dst_end_idx_t = dst_end_idx;
                                __constexpr int src_end_idx_t = src_start_idx + N;
                                Loop<dst_width, dst_base, src_width, src_base, count, dst_whole,
                                     src_whole, dst_cnt, dst_start_mod, src_cnt, src_start_mod,
                                     src_end_idx, dst_end_idx_t, src_end_idx_t, _nxz,
                                     N>::shm_int_unroll(dst_int, src_int);
#endif
                            }
                        else if
                            __constexpr(dst_start_mod > src_start_mod) {
                                __constexpr unsigned int lshift = dst_start_mod - src_start_mod;
                                __constexpr unsigned int rshift = 32 - lshift;

#ifndef USE_SHM_INT_UNROLL
                                shm_int_common<dst_whole, src_whole>(
                                    dst_int_mask_half[dst_start_idx], (dst_int + dst_start_idx),
                                    (src_int[src_start_idx] << lshift));

                                for (int i = src_start_idx + 1, j = dst_start_idx + 1;
                                     j <= dst_end_idx; i++, j++) {
                                    if (i <= src_end_idx) {
                                        shm_int_common<dst_whole, src_whole>(
                                            dst_int_mask_half[j], (dst_int + j),
                                            ((src_int[i - 1] >> rshift) | (src_int[i] << lshift)));
                                    } else {
                                        shm_int_common<dst_whole, src_whole>(
                                            dst_int_mask_half[j], (dst_int + j),
                                            (src_int[i - 1] >> rshift));
                                    }
                                }
#else
                                shm_int<dst_whole, src_whole, dst_int_mask_half[dst_start_idx]>(
                                    (dst_int + dst_start_idx), (src_int[src_start_idx] << lshift));

                                if
                                    __constexpr(dst_end_idx > dst_start_idx) {
                                        __constexpr int N =
                                            dst_end_idx - (dst_start_idx + 1); // (N >= 0)
                                        __constexpr int dst_end_idx_t = dst_end_idx;
                                        __constexpr int src_end_idx_t = (src_start_idx + 1) + N;
                                        Loop<dst_width, dst_base, src_width, src_base, count,
                                             dst_whole, src_whole, dst_cnt, dst_start_mod, src_cnt,
                                             src_start_mod, src_end_idx, dst_end_idx_t,
                                             src_end_idx_t, _nxz, N>::shm_int_unroll(dst_int,
                                                                                     src_int);
                                    }
#endif
                            }
                        else { // dst_start_mod < src_start_mod
                            __constexpr unsigned int rshift = src_start_mod - dst_start_mod;
                            __constexpr unsigned int lshift = 32 - rshift;

#ifndef USE_SHM_INT_UNROLL
                            for (int i = src_start_idx, j = dst_start_idx; j <= dst_end_idx;
                                 i++, j++) {
                                if (i + 1 <= src_end_idx) {
                                    shm_int_common<dst_whole, src_whole>(
                                        dst_int_mask_half[j], (dst_int + j),
                                        ((src_int[i] >> rshift) | (src_int[i + 1] << lshift)));
                                } else {
                                    shm_int_common<dst_whole, src_whole>(dst_int_mask_half[j],
                                                                         (dst_int + j),
                                                                         (src_int[i] >> rshift));
                                }
                            }
#else
                            __constexpr int N = dst_end_idx - dst_start_idx; // (N >= 0)
                            __constexpr int dst_end_idx_t = dst_end_idx;
                            __constexpr int src_end_idx_t = src_start_idx + N;
                            Loop<dst_width, dst_base, src_width, src_base, count, dst_whole,
                                 src_whole, dst_cnt, dst_start_mod, src_cnt, src_start_mod,
                                 src_end_idx, dst_end_idx_t, src_end_idx_t, _nxz,
                                 N>::shm_int_unroll(dst_int, src_int);
#endif
                        }

                        if
                            __constexpr(_edge) {
                                if
                                    __constexpr(count > 1) {
                                        if
                                            __constexpr(!aligned) {
                                                if (edge == pint_nonedge) {
                                                    edge =
                                                        get_edge_other<true, dst_width, dst_base,
                                                                       count, decltype(from_valid),
                                                                       (from_valid.size() >> 1),
                                                                       _nxz>(from_valid, dst);
                                                }
                                            }
                                    }
                            }
                    }
                }
            }
        }

    } // nxz

    return edge;
}
#pragma GCC pop_options

#define pint_copy_signal_common_edge(res, src, src_width, src_base, dst, dst_width, dst_base,      \
                                     count, _nxz)                                                  \
    do {                                                                                           \
        if ((dst_base >= 0) && (dst_base + count <= dst_width)) {                                  \
            res = copy_signal_common_edge<src_width, src_base, dst_width, __TO_CONST(dst_base),    \
                                          count, true, _nxz>(                                      \
                src, dst, dst_base, typename TagDispatchTrait<__TO_CONST(dst_base)>::Tag{});       \
        } else {                                                                                   \
            res = pint_copy_sig_out_range_edge<_nxz>(src, src_width, src_base, dst, dst_width,     \
                                                     dst_base, count);                             \
        }                                                                                          \
    } while (0)

#define pint_copy_signal_common_noedge(src, src_width, src_base, dst, dst_width, dst_base, count,  \
                                       _nxz)                                                       \
    do {                                                                                           \
        if ((dst_base >= 0) && (dst_base + count <= dst_width)) {                                  \
            copy_signal_common_edge<src_width, src_base, dst_width, __TO_CONST(dst_base), count,   \
                                    false, _nxz>(                                                  \
                src, dst, dst_base, typename TagDispatchTrait<__TO_CONST(dst_base)>::Tag{});       \
        } else {                                                                                   \
            pint_copy_sig_out_range_noedge<_nxz>(src, src_width, src_base, dst, dst_width,         \
                                                 dst_base, count);                                 \
        }                                                                                          \
    } while (0)

#else
#define IS_CONSTEXPR(x) (__builtin_constant_p(x))
#define __TO_CONST(x) (IS_CONSTEXPR(x) ? (x) : PINT_INT_MASK)

template <bool dst_whole, bool src_whole>
inline void shm_char_common(unsigned char mask, unsigned char *dst, unsigned char src) {
    if
        __constexpr(dst_whole) {
            if
                __constexpr(src_whole) { dst[0] = src; }
            else {
                dst[0] = src & mask;
            }
        }
    else {
#ifdef CPU_MODE
        dst[0] = (dst[0] & ~mask) | (src & mask);
#else
        unsigned int rs2;
        if ((unsigned int)dst & 0x01) {
            rs2 = (mask << 24) | (src << 8);
        } else {
            rs2 = (mask << 16) | src;
        }
        asm volatile("uap.shm\t%1, %0" : "+m"(*dst), "+r"(rs2));
#endif
    }
}

template <bool dst_whole, bool src_whole>
inline void shm_int_common(unsigned int mask, unsigned int *dst, unsigned int src) {
    if
        __constexpr(dst_whole) {
            if
                __constexpr(src_whole) { dst[0] = src; }
            else {
                dst[0] = src & mask;
            }
        }
    else {
        if (mask == PINT_INT_MASK) {
            dst[0] = src;
        } else {
#ifdef CPU_MODE
            dst[0] = (dst[0] & ~mask) | (src & mask);
#else
            unsigned int mask_l = mask << 16;
            unsigned int mask_h = mask & 0xffff0000;
            unsigned short *dst_short = (unsigned short *)dst;
            unsigned int rs2;

            if (mask_l == 0xffff0000) {
                dst_short[0] = (unsigned short)src;
            } else {
                rs2 = mask_l | (src & 0xffff);
                asm volatile("uap.shm\t%1, %0" : "+m"(*dst_short), "+r"(rs2));
            }

            if (mask_h == 0xffff0000) {
                dst_short[1] = (unsigned short)(src >> 16);
            } else {
                rs2 = mask_h | (src >> 16);
                asm volatile("uap.shm\t%1, %0" : "+m"(*(dst_short + 1)), "+r"(rs2));
            }
#endif
        }
    }
}

struct NormalVersionTag {};
struct PartialVersionTag {};

template <unsigned int dst_base> struct TagDispatchTrait { using Tag = PartialVersionTag; };

template <> struct TagDispatchTrait<PINT_INT_MASK> { using Tag = PartialVersionTag; };

template <unsigned int src_width, unsigned int src_base, unsigned int dst_width,
          unsigned int dst_base, unsigned int count, bool _edge = true, bool _nxz = false>
/*inline*/ pint_edge_t copy_signal_common_edge(const void *src, void *dst, unsigned int dst_base_,
                                               PartialVersionTag) {
    NCORE_PERF_PINT_NET_SUMMARY(184);
    if
        __constexpr((dst_width <= 4) && (src_width <= 4)) {
            //// at (count == dst_width), dst_base must be 0
            //// at (count == src_width), src_base must be 0
            if
                __constexpr(dst_width == src_width) {
                    if
                        __constexpr(count == dst_width) {
                            if
                                __constexpr(!_edge) {
                                    ((unsigned char *)dst)[0] = ((unsigned char *)src)[0];
                                    return pint_nonedge;
                                }
                            else {
                                return (pint_edge_t)pint_copy_char_char_same_edge<_nxz>(src, dst);
                            }
                        }
                }
            if
                __constexpr(!_edge) {
                    pint_copy_char_char<_nxz>(src, src_base, dst, dst_base_, count);
                    return pint_nonedge;
                }
            else {
                return (pint_edge_t)pint_copy_char_char_edge<_nxz>(src, src_base, dst, dst_base_,
                                                                   count);
            }
        }
    else if
        __constexpr((dst_width <= 4) && (src_width > 4)) {
            if
                __constexpr(!_edge) {
                    pint_copy_int_char<_nxz>(src, src_width, src_base, dst, dst_base_, count);
                    return pint_nonedge;
                }
            else {
                return (pint_edge_t)pint_copy_int_char_edge<_nxz>(src, src_width, src_base, dst,
                                                                  dst_base_, count);
            }
        }
    else if
        __constexpr((dst_width > 4) && (src_width <= 4)) {
            if
                __constexpr(!_edge) {
                    pint_copy_char_int<_nxz>(src, src_base, dst, dst_width, dst_base_, count);
                    return pint_nonedge;
                }
            else {
                return (pint_edge_t)pint_copy_char_int_edge<_nxz>(src, src_base, dst, dst_width,
                                                                  dst_base_, count);
            }
        }
    else {
        //// at (count == dst_width), dst_base must be 0
        //// at (count == src_width), src_base must be 0
        if
            __constexpr(dst_width == src_width) {
                if
                    __constexpr(count == dst_width) {
                        if
                            __constexpr(!_edge) {
                                for (int i = 0; i < (dst_width + 31) / 32 * 2; i++) {
                                    ((unsigned int *)dst)[i] = ((unsigned int *)src)[i];
                                }
                                return pint_nonedge;
                            }
                        else {
                            return (pint_edge_t)pint_copy_int_int_same_edge<_nxz>(src, dst,
                                                                                  dst_width);
                        }
                    }
            }
        if
            __constexpr(!_edge) {
                pint_copy_int_int<_nxz>(src, src_width, src_base, dst, dst_width, dst_base_, count);
                return pint_nonedge;
            }
        else {
            return (pint_edge_t)pint_copy_int_int_edge<_nxz>(src, src_width, src_base, dst,
                                                             dst_width, dst_base_, count);
        }
    }
}

#define pint_copy_signal_common_edge(res, src, src_width, src_base, dst, dst_width, dst_base,      \
                                     count, _nxz)                                                  \
    do {                                                                                           \
        if ((dst_base >= 0) && (dst_base + count <= dst_width)) {                                  \
            res = copy_signal_common_edge<src_width, src_base, dst_width, PINT_INT_MASK, count,    \
                                          true, _nxz>(src, dst, dst_base, PartialVersionTag{});    \
        } else {                                                                                   \
            res = pint_copy_sig_out_range_edge<_nxz>(src, src_width, src_base, dst, dst_width,     \
                                                     dst_base, count);                             \
        }                                                                                          \
    } while (0)

#define pint_copy_signal_common_noedge(src, src_width, src_base, dst, dst_width, dst_base, count,  \
                                       _nxz)                                                       \
    do {                                                                                           \
        if ((dst_base >= 0) && (dst_base + count <= dst_width)) {                                  \
            copy_signal_common_edge<src_width, src_base, dst_width, PINT_INT_MASK, count, false,   \
                                    _nxz>(src, dst, dst_base, PartialVersionTag{});                \
        } else {                                                                                   \
            pint_copy_sig_out_range_noedge<_nxz>(src, src_width, src_base, dst, dst_width,         \
                                                 dst_base, count);                                 \
        }                                                                                          \
    } while (0)

#endif // COPY_USE_TPL

#if 1
template <bool _nxz = false>
void pint_mask_copy_char(unsigned char *out, const unsigned char *in0,
                         const unsigned char *addr_mask) {
    NCORE_PERF_MEASURE(pint_mask_copy_char, 2);
    // pint_assert(size <= PINT_CHAR_OFFSET, "size if not valid for char.");
    unsigned char mask = *addr_mask;
    if
        __constexpr(!_nxz) { mask |= mask << 4; }
    shm_char_common<false, false>(mask, out, *in0);
}

template <bool _nxz = false>
void pint_mask_copy_int(unsigned int *out, const unsigned int *in0, const unsigned int *mask,
                        unsigned int width) {
    NCORE_PERF_MEASURE(pint_mask_copy_int, 2);
    unsigned cnt = (width + 0x1f) >> 5;
    int i;
    for (i = 0; i < cnt; i++) {
        shm_int_common<false, false>(mask[i], out + i, in0[i]);
        if
            __constexpr(!_nxz) { shm_int_common<false, false>(mask[i], out + i + cnt, in0[i + cnt]); }
    }
}
#endif

/*__constexpr*/ forceinline void pint_set_mask(unsigned int *out, unsigned int updt_base,
                                             unsigned int updt_size, unsigned int width) {
    if (updt_base >= width || !updt_size) {
        return;
    }
    updt_size = (updt_base + updt_size) > width ? width - updt_base : updt_size;
    unsigned idx_st = updt_base >> 5;
    unsigned idx_ed = (updt_base + updt_size - 1) >> 5;
    unsigned off, i;
    switch (idx_ed - idx_st) {
    case 0:
        if (updt_size != 32) {
            out[idx_st] |= ((1 << updt_size) - 1) << (updt_base & 0x1f);
        } else {
            out[idx_st] |= 0xffffffff;
        }
        break;
    case 1:
        out[idx_st] |= 0xffffffff << (updt_base & 0x1f);
        off = (updt_base + updt_size) & 0x1f;
        if (off) {
            out[idx_ed] |= (1 << off) - 1;
        } else {
            out[idx_ed] |= 0xffffffff;
        }
        break;
    default:
        out[idx_st] |= 0xffffffff << (updt_base & 0x1f);
        for (i = idx_st + 1; i < idx_ed; i++) {
            out[i] |= 0xffffffff;
        }
        off = (updt_base + updt_size) & 0x1f;
        if (off) {
            out[idx_ed] |= (1 << off) - 1;
        } else {
            out[idx_ed] |= 0xffffffff;
        }
        break;
    }
}

/*__constexpr*/ forceinline void pint_set_mult_arr_updt_word(unsigned int *updt, unsigned int word) {
    unsigned num = updt[0];
    unsigned i;
    if (num > 3) {
        return;
    }
    for (i = 1; i <= num; i++) {
        if (updt[i] == word) {
            return;
        }
    }
    updt[0] = ++num;
    if (num <= 3) {
        updt[num] = word;
    }
}

#endif
