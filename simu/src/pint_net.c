#include "../inc/pint_simu.h"
#include "../inc/pint_net.h"
#include "../inc/pint_simu.h"
#include "../common/perf.h"

#ifndef USE_INLINE
#define forceinline
#include "../inc/pint_net.inc"
#endif

#if 0 // nxz move to pint_net.inc
template<bool _nxz = false>
__inline__ __attribute__((always_inline)) int edge_of_copy(char from, char to){
    if (to == from){
        return pint_nonedge;
    }
    else{
        to &= 0x11;
        from &= 0x11;
        if ((to > 0) && (from == 0)){
            return pint_posedge;
        }
        else if ((to == 0) && (from > 0)){
            return pint_negedge;
        }
        else if ((to != 1) && (from == 1)){
            return pint_negedge;
        }
        else if ((to == 1) && (from != 1)){
            return pint_posedge;
        }
        else{
            return pint_otherchange;
        }
    }
}

template<bool _nxz = false>
int pint_copy_char_char_same_edge(const void* src, void* dst){
    unsigned char* p_src = (unsigned char*)src;
    unsigned char* p_dst = (unsigned char*)dst;
    unsigned char data_src = p_src[0];
    unsigned char data_dst = p_dst[0];

    int flag = edge_of_copy(data_dst, data_src);
    if (flag != pint_nonedge){
        p_dst[0] = p_src[0];
    }
    return flag;
}

template<bool _nxz = false>
int pint_copy_char_char_edge(const void* src, unsigned src_base, 
    void*dst, unsigned dst_base, unsigned count){

    unsigned char src_value = *(unsigned char*)src;
    unsigned char dst_value = *(unsigned char*)dst;
    unsigned mask = (1 << count) - 1;
    mask |= (mask << 4);
    char from = (dst_value >> dst_base) & mask;
    char to = (src_value >> src_base) & mask;

    int flag = edge_of_copy(from, to);
    if (flag != pint_nonedge){
        pint_set_subarray_char((unsigned char*)dst, (unsigned char*)&to, dst_base, count, 4);
    }
    return flag;
}

template<bool _nxz = false>
void pint_copy_int_int_same(const void* src, void* dst, unsigned width){
    unsigned word_num = (width + 31) / 32 * 2;
    unsigned int* p_src = (unsigned int*)src;
    unsigned int* p_dst = (unsigned int*)dst;
    
    for (int i = 0; i < word_num; i++){
        p_dst[i] = p_src[i];
    }
}

template<bool _nxz = false>
int pint_copy_int_int_same_edge(const void* src, void* dst, unsigned width){
    unsigned half_word_num = (width + 31) / 32;
    unsigned int* p_src_l = (unsigned int*)src;
    unsigned int* p_dst_l = (unsigned int*)dst;
    unsigned int* p_src_h = p_src_l + half_word_num;
    unsigned int* p_dst_h = p_dst_l + half_word_num;
    char from = (p_dst_l[0]&0x1) + ((p_dst_h[0]&0x1) << 4);
    char to = (p_src_l[0]&0x1) + ((p_src_h[0]&0x1) << 4);

    int flag = edge_of_copy(from, to);
    
    if (flag != pint_nonedge){
        for (int i = 0; i < half_word_num * 2; i++){
            p_dst_l[i] = p_src_l[i];
        }
    }
    else {
        for (int i = 0; i < half_word_num * 2; i++){
            if (p_src_l[i] != p_dst_l[i]){
                p_dst_l[i] = p_src_l[i];
                flag = pint_otherchange;
            }
        }
    }
    return flag;
}

template<bool _nxz = false>
int pint_copy_int_int_edge(const void* src, unsigned src_width, unsigned src_base, 
    void*dst, unsigned dst_width, unsigned dst_base, unsigned count){
    unsigned tmp_sig_half_word_num = (count + 31) / 32;
    unsigned tmp_p_src[tmp_sig_half_word_num * 2];
    pint_get_subarray_int_int(tmp_p_src, (unsigned*)src, src_base, count, src_width);
    unsigned tmp_p_dst[tmp_sig_half_word_num * 2];
    pint_get_subarray_int_int(tmp_p_dst, (unsigned*)dst, dst_base, count, dst_width);
    
    unsigned mask;
    if (count < 4){
        mask = (1 << count) - 1;
    }
    else{
        mask = 0xf;
    }
    
    char from = (tmp_p_dst[0] & mask) + ((tmp_p_dst[tmp_sig_half_word_num] & mask) << 4);
    char to = (tmp_p_src[0] & mask) + ((tmp_p_src[tmp_sig_half_word_num] & mask) << 4);

    int flag = edge_of_copy(from, to);

    if (flag == pint_nonedge){
        for (int i = 0; i < tmp_sig_half_word_num * 2; i++){
            if (tmp_p_src[i] != tmp_p_dst[i]){
                flag = pint_otherchange;
                break;
            }
        }
    }

    if (flag != pint_nonedge){
        pint_set_subarray_int((unsigned*)dst, tmp_p_src, dst_base, count, dst_width);
    }

    return flag;
}

template<bool _nxz = false>
int pint_copy_int_char_edge(const void* src, unsigned src_width, unsigned src_base, 
    void*dst, unsigned dst_base, unsigned count){
    pint_assert((dst_base < 4)&&(dst_base + count <= 4), "%d, %d", dst_base, count);

    unsigned tmp_value[2];
    pint_get_subarray_int_int(tmp_value, (unsigned*)src, src_base, count, src_width);

    unsigned char dst_value = *(unsigned char*)dst;
    unsigned mask = (1 << count) - 1;
    unsigned char from = (dst_value >> dst_base) & (mask | (mask << 4));
    unsigned char to = (tmp_value[0] & mask) + ((tmp_value[1] & mask) << 4);

    int flag = edge_of_copy(from, to);
    
    if (flag == pint_nonedge){
        if (from != to){
            flag = pint_otherchange;
        }
    }

    if (flag != pint_nonedge){
        pint_set_subarray_char((unsigned char*)dst, &to, dst_base, count, 4);
    }

    return flag;
}

template<bool _nxz = false>
int pint_copy_char_int_edge(const void* src, unsigned src_base, 
    void*dst, unsigned dst_width, unsigned dst_base, unsigned count){
    pint_assert((src_base < 4)&&(src_base + count <= 4), "%d, %d", src_base, count);

    unsigned tmp_dst_value[2];
    pint_get_subarray_int_int(tmp_dst_value, (unsigned*)dst, dst_base, count, dst_width);

    unsigned char src_value = *(unsigned char*)src;
    unsigned mask = (1 << count) - 1;
    unsigned char to = (src_value >> src_base) & (mask | (mask << 4));
    unsigned char from = (tmp_dst_value[0] & mask) + ((tmp_dst_value[1] & mask) << 4);

    int flag = edge_of_copy(from, to);
    
    if (flag == pint_nonedge){
        if (from != to){
            flag = pint_otherchange;
        }
    }

    if (flag != pint_nonedge){
        unsigned src_value_int[2] = {(unsigned)(to&0xf), (unsigned)(to>>4)};
        pint_set_subarray_int((unsigned int*)dst, src_value_int, dst_base, count, dst_width);
    }

    return flag;
}
#endif

#if 0 // nxz move to pint_net.h
template<bool _nxz = false>
void pint_mask_copy_char(unsigned char* out, const unsigned char* in0, const unsigned char* addr_mask){
    NCORE_PERF_MEASURE(pint_mask_copy_char, 2);
    pint_assert(size <= PINT_CHAR_OFFSET, "size if not valid for char.");
    unsigned char mask = *addr_mask;
    if constexpr(!_nxz) {
        mask |= mask << 4;
    }
    shm_char_common<false, false>(mask, out, *in0);
}

template<bool _nxz = false>
void pint_mask_copy_int(unsigned int*  out, const unsigned int*  in0, const unsigned int*  mask, unsigned int width){
    NCORE_PERF_MEASURE(pint_mask_copy_int, 2);
    unsigned    cnt = (width + 0x1f) >> 5;
    int i;
    for(i =0; i < cnt; i++){
        shm_int_common<false, false>(mask[i], out +i, in0[i]);
        if constexpr(!_nxz) {
            shm_int_common<false, false>(mask[i], out +i +cnt, in0[i +cnt]);
        }
    }
}
#endif

#if 0 // nxz move to pint_net.inc
template<bool _nxz = false>
bool pint_mask_case_equality_int(unsigned int*  in0, const unsigned int*  in1, const unsigned int*  mask, unsigned int width){
    NCORE_PERF_MEASURE(pint_mask_case_equality_int<_nxz>, 2);
    unsigned    cnt = (width + 0x1f) >> 5;
    int i;    
    for(i =0; i < cnt; i++){
        if((in0[i] ^ in1[i]) & mask[i]) return 0;
        if constexpr(!_nxz) {
            if((in0[i +cnt] ^ in1[i +cnt]) & mask[i]) return 0;
        }
    }
    return 1;
}
#endif

// _nxz = true
int pint_get_string_len(unsigned char *in) {
    int len = 0;
    while (in[len++])
        ;
    return len - 1;
}

// _nxz = false
void pint_set_sig_x(void *buf, unsigned int width) {
    if (width <= 4) {
        unsigned char tmp = (1 << width) - 1;
        tmp = tmp | (tmp << 4);
        *(unsigned char *)buf = tmp;
    } else {
        int half_word_num = (width + 31) / 32;
        int word_num = half_word_num * 2;
        int mod = width % 32;
        unsigned *int_p = (unsigned *)buf;

        for (int i = 0; i < word_num; i++) {
            int_p[i] = 0xffffffff;
        }

        if (mod) {
            int_p[half_word_num - 1] = (1 << mod) - 1;
            int_p[word_num - 1] = (1 << mod) - 1;
            ;
        }
    }
}

void pint_set_sig_z(void *buf, unsigned int width) {
    if (width <= 4) {
        unsigned char tmp = (1 << width) - 1;
        tmp = (tmp << 4);
        *(unsigned char *)buf = tmp;
    } else {
        int half_word_num = (width + 31) / 32;
        int word_num = half_word_num * 2;
        int mod = width % 32;
        unsigned *int_p = (unsigned *)buf;

        for (int i = 0; i < half_word_num; i++) {
            int_p[i] = 0;
        }

        for (int i = half_word_num; i < word_num; i++) {
            int_p[i] = 0xffffffff;
        }

        if (mod) {
            int_p[word_num - 1] = (1 << mod) - 1;
            ;
        }
    }
}

void pint_power_recursive(unsigned int *out, unsigned int *x, unsigned int *y, unsigned int size) {
    NCORE_PERF_MEASURE(pint_power_recursive, 2);
    NCORE_PERF_PINT_NET_SUMMARY(58);
    const int tmp_cnt = ((size + 0x1f) >> 5) << 1;
    unsigned int *tmp_buffer =
        (unsigned int *)pint_mem_alloc(tmp_cnt * sizeof(unsigned int) * 5, SUB_T_CLEAR);
    unsigned int *zero_tmp = tmp_buffer;
    unsigned int *y_tmp = tmp_buffer + tmp_cnt;
    unsigned int *out_tmp = tmp_buffer + tmp_cnt * 2;
    unsigned int *inplace_tmp = tmp_buffer + tmp_cnt * 3;
    unsigned int *out2_tmp = tmp_buffer + tmp_cnt * 4;

    memset(tmp_buffer, 0x00, tmp_cnt * sizeof(unsigned int) * 5);
    memset(out, 0x00, tmp_cnt * sizeof(unsigned int));
    pint_copy_int(y_tmp, y, size);

    /* If we have a zero exponent just return 1. */
    if (pint_logical_equality_int_ret(y_tmp, zero_tmp, size)) {
        out[0] = 0x01;
#ifdef CPU_MODE
        free(tmp_buffer);
#endif
        return;
    }

    /* Is the value odd? */
    if (y_tmp[0] & 0x01) {
        y_tmp[0] = y_tmp[0] & 0xfffffffe; // A quick subtract by 1.
        pint_power_recursive(out_tmp, x, y_tmp, size);
        pint_mul_int_u(out, x, out_tmp, size);
#ifdef CPU_MODE
        free(tmp_buffer);
#endif
        return;
    }
    pint_rshift_int_u(inplace_tmp, y_tmp, 1, size); /* y >>= 1; */
    pint_copy_int(y_tmp, inplace_tmp, size);
    pint_power_recursive(out_tmp, x, y_tmp, size); /* z = pow(x, y); */
    pint_copy_int(out2_tmp, out_tmp, size);
    pint_mul_int_u(out, out_tmp, out2_tmp, size); /* res = z * z; */
#ifdef CPU_MODE
    free(tmp_buffer);
#endif
}

void pint_power_int_u(unsigned int *out, const unsigned int *in0, const unsigned int *in1,
                      unsigned int size) {
    NCORE_PERF_MEASURE(pint_power_int_u, 2);
    NCORE_PERF_PINT_NET_SUMMARY(60);
    /* Check for any XZ values ahead of time in a first pass.  */
    const int cnt = (size + 0x1f) >> 5;
    for (int idx = 0; idx < cnt; idx += 1) {
        unsigned int lval = in0[cnt + idx];
        unsigned int rval = in1[cnt + idx];

        if (lval || rval) {
            pint_set_int(out, size, BIT_X);
            return;
        }
    }

    pint_power_recursive(out, in0, in1, size);
}

void pint_power_int_s(unsigned int *out, int in0, int in1, unsigned int size) {
    /* Check for signed bit.  */
    const int cnt = (size + 0x1f) >> 5;
    unsigned left = size % 0x20;
    if (left == 0)
        left = 0x20;
    unsigned signed_bit = 1 << (left - 1);

    unsigned mask;
    if ((cnt > 1) || (left == 0x20))
        mask = PINT_INT_MASK;
    else
        mask = (1 << left) - 1;

    for (int ii = 0; ii < 2 * cnt; ii++)
        out[ii] = 0x00;
    // printf("pint_power_int_s, in0=%d,in1=%d\n", in0, in1);

    bool signed_flag = false;

    if (in0 < 0) {
        if ((in1 > 0) && ((in1 % 2) > 0))
            signed_flag = true;
        else if ((in1 < 0) && ((-1 * in1) % 2) > 0)
            signed_flag = true;

        in0 = -1 * in0;
    }
    int bit_count = 1;
    unsigned tmp_in0 = (unsigned)in0;
    while (tmp_in0 > 1) {
        tmp_in0 = tmp_in0 / 2;
        bit_count++;
    }
    int each_max = 32 / bit_count;
    if (in1 <= each_max) {
        unsigned result = 1;
        int ii = in1;
        while (ii > 0) {
            result *= in0;
            ii--;
        }

        if (!signed_flag)
            out[0] = result;
        else {
            result = ~result;
            result += 1;
            result = result & mask;
            out[0] = result;
            for (int ii = 1; ii < cnt; ii++)
                out[ii] = PINT_INT_MASK;

            mask = (1 << left) - 1;
            out[cnt - 1] = mask;
        }
    } else {
        unsigned result = 1;
        int ii = each_max;
        while (ii > 0) {
            result *= in0;
            ii--;
        }

        int num = in1 / each_max;
        int t_left = in1 % each_max;
        unsigned left_result = 1;
        ii = t_left;
        while (ii > 0) {
            left_result *= in0;
            ii--;
        }

        unsigned int *tmp_buffer_alloc =
            (unsigned int *)pint_mem_alloc(cnt * sizeof(unsigned int) * 2 * 3, SUB_T_CLEAR);
        unsigned int *tmp_buffer = tmp_buffer_alloc;
        memset(tmp_buffer, 0x00, cnt * sizeof(unsigned int) * 2 * 3);
        unsigned int *tmp_buffer_result = tmp_buffer;
        unsigned int *tmp_buffer_A = tmp_buffer + 2 * cnt;
        unsigned int *tmp_buffer_B = tmp_buffer + 4 * cnt;

        tmp_buffer_A[0] = result;
        if (num == 1) {
            tmp_buffer_B[0] = left_result;
            pint_mul_int_u(tmp_buffer_result, tmp_buffer_A, tmp_buffer_B, size);
        } else {
            tmp_buffer_B[0] = result;
            for (int ii = 1; ii < num; ii++) {
                if ((ii % 2) == 0) {
                    memset(tmp_buffer_B, 0x00, cnt * sizeof(unsigned int) * 2);
                    pint_mul_int_u(tmp_buffer_B, tmp_buffer_A, tmp_buffer_result, size);
                } else {
                    memset(tmp_buffer_result, 0x00, cnt * sizeof(unsigned int) * 2);
                    pint_mul_int_u(tmp_buffer_result, tmp_buffer_A, tmp_buffer_B, size);
                }
            }

            tmp_buffer_A[0] = left_result;
            if (((num - 1) % 2) == 0) {
                memset(tmp_buffer_result, 0x00, cnt * sizeof(unsigned int) * 2);
                pint_mul_int_u(tmp_buffer_result, tmp_buffer_A, tmp_buffer_B, size);

                tmp_buffer = tmp_buffer_result;
            } else {
                memset(tmp_buffer_B, 0x00, cnt * sizeof(unsigned int) * 2);
                pint_mul_int_u(tmp_buffer_B, tmp_buffer_A, tmp_buffer_result, size);
                tmp_buffer = tmp_buffer_B;
            }

            if (signed_flag) {
                for (int ii = 0; ii < cnt; ii++)
                    tmp_buffer[ii] = ~tmp_buffer[ii];

                // tmp_buffer[0] += 1;
                for (int ii = 0; ii < cnt; ii++) {
                    if (PINT_INT_MASK == tmp_buffer[ii]) {
                        tmp_buffer[ii] = 0x00;
                    } else {
                        tmp_buffer[ii] += 1;
                        break;
                    }
                }

                mask = (1 << left) - 1;
                tmp_buffer[cnt - 1] &= mask;

                for (int ii = 0; ii < 2 * cnt; ii++)
                    out[ii] = tmp_buffer[ii];
            } else {
                for (int ii = 0; ii < 2 * cnt; ii++)
                    out[ii] = tmp_buffer[ii];
            }
        }
#ifdef CPU_MODE
        free(tmp_buffer_alloc);
#endif
    }
}

#if 0 // nxz move to pint_net.inc
static unsigned long long pint_power_value(unsigned in0, unsigned in1){
    if (in1 == 0){
        return 1;
    }
    else if (in1 == 1){
        return in0;
    }
    else{
        unsigned half_in1 = in1 / 2;
        unsigned long long result = pint_power_value(in0, half_in1) * pint_power_value(in0, half_in1);
        if (in1 % 2){
            return result * in0; 
        }
        else{
            return result;
        }
    }
}

static unsigned long long pint_power_value_s(int in0, int in1){
    if (in1 == 0){
        return 1;
    }
    else if (in1 == 1){
        return in0;
    }
    else{
        unsigned half_in1 = in1 / 2;
        unsigned long long result = pint_power_value_s(in0, half_in1) * pint_power_value_s(in0, half_in1);
        if (in1 % 2){
            return result * in0; 
        }
        else{
            return result;
        }
    }
}

template<bool _nxz = false>
void pint_power_u_common(void  *out, unsigned out_size, const void  *in0, unsigned in0_size, const void  *in1, unsigned in1_size){
    NCORE_PERF_MEASURE(pint_power_u_common<_nxz_in>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(61);
    if (out_size > 64){
        pint_assert((out_size == in0_size) && (in0_size == in1_size), "%u, %u, %u", out_size, in0_size, in1_size);
        pint_power_int_u((unsigned int*)out, (unsigned int*)in0, (unsigned int*)in1, out_size);
        return;
    }
    
    unsigned values[2];
    unsigned in_sizes[2] = {in0_size, in1_size};
    void* in_ptrs[2] = {in0, in1};
    unsigned xz_flags[2];
    
    unsigned long long result;

    for (int i = 0; i < 2; i++){
        if (in_sizes[i] <= 4){
            values[i] = pint_get_value_char_u((unsigned char*)in_ptrs[i], in_sizes[i]);
        }
        else{
            values[i] = pint_get_value_int_u((unsigned int*)in_ptrs[i], in_sizes[i]);
        }
        xz_flags[i] = (values[i] == VALUE_XZ);
    }

    if ((!xz_flags[0]) && (!xz_flags[1])){
        unsigned signed_bit = 1<< (in1_size -1);
        if((signed_bit & values[1]) && values[0] == 0){
            pint_set_sig_x(out, out_size);
            return;
        }
        else
            result = pint_power_value(values[0], values[1]);

        if (out_size != 64){
            result %= ((unsigned long long)1 << out_size);
        }

        if (out_size > 4){
            unsigned out_word_num = (out_size + 31) / 32 * 2;
            for (int i = 0; i < out_word_num; i++){
                ((unsigned*)out)[i] = 0;
            }
            if (out_size > 32){
                ((unsigned long long*)out)[0] = result;
            }
            else{
                ((unsigned*)out)[0] = (unsigned)result;
            }
        }
        else{
            ((unsigned char*)out)[0] = result & 0xf;
        }
    }
    else{
        pint_set_sig_x(out, out_size);
    }
}


template<bool _nxz = false>
void pint_power_s_common(void  *out, unsigned out_size, const void  *in0, unsigned in0_size, unsigned in0_signed, 
    const void  *in1, unsigned in1_size, unsigned in1_signed){
    int values[2];
    unsigned in_sizes[2] = {in0_size, in1_size};
    unsigned in_signed[2] = {in0_signed, in1_signed};
    void* in_ptrs[2] = {in0, in1};
    unsigned xz_flags[2];
    
    unsigned long long result;

    for (int i = 0; i < 2; i++){
        if (in_sizes[i] <= 4){
            if(in_signed[i])
                values[i] = pint_get_value_char_s((unsigned char*)in_ptrs[i], in_sizes[i]);
            else
                values[i] = pint_get_value_char_u((unsigned char*)in_ptrs[i], in_sizes[i]);
        }
        else{
            if(in_signed[i])
                values[i] = pint_get_value_int_s((unsigned int*)in_ptrs[i], in_sizes[i]);
            else
                values[i] = pint_get_value_int_u((unsigned int*)in_ptrs[i], in_sizes[i]);
        }
        //printf("values[%d]=%d\n",i,values[i]);
        xz_flags[i] = (values[i] == VALUE_XZ);
    }

    if ((xz_flags[0]) || (xz_flags[1])){
        pint_set_sig_x(out, out_size);
        return;
    }

    if (out_size > 64){
        pint_assert((out_size == in0_size), "%u, %u", out_size, in0_size);
        pint_assert((in1_size <= 0x20), "pint_power_s_common, in1_size = %u", in1_size);

        pint_power_int_s((unsigned int*)out, values[0], values[1], out_size);
        return;
    }
    else{
            unsigned signed_bit = 1<< (in1_size -1);
            if((signed_bit & values[1]) && values[0] == 0){
                pint_set_sig_x(out, out_size);
                return;
            }
            else{
                if(values[1] < 0)
                {
                    if((values[0]>1) || (values[0] < -1))
                        result = 0;
                    else if(values[0] == 1)
                        result = 1;
                    else if(values[0] == -1){
                        if((-1*values[1]) % 2 == 0)
                            result = 1;
                        else
                            result = -1;
                    }
                }
                else
                    result = pint_power_value_s(values[0], values[1]);
            }
                
            if (out_size != 64){
                result %= ((unsigned long long)1 << out_size);
            }

            if (out_size > 4){
                unsigned out_word_num = (out_size + 31) / 32 * 2;
                for (int i = 0; i < out_word_num; i++){
                    ((unsigned*)out)[i] = 0;
                }
                if (out_size > 32){
                    ((unsigned long long*)out)[0] = result;
                }
                else{
                    ((unsigned*)out)[0] = (unsigned)result;
                }
            }
            else{
                ((unsigned char*)out)[0] = result & 0xf;
            }
    }
}
#endif

void pint_copy_bits_common(void *out, const void *in0, int base, int size, int size_in) {
    pint_set_sig_x(out, size);

    // no overlapping
    if ((base >= size_in) || (base + size <= 0)) {
        return;
    }

    int dst_from_bits = base >= 0 ? 0 : base * -1;
    int src_from_bits = base >= 0 ? base : 0;
    int src_end_bits = base + size > size_in ? size_in : base + size;
    int cnt_bits = src_end_bits - src_from_bits;
    int valid_bits_word_num = (cnt_bits + 31) / 32;
    unsigned *bits_tmp_l =
        (unsigned *)pint_mem_alloc(valid_bits_word_num * 2 * sizeof(unsigned), SUB_T_CLEAR);
    unsigned *bits_tmp_h = bits_tmp_l + valid_bits_word_num;
    int i;

    for (i = 0; i < valid_bits_word_num * 2; i++) {
        bits_tmp_l[i] = 0;
    }

    // get bits
    if (size_in <= 4) {
        unsigned char tmp = *(unsigned char *)in0;
        tmp = tmp >> src_from_bits;
        unsigned mask = (1 << cnt_bits) - 1;
        mask |= mask << 4;
        tmp &= mask;
        bits_tmp_l[0] = tmp & 0xf;
        bits_tmp_h[0] = tmp >> 4;
    } else {
        for (i = 0; i < cnt_bits; i++) {
            int dst_wrod_idx = i / 32;
            int dst_bit_idx = i % 32;

            int src_wrod_idx = (i + src_from_bits) / 32;
            int src_bit_idx = (i + src_from_bits) % 32;

            unsigned *in_l = (unsigned *)in0;
            unsigned *in_h = in_l + (size_in + 31) / 32;

            bits_tmp_l[dst_wrod_idx] |= ((in_l[src_wrod_idx] >> src_bit_idx) & 1) << dst_bit_idx;
            bits_tmp_h[dst_wrod_idx] |= ((in_h[src_wrod_idx] >> src_bit_idx) & 1) << dst_bit_idx;
        }
    }

    // set bits
    if (size <= 4) {
        for (i = 0; i < cnt_bits; i++) {
            *(unsigned char *)out &= ~(0x11 << (i + dst_from_bits));
            *(unsigned char *)out |= (((bits_tmp_l[0] >> i) & 1) + ((bits_tmp_h[0] >> i) & 1) * 16)
                                     << (i + dst_from_bits);
        }
    } else {
        unsigned *out_l = (unsigned *)out;
        unsigned *out_h = out_l + (size + 31) / 32;

        for (i = 0; i < cnt_bits; i++) {
            int dst_wrod_idx = (i + dst_from_bits) / 32;
            int dst_bit_idx = (i + dst_from_bits) % 32;

            int src_wrod_idx = i / 32;
            int src_bit_idx = i % 32;

            out_l[dst_wrod_idx] &= ~(1U << dst_bit_idx);
            out_l[dst_wrod_idx] |= ((bits_tmp_l[src_wrod_idx] >> src_bit_idx) & 1) << dst_bit_idx;

            out_h[dst_wrod_idx] &= ~(1U << dst_bit_idx);
            out_h[dst_wrod_idx] |= ((bits_tmp_h[src_wrod_idx] >> src_bit_idx) & 1) << dst_bit_idx;
        }
    }
#ifdef CPU_MODE
    free(bits_tmp_l);
#endif
}

#if 0 // nxz move to pint_net.inc
template<bool _nxz = false>
pint_edge_t pint_copy_sig_out_range_edge(const void* src, int src_width, int src_base, void* dst, int dst_width, int dst_base, int count)
{
    //no overlapping
    if ((dst_base + count <= 0)||(dst_base >= dst_width)){
        return pint_nonedge;
    }
    
    if (dst_base < 0){
        src_base -= dst_base;
        count += dst_base;
        dst_base = 0;
    }

    if (dst_base + count > dst_width){
        count -= (dst_base + count - dst_width);
    }

    if ((src_width <= 4)&&(dst_width <= 4)){
        return pint_copy_char_char_edge<_nxz>(src, src_base, dst, dst_base, count);
    }
    else if ((src_width > 4)&&(dst_width <= 4)){
        return pint_copy_int_char_edge<_nxz>(src, src_width, src_base, dst, dst_base, count);
    }
    else if ((src_width <= 4)&&(dst_width > 4)){
        return pint_copy_char_int_edge<_nxz>(src, src_base, dst, dst_width, dst_base, count);
    }
    else{
        return pint_copy_int_int_edge<_nxz>(src, src_width, src_base, dst, dst_width, dst_base, count);
    }
}


template<bool _nxz = false>
void pint_copy_sig_out_range_noedge(const void* src, int src_width, int src_base, void* dst, int dst_width, int dst_base, int count)
{
    //no overlapping
    if ((dst_base + count <= 0)||(dst_base >= dst_width)){
        return;
    }
    
    if (dst_base < 0){
        src_base -= dst_base;
        count += dst_base;
        dst_base = 0;
    }

    if (dst_base + count > dst_width){
        count -= (dst_base + count - dst_width);
    }

    if ((src_width <= 4)&&(dst_width <= 4)){
        pint_copy_char_char<_nxz>(src, src_base, dst, dst_base, count);
    }
    else if ((src_width > 4)&&(dst_width <= 4)){
        pint_copy_int_char<_nxz>(src, src_width, src_base, dst, dst_base, count);
    }
    else if ((src_width <= 4)&&(dst_width > 4)){
        pint_copy_char_int<_nxz>(src, src_base, dst, dst_width, dst_base, count);
    }
    else{
        pint_copy_int_int<_nxz>(src, src_width, src_base, dst, dst_width, dst_base, count);
    }
}
#endif

void pint_merge_multi_drive(void *pint_out, unsigned *muldrv_table, int idx, int base, int size,
                            int sig_type) {
    static unsigned char merge_lut_norm[4][4] = {{BIT4_0, BIT4_X, BIT4_0, BIT4_X},
                                                 {BIT4_X, BIT4_1, BIT4_1, BIT4_X},
                                                 {BIT4_0, BIT4_1, BIT4_Z, BIT4_X},
                                                 {BIT4_X, BIT4_X, BIT4_X, BIT4_X}};

    static unsigned char merge_lut_and[4][4] = {{BIT4_0, BIT4_0, BIT4_0, BIT4_0},
                                                {BIT4_0, BIT4_1, BIT4_1, BIT4_X},
                                                {BIT4_0, BIT4_1, BIT4_Z, BIT4_X},
                                                {BIT4_0, BIT4_X, BIT4_X, BIT4_X}};

    static unsigned char merge_lut_or[4][4] = {{BIT4_0, BIT4_1, BIT4_0, BIT4_X},
                                               {BIT4_1, BIT4_1, BIT4_1, BIT4_1},
                                               {BIT4_0, BIT4_1, BIT4_Z, BIT4_X},
                                               {BIT4_X, BIT4_1, BIT4_X, BIT4_X}};

    unsigned char(*merge_lut)[4];

    if (sig_type == 7) {
        merge_lut = merge_lut_and;
    } else if (sig_type == 8) {
        merge_lut = merge_lut_or;
    } else {
        merge_lut = merge_lut_norm;
    }

    int mul_drv_num = 0;
    unsigned width = muldrv_table[1];
    while (mul_drv_num == 0) {
        pint_atomic_swap(muldrv_table, mul_drv_num);
    }

    if (width <= 4) {
        unsigned char *items = (unsigned char *)(muldrv_table + 2);
        pint_copy_sig_out_range_noedge(pint_out, size, 0, items + idx, width, base, size);

        // merge multi drive
        unsigned char result = items[0];
        unsigned char bit_sel_res;
        unsigned char bit_sel_drv;
        unsigned char bit_sel_tmp;
        for (int bit_i = 0; bit_i < width; bit_i++) {
            for (int drv_i = 1; drv_i < mul_drv_num; drv_i++) {
                bit_sel_drv = items[drv_i] >> bit_i;
                bit_sel_drv = (bit_sel_drv & 1) + ((bit_sel_drv >> 3) & 2);
                bit_sel_res = result >> bit_i;
                bit_sel_res = (bit_sel_res & 1) + ((bit_sel_res >> 3) & 2);
                bit_sel_tmp = merge_lut[bit_sel_drv][bit_sel_res];
                if (bit_sel_tmp != bit_sel_res) {
                    result &= ~(0x11 << bit_i);
                    result |= ((bit_sel_tmp & 1) << bit_i) + ((bit_sel_tmp & 2) << (bit_i + 3));
                }
            }
        }
        result = result >> base;
        unsigned mask = (1 << size) - 1;
        result &= mask + mask * 16;
        *(unsigned char *)pint_out = result;
    } else {
        int word_num = (width + 31) / 32;
        unsigned *items = (unsigned *)(muldrv_table + 2);
        pint_copy_sig_out_range_noedge(pint_out, size, 0, items + word_num * 2 * idx, width, base,
                                       size);

        unsigned *tmp_buf = (unsigned *)pint_mem_alloc(word_num * 8, SUB_T_CLEAR);
        unsigned *tmp_l = (unsigned *)tmp_buf;
        unsigned *tmp_h = tmp_l + word_num;

        // merge multi drive
        int word_bits;
        unsigned char bit_sel_drv;
        unsigned char bit_sel_tmp;

        for (int word_i = 0; word_i < word_num; word_i++) {
            word_bits = (word_i == word_num - 1) ? (width % 32) : 32;
            word_bits = (word_bits == 0) ? 32 : word_bits;
            unsigned word_res_tmp[2] = {0};
            for (int bit_i = 0; bit_i < word_bits; bit_i++) {
                bit_sel_tmp = BIT4_Z;
                for (int drv_i = 0; drv_i < mul_drv_num; drv_i++) {
                    unsigned *l_bits = items + drv_i * word_num * 2 + word_i;
                    unsigned *h_bits = l_bits + word_num;
                    bit_sel_drv = ((*l_bits >> bit_i) & 1) + ((*h_bits >> bit_i) & 1) * 2;
                    // printf("dri %d && %x and %x -> %x\n", drv_i, bit_sel_drv, bit_sel_tmp,
                    // merge_lut[bit_sel_drv][bit_sel_tmp]);
                    bit_sel_tmp = merge_lut[bit_sel_drv][bit_sel_tmp];
                }

                word_res_tmp[0] |= (bit_sel_tmp & 1) << bit_i;
                word_res_tmp[1] |= (bit_sel_tmp >> 1) << bit_i;
                // printf("2222 bit_i %d word_res_tmp = 0x%x 0x%x\n", bit_i, word_res_tmp[0],
                // word_res_tmp[1]);
            }

            if (word_res_tmp[0] != tmp_l[word_i]) {
                tmp_l[word_i] = word_res_tmp[0];
            }
            if (word_res_tmp[1] != tmp_h[word_i]) {
                tmp_h[word_i] = word_res_tmp[1];
            }
        }

        if (size <= 4) {
            pint_get_subarray_int_char((unsigned char *)pint_out, tmp_l, base, size, width);
        } else {
            pint_get_subarray_int_int((unsigned int *)pint_out, tmp_l, base, size, width);
        }
#ifdef CPU_MODE
        free(tmp_buf);
#endif
    }
}

void pint_finish_multi_drive(unsigned *muldrv_table, int mul_drv_num) {
    pint_wait_write_done();
    pint_atomic_swap(muldrv_table, mul_drv_num);
}
