#include "../inc/pint_simu.h"
#include "../inc/pint_vpi.h"
#include "../inc/pint_net.h"
#include "../common/perf.h"

volatile int pint_h2d_completed[256] __attribute__((section(".builtin_simu_buffer")));
volatile int pint_h2d_mcore_completed __attribute__((section(".builtin_simu_buffer")));

int width_to_len(int width) { return (width + 0x1f) >> 5; }

bool is_sig_zero(unsigned int *number, int width) {
    int i;
    int len = width_to_len(width);
    for (i = 0; i < len; i++) {
        if (number[i] != 0)
            return false;
    }
    return true;
}

void reverse_string(unsigned char *str, int len) {
    int i, j;
    for (i = 0, j = len - 1; i < j; i++, j--) {
        char c;
        c = str[i];
        str[i] = str[j];
        str[j] = c;
    }
}

void copy_sig(unsigned int *target, unsigned int *number, int width) {
    int i;
    int len = width_to_len(width);
    for (i = 0; i < len; i++) {
        target[i] = number[i];
    }
}

void clear_sig(unsigned int *target, int width) {
    int i;
    int len = width_to_len(width);
    for (i = 0; i < len; i++) {
        target[i] = 0;
    }
}

void left_shift_sig(unsigned int *target, unsigned int *number, int value, int width) {
    int i, j;
    int len = width_to_len(width);
    int vid = value >> 5;
    int mod = value & 0x1f;

    unsigned int *tmp = (unsigned int *)pint_mem_alloc(len * sizeof(unsigned int), SUB_T_CLEAR);
    clear_sig(tmp, width);

    unsigned int mask;
    if (width & 0x1f)
        mask = (1 << (width & 0x1f)) - 1;
    else
        mask = 0xFFFFFFFF;

    if (vid >= len) {
        clear_sig(target, width);
#ifdef CPU_MODE
        free(tmp);
#endif
        return;
    }
    if (value == 0) {
        copy_sig(target, number, width);
#ifdef CPU_MODE
        free(tmp);
#endif
        return;
    }

    for (i = vid; i < len; i++) {
        for (j = 0; j < vid; j++) {
            tmp[j] = 0;
        }

        tmp[i] = (number[i - vid] << mod);
        if (i - vid != 0 && mod > 0) {
            tmp[i] |= number[i - vid - 1] >> (PINT_INT_SIZE - mod);
        }
    }
    tmp[len - 1] &= mask;
    copy_sig(target, tmp, width);
#ifdef CPU_MODE
    free(tmp);
#endif
}

void right_shift_sig(unsigned int *target, unsigned int *number, int value, int width) {
    int i, j;
    int len = width_to_len(width);
    int vid = value >> 5;
    int mod = value & 0x1f;

    unsigned int *tmp = (unsigned int *)pint_mem_alloc(len * sizeof(unsigned int), SUB_T_CLEAR);
    clear_sig(tmp, width);

    unsigned int mask;
    if (width & 0x1f)
        mask = (1 << (width & 0x1f)) - 1;
    else
        mask = 0xFFFFFFFF;

    if (vid >= len) {
        clear_sig(target, width);
#ifdef CPU_MODE
        free(tmp);
#endif
        return;
    }
    if (value == 0) {
        copy_sig(target, number, width);
#ifdef CPU_MODE
        free(tmp);
#endif
        return;
    }

    for (i = len - vid - 1; i >= 0; i--) {
        for (j = len - 1; j > len - vid - 1; j--) {
            tmp[j] = 0;
        }
        tmp[i] = number[i + vid] >> mod;
        if (i + vid != len - 1 && mod > 0) {
            tmp[i] |= number[i + vid + 1] << (PINT_INT_SIZE - mod);
        }
    }
    tmp[len - 1] &= mask;
    copy_sig(target, tmp, width);
#ifdef CPU_MODE
    free(tmp);
#endif
}

void minus_sig(unsigned int *target, unsigned int *lnumber, unsigned int *rnumber, int width) {
    int i;
    int len = width_to_len(width);
    unsigned int *tmp = (unsigned int *)pint_mem_alloc(len * sizeof(unsigned int), SUB_T_CLEAR);
    clear_sig(tmp, width);

    unsigned int mask;
    if (width & 0x1f)
        mask = (1 << (width & 0x1f)) - 1;
    else
        mask = 0xFFFFFFFF;

    int borrow = 0;
    for (i = 0; i < len; i++) {
        unsigned int lvalue = lnumber[i];
        if (i != 0) {
            if ((tmp[i - 1] > lnumber[i - 1]) || (1 == borrow)) {
                borrow = (lvalue >= 1) ? 0 : 1;
                lvalue -= 1;
            }
        }
        tmp[i] = lvalue - rnumber[i];
    }
    tmp[len - 1] &= mask;
    copy_sig(target, tmp, width);
#ifdef CPU_MODE
    free(tmp);
#endif
}

int work_bits_sig(unsigned int *number, int width) {
    int i, res = 0;
    int len = width_to_len(width);
    for (i = len - 1; i >= 0; i--) {
        if (number[i]) {
            res += i * PINT_INT_SIZE;
            unsigned int num = number[i];
            while (num) {
                num >>= 1;
                res++;
            }
            break;
        }
    }
    return res;
}

bool is_sig_equal(unsigned int *lnumber, unsigned int *rnumber, int width) {
    int i;
    int len = width_to_len(width);
    for (i = 0; i < len; i++) {
        if (lnumber[i] != rnumber[i])
            return false;
    }
    return true;
}

bool is_sig_gt(unsigned int *lnumber, unsigned int *rnumber, int width) {
    int i;
    int len = width_to_len(width);
    for (i = len - 1; i >= 0; i--) {
        if (lnumber[i] > rnumber[i])
            return true;
        else if (lnumber[i] < rnumber[i])
            return false;
    }
    return false;
}

bool is_sig_gte(unsigned int *lnumber, unsigned int *rnumber, int width) {
    return is_sig_gt(lnumber, rnumber, width) || is_sig_equal(lnumber, rnumber, width);
}

void or_sig(unsigned int *target, unsigned int *lnumber, unsigned int *rnumber, int width) {
    int i;
    int len = width_to_len(width);
    for (i = 0; i < len; i++) {
        target[i] = lnumber[i] | rnumber[i];
    }
}

void div_mod_sig(unsigned int *lnumber, unsigned int *rnumber, unsigned int *ret_div,
                 unsigned int *ret_mod, int width) {
    int i;
    int len = width_to_len(width);
    unsigned int *copyd = (unsigned int *)pint_mem_alloc(len * sizeof(unsigned int), SUB_T_CLEAR);
    unsigned int *adder = (unsigned int *)pint_mem_alloc(len * sizeof(unsigned int), SUB_T_CLEAR);
    unsigned int *div = (unsigned int *)pint_mem_alloc(len * sizeof(unsigned int), SUB_T_CLEAR);
    unsigned int *mod = (unsigned int *)pint_mem_alloc(len * sizeof(unsigned int), SUB_T_CLEAR);

    unsigned int *one = (unsigned int *)pint_mem_alloc(len * sizeof(unsigned int), SUB_T_CLEAR);
    one[0] = 1;
    for (int j = 1; j < len; j++) {
        one[j] = 0;
    }

    int diff_bits = work_bits_sig(lnumber, width) - work_bits_sig(rnumber, width);
    clear_sig(div, width);
    copy_sig(mod, lnumber, width);

    if (is_sig_gt(rnumber, lnumber, width)) {
        copy_sig(ret_mod, lnumber, width);
        clear_sig(ret_div, width);
    } else {
        left_shift_sig(copyd, rnumber, diff_bits, width);
        left_shift_sig(adder, one, diff_bits, width);

        if (is_sig_gt(copyd, mod, width)) {
            right_shift_sig(copyd, copyd, 1, width);
            right_shift_sig(adder, adder, 1, width);
        }
        while (is_sig_gte(mod, rnumber, width)) {
            if (is_sig_gte(mod, copyd, width)) {
                minus_sig(mod, mod, copyd, width);
                or_sig(div, adder, div, width);
            }

            right_shift_sig(copyd, copyd, 1, width);
            right_shift_sig(adder, adder, 1, width);
        }
        copy_sig(ret_div, div, width);
        copy_sig(ret_mod, mod, width);
    }
#ifdef CPU_MODE
    free(copyd);
    free(adder);
    free(div);
    free(mod);
    free(one);
#endif
}

void longlong_decimal_to_string(char *out, unsigned long long *data) {
    /* unsigned long long max is 18446744073709551616, 20 chars */
    unsigned last_data_array[20] = {0};
    static const unsigned data_array_of_0x100000000[10] = {6, 9, 2, 7, 6, 9, 4, 9, 2, 4};
    unsigned *walk = last_data_array;
    unsigned data_low = ((unsigned *)data)[0];
    unsigned data_high = ((unsigned *)data)[1];
    unsigned char_i = 0;

    if (data[0] == 0) {
        out[0] = '0';
        out[1] = 0;
        return;
    }

    for (int i = 0; i < 10; i++) {
        last_data_array[i] = data_array_of_0x100000000[i] * data_high;
    }

    while (data_low != 0) {
        *(walk++) += data_low % 10;
        data_low /= 10;
    }

    for (int i = 0; i < 19; i++) {
        last_data_array[i + 1] += last_data_array[i] / 10;
        last_data_array[i] %= 10;
    }

    walk = &last_data_array[19];
    while (*walk == 0) {
        walk--;
    }

    while (walk >= last_data_array) {
        out[char_i++] = '0' + *(walk--);
    }
    out[char_i] = 0;
}

int sig_to_dec_string(int *number, char *out, int width) {
    int i;
    int len = width_to_len(width);
    unsigned int *div = (unsigned int *)pint_mem_alloc(len * sizeof(unsigned int), SUB_T_CLEAR);
    unsigned int *mod = (unsigned int *)pint_mem_alloc(len * sizeof(unsigned int), SUB_T_CLEAR);

    unsigned int *base = (unsigned int *)pint_mem_alloc(len * sizeof(unsigned int), SUB_T_CLEAR);
    base[0] = 10;
    for (int j = 1; j < len; j++) {
        base[j] = 0;
    }

    unsigned int offset = 0;
    copy_sig(div, (unsigned *)number, width);

    do {
        div_mod_sig(div, base, div, mod, width);
        out[offset++] = (char)(mod[0] + '0');
    } while (!is_sig_zero(div, width));

    out[offset] = '\0';
    reverse_string((unsigned char *)out, offset);
#ifdef CPU_MODE
    free(div);
    free(mod);
    free(base);
#endif
    return offset;
}

void pint_simu_print_d(unsigned int *addr, int *data, int width, int type, int signed_flag,
                       bool extend_zero = false, int fmt_size = 0) {
    int i = 0;
    int val = 0;
    int num_width = 0;
    int len = width_to_len(width);
    char *str = (char *)pint_mem_alloc(width + 4, SUB_T_CLEAR);
    int char_num = 0;
    char c_h, c_l, cc;
    int tmp_width = 0;
    unsigned tmp_mask = 0;
    bool xz_flag = false;

    if (type == PINT_SIGNAL) {
        if (width <= 4) {
            unsigned mask = (1 << width) - 1;
            val = *(char *)data;
            if (val & (mask << 4)) {
                c_l = val & mask;
                c_h = (val >> 4) & mask;
                cc = c_l ^ c_h;
                cc = ~cc;
                xz_flag = true;
                if (cc & c_l) {
                    if (((mask << 4) == (unsigned)(val & (mask << 4))) &&
                        (mask == (unsigned)(val & mask))) {
                        str[char_num++] = 'x';
                    } else {
                        str[char_num++] = 'X';
                    }
                } else {
                    if ((mask << 4) == (unsigned)(val & (mask << 4))) {
                        str[char_num++] = 'z';
                    } else {
                        str[char_num++] = 'Z';
                    }
                }
            }

            if (!xz_flag) {
                if (signed_flag) {
                    int signed_bit = (1 << (width - 1));
                    if (val & signed_bit) {
                        str[char_num++] = '-';
                        val = (1 << width) - val;
                    }
                }
                int val_tmp = val;
                do {
                    num_width++;
                    val_tmp /= 10;
                } while (val_tmp);

                char_num += num_width;
                num_width = 1;
                do {
                    str[char_num - num_width] = val % 10 + '0';
                    val = val / 10;
                    num_width++;
                } while (val);
            }
        } else {
            tmp_width = 0x20;
            for (i = len; i < 2 * len; i++) {
                if (i == (2 * len - 1)) {
                    tmp_width = width % 0x20;
                    if (tmp_width == 0) {
                        tmp_width = 0x20;
                    }
                }

                if (tmp_width == 32) {
                    tmp_mask = PINT_INT_MASK;
                } else {
                    tmp_mask = (1 << tmp_width) - 1;
                }

                if (data[i]) {
                    if ((unsigned(data[i]) == tmp_mask) && (unsigned(data[i - len]) == tmp_mask)) {
                        str[char_num++] = 'x';
                    } else if ((unsigned(data[i]) == tmp_mask) && (unsigned(data[i - len]) == 0)) {
                        str[char_num++] = 'z';
                    } else {
                        int tmp1 = data[i] ^ data[i - len];
                        tmp1 = ~tmp1;
                        int tmp2 = tmp1 & data[i - len];
                        if (tmp2) {
                            str[char_num++] = 'X';
                        } else {
                            str[char_num++] = 'Z';
                        }
                    }
                    xz_flag = true;
                    break;
                }
            }

            if (!xz_flag) {
                if (signed_flag) {
                    unsigned mod = width & 0x1f;
                    unsigned mask = 0x80000000;
                    if (mod > 0) {
                        mask = 1 << (mod - 1);
                    }
                    if (data[len - 1] & mask) {
                        str[char_num++] = '-';
                        unsigned int *one =
                            (unsigned int *)pint_mem_alloc(len * sizeof(unsigned int), SUB_T_CLEAR);
                        one[0] = 1;
                        for (int j = 1; j < len; j++) {
                            one[j] = 0;
                        }

                        unsigned int *minus =
                            (unsigned int *)pint_mem_alloc(len * sizeof(unsigned int), SUB_T_CLEAR);
                        minus_sig(minus, (unsigned int *)data, one, width);
                        for (int j = 0; j < len; j++) {
                            minus[j] = ~minus[j];
                        }
                        if (mod > 0) {
                            minus[len - 1] &= ((1 << mod) - 1);
                        }
                        char_num += sig_to_dec_string((int *)minus, str + char_num, width);
#ifdef CPU_MODE
                        free(one);
                        free(minus);
#endif
                    } else {
                        char_num += sig_to_dec_string(data, str + char_num, width);
                    }
                } else {
                    char_num += sig_to_dec_string(data, str + char_num, width);
                }
            }
        }
    } else {
        // printf("\nwidth=%d,*data=%d\n",width,*data);
        unsigned int tmp_val = *data;
        unsigned int new_data = 0;
        if ((width > 0) && signed_flag) {
            int signed_bit = (1 << (width - 1));
            if (tmp_val & signed_bit) {
                str[char_num++] = '-';

                if (width == 32) {
                    tmp_val = (PINT_INT_MASK - tmp_val) + 1;
                } else {
                    tmp_val = (1 << width) - tmp_val;
                }
            }
        }

        if (type == PINT_STRING) { // char*
            while (tmp_val > 0) {
                new_data = new_data << 8;
                new_data = (new_data | (tmp_val & 0xFF));
                tmp_val = tmp_val >> 8;
            }
            tmp_val = new_data;
        }

        unsigned val_tmp = tmp_val;
        do {
            num_width++;
            val_tmp /= 10;
        } while (val_tmp);

        char_num += num_width;
        num_width = 1;
        do {
            str[char_num - num_width] = tmp_val % 10 + '0';
            tmp_val = tmp_val / 10;
            num_width++;
        } while (tmp_val);
    }

    if (extend_zero && (char_num < fmt_size)) {
        char *str0 = (char *)pint_mem_alloc(fmt_size + 1, SUB_T_CLEAR);
        str0[fmt_size] = '\0';
        for (int i0 = 0; i0 < fmt_size - char_num; i0++) {
            str0[i0] = '0';
        }

        for (int i0 = fmt_size - char_num; i0 < fmt_size; i0++) {
            str0[i0] = str[i0 + char_num - fmt_size];
        }
        PRINT_STR(addr, str0);
#ifdef CPU_MODE
        free(str0);
#endif
    } else {
        str[char_num++] = 0;
        PRINT_STR(addr, str);
    }

#ifdef CPU_MODE
    free(str);
#endif
    return;
}

void pint_simu_print_x(unsigned int *addr, int *data, int width, int type, bool keep_zero = true) {
    int j = 0;
    unsigned val = 0;
    int num_width = (width + 3) >> 2;
    int len = width_to_len(width);
    unsigned mask;
    unsigned char_to_int[2];
    if (type == PINT_SIGNAL) {
        unsigned *data_low = (unsigned *)data;
        unsigned *data_high = data_low + len;
        if (width <= 4) {
            val = *(unsigned char *)data;
            char_to_int[0] = val & 0xf;
            char_to_int[1] = val >> 4;
            data_low = char_to_int;
            data_high = data_low + 1;
        }
        int group_size;
        bool print_flag = keep_zero;
        for (int char_i = num_width - 1; char_i >= 0; char_i--) {
            val = (data_low[char_i / 8] >> ((char_i % 8) * 4)) & 0xf;
            val += ((data_high[char_i / 8] >> ((char_i % 8) * 4)) & 0xf) * 16;
            group_size = width - (char_i * 4);
            group_size = (group_size >= 4) ? 4 : group_size;
            mask = (1 << group_size) - 1;
            print_flag |= (val != 0);
            print_flag |= (char_i == 0);
            // all bits x
            if (val == mask + mask * 16) {
                PRINT_CHAR(addr, 'x');
            }
            // all bits z
            else if (val == mask * 16) {
                PRINT_CHAR(addr, 'z');
            }
            // some bits x
            else if (val & (val >> 4)) {
                PRINT_CHAR(addr, 'X');
            }
            // some bits  z
            else if (((val & (val >> 4)) == 0) && (val > 16)) {
                PRINT_CHAR(addr, 'Z');
            } else if (val >= 10) {
                PRINT_CHAR(addr, 'a' + val - 10);
            } else if (val > 0) {
                PRINT_CHAR(addr, '0' + val);
            } else {
                if (print_flag) {
                    PRINT_CHAR(addr, '0' + val);
                }
            }
        }
    } else if (type == PINT_VALUE) {
        val = *data;
        int tmp_val = 0;
        if (width > 0) {
            for (j = num_width - 1; j >= 0; j--) {
                tmp_val = (val >> (j << 2)) & 0xf;
                if (tmp_val < 10) {
                    PRINT_CHAR(addr, '0' + tmp_val);
                } else {
                    PRINT_CHAR(addr, 'a' + tmp_val - 10);
                }
            }
        }
    } else if (type == PINT_STRING) {
        char *tmp_val = (char *)data;
        int len = 0;
        while (tmp_val[len] != '\0')
            len++;
        char cc, t_cc;

        for (int ii = 0; ii < len; ii++) {
            cc = tmp_val[ii];
            t_cc = (cc & 0xf0) >> 4;

            if (t_cc < 10)
                PRINT_CHAR(addr, '0' + t_cc);
            else
                PRINT_CHAR(addr, 'a' + t_cc - 10);

            t_cc = (cc & 0x0f);
            if (t_cc < 10)
                PRINT_CHAR(addr, '0' + t_cc);
            else
                PRINT_CHAR(addr, 'a' + t_cc - 10);
        }
    } else {
    }
}

void pint_simu_print_o(unsigned int *addr, int *data, int width, int type, bool keep_zero = true) {
    int j = 0;
    unsigned val = 0;
    int num_width = (width + 2) / 3;
    int len = width_to_len(width);
    unsigned mask;
    unsigned char_to_int[2];

    if (type == PINT_SIGNAL) {
        unsigned *data_low = (unsigned *)data;
        unsigned *data_high = data_low + len;
        if (width <= 4) {
            val = *(unsigned char *)data;
            char_to_int[0] = val & 0xf;
            char_to_int[1] = val >> 4;
            data_low = char_to_int;
            data_high = data_low + 1;
        }
        int group_size;
        bool print_flag = keep_zero;
        for (int char_i = num_width - 1; char_i >= 0; char_i--) {
            int bit_base = char_i * 3;
            int word_off = bit_base / 32;
            int bit_off = bit_base % 32;

            val = (data_low[word_off] >> bit_off) & 0x7;
            val += ((data_high[word_off] >> bit_off) & 0x7) * 16;

            if ((bit_off > 29) && (((bit_base + 31) & 0xffffffe0) < width)) {
                int another_part_bits = bit_off - 29;
                mask = (1 << another_part_bits) - 1;
                val += data_low[word_off + 1] & mask;
                val += (data_high[word_off + 1] & mask) * 16;
            }

            group_size = width - (char_i * 3);
            group_size = (group_size >= 3) ? 3 : group_size;
            mask = (1 << group_size) - 1;
            print_flag |= (val != 0);
            print_flag |= (char_i == 0);
            // all bits x
            if (val == mask + mask * 16) {
                PRINT_CHAR(addr, 'x');
            }
            // all bits z
            else if (val == mask * 16) {
                PRINT_CHAR(addr, 'z');
            }
            // some bits x
            else if (val & (val >> 4)) {
                PRINT_CHAR(addr, 'X');
            }
            // some bits  z
            else if (((val & (val >> 4)) == 0) && (val > 16)) {
                PRINT_CHAR(addr, 'Z');
            } else if (val > 0) {
                PRINT_CHAR(addr, '0' + val);
            } else {
                if (print_flag) {
                    PRINT_CHAR(addr, '0' + val);
                }
            }
        }
    } else if (type == PINT_VALUE) {
        val = *data;
        int tmp_val = 0;
        if (width > 0) {
            for (j = num_width - 1; j >= 0; j--) {
                tmp_val = (val >> (j * 3)) & 0x7;
                PRINT_CHAR(addr, '0' + tmp_val);
            }
        }
    } else {
    }
}

void pint_simu_print_t(unsigned int *addr, int *data, int width, int type, int signed_flag) {
    char tmp[128];
    longlong_decimal_to_string(tmp, (unsigned long long *)data);
    PRINT_STR(addr, tmp);
    return;
}

char printb[4] = {'0', '1', 'z', 'x'};
void pint_simu_print_b(unsigned int *addr, int *data, int width, int type, int fmt_size) {
    int j;
    int val = 0;
    int len = width_to_len(width);
    int print_en = 1;
    if (0 == fmt_size)
        print_en = 0;

    if (type == PINT_SIGNAL) {
        if (width <= 4) {
            val = *(char *)data;
            for (j = width - 1; j >= 0; j--) {
                int high_bit = ((val & (1 << (j + 4))) >> (j + 4)) & 0x1;
                int low_bit = ((val & (1 << j)) >> j) & 0x1;
                int idx = (high_bit << 1) | low_bit;
                if (idx != 0 || 0 == j)
                    print_en = 1;
                if (print_en)
                    PRINT_CHAR(addr, printb[idx]);
            }
        } else {
            unsigned int *tmp_val =
                (unsigned int *)pint_mem_alloc(len * sizeof(unsigned int), SUB_T_CLEAR);
            for (j = 0; j < len; j++) {
                tmp_val[j] = 0;
            }

            for (j = width - 1; j >= 0; j--) {
                right_shift_sig(tmp_val, (unsigned *)(data + len), j, width);
                int high_bit = tmp_val[0] & 0x1;
                right_shift_sig(tmp_val, (unsigned *)data, j, width);
                int low_bit = tmp_val[0] & 0x1;
                int idx = (high_bit << 1) | low_bit;
                if (idx != 0 || 0 == j)
                    print_en = 1;
                if (print_en)
                    PRINT_CHAR(addr, printb[idx]);
            }
#ifdef CPU_MODE
            free(tmp_val);
#endif
        }
    } else if (type == PINT_STRING) { // char*
        char *tmp_val = (char *)data;
        int len = 0;
        while (tmp_val[len] != '\0')
            len++;
        char cc, t_cc;

        for (int ii = 0; ii < len; ii++) {
            cc = tmp_val[ii];
            for (int jj = 0; jj < 8; jj++) {
                t_cc = (cc & (1 << (7 - jj))) >> (7 - jj);
                PRINT_CHAR(addr, printb[t_cc]);
            }
        }
    } else {
        int low_val = *data;
        for (j = width - 1; j >= 0; j--) {
            int low_bit = (low_val & (1 << j)) >> j;
            int idx = low_bit & 0x1;
            if (idx != 0 || 0 == j)
                print_en = 1;
            if (print_en)
                PRINT_CHAR(addr, printb[idx]);
        }
    }
}

void pint_simu_print_v(unsigned int *addr, int *data, int width, int type) {
    static char *v_str[] = {"St0", "St1", "StZ", "StX"};
    if ((type == PINT_SIGNAL) && (width == 1)) {
        unsigned char val = (*(unsigned char *)data) & 0x11;

        // z->2   x->3
        if (val & 0xf) {
            val -= 14;
        }

        PRINT_STR(addr, v_str[val]);
    }
}

void pint_simu_display(unsigned int vpi_type, unsigned fd, char *fmt, va_list ap) {
    int i = 0;
    int width = 0;
    int type = 0;
    int signed_flag = 0;
    int x;
    char *str;
    char *str_tmp0 = NULL;
    char *str_tmp = NULL;
    int *data;
    char str_flag = true;
    char extend_zero = false;
    char keep_zero = true;
    bool init_flag = false;
    unsigned *int_ptr;
    int num;

#ifdef CPU_MODE
    unsigned char *data_buff;
#endif

    unsigned int *addr = (unsigned int *)0xffff0000;
    addr = addr + 1; // size of unsigned int * is 4, so this means addr += stream<<2

    PRINT_LOCK();
    PRINT_CHAR(addr, '0' + (int)vpi_type);
    PRINT_CHAR(addr, ',');
    if ((vpi_type == PINT_FDISPLAY) || (vpi_type == PINT_FWRITE)) {
        PRINT_UINT(addr, fd);
        PRINT_CHAR(addr, ',');
    }

    while (fmt[i] != '\0') {
        if (fmt[i] != '%') {
            PRINT_CHAR(addr, fmt[i++]);
        } else if (fmt[i] == '%' && fmt[i + 1] == '%') {
            PRINT_CHAR(addr, '%');
            i += 2;
        } else {
            i++;
            str_flag = false;

            if (fmt[i] == '0') {
                if ((fmt[i + 1] > '0') && (fmt[i + 1] <= '9')) {
                    extend_zero = true;
                } else {
                    keep_zero = false;
                }
            }

            int j = 0;
            while ((fmt[i] >= '0') && (fmt[i] <= '9')) {
                j++;
                i++;
            }

            int fmt_size = -1;
            if (j > 0) {
                fmt_size = 0;
                unsigned dbase = 1;
                for (int k = 0; k < j; k++) {
                    fmt_size += (fmt[i - 1 - k] - '0') * dbase;
                    dbase *= 10;
                }
            }

            width = va_arg(ap, int);
            type = va_arg(ap, int);
            signed_flag = va_arg(ap, int);

            switch (fmt[i]) {
            case 'd':
            case 'D':
                data = va_arg(ap, int *);
#ifdef CPU_MODE
                if (type == PINT_STRING) {
                    unsigned len = strlen((unsigned char *)data);
                    data_buff = calloc(4, len + 1);
                    for (unsigned i = 0; i < len; i++) {
                        data_buff[i] = ((unsigned char *)(data))[i];
                    }
                    data = (unsigned *)data_buff;
                }
#endif
                pint_simu_print_d(addr, data, width, type, signed_flag, extend_zero, fmt_size);
#ifdef CPU_MODE
                if (type == PINT_STRING) {
                    free(data_buff);
                }
#endif

                break;
            case 't':
            case 'T':
                data = va_arg(ap, int *);
                pint_simu_print_t(addr, data, width, type, signed_flag);
                break;
            case 'h':
            case 'H':
            case 'x':
            case 'X':
                data = va_arg(ap, int *);

#ifdef CPU_MODE
                if (type == PINT_STRING) {
                    unsigned len = strlen((unsigned char *)data);
                    data_buff = calloc(4, len + 1);
                    for (unsigned i = 0; i < len; i++) {
                        data_buff[i] = ((unsigned char *)(data))[i];
                    }
                    data = (unsigned *)data_buff;
                }
#endif
                pint_simu_print_x(addr, data, width, type, keep_zero);
#ifdef CPU_MODE
                if (type == PINT_STRING) {
                    free(data_buff);
                }
#endif
                break;
            case 'o':
            case 'O':
                data = va_arg(ap, int *);
                pint_simu_print_o(addr, data, width, type, keep_zero);
                break;
            case 'b':
            case 'B':
                data = va_arg(ap, int *);
#ifdef CPU_MODE
                if (type == PINT_STRING) {

                    unsigned len = strlen((unsigned char *)data);
                    data_buff = calloc(4, len + 1);
                    for (unsigned i = 0; i < len; i++) {
                        data_buff[i] = ((unsigned char *)(data))[i];
                    }
                    data = (unsigned *)data_buff;
                }
#endif
                pint_simu_print_b(addr, data, width, type, fmt_size);
#ifdef CPU_MODE
                if (type == PINT_STRING) {
                    free(data_buff);
                }
#endif
                break;
            case 's':
            case 'S':
                str = (char *)va_arg(ap, unsigned char *);

                // reverse bytes if it's signal to be printed
                init_flag = false;
                if (width > 4) {
                    int_ptr = (unsigned *)str;
                    num = ((width + 31) / 32) * 2;
                    for (int ii = 0; ii < num; ii++) {
                        if (int_ptr[ii] != 0xffffffff) {
                            init_flag = true;
                            break;
                        }
                    }
                }
                if ((!init_flag) && (width > 4)) {
                    unsigned char_num = (width + 7) / 8;
                    str_tmp0 = (char *)pint_mem_alloc(char_num + 1, 0);
                    int char_i;
                    for (char_i = 0; char_i < char_num; char_i++) {
                        str_tmp0[char_i] = ' ';
                    }
                    str_tmp0[char_i] = 0;
                    str = str_tmp0;
                } else {
                    if (width > 0) {
                        unsigned char_num = (width + 7) / 8;
                        str_tmp = (char *)pint_mem_alloc(char_num + 1, 0);
                        unsigned null_char_num = 0;
                        int char_i;
                        for (char_i = 0; char_i < char_num; char_i++) {
                            str_tmp[char_i] = str[char_num - char_i - 1];
                        }
                        str_tmp[char_i] = 0;

                        for (char_i = 0; char_i < char_num; char_i++) {
                            if (str_tmp[char_i] == 0) {
                                null_char_num++;
                            } else {
                                break;
                            }
                        }
                        str = &str_tmp[null_char_num];
                    }
                }

                PRINT_STR(addr, str);
#ifdef CPU_MODE
                if (NULL != str_tmp0) {
                    free(str_tmp0);
                    str_tmp0 = NULL;
                }
                if (NULL != str_tmp) {
                    free(str_tmp);
                    str_tmp = NULL;
                }
#endif
                break;
            case 'c':
            case 'C':
                data = va_arg(ap, int *);
                if (type == PINT_SIGNAL) {
                    PRINT_CHAR(addr, data[0] & 0xff);
                } else {
                    PRINT_CHAR(addr, *data & 0xff);
                }

                break;
            case 'v':
            case 'V':
                data = va_arg(ap, int *);
                pint_simu_print_v(addr, data, width, type);
                break;
            case '%':
                PRINT_CHAR(addr, '%');
                break;
            }
            i++;
            extend_zero = false;
            keep_zero = true;
        }
    }

    if ((vpi_type == PINT_DISPLAY) || (vpi_type == PINT_FDISPLAY)) {
        PRINT_CHAR(addr, '\n');
    }
    PRINT_CHAR(addr, '\0');
    PRINT_UNLOCK();

    return;
}

// dump signal
void pint_simu_dump_file(vpi_type type, unsigned int stream, unsigned data) {
    PRINT_LOCK();
    unsigned int *addr = (unsigned int *)0xffff0000;
    addr = addr + stream; // size of unsigned int * is 4, so this means addr += stream<<2
    // encode start
    PRINT_CHAR(addr, '0' + (int)type);
    PRINT_CHAR(addr, ',');
    PRINT_UINT(addr, data);
    PRINT_CHAR(addr, ',');
    PRINT_CHAR(addr, '\0');
    PRINT_UNLOCK();
}

void pint_simu_dump_var(vpi_type type, unsigned int stream, char *data) {
    PRINT_LOCK();
    unsigned int *addr = (unsigned int *)0xffff0000;
    addr = addr + stream; // size of unsigned int * is 4, so this means addr += stream<<2
    // encode start
    PRINT_CHAR(addr, '0' + (int)type);
    PRINT_CHAR(addr, ',');
    PRINT_STR(addr, data);
    PRINT_CHAR(addr, '\n');
    PRINT_CHAR(addr, '\0');
    PRINT_UNLOCK();
}

void pint_vcddump_char(unsigned char sig, unsigned size, const char *symbol) {
    unsigned char *buf;
    unsigned char buf_s[PINT_STATIC_ARRAY_SIZE + 8];
    unsigned idx = 0;
    unsigned i = 0;
    unsigned char val = sig & 0x0f;
    unsigned char val_h = (sig >> 4) & 0x0f;
    unsigned char mask;

    if (!pint_dump_enable)
        return;

    if (size <= PINT_STATIC_ARRAY_SIZE) {
        buf = buf_s;
    } else {
        buf = (unsigned char *)pint_mem_alloc(size + 8, SUB_T_CLEAR);
    }

    if (size != 1) {
        buf[idx++] = 'b';
    }

    mask = 1 << (size - 1);
    while (mask) { // has an extra incomplete word
        if (val_h & mask)
            buf[idx++] = (val & mask) ? 'x' : 'z';
        else
            buf[idx++] = (val & mask) ? '1' : '0';
        mask = mask >> 1;
    }

    if (size != 1) {
        buf[idx++] = ' ';
    }
    i = 0;
    while (symbol[i]) {
        buf[idx++] = symbol[i++];
    }
    buf[idx] = 0;
    pint_simu_vpi(PINT_DUMP_VAR, buf);
#ifdef CPU_MODE
    if (size > PINT_STATIC_ARRAY_SIZE) {
        free(buf);
    }
#endif
}

void pint_vcddump_int(unsigned int *sig, unsigned size, const char *symbol) {
    unsigned char *buf;
    unsigned char buf_s[PINT_STATIC_ARRAY_SIZE + 8];
    unsigned idx = 0;
    unsigned i = 0;
    unsigned words = size / 32;
    unsigned words_h_start = size / 32 + ((size % 32) ? 1 : 0);
    unsigned val;
    unsigned val_h;
    unsigned mask;

    if (!pint_dump_enable)
        return;

    if (size <= PINT_STATIC_ARRAY_SIZE) {
        buf = buf_s;
    } else {
        buf = (unsigned char *)pint_mem_alloc(size + 8, SUB_T_CLEAR);
    }

    buf[idx++] = 'b';
    i = size % 32;
    mask = (i == 0) ? 0 : 1 << (i - 1);
    if (mask) {
        val_h = sig[words * 2 + 1];
        val = sig[words];
    }
    while (mask) { // has an extra incomplete word
        if (val_h & mask)
            buf[idx++] = (val & mask) ? 'x' : 'z';
        else
            buf[idx++] = (val & mask) ? '1' : '0';
        mask = mask >> 1;
    }
    for (unsigned w = words; w > 0; w--) {
        val_h = sig[w - 1 + words_h_start];
        val = sig[w - 1];
        mask = 0x80000000;
        while (mask) {
            if (val_h & mask)
                buf[idx++] = (val & mask) ? 'x' : 'z';
            else
                buf[idx++] = (val & mask) ? '1' : '0';
            mask = mask >> 1;
        }
    }
    buf[idx++] = ' ';
    i = 0;
    while (symbol[i]) {
        buf[idx++] = symbol[i++];
    }
    buf[idx] = 0;
    pint_simu_vpi(PINT_DUMP_VAR, buf);
#ifdef CPU_MODE
    if (size > PINT_STATIC_ARRAY_SIZE) {
        free(buf);
    }
#endif
}

void pint_vcddump_char_without_check(unsigned char sig, unsigned size, const char *symbol) {
    unsigned char *buf;
    unsigned char buf_s[PINT_STATIC_ARRAY_SIZE + 8];
    unsigned idx = 0;
    unsigned i = 0;
    unsigned char val = sig & 0x0f;
    unsigned char val_h = (sig >> 4) & 0x0f;
    unsigned char mask;

    if (size <= PINT_STATIC_ARRAY_SIZE) {
        buf = buf_s;
    } else {
        buf = (unsigned char *)pint_mem_alloc(size + 8, SUB_T_CLEAR);
    }

    if (size != 1) {
        buf[idx++] = 'b';
    }

    mask = 1 << (size - 1);
    while (mask) { // has an extra incomplete word
        if (val_h & mask)
            buf[idx++] = (val & mask) ? 'x' : 'z';
        else
            buf[idx++] = (val & mask) ? '1' : '0';
        mask = mask >> 1;
    }

    if (size != 1) {
        buf[idx++] = ' ';
    }
    i = 0;
    while (symbol[i]) {
        buf[idx++] = symbol[i++];
    }
    buf[idx] = 0;
    pint_simu_vpi(PINT_DUMP_VAR, buf);
#ifdef CPU_MODE
    if (size > PINT_STATIC_ARRAY_SIZE) {
        free(buf);
    }
#endif
}

void pint_vcddump_int_without_check(unsigned int *sig, unsigned size, const char *symbol) {
    unsigned char *buf;
    unsigned char buf_s[PINT_STATIC_ARRAY_SIZE + 8];
    unsigned idx = 0;
    unsigned i = 0;
    unsigned words = size / 32;
    unsigned words_h_start = size / 32 + ((size % 32) ? 1 : 0);
    unsigned val;
    unsigned val_h;
    unsigned mask;

    if (size <= PINT_STATIC_ARRAY_SIZE) {
        buf = buf_s;
    } else {
        buf = (unsigned char *)pint_mem_alloc(size + 8, SUB_T_CLEAR);
    }

    buf[idx++] = 'b';
    i = size % 32;
    mask = (i == 0) ? 0 : 1 << (i - 1);
    if (mask) {
        val_h = sig[words * 2 + 1];
        val = sig[words];
    }
    while (mask) { // has an extra incomplete word
        if (val_h & mask)
            buf[idx++] = (val & mask) ? 'x' : 'z';
        else
            buf[idx++] = (val & mask) ? '1' : '0';
        mask = mask >> 1;
    }
    for (unsigned w = words; w > 0; w--) {
        val_h = sig[w - 1 + words_h_start];
        val = sig[w - 1];
        mask = 0x80000000;
        while (mask) {
            if (val_h & mask)
                buf[idx++] = (val & mask) ? 'x' : 'z';
            else
                buf[idx++] = (val & mask) ? '1' : '0';
            mask = mask >> 1;
        }
    }
    buf[idx++] = ' ';
    i = 0;
    while (symbol[i]) {
        buf[idx++] = symbol[i++];
    }
    buf[idx] = 0;
    pint_simu_vpi(PINT_DUMP_VAR, buf);
#ifdef CPU_MODE
    if (size > PINT_STATIC_ARRAY_SIZE) {
        free(buf);
    }
#endif
}

void pint_vcddump_value(simu_time_t *cur_T) {
    char T_string[128] = "#";
    longlong_decimal_to_string(T_string + 1, cur_T);
    pint_simu_vpi(PINT_DUMP_VAR, T_string);
}

unsigned pint_int_to_hex(char *buff, unsigned x) {
    unsigned offset = 0;
    unsigned i;
    unsigned j;
    for (j = 7; j >= 0; j--) {
        i = (x >> (j << 2)) & 0xf;
        if (i < 10) {
            buff[offset++] = '0' + i;
        } else {
            buff[offset++] = 'A' + i;
        }
    }
    return offset;
}

unsigned pint_int(char *buff, int x) {
    unsigned offset = 0;
    unsigned i;
    unsigned d = 1;
    if (x < 0) {
        buff[offset++] = '-';
        x = -x;
    }

    while (x / d > 9)
        d *= 10;

    while (d >= 1) {
        i = x / d;
        x = x % d;
        d = d / 10;
        buff[offset++] = '0' + i;
    }
    return offset;
}

unsigned pint_uint(char *buff, unsigned x) {
    unsigned offset = 0;
    unsigned i;
    unsigned d = 1;
    while (x / d > 9)
        d *= 10;

    while (d >= 1) {
        i = x / d;
        x = x % d;
        d = d / 10;
        buff[offset++] = '0' + i;
    }

    return offset;
}

int pint_str(char *buff, char *s) {
    int offset = 0;
    int i = 0;
    while (s[i] != '\0') {
        buff[offset++] = s[i++];
    }
    return offset;
}

void pint_sprintf(char *buff, const char fmt[], ...) {
    va_list ap;
    va_start(ap, fmt);

    int i = 0;
    int j = 0;
    char *str;
    unsigned offset = 0;
    unsigned x = 0;

    while (fmt[i] != '\0') {
        if (fmt[i] != '%') {
            buff[j++] = fmt[i++];
        } else {
            i++;
            switch (fmt[i]) {
            case 'x':
                x = va_arg(ap, unsigned);
                offset = pint_int_to_hex(&(buff[j]), x);
                j += offset;
                break;
            case 'd':
                x = va_arg(ap, int);
                offset = pint_int(&(buff[j]), x);
                j += offset;
                break;
            case 'u':
                x = va_arg(ap, unsigned int);
                offset = pint_uint(&(buff[j]), x);
                j += offset;
                break;
            case 's':
                str = va_arg(ap, char *);
                offset = pint_str(&(buff[j]), str);
                j += offset;
                break;
            case 'f':
            case 'g':
                offset = 0;
                break;
            case 'c':
                x = va_arg(ap, int);
                buff[j++] = x;
                break;
            case '%':
                buff[j++] = '%';
                break;
            default:
                break;
            }
            i++;
        }
    }
    buff[j] = '\0';
}

void pint_sendmsg(char *data) {
    PRINT_LOCK();
    unsigned int *addr = (unsigned int *)0xffff0000;
    addr = addr + 1; // size of unsigned int * is 4, so this means addr += stream<<2
    // encode start
    PRINT_CHAR(addr, '0' + (int)PINT_SEND_MSG);
    PRINT_CHAR(addr, ',');
    PRINT_STR(addr, data);
    PRINT_CHAR(addr, '\0');
    PRINT_UNLOCK();
}

/*msg: msg_id | msg_type | msg_len | msg_buffer \0
 *  msg_id: 0-0xffffffff
 *  type = 0 string, type = 1 binary, if (type == 1) need init data len.
 *  msg_buffer: vpi_name | arg ..., completed_flag_addr
 */
void pint_simu_sendmsg(va_list ap) {
    char msg_buff[1024];
    int msg_id = va_arg(ap, int); //
    int msg_type = 0;
    int msg_len = 0;
    int core_id = pint_core_id();
    char *vpi_name = va_arg(ap, char *);
    char *file_name = NULL;
    pint_h2d_completed[core_id] = 0;
    unsigned int base_addr;
    if (vpi_name == "$fopen") {
        char *arg;
        unsigned int arg_type = 0;
        file_name = va_arg(ap, char *);
        base_addr = va_arg(ap, unsigned int);
        arg = va_arg(ap, char *);
        // r rb w wb , if type is omitted the file is opened for writing. (if write truncate to zero
        // length or create).
        if (NULL != arg) {
            if (arg[0] == 'r') {
                if (arg[1] == 'b') {
                    arg_type = 2;
                } else {
                    arg_type = 1;
                }
            } else if (arg[0] == 'w') {
                if (arg[1] == 'b') {
                    arg_type = 4;
                } else {
                    arg_type = 3;
                }
            } else {
                arg_type = 0;
            }
        } else {
            arg_type = 0;
        }
        pint_sprintf(msg_buff, "%d,%d,%d,%s ,%s ,%u,%u,0,0,%u\n", msg_id, msg_type, msg_len,
                     &vpi_name[1], file_name, base_addr, arg_type, &pint_h2d_completed[core_id]);
    } else if ((vpi_name == "$readmemh") || (vpi_name == "$readmemb")) {
        file_name = va_arg(ap, char *);
        base_addr = va_arg(ap, unsigned int);
        unsigned int start_addr = va_arg(ap, unsigned int);
        unsigned int end_addr = va_arg(ap, unsigned int);
        unsigned int sig_width = va_arg(ap, unsigned int);
        pint_sprintf(msg_buff, "%d,%d,%d,%s ,%s ,%u,%u,%u,%u,%u\n", msg_id, msg_type, msg_len,
                     &vpi_name[1], file_name, base_addr, start_addr, end_addr, sig_width,
                     &pint_h2d_completed[core_id]);
    } else if (vpi_name == "$fclose") {
        unsigned fd_value = va_arg(ap, unsigned);
        pint_sprintf(msg_buff, "%d,%d,%d,fclose ,NULL ,%u,0,0,0,%u\n", msg_id, msg_type, msg_len,
                     fd_value, &pint_h2d_completed[core_id]);
    } else if (vpi_name == "$fgetc") {
        unsigned fd_value = va_arg(ap, unsigned);
        unsigned int start_addr = va_arg(ap, unsigned int);
        unsigned int sig_width = 8;
        pint_sprintf(msg_buff, "%d,%d,%d,fgetc ,NULL ,%u,%u,1,%u,%u\n", msg_id, msg_type, msg_len,
                     fd_value, start_addr, sig_width, &pint_h2d_completed[core_id]);
    } else if (vpi_name == "$ungetc") {
        unsigned fd = va_arg(ap, unsigned);
        unsigned ch = va_arg(ap, unsigned);
        unsigned ret_addr = va_arg(ap, unsigned);
        pint_sprintf(msg_buff, "%d,%d,%d,ungetc ,NULL ,%u,%u,1,%u,%u\n", msg_id, msg_type, msg_len,
                     fd, ch, ret_addr, &pint_h2d_completed[core_id]);
    } else if (vpi_name == "$fgets") {
        unsigned fd = va_arg(ap, unsigned);
        unsigned str = va_arg(ap, unsigned);
        unsigned ret_addr = va_arg(ap, unsigned);
        unsigned sig_width = va_arg(ap, unsigned);
        pint_sprintf(msg_buff, "%d,%d,%d,fgets ,NULL ,%u,%u,%u,%u,%u\n", msg_id, msg_type, msg_len,
                     fd, str, ret_addr, sig_width, &pint_h2d_completed[core_id]);
    } else if (vpi_name == "$feof" || vpi_name == "$ftell" || vpi_name == "$rewind") {
        unsigned fd_value = va_arg(ap, unsigned);
        unsigned ret_addr = va_arg(ap, unsigned);
        pint_sprintf(msg_buff, "%d,%d,%d,%s ,NULL ,%u,%u,0,0,%u\n", msg_id, msg_type, msg_len,
                     &vpi_name[1], fd_value, ret_addr, &pint_h2d_completed[core_id]);
    } else if (vpi_name == "$fseek") {
        unsigned fd = va_arg(ap, unsigned);
        unsigned offset = va_arg(ap, unsigned);
        unsigned whence = va_arg(ap, unsigned);
        unsigned ret_addr = va_arg(ap, unsigned);
        // printf("fseek:%u %u %u %u\n", fd, offset, whence, ret_addr);
        pint_sprintf(msg_buff, "%d,%d,%d,fseek ,NULL ,%u,%u,%u,%u,%u\n", msg_id, msg_type, msg_len,
                     fd, offset, whence, ret_addr, &pint_h2d_completed[core_id]);
    } else {
        printf("error Not support vpi_name %s\n", vpi_name);
        return;
    }

    pint_sendmsg(msg_buff);

    // printf(">>>%s %s completed! core_id:%d flag:%d base_addr:%d\n", vpi_name, file_name ?
    // file_name:"", core_id, pint_h2d_completed[core_id], base_addr);
}

/*profdata msg: msg_id | msg_type | msg_len | msg_buffer \0
 *  msg_id: 0-0xffffffff
 *  type = 0 string, type = 1 binary, if (type == 1) need init data len.
 *  msg_buffer: vpi_name | gperf_total_event | total_event_size
 *                       | gperf_event_realnum_per_ncore | event_realnum_size
 *                       | completed_flag_addr
 */
#ifndef CPU_MODE
void pint_profdata_send() {
    char msg_buff[1024];
    int msg_id = 0;
    int msg_type = 0;
    int msg_len = 0;
    unsigned int events_max_per_core = 200000;
    int total_event_size =
        gperf_ncore_num_to_perf * events_max_per_core * sizeof(perf_event_s); // 136MB
    int event_realnum_size = gperf_ncore_num_to_perf * sizeof(unsigned int);
    void *mem_addr = (void *)(0x20000000);
    int mem_lens = (int)0x10000;
    pint_h2d_mcore_completed = 0;
    const int ncore_num_to_perf = gperf_ncore_num_to_perf;

    pint_sprintf(msg_buff, "%d,%d,%d,prof ,NULL ,%u,%u,%u,%u,%u\n", msg_id, msg_type, msg_len,
                 gperf_total_event, total_event_size, gperf_event_realnum_per_ncore,
                 event_realnum_size, &pint_h2d_mcore_completed);
    // pint_printf("pint: %s.", msg_buff);

    // pint_printf((char*)"\npint_profdata_send gperf_event_realnum_per_ncore:\n");
    // for(int i=0; i<ncore_num_to_perf; i++)
    // {
    //   pint_printf((char*)"%d, ",gperf_event_realnum_per_ncore[i]);
    // }
    // pint_printf((char*)"\n\n");

    pint_sendmsg(msg_buff);

    int flag = 0;
    while (flag == 0) {
        asm volatile("lw\t%0, %1" : "+r"(flag) : "m"(pint_h2d_mcore_completed));
    }
    pint_dcache_discard();
}
#endif

void pint_simu_vpi(vpi_type type, ...) {
    va_list ap;
    va_start(ap, type);
    char *fmt;
    unsigned fd;

    switch (type) {
    case PINT_DISPLAY:
    case PINT_WRITE:
        fmt = va_arg(ap, char *);
        pint_simu_display(type, 1, fmt, ap);
        break;
    case PINT_DUMP_FILE:
        //        fmt = va_arg(ap, char*);
        fd = va_arg(ap, unsigned);
        pint_simu_dump_file(PINT_DUMP_FILE, 1, fd);
        break;
    case PINT_DUMP_VAR:
        fmt = va_arg(ap, char *);
        pint_simu_dump_var(PINT_DUMP_VAR, 1, fmt);
        break;

    case PINT_FDISPLAY:
    case PINT_FWRITE:
        fd = va_arg(ap, unsigned);
        fmt = va_arg(ap, char *);
        // va_start(ap, type);
        // printf("\n+++fd:%d fmt:%s\n", fd, fmt);
        pint_simu_display(type, fd, fmt, ap);
        break;

    case PINT_SEND_MSG:
        pint_simu_sendmsg(ap);
        break;

    case PINT_USER:

        break;
    default:
        printf("unknown type of vpi:%d\n", type);
        break;
    }
    va_end(ap);

#ifdef CPU_MODE
    pint_simu_log(0, (char *)print_str, strlen(print_str));
#endif
    if (type == PINT_SEND_MSG) {
        int core_id = pint_core_id();
        volatile int flag = 0;
        while (flag == 0) {
#ifndef CPU_MODE
            asm volatile("lw\t%0, %1" : "+r"(flag) : "m"(pint_h2d_completed[core_id]));
#else
            flag = pint_h2d_completed[core_id];
#endif
        }
#ifndef CPU_MODE
        pint_dcache_discard();
#endif
    }

    return;
}

#ifndef CPU_MODE
int pint_random(void) { return (int)random_num++; }
unsigned int pint_urandom(void) { return random_num++; }
#else
int pint_random(void) { return (int)rand(); }
unsigned int pint_urandom(void) { return rand(); }
#endif

void pint_vcddump_off(unsigned size, const char *symbol) {
    unsigned char *buf = (unsigned char *)pint_mem_alloc(size + 128, SUB_T_CLEAR);
    unsigned idx = 0;
    unsigned i = 0;
    unsigned char mask;

    if (size != 1) {
        buf[idx++] = 'b';
    }
    buf[idx++] = 'x';

    if (size != 1) {
        buf[idx++] = ' ';
    }
    while (symbol[i]) {
        buf[idx++] = symbol[i++];
    }
    buf[idx] = 0;
    pint_simu_vpi(PINT_DUMP_VAR, buf);
#ifdef CPU_MODE
    free(buf);
#endif
}
