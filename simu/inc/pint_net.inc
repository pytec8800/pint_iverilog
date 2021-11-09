#define LYQ_TWO 2
// #define IS_BINARY_DYNAMIC

// /*__constexpr*/ forceinline  
// void check_is_xz(const void* sig, unsigned width){
//     bool xz_flag = false;
//     if (width <= 4){
//         unsigned char data = *(unsigned char*)sig;
//         xz_flag = (data >= 16);
//     }
//     else{
//         unsigned word_num = (width + 31) / 32;
//         unsigned* h_bits = ((unsigned*)sig) + word_num;
//         for (int i = 0; i < word_num; i++){
//             if (h_bits[i] > 0){
//                 xz_flag = true;
//                 break;
//             }
//         }
//     }
//     if (xz_flag){
//         printf("LYQ Error: regard xz as binary!\n");
//         while(1);
//     }
// }
#define check_is_xz(sig, width)


#ifdef IS_BINARY_DYNAMIC
/*__constexpr*/ forceinline  
bool is_binary_sqing(void* sig, unsigned width){
    if (width <= 4){
        unsigned char data = *(unsigned char*)sig;
        return (data < 16);
    }
    else{
        unsigned word_num = (width + 31) / 32;
        unsigned* h_bits = ((unsigned*)sig) + word_num;
        for (int i = 0; i < word_num; i++){
            if (h_bits[i] > 0){
                return false;
            }
        }
        return true;
    }
}
#endif

/*__constexpr*/ forceinline 
unsigned int add_with_carry(unsigned int a, unsigned int b, unsigned int* carry){
    unsigned int sum   = b + *carry;
    unsigned int c_new = 0;
    if(sum < b)  c_new = 1;
    sum += a;
    if(sum < a || sum < b)  c_new = 1;
    *carry = c_new;
    return sum;
}

/*__constexpr*/ forceinline 
unsigned int multiply_with_carry(unsigned int a, unsigned int b, unsigned int*carry)
{
      const unsigned int mask = (1UL << (PINT_INT_SIZE/2)) - 1;
      unsigned int a0 = a & mask;
      unsigned int a1 = (a >> (PINT_INT_SIZE/2)) & mask;
      unsigned int b0 = b & mask;
      unsigned int b1 = (b >> (PINT_INT_SIZE/2)) & mask;

      unsigned int tmp = a0 * b0;

      unsigned int r00 = tmp & mask;
      unsigned int c00 = (tmp >> (PINT_INT_SIZE/2)) & mask;

      tmp = a0 * b1;

      unsigned int r01 = tmp & mask;
      unsigned int c01 = (tmp >> (PINT_INT_SIZE/2)) & mask;

      tmp = a1 * b0;

      unsigned int r10 = tmp & mask;
      unsigned int c10 = (tmp >> (PINT_INT_SIZE/2)) & mask;

      tmp = a1 * b1;

      unsigned int r11 = tmp & mask;
      unsigned int c11 = (tmp >> (PINT_INT_SIZE/2)) & mask;

      unsigned int r1 = c00 + r01 + r10;
      unsigned int r2 = (r1 >> (PINT_INT_SIZE/2)) & mask;
      r1 &= mask;
      r2 += c01 + c10 + r11;
      unsigned int r3 = (r2 >> (PINT_INT_SIZE/2)) & mask;
      r2 &= mask;
      r3 += c11;
      r3 &= mask;

      *carry = (r3 << (PINT_INT_SIZE/2)) + r2;
      return (r1 << (PINT_INT_SIZE/2)) + r00;
}

#if 1  // nxz
template<bool _nxz = false>
bool pint_mask_case_equality_int(unsigned int*  in0, const unsigned int*  in1, const unsigned int*  mask, unsigned int width){
    NCORE_PERF_MEASURE(pint_mask_case_equality_int<_nxz>, 2);
    unsigned    cnt = (width + 0x1f) >> 5;
    int i;    
    for(i =0; i < cnt; i++){
        if((in0[i] ^ in1[i]) & mask[i]) return 0;
        if __constexpr(!_nxz) {
            if((in0[i +cnt] ^ in1[i +cnt]) & mask[i]) return 0;
        }
    }
    return 1;
}
#endif

//  Function:   out[base+size-1, base] = in;
//  Character:	size(The width of signal in0);
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_set_subarray_char(unsigned char* out, const unsigned char* in0, unsigned int base, unsigned int size, unsigned int size_out){
    NCORE_PERF_MEASURE(pint_set_subarray_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(1);
    pint_assert(size <= PINT_CHAR_OFFSET, "size if not valid for char.");

    if (base >= size_out){
	    return;
    }
    
    if (base + size > size_out){
	    size = size_out - base;
    }
    
    unsigned char mask = ((1 << size) - 1) << base;
    if __constexpr(!_nxz) {
        mask |= mask << 4;
    }
#ifdef CPU_MODE
    *out  = (*out & (~mask)) | ((*in0 << base) & mask);
#else
    unsigned int rs2;
    if ((unsigned int)out & 0x1) {
        rs2 = ((mask << 8) << 16) | (((*in0 << base) << 8) & 0xFFFF);
    } else {
        rs2 = (mask << 16) | (*in0 << base);
    }
    asm volatile("uap.shm\t%1, %0":"+m" (*out), "+r" (rs2));
#endif
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_set_subarray_char_value(unsigned char* out, unsigned char  in0, unsigned int base, unsigned int size){
    NCORE_PERF_MEASURE(pint_set_subarray_char_value<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(2);
    pint_set_subarray_char<_nxz>(out, &in0, base, size, 4);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_set_subarray_int_char(unsigned int*  out, const unsigned char* in0, unsigned int base, unsigned int size, unsigned int size_out){
    NCORE_PERF_MEASURE(pint_set_subarray_int_char<_nxz>, 2);
NCORE_PERF_PINT_NET_SUMMARY(3);

    if (base >= size_out){
	return;
    }
    
    if (base + size > size_out){
	size = size_out - base;
    }
    
#ifdef CPU_MODE
    unsigned int cnt    = (size_out + 0x1f) >> 5;
    unsigned int idx_b  =  base >> 5;
    unsigned int idx_s  = (base + size) >> 5;
    unsigned int off    =  base & 0x1f;
    unsigned int mask   = ((1 << size) -1);
    out[idx_b] = (out[idx_b] & (~(mask<<off))) | ((unsigned int)((*in0 & 0x0f)&mask) << off);
    if __constexpr(!_nxz) {
        out[idx_b +cnt] = (out[idx_b +cnt] & (~(mask<<off))) | ((unsigned int)(((*in0 >> 4)&mask) << off));
    }
    if(idx_b != idx_s){     // idx_s = idx_b + 1
        off  = 32 - (base & 0x1f);
        mask = ((1 << size) -1);
        out[idx_s] = (out[idx_s] & (~(mask>>off))) | ((unsigned int)((*in0 & 0x0f)&mask) >> off);
        if __constexpr(!_nxz) {
            out[idx_s +cnt] = (out[idx_s +cnt] & (~(mask>>off))) | ((unsigned int)((*in0 >> 4)&mask) >> off);
        }
    }
#else
    unsigned int cnt = (size_out + 0x1f) >> 5;
    unsigned int half_idx_b = base >> 4;
    unsigned int half_idx_s = (base + size) >> 4;
    unsigned int half_mod = base & 0xf;
    unsigned int half_mask = ((1 << size) - 1) << half_mod;
    unsigned int rs2;
    unsigned short *addr;

    rs2 = (half_mask << 16) | (((unsigned int)(*in0 & 0x0f) << half_mod) & 0xFFFF);
    addr = (unsigned short *)out + half_idx_b;
    asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
    if __constexpr(!_nxz) {
        rs2 = (half_mask << 16) | (((unsigned int)(*in0 >> 4) << half_mod) & 0xFFFF);
        addr = (unsigned short *)out + 2*cnt + half_idx_b;
        asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
    }
    if (half_idx_b != half_idx_s) {
        unsigned int half_off = 16 - half_mod;
        half_mask = ((1 << size) - 1) >> half_off;
        rs2 = (half_mask << 16) | ((unsigned int)(*in0 & 0x0f) >> half_off);
        addr = (unsigned short *)out + half_idx_b + 1;
        asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
        if __constexpr(!_nxz) {
            rs2 = (half_mask << 16) | ((unsigned int)(*in0 >> 4) >> half_off);
            addr = (unsigned short *)out + 2*cnt + half_idx_b + 1;
            asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
        }
    }
#endif
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_set_subarray_int_char_value(unsigned int*  out, unsigned char  in0, unsigned int base, unsigned int size, unsigned int size_out){
    NCORE_PERF_MEASURE(pint_set_subarray_int_char_value<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(4);
    pint_set_subarray_int_char<_nxz>(out, &in0, base,  size,  size_out);
}

#ifdef CPU_MODE
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_set_subarray_int(unsigned int* out, const unsigned int* in0, unsigned int base, unsigned int size, unsigned int size_out){
    NCORE_PERF_MEASURE(pint_set_subarray_int<_nxz>, 2);
    pint_assert((base + size) <= size_out, "size is not valid.");

    if (base >= size_out){
	return;
    }
    
    if (base + size > size_out){
	size = size_out - base;
    }
    
    unsigned int cnt    = (size_out + 0x1f) >> 5;
    unsigned int cnt_in = (size + 0x1f) >> 5;
    unsigned int cnt_b  =  base >> 5;
    unsigned int mod_b  =  base & 0x1f;
    unsigned int mask, idx, i;
    if(mod_b){      // mod_b, [1, 31]
        unsigned int mod_in = (size & 0x1f);
        unsigned int mod_all = mod_b + mod_in;
        mask = (1 << mod_b) - 1;
        for (i = 0; i < (size >> 5); i++){
            idx = i + cnt_b;
            out[idx] = (out[idx] & mask) | (in0[i] << mod_b);
            idx ++;
            out[idx] = (out[idx] & (~mask)) | (in0[i] >> (32 - mod_b));

            if __constexpr(!_nxz) {
                idx = i + cnt_b + cnt;
                out[idx] = (out[idx] & mask) | (in0[i + cnt_in] << mod_b);
                idx ++;
                out[idx] = (out[idx] & (~mask)) | (in0[i + cnt_in] >> (32 - mod_b));
            }
        }
        if (mod_in > 0) {
            if(mod_all <= 32){
                mask |= 0xfffffffe << (mod_all - 1);    // [1, 32]
                idx   = cnt_b + cnt_in - 1;
                out[idx] = (out[idx] & mask) | (in0[cnt_in - 1] << mod_b);
                if __constexpr(!_nxz) {
                    idx  += cnt;
                    out[idx] = (out[idx] & mask) | (in0[2*cnt_in - 1] << mod_b);
                }
            } else {
                mask = (1 << mod_b) - 1;
                idx = cnt_b + cnt_in - 1;
                out[idx] = (out[idx] & mask) | (in0[cnt_in - 1] << mod_b);
                idx++;
                mask = (1 << (mod_all & 0x1f)) - 1;
                out[idx] = (out[idx] & ~mask) | (in0[cnt_in - 1] >> (32 - mod_b));

                if __constexpr(!_nxz) {
                    mask = (1 << mod_b) - 1;
                    idx = cnt_b + cnt_in - 1 + cnt;
                    out[idx] = (out[idx] & mask) | (in0[2*cnt_in - 1] << mod_b);
                    idx++;
                    mask = (1 << (mod_all & 0x1f)) - 1;
                    out[idx] = (out[idx] & ~mask) | (in0[2*cnt_in - 1] >> (32 - mod_b));
                }
            }
        }
    }else{
        unsigned int mod_all = size & 0x1f;
        if (0 == mod_all) {
            for(i=0; i < cnt_in; i++){
                out[i + cnt_b] = in0[i];
                if __constexpr(!_nxz) {
                    out[i + cnt_b + cnt] = in0[i + cnt_in];
                }
            }
        } else {
            for(i=0; i < cnt_in-1; i++){
                out[i + cnt_b] = in0[i];
                if __constexpr(!_nxz) {
                    out[i + cnt_b + cnt] = in0[i + cnt_in];
                }
            }
            mask = 0xfffffffe << (mod_all - 1);      // [1, 31]
            idx  = cnt_b + cnt_in - 1;
            out[idx] = (out[idx] & mask) | in0[cnt_in - 1];
            if __constexpr(!_nxz) {
                idx += cnt;
                out[idx] = (out[idx] & mask) | in0[2*cnt_in - 1];
            }
        }
    }
}
#else
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_set_subarray_int(unsigned int* out, const unsigned int* in0, unsigned int base, unsigned int size, unsigned int size_out){
    NCORE_PERF_MEASURE(pint_set_subarray_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(6);
    pint_assert((base + size) <= size_out, "size is not valid.");

    if (base >= size_out){
	return;
    }
    
    if (base + size > size_out){
	size = size_out - base;
    }
    
    unsigned int cnt    = (size_out + 0x1f) >> 5;
    unsigned int cnt_in = (size + 0x1f) >> 5;
    unsigned int half_cnt_in = (size + 0xf) >> 4;
    unsigned int half_cnt_b  = base >> 4;
    unsigned int half_mod_b  = base & 0xf;
    unsigned int half_mask, idx, i;
    unsigned int rs2;
    unsigned short *addr;
    if(half_mod_b){      // half_mod_b, [1, 15]
        unsigned int half_mod_in = (size & 0xf);
        unsigned int half_mod_all = half_mod_b + half_mod_in;
        half_mask = (1 << half_mod_b) - 1;
        for (i = 0; i < (size >> 4); i++) {
            rs2 = ((~half_mask) << 16) | ((*((unsigned short *)in0 + i) << half_mod_b) & 0xFFFF);
            addr = (unsigned short *)out + i + half_cnt_b;
            asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
            rs2 = (half_mask << 16) | (*((unsigned short *)in0 + i) >> (16 - half_mod_b));
            addr = (unsigned short *)out + i + 1 + half_cnt_b;
            asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));

            if __constexpr(!_nxz) {
                rs2 = ((~half_mask) << 16) | ((*((unsigned short *)in0 + i + 2*cnt_in) << half_mod_b) & 0xFFFF);
                addr = (unsigned short *)out + i + half_cnt_b + 2*cnt;
                asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
                rs2 = (half_mask << 16) | (*((unsigned short *)in0 + i + 2*cnt_in) >> (16 - half_mod_b));
                addr = (unsigned short *)out + i + 1 + half_cnt_b + 2*cnt;
                asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
            }
        }
        if (half_mod_in > 0) {
            if (half_mod_all <= 16) {
                half_mask |= (0xFFFF << half_mod_all);
                rs2 = ((~half_mask) << 16) | ((*((unsigned short *)in0 + half_cnt_in - 1) << half_mod_b) & 0xFFFF);
                addr = (unsigned short *)out + half_cnt_in - 1 + half_cnt_b;
                asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
                if __constexpr(!_nxz) {
                    rs2 = ((~half_mask) << 16) | ((*((unsigned short *)in0 + half_cnt_in - 1 + 2*cnt_in) << half_mod_b) & 0xFFFF);
                    addr = (unsigned short *)out + half_cnt_in - 1 + half_cnt_b + 2*cnt;
                    asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
                }
            } else {
                half_mask = (1 << half_mod_b) - 1;
                rs2 = ((~half_mask) << 16) | ((*((unsigned short *)in0 + half_cnt_in - 1) << half_mod_b) & 0xFFFF);
                addr = (unsigned short *)out + half_cnt_in - 1 + half_cnt_b;
                asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
                half_mask = (1 << (half_mod_all & 0xf)) - 1;
                rs2 = (half_mask << 16) | (*((unsigned short *)in0 + half_cnt_in - 1) >> (16 - half_mod_b));
                addr = (unsigned short *)out + half_cnt_in + half_cnt_b;
                asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));

                if __constexpr(!_nxz) {
                    half_mask = (1 << half_mod_b) - 1;
                    rs2 = ((~half_mask) << 16) | ((*((unsigned short *)in0 + half_cnt_in - 1 + 2*cnt_in) << half_mod_b) & 0xFFFF);
                    addr = (unsigned short *)out + half_cnt_in - 1 + half_cnt_b + 2*cnt;
                    asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
                    half_mask = (1 << (half_mod_all & 0xf)) - 1;
                    rs2 = (half_mask << 16) | (*((unsigned short *)in0 + half_cnt_in - 1 + 2*cnt_in) >> (16 - half_mod_b));
                    addr = (unsigned short *)out + half_cnt_in + half_cnt_b + 2*cnt;
                    asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
                }
            }
        }
    }else{
        for (i=0; i < half_cnt_in-1; i++) {
            rs2 = 0xFFFF0000 | *((unsigned short *)in0 + i);
            addr = (unsigned short *)out + i + half_cnt_b;
            asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
            if __constexpr(!_nxz) {
                rs2 = 0xFFFF0000 | *((unsigned short *)in0 + i + 2*cnt_in);
                addr = (unsigned short *)out + i + half_cnt_b + 2*cnt;
                asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
            }
        }

        half_mask = 0xFFFF;
        unsigned int half_mod_all = size & 0xf;
        if (half_mod_all > 0) {
            half_mask = (1 << half_mod_all) - 1;
        }
        rs2 = (half_mask << 16) | *((unsigned short *)in0 + half_cnt_in - 1);
        addr = (unsigned short *)out + half_cnt_in - 1 + half_cnt_b;
        asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
        if __constexpr(!_nxz) {
            rs2 = (half_mask << 16) | *((unsigned short *)in0 + half_cnt_in - 1 + 2*cnt_in);
            addr = (unsigned short *)out + half_cnt_in - 1 + half_cnt_b + 2*cnt;
            asm volatile("uap.shm\t%1, %0":"+m" (*addr), "+r" (rs2));
        }
    }
}
#endif

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_set_subarray_int_value(unsigned int* out, unsigned int in0, unsigned int base, unsigned int size, unsigned int size_out){
    NCORE_PERF_MEASURE(pint_set_subarray_int_value<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(8);
    unsigned int in[2];
    in[0] = in0;
    in[1] = 0;
    pint_set_subarray_int<_nxz>(out, in, base, size, size_out);
}

//  Function:   get_value(exclude x, z);
//  Character:	3'b01z => 0, 3'b01x => 0;
template<bool _nxz = false>
/*__constexpr*/ forceinline 
int pint_get_value_char_u(const unsigned char* in,unsigned char size){
    NCORE_PERF_MEASURE(pint_get_value_char_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(12);
    if __constexpr(_nxz) {
        return (int)(*in);
    } else {
        unsigned char d0 = *in;
        if((d0 & 0xf0) == 0){
            return (int)d0;
        }
        else{
            return VALUE_XZ;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
int pint_get_value_char_s(const unsigned char* in,unsigned char size){
    NCORE_PERF_MEASURE(pint_get_value_char_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(13);
    if __constexpr(_nxz) {
        const unsigned char   d0 = *in;
        if ((d0 >> (size - 1)) & 1){
            return 0xffffffff - ((1 << size) - 1) | d0;
        } 
        else{
            return (int)d0;
        }
    } else {
        const unsigned char   d0 = *in;
        if((d0 & 0xf0) == 0){
            if ((d0 >> (size - 1)) & 1){
                return 0xffffffff - ((1 << size) - 1) | d0;
            } 
            else{
                return (int)d0;
            }
        }
        else{
            return VALUE_XZ;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
int pint_get_value_int_u(const unsigned int* in,unsigned int size){
NCORE_PERF_MEASURE(pint_get_value_int_u<_nxz>, 2);
NCORE_PERF_PINT_NET_SUMMARY(14);
    int word_num = (size + 31) / 32;
    const unsigned int* in_h = in + word_num;

    if __constexpr(!_nxz) {
        for (int i = 0; i < word_num; i++){
            if (in_h[i]){
                return VALUE_XZ;
            }
        }
    }

    for (int i = 1; i < word_num; i++){
	if (in[i]){
	    return VALUE_MAX;
	}
    }

    return (in[0] > VALUE_MAX) ? VALUE_MAX : (int)in[0];
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
int pint_get_value_int_s(const unsigned int* in,unsigned int size){
NCORE_PERF_MEASURE(pint_get_value_int_s<_nxz>, 2);
NCORE_PERF_PINT_NET_SUMMARY(15);
    int word_num = (size + 31) / 32;
    int bit_off = size % 32;
    int bit_sel = (bit_off == 0) ? 0x80000000 : (1 << (bit_off - 1));
    int bit_mask = (bit_off == 0) ? 0xffffffff : (1 << bit_off) - 1;
    const unsigned int* in_h = in + word_num;

    if __constexpr(!_nxz) {
        for (int i = 0; i < word_num; i++){
            if (in_h[i]){
                return VALUE_XZ;
            }
        }
    }

    if ((in[word_num - 1] & bit_sel) == 0){
        for (int i = 1; i < word_num; i++){
	    if (in[i]){
	        return VALUE_MAX;
	    }
        }
        return (in[0] > VALUE_MAX) ? VALUE_MAX : (int)in[0];
    }
    else{
    	int value = *(int*)in;
    	if (word_num == 1){
	    value |= ~bit_mask;
    	}
	for (int i = 0; i < word_num; i++){
	    if ((i > 0)){
	    	if (i == word_num - 1){
    	    	    if (in[i] != bit_mask){
    	                return VALUE_MIN;
    	            }
	        }
	        else{
		    if (in[i] != 0xffffffff){
			return VALUE_MIN;
		    }
	        }
	    }
        }
        
        return  (value == 0x80000000) ? VALUE_MIN : value;
    }
}

//  Function:   copy
/*__constexpr*/ forceinline 
void pint_copy_char(unsigned char* out, const unsigned char* in0, unsigned int size){
    //NCORE_PERF_MEASURE(pint_copy_char, 2);  // no profile due to Mcore call
    NCORE_PERF_PINT_NET_SUMMARY(16);
    *out = *in0;
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_copy_int(unsigned int* out, const unsigned int* in0, unsigned int size){
    //NCORE_PERF_MEASURE(pint_copy_int<_nxz>, 2);  // no profile due to Mcore call
    NCORE_PERF_PINT_NET_SUMMARY(17);
    if __constexpr(_nxz) {
        unsigned int count = (size + 0x1f) >> 5;
        for (size_t i = 0; i < count; ++i) {
            out[i] = in0[i];
        }
    } else {
        unsigned int i,cnt;
        cnt = ((size+0x1f) >> 5);
        for(i=0;i<2*cnt;i++){
            out[i]=in0[i];
        }
    }
}

template<bool _nxz = false>  // _nxz = true
/*__constexpr*/ forceinline 
void pint_copy_value(unsigned int* out, unsigned int in0, unsigned int size){
    //NCORE_PERF_MEASURE(pint_copy_value<_nxz>, 2);  // no profile due to Mcore call
    NCORE_PERF_PINT_NET_SUMMARY(18);
    if __constexpr(_nxz) {
        unsigned int count = (size + 0x1f) >> 5;
        out[0] = in0;
        for (size_t i = 1; i < count; ++i) {
            out[i] = 0;
        }
    } else {
        unsigned int cnt = (size + 0x1f) >> 5;
        out[0] = in0;
        for(int i=1; i < cnt*2; i++){
            out[i] = 0;
        }
    }
}

//  Function: these functions are used to set each bit of a signal
/*__constexpr*/ forceinline 
void pint_set_char(unsigned char* out, unsigned int size, unsigned int val){
    NCORE_PERF_MEASURE(pint_set_char, 2);
    NCORE_PERF_PINT_NET_SUMMARY(19);
    unsigned char mask_l = (1 << size) -1;
    switch (val){
    case BIT_0:
        *out = 0;
        return;
    case BIT_1:
        *out = mask_l;
        return;
    case BIT_X:
        *out = mask_l | (mask_l << PINT_CHAR_OFFSET);
        return;
    case BIT_Z:
        *out = mask_l << PINT_CHAR_OFFSET;
        return;
    default:
        return;
    }
}

template<bool _nxz = false>  // _nxz = true
/*__constexpr*/ forceinline 
void pint_set_int(unsigned int*  out, unsigned int size, unsigned int val){
    NCORE_PERF_MEASURE(pint_set_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(20);
    unsigned int cnt = (size + 0x1f) >> 5;
    unsigned int mod = size & 0x1f;
    unsigned int i;
    switch(val){
    case BIT_0:
        for(i=0; i < cnt; i++){
            out[i] = 0;
            out[i + cnt] = 0;
        }
        break;
    case BIT_1:
        for(i=0; i < cnt; i++){
            out[i] = PINT_INT_MASK;
            out[i + cnt] = 0;
        }
        break;
    case BIT_X:
        for(i=0; i < cnt; i++){
            out[i] = PINT_INT_MASK;
            out[i + cnt] = PINT_INT_MASK;
        }
        break;
    case BIT_Z:
        for(i=0; i < cnt; i++){
            out[i] = 0;
            out[i + cnt] = PINT_INT_MASK;
        }
        break;
    default:
        break;
    }

    if(mod){
        unsigned int mask = ~(PINT_INT_MASK << mod);
        out[cnt - 1] &= mask;
        out[2*cnt - 1] &= mask;
    }
}

//  Function: these functions are used to set only 1 bit of a signal
/*__constexpr*/ forceinline 
void pint_set_bit_char(unsigned char* out, unsigned int place, unsigned int val){
    NCORE_PERF_MEASURE(pint_set_bit_char, 2);
    NCORE_PERF_PINT_NET_SUMMARY(21);
    unsigned char mask = ~((1<<(place+PINT_CHAR_OFFSET)) | (1<<place));

    *out=(*out&mask);
    switch(val){
        case BIT_0:
        break;
        case BIT_1:
        *out=*out|(1<<place);
        break;
        case BIT_X:
        *out=*out|((1<<place) | (1<<(place+PINT_CHAR_OFFSET)));
        break;
        case BIT_Z:
        *out=*out|(1<<(place+PINT_CHAR_OFFSET));
        break;
        default:
        break;
    }
}

/*__constexpr*/ forceinline 
void pint_set_bit_int(unsigned int*  out, unsigned int place, unsigned int size, unsigned int val){
    NCORE_PERF_MEASURE(pint_set_bit_int, 2);
    NCORE_PERF_PINT_NET_SUMMARY(22);
    unsigned int cnt,cnt_place,mod_place,mask;
    cnt_place = ((place) >> 5);
    cnt = ((size+0x1f) >> 5);
    mod_place = place & 0x1f;
    mask = ~(1<<mod_place);

    out[cnt_place] = out[cnt_place]&mask;
    out[cnt_place+cnt] = out[cnt_place+cnt]&mask;
    switch(val){
        case BIT_0:
        break;
        case BIT_1:
        out[cnt_place] = out[cnt_place]|(1<<mod_place);
        break;
        case BIT_X:
        out[cnt_place] = out[cnt_place]|(1<<mod_place);
        out[cnt_place+cnt] = out[cnt_place+cnt]|(1<<mod_place);
        break;
        case BIT_Z:
        out[cnt_place+cnt] = out[cnt_place+cnt]|(1<<mod_place);
        break;
        default:
        break;
    }
}

//  Function: pad, broaden the width of a signal
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_pad_char_to_char_s(unsigned char* out, const unsigned char* in0, unsigned int size_in, unsigned int size_out){
    NCORE_PERF_MEASURE(pint_pad_char_to_char_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(23);
    pint_assert(size_in <= size_out, "size is not valid");
    unsigned char flag_bit;
    int i;
    unsigned char mask = 1<<(size_in-1);
    if __constexpr(!_nxz) {
        mask |= (mask << PINT_CHAR_OFFSET);
    }

    flag_bit = *in0 & mask;
    *out = *in0;
    for (i=0; i<(size_out-size_in); i++) {
        *out |= (flag_bit<<(i+1));
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_pad_char_to_int_s(unsigned int* out, const unsigned char* in0, unsigned int size_in, unsigned int size_out){
    NCORE_PERF_MEASURE(pint_pad_char_to_int_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(24);
    unsigned char flag_bit;
    unsigned int word_pad_hbits,word_pad_lbits;
    unsigned int cnt,mod,i,mask;
    unsigned char mask_l = (1<<size_in) - 1;
    unsigned char mask_h = mask_l<<PINT_CHAR_OFFSET;
    unsigned char flag_mask = 1<<(size_in-1);
    if __constexpr(!_nxz) {
        flag_mask |= (flag_mask << PINT_CHAR_OFFSET);
    }

    cnt = ((size_out+0x1f) >> 5);
    flag_bit = ((*in0&flag_mask) >> (size_in-1));
    switch(flag_bit){
        case BIT_0_CHAR:
        word_pad_hbits = 0;
        word_pad_lbits = 0;
        break;
        case BIT_1_CHAR:
        word_pad_hbits = 0;
        word_pad_lbits = PINT_INT_MASK;
        break;
        case BIT_X_CHAR:
        word_pad_hbits = PINT_INT_MASK;
        word_pad_lbits = PINT_INT_MASK;
        break;
        case BIT_Z_CHAR:
        word_pad_hbits = PINT_INT_MASK;
        word_pad_lbits = 0;
        break;
        default:
        printf("ERROR:pint_pad_char_to_int_s falg_bit[0x%x] is error\n", flag_bit);
        break;
    }
    out[0] = (*in0&mask_l) | (word_pad_lbits<<size_in);
    if __constexpr(!_nxz) {
        out[cnt] = ((*in0&mask_h)>>PINT_CHAR_OFFSET) | (word_pad_hbits<<size_in);
    }
    for(i=1;i<cnt;i++){
        out[i] = word_pad_lbits;
        if __constexpr(!_nxz) {
            out[i+cnt] = word_pad_hbits;
        }
    }

    mod = size_out & 0x1f;
    if (mod > 0) {
        mask = ~(PINT_INT_MASK<<mod);
        out[cnt-1] &= mask;
        if __constexpr(!_nxz) {
            out[2*cnt-1] &= mask;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_pad_int_to_int_s(unsigned int* out, const unsigned int* in0, unsigned int size_in, unsigned int size_out){
    NCORE_PERF_MEASURE(pint_pad_int_to_int_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(25);
    unsigned char flag_bit_l,flag_bit_h;
    unsigned int word_pad_lbits;
    unsigned int word_pad_hbits = 0;
    unsigned int ii;
    unsigned int cnt_out,mod_out,cnt_in,mod_in;
    unsigned int mask;

    cnt_out = (size_out+0x1f) >> 5;
    mod_out = size_out & 0x1f;
    cnt_in  = (size_in+0x1f) >> 5;
    mod_in  = size_in & 0x1f;

    if (mod_in > 0) {
        flag_bit_l = in0[cnt_in-1] >> (mod_in-1);
        if __constexpr(!_nxz) {
            flag_bit_h = in0[2*cnt_in-1] >> (mod_in-1);
        }
    } else {
        flag_bit_l = in0[cnt_in-1] >> (PINT_INT_SIZE-1);
        if __constexpr(!_nxz) {
            flag_bit_h = in0[2*cnt_in-1] >> (PINT_INT_SIZE-1);
        }
    }

    switch(flag_bit_l){
        case 1:
        word_pad_lbits = PINT_INT_MASK;
        break;
        case 0:
        word_pad_lbits = 0;
        break;
        default:
        printf("ERROR:pint_pad_int_to_int_s flag_bit_l[0x%x] is error\n", flag_bit_l);
        break;
    }
    if __constexpr(!_nxz) {
        switch(flag_bit_h){
            case 1:
            word_pad_hbits = PINT_INT_MASK;
            break;
            case 0:
            word_pad_hbits = 0;
            break;
            default:
            printf("ERROR:pint_pad_int_to_int_s flag_bit_h[0x%x] is error\n", flag_bit_h);
            break;
        }
    }

    for (ii=0; ii<cnt_in; ii++) {
        out[ii] = in0[ii];
        if __constexpr(!_nxz) {
            out[ii+cnt_out] = in0[ii+cnt_in];
        }
    }
    if (mod_in > 0) {
        out[cnt_in-1] |= (word_pad_lbits<<mod_in);
        if __constexpr(!_nxz) {
            out[cnt_in-1+cnt_out] |= (word_pad_hbits<<mod_in);
        }
    }

    for (ii=cnt_in; ii<cnt_out; ii++){
        out[ii] = word_pad_lbits;
        if __constexpr(!_nxz) {
            out[ii+cnt_out] = word_pad_hbits;
        }
    }
    if (mod_out > 0) {
        mask = ~(PINT_INT_MASK<<mod_out);
        out[cnt_out-1] &= mask;
        if __constexpr(!_nxz) {
            out[2*cnt_out-1] &= mask;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_pad_char_to_int_u(unsigned int* out, const unsigned char* in0, unsigned int size_in, unsigned int size_out){
    NCORE_PERF_MEASURE(pint_pad_char_to_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(26);
    unsigned int cnt,ii;
    unsigned char mask_l = (1<<size_in) - 1;
    unsigned char mask_h = 0;
    if __constexpr(!_nxz) {
        mask_h = mask_l<<PINT_CHAR_OFFSET;
    }

    cnt = ((size_out+0x1f) >> 5);

    out[0] = *in0&mask_l;
    if __constexpr(!_nxz) {
        out[cnt] = (*in0&mask_h) >> PINT_CHAR_OFFSET;
    }
    for(ii=1;ii<cnt;ii++){
        out[ii] = 0;
        if __constexpr(!_nxz) {
            out[ii+cnt] = 0;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_pad_int_to_int_u(unsigned int* out, const unsigned int* in0, unsigned int size_in, unsigned int size_out){
    NCORE_PERF_MEASURE(pint_pad_int_to_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(27);
    unsigned int ii;
    unsigned int cnt_out,cnt_in;

    cnt_out = ((size_out+0x1f) >> 5);
    cnt_in  = ((size_in+0x1f) >> 5);

    for(ii=0;ii<cnt_in;ii++){
        out[ii] = in0[ii];
        if __constexpr(!_nxz) {
            out[ii+cnt_out] = in0[ii+cnt_in];
        }
    }
    for(ii=cnt_in;ii<cnt_out;ii++){
        out[ii] = 0;
        if __constexpr(!_nxz) {
            out[ii+cnt_out] = 0;
        }
    }
}

//  Function: reduce the width of a signal
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduce_char_to_char(unsigned char* out, const unsigned char* in0, unsigned int size_in, unsigned int size_out){
    NCORE_PERF_MEASURE(pint_reduce_char_to_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(28);
    unsigned char mask = (1<<size_out) - 1;

    if __constexpr(!_nxz) {
        mask = ((mask << PINT_CHAR_OFFSET) | mask);
    }
    *out = *in0&mask;
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduce_int_to_char(unsigned char* out, const unsigned int* in0, unsigned int size_in, unsigned int size_out){
    NCORE_PERF_MEASURE(pint_reduce_int_to_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(29);
    pint_assert(size_out <= 4, "output size is not valid");
    unsigned int cnt_in;
    unsigned int mask;

    mask=(1<<size_out)-1;
    if __constexpr(_nxz) {
        *out = in0[0]&mask;
    } else {
        cnt_in = ((size_in+0x1f) >> 5);
        *out = (in0[0]&mask) | ((in0[cnt_in]&mask)<<PINT_CHAR_OFFSET);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduce_int_to_int(unsigned int* out, const unsigned int* in0, unsigned int size_in, unsigned int size_out){
    NCORE_PERF_MEASURE(pint_reduce_int_to_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(30);
    pint_assert(size_out <= size_in, "size is not valid");
    unsigned int cnt_out,cnt_in;
    unsigned int ii;
    unsigned int mask;
    unsigned int mod_out;

    cnt_out = ((size_out+0x1f) >> 5);
    cnt_in  = ((size_in+0x1f) >> 5);
    mod_out = size_out & 0x1f;

    for (ii = 0; ii < cnt_out; ii++) {
        out[ii] = in0[ii];
        if __constexpr(!_nxz) {
            out[ii+cnt_out] = in0[ii+cnt_in];
        }
    }

    if (mod_out > 0) {
        mask = ~(PINT_INT_MASK << mod_out);
        out[cnt_out - 1] &= mask;
        if __constexpr(!_nxz) {
            out[2*cnt_out - 1] &= mask;
        }
    }
}

//  Function: these functions are used in "if()" in verilog.
//            if *in0 is not 0,x,z,return true, else return false.
template<bool _nxz = false>
/*__constexpr*/ forceinline 
bool pint_is_true_char(const unsigned char *in0,unsigned int size){
    NCORE_PERF_MEASURE(pint_is_true_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(31);

    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        return *in0 > 0;
    }
    else{
    unsigned char h_in0, l_in0, r0,t0;
    t0 = (1<<size) - 1;
    h_in0 = (*in0>>PINT_CHAR_OFFSET);
   
    h_in0 = ~h_in0;
    l_in0 = *in0 & t0;
    r0 = h_in0 & l_in0;
    if(r0)
        return true;
    return false;
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
bool pint_is_true_int(const unsigned int *in0,unsigned int size){
    NCORE_PERF_MEASURE(pint_is_true_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(32);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        unsigned cnt = ((size+PINT_INT_SIZE-1)/PINT_INT_SIZE);
        for(int ii = 0; ii < cnt; ii++){
            if (in0[ii]){
                return true;
            }
        }
        return false;
    }
    else{
    unsigned int cnt, ii, h_in0, l_in0, r0,t0;
    t0 = 0xFFFFFFFF;
    cnt = ((size+PINT_INT_SIZE-1)/PINT_INT_SIZE);

    for(ii=0;ii<cnt;ii++)
    {
        h_in0 = ~in0[ii+cnt];
        l_in0 = in0[ii];
        r0 = h_in0 & l_in0;
        if(r0)
            return true;
    }
    return false;
}
}


/*__constexpr*/ forceinline 
bool pint_is_xz_char(unsigned char *in0,unsigned int size)
{
    NCORE_PERF_MEASURE(pint_is_xz_char, 2);
    NCORE_PERF_PINT_NET_SUMMARY(33);
    if(*in0 >= 16)return true;
    return false;
}

/*__constexpr*/ forceinline 
bool pint_is_xz_int(unsigned int *in0,unsigned int size)
{
    NCORE_PERF_MEASURE(pint_is_xz_int, 2);
    NCORE_PERF_PINT_NET_SUMMARY(34);
    unsigned int cnt,ii;
    cnt = ((size+PINT_INT_SIZE-1)/PINT_INT_SIZE);

    for(ii=0;ii<cnt;ii++){
        if(in0[ii+cnt])return true;
    }
    return false;
}

/************************************************** <Arithmetic Operator> **************************************************/
// Function: in0 + in1
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_add_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_add_char_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(35);

    unsigned char d0 = *in0;
    unsigned char d1 = *in1;
    unsigned char mask_l = (1 << size) -1;

    #ifdef IS_BINARY_DYNAMIC
    if(is_binary_sqing(in0,size) && is_binary_sqing(in1,size)) {
    #else
    if __constexpr(_nxz) {
    #endif
      *out = (d0 + d1) & mask_l;
    }
    else {
      if((d0 | d1) & 0xf0)
        *out = mask_l | (mask_l << 4);    // x
      else
        *out = (d0 + d1) & mask_l;
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_add_int_u(unsigned int *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_add_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(36);

    unsigned int cnt = (size + 0x1f) >> 5;
    unsigned int carry = 0;
    unsigned int ii;

    #ifdef IS_BINARY_DYNAMIC
    if(!is_binary_sqing(in0,size) || !is_binary_sqing(in1,size)) {
    #else
    if __constexpr(!_nxz) {
    #endif
      for(ii=0;ii<cnt;ii++){
          if(in0[ii+cnt] || in1[ii+cnt]){
              pint_set_int(out, size, BIT_X);
              return;
          }
      } 
    }

    for(ii=0;ii<cnt;ii++){
        out[ii] = add_with_carry(in0[ii], in1[ii], &carry);
        out[ii+cnt] = 0;
    }
    unsigned int mod = size & 0x1f;
    if(mod) out[cnt-1] &= ~( -1 << mod);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_add_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_add_char_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(37);
    pint_add_char_u<_nxz>(out, in0, in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_add_int_s (unsigned int  *out, const unsigned int  *in0, const unsigned int  *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_add_int_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(38);

    unsigned int cnt = (size + 0x1f) >> 5;
    unsigned int carry = 0;
    unsigned int ii;

    #ifdef IS_BINARY_DYNAMIC
    if(!is_binary_sqing(in0,size) || !is_binary_sqing(in1,size)) {
    #else
    if __constexpr(!_nxz) {
    #endif
      for(ii=0;ii<cnt;ii++){
          if(in0[ii+cnt] || in1[ii+cnt]){
              pint_set_int(out, size, BIT_X);
              return;
          }
      }
    }

    for(ii=0;ii<cnt;ii++){
        out[ii] = add_with_carry(in0[ii], in1[ii], &carry);
        out[ii+cnt] = 0;
    }
    unsigned int mod = size & 0x1f;
    if(mod) out[cnt-1] &= ~( -1 << mod);
}

// Function: in0 - in1
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_sub_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_sub_char_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(39);

    unsigned char d0 = *in0;
    unsigned char d1 = *in1;
    unsigned char mask_l = (1 << size) -1;

    #ifdef IS_BINARY_DYNAMIC
    if(is_binary_sqing(in0,size) && is_binary_sqing(in1,size)) {
    #else
    if __constexpr(_nxz) {
    #endif
        *out = (d0 - d1) & mask_l;      
    }
    else {
      if((d0 | d1) & 0xf0) {
          *out = mask_l | (mask_l << 4);    // x
      } 
      else {
          *out = (d0 - d1) & mask_l;
      } 
    }

}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_sub_int_u(unsigned int  *out, const unsigned int  *in0, const unsigned int  *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_sub_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(40);

    unsigned int cnt = (size + 0x1f) >> 5;
    unsigned int carry = 1;
    unsigned int ii;

    #ifdef IS_BINARY_DYNAMIC
    if(!is_binary_sqing(in0,size) || !is_binary_sqing(in1,size)) {
    #else
    if __constexpr(!_nxz) {
    #endif
      for(ii=0;ii<cnt;ii++){
          if(in0[ii+cnt]||in1[ii+cnt]){
              pint_set_int(out, size ,BIT_X);
              return;
          }
      }
    }
    for(ii=0;ii<cnt;ii++){
        out[ii] = add_with_carry(in0[ii], ~in1[ii], &carry);
        out[ii+cnt] = 0;
    }
    unsigned int mod = size & 0x1f;
    if(mod) out[cnt-1] &= ~( -1 << mod);
}

/*__constexpr*/ forceinline 
void pint_decrement_char_u(unsigned char *in0, unsigned char decrement_val, unsigned int size)
{
    NCORE_PERF_MEASURE(pint_decrement_char_u, 2);
    NCORE_PERF_PINT_NET_SUMMARY(41);
    unsigned char *out;
    pint_sub_char_u(out, in0, &decrement_val, size);
    *in0 = *out;
}

/*__constexpr*/ forceinline 
void pint_decrement_int_u(unsigned int *in0, unsigned decrement_val, unsigned int size)
{
    NCORE_PERF_MEASURE(pint_decrement_int_u, 2);
    NCORE_PERF_PINT_NET_SUMMARY(42);
    unsigned int *out;
    unsigned decrement_val_array[2] = {decrement_val, 0x0};
    pint_sub_int_u(out, in0, decrement_val_array, size);
    *in0 = *out;
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_sub_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_sub_char_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(43);
    pint_sub_char_u<_nxz>(out, in0, in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_sub_int_s (unsigned int  *out, const unsigned int  *in0, const unsigned int  *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_sub_int_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(44);
    pint_sub_int_u<_nxz>(out, in0, in1, size);
}

//  Function: out = in0 * in1
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_mul_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_mul_char_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(45);
    unsigned char d0 = *in0;
    unsigned char d1 = *in1;
    unsigned char mask_l = (1 << size) -1;

    #ifdef IS_BINARY_DYNAMIC
    if(is_binary_sqing(in0,size) && is_binary_sqing(in1,size)) {
    #else    
    if __constexpr(_nxz) {
    #endif
        *out = (d0 * d1) & mask_l;
    }
    else {
        if((d0 | d1) & 0xf0) {
            *out = mask_l | (mask_l << 4);      // x
        } 
        else {
            *out = (d0 * d1) & mask_l;
        } 
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_mul_int_u(unsigned int  *out, const unsigned int  *in0, const unsigned int  *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_mul_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(46);
    if(size <= 32){
        unsigned int mask = ~(0xfffffffe << (size -1));
        if(mask == 32)  mask = -1;
        #ifdef IS_BINARY_DYNAMIC
        if(is_binary_sqing(in0,size) && is_binary_sqing(in1,size)) {
        #else    
        if __constexpr(_nxz) {
        #endif
            out[0] = (in0[0] * in1[0]) & mask;
            // out[1] = 0;
        }else {
            if(in0[1] || in1[1]) {
                out[0] =mask;
                out[1] = mask;    // x
            } 
            else{
                out[0] = (in0[0] * in1[0]) & mask;
                out[1] = 0;
            }
        }
    }else{
        const int cnt = (size+0x1f) >> 5;
        memset(out, 0x00, cnt*sizeof(unsigned int)*2);

        unsigned int mask;
        unsigned int tail = size%PINT_INT_SIZE;
        if (tail) {
            mask = ~( -1UL << tail );
        } else {
            mask = ~0UL;
        }

        // Check for any XZ values ahead of time in a first pass. If
        // we find any, then force the entire result to be X and be
        // done.
        #ifdef IS_BINARY_DYNAMIC
        if(!is_binary_sqing(in0,size) || !is_binary_sqing(in1,size)) {
        #else
        if __constexpr(!_nxz) {
        #endif
            for (int idx = 0 ; idx < cnt ; idx += 1) {
                unsigned int lval = in0[cnt + idx];
                unsigned int rval = in1[cnt + idx];

                if (lval || rval) {
                    pint_set_int<_nxz>(out, size, BIT_X);
                    return;
                }
            }
        }

            
        // Calculate the result into a res array. We need to keep is
        // separate from the "this" array because we are making
        // multiple passes.
        for (int mul_a = 0 ; mul_a < cnt ; mul_a += 1) {
            unsigned int lval = in0[mul_a];

            for (int mul_b = 0 ; mul_b < (cnt-mul_a) ; mul_b += 1) {
                unsigned int rval = in1[mul_b];
                unsigned int sum;
                unsigned int tmp = multiply_with_carry(lval, rval, &sum);
                int base = mul_a + mul_b;
                unsigned int carry = 0;
                out[base] = add_with_carry(out[base], tmp, &carry);

                for (int add_idx = base+1 ; add_idx < cnt ; add_idx += 1) {
                    out[add_idx] = add_with_carry(out[add_idx], sum, &carry);
                    sum = 0;
                }
            }
        }
        out[cnt-1] = out[cnt-1] & mask;
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_mul_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_mul_char_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(47);
    pint_mul_char_u<_nxz>(out, in0, in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_mul_int_s(unsigned int  *out, const unsigned int  *in0, const unsigned int  *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_mul_int_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(48);
    pint_mul_int_u<_nxz>(out, in0, in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_div_mod_int_u(unsigned int *remainder, unsigned int *quotient, 
                        const unsigned int *divisor, const unsigned int *dividend, 
                        unsigned int size) 
{
    NCORE_PERF_MEASURE(pint_div_mod_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(49);
    const int divtmp_cnt = ((size+0x20) >> 5) << 1;
    const int divtmp_cnt_2 =  ((size+0x1f) >> 5) << 1;
    unsigned int * tmp_buffer = (unsigned int *)pint_mem_alloc(divtmp_cnt*sizeof(unsigned int)*6, SUB_T_CLEAR);
    memset(tmp_buffer, 0x00, divtmp_cnt*sizeof(unsigned int)*6);

    unsigned int * dividend_tmp = tmp_buffer;
    unsigned int * divisor_tmp  = tmp_buffer + divtmp_cnt;
    unsigned int * quotient_tmp = tmp_buffer + divtmp_cnt*2;
    unsigned int * divtmp       = tmp_buffer + divtmp_cnt*3;
    unsigned int * mask_tmp     = tmp_buffer + divtmp_cnt*4;
    unsigned int * inplace_tmp  = tmp_buffer + divtmp_cnt*5;

    pint_pad_int_to_int_u<_nxz>(dividend_tmp, dividend, size, size+1);
    pint_pad_int_to_int_u<_nxz>(divisor_tmp, divisor, size, size+1);
    pint_pad_int_to_int_u<_nxz>(divtmp, divisor, size, size+1);
    mask_tmp[0] = 1;  

    if(pint_logical_equality_int_ret<_nxz>(quotient_tmp, divisor_tmp, size+1)) {
        pint_set_int<_nxz>(remainder, size, BIT_X);
        pint_set_int<_nxz>(quotient, size, BIT_X);
#ifdef CPU_MODE
        free(tmp_buffer);
#endif
        return;
    }
    unsigned char result = 0;
    pint_less_than_int_u<_nxz>(&result, dividend_tmp, divisor_tmp, size+1); 
    if(result == BIT_1_CHAR) {
        pint_reduce_int_to_int<_nxz>(remainder, dividend_tmp, size+1, size);
        pint_reduce_int_to_int<_nxz>(quotient, quotient_tmp, size+1, size);
#ifdef CPU_MODE
        free(tmp_buffer);
#endif
        return;            
    }

    pint_less_than_int_u<_nxz>(&result, divtmp, dividend_tmp, size+1);
    while(result == BIT_1_CHAR) {
        pint_lshift_int<_nxz>(inplace_tmp, divtmp, 1, size+1);
        pint_copy_int<_nxz>(divtmp, inplace_tmp, size+1);
        pint_lshift_int<_nxz>(inplace_tmp, mask_tmp, 1, size+1);
        pint_copy_int<_nxz>(mask_tmp, inplace_tmp, size+1);
        pint_less_than_int_u<_nxz>(&result, divtmp, dividend_tmp, size+1);
    }

    pint_greater_than_oreq_int_u<_nxz>(&result, dividend_tmp, divisor_tmp, size+1);
    while(result == BIT_1_CHAR) {
        pint_less_than_oreq_int_u<_nxz>(&result, divtmp, dividend_tmp, size+1);
        if(result == BIT_1_CHAR) {
            pint_sub_int_u<_nxz>(inplace_tmp, dividend_tmp, divtmp, size+1);
            pint_copy_int<_nxz>(dividend_tmp, inplace_tmp, size+1);
            pint_add_int_u<_nxz>(inplace_tmp, quotient_tmp, mask_tmp, size+1);
            pint_copy_int<_nxz>(quotient_tmp, inplace_tmp, size+1);
        }

        pint_rshift_int_u<_nxz>(inplace_tmp, divtmp, 1, size+1);
        pint_copy_int<_nxz>(divtmp, inplace_tmp, size+1);
        pint_rshift_int_u<_nxz>(inplace_tmp, mask_tmp, 1, size+1);
        pint_copy_int<_nxz>(mask_tmp, inplace_tmp, size+1);
        pint_greater_than_oreq_int_u<_nxz>(&result, dividend_tmp, divisor_tmp, size+1);             
    }
    pint_reduce_int_to_int<_nxz>(quotient, quotient_tmp, size+1, size);
    pint_reduce_int_to_int<_nxz>(remainder, dividend_tmp, size+1, size);

#ifdef CPU_MODE
    free(tmp_buffer);
#endif
}


//  Function: out = in0 / in1
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_divided_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_divided_char_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(50);


    unsigned char d0 = *in0;
    unsigned char d1 = *in1;
    unsigned char mask_l = (1 << size) -1;

    #ifdef IS_BINARY_DYNAMIC
    if(is_binary_sqing(in0,size) && is_binary_sqing(in1,size)) {
    #else    
    if __constexpr(_nxz) {
    #endif
        if(!d1)
            *out = mask_l | (mask_l << 4);      // x(when d1 = 0)
        else
            *out = (d0 / d1) & mask_l;
    }
    else {
        if((d0 | d1) & 0xf0)
            *out = mask_l | (mask_l << 4);      // x
        else if(!d1)
            *out = mask_l | (mask_l << 4);      // x(when d1 = 0)
        else
            *out = (d0 / d1) & mask_l;
    }   


}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_divided_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_divided_char_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(51);
    char d0 = *in0;
    char d1 = *in1;
    unsigned char mask_l = (1 << size) -1;
    #ifdef IS_BINARY_DYNAMIC
    if(is_binary_sqing(in0,size) && is_binary_sqing(in1,size)) {
    #else    
    if __constexpr(_nxz) {
    #endif
        if(!d1)
            *out = mask_l | (mask_l << 4);      // x(when d1 = 0)
        else{
            char ret = (char)(d0 << (8 - size)) / (char)(d1 << (8 - size));
            *out = ret & mask_l;
        }    
    }
    else {
        if((d0 | d1) & 0xf0)    
            *out = mask_l | (mask_l << 4);      // x
        else if(!d1)            
           *out = mask_l | (mask_l << 4);      // x(when d1 = 0)
        else{
            char ret = (char)(d0 << (8 - size)) / (char)(d1 << (8 - size));
            *out = ret & mask_l;
        }
    }

}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_divided_int_u (unsigned int  *quotient, const unsigned int  *dividend,
                         const unsigned int  *divisor, unsigned int size){
    NCORE_PERF_MEASURE(pint_divided_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(52);

    if(size <= 32){
        unsigned int mask = ~(0xfffffffe << (size -1));
        #ifdef IS_BINARY_DYNAMIC
        if(is_binary_sqing(dividend,size) && is_binary_sqing(divisor,size)) {
        #else    
        if __constexpr(_nxz) {
        #endif
            if(!divisor[0]) {
                quotient[0] = mask;
                quotient[1] = mask;   // x(when d1 = 0)
            }
            else{
                quotient[0] = (dividend[0] / divisor[0]) & mask;
                quotient[1] = 0;
            }
        }
        else {
            if(dividend[1] || divisor[1]) {
                quotient[0] = mask;
                quotient[1] = mask;   // x
            }
            else if(!divisor[0]) {
                quotient[0] = mask;
                quotient[1] = mask;   // x(when d1 = 0)
            }
            else{
                quotient[0] = (dividend[0] / divisor[0]) & mask;
                quotient[1] = 0;
            }
        }


    }else{
        const int divtmp_cnt = ((size+0x1f) >> 5) << 1;
        const int cnt = divtmp_cnt >> 1;
        unsigned int * remainder_tmp = (unsigned int *)pint_mem_alloc(divtmp_cnt*sizeof(unsigned int), SUB_T_CLEAR);

        // Check for any XZ values ahead of time in a first pass. If
        // we find any, then force the entire result to be X and be
        // done.
        #ifdef IS_BINARY_DYNAMIC
        if(!is_binary_sqing(dividend,size) || !is_binary_sqing(divisor,size)) {
        #else
        if __constexpr(!_nxz) {
        #endif
            for (int idx = 0 ; idx < cnt; idx += 1) {
                unsigned int lval = dividend[cnt + idx];
                unsigned int rval = divisor[cnt + idx];

                if (lval || rval) {
                    pint_set_int<_nxz>(quotient, size, BIT_X);
#ifdef CPU_MODE
                    free(remainder_tmp);
#endif
                    return;
                }
            }
        }
           
        pint_div_mod_int_u<_nxz>(remainder_tmp, quotient, divisor, dividend, size);
#ifdef CPU_MODE
        free(remainder_tmp);
#endif
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_divided_int_s (unsigned int  *quotient, const unsigned int  *dividend, 
                         const unsigned int  *divisor, unsigned int size){
    NCORE_PERF_MEASURE(pint_divided_int_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(53);
    if(size <= 32){
      unsigned int mask = ~(0xfffffffe << (size -1));
      #ifdef IS_BINARY_DYNAMIC
      if(is_binary_sqing(dividend,size) && is_binary_sqing(divisor,size)) {
      #else
      if __constexpr(_nxz) {
      #endif
          if(!divisor[0]) {
              quotient[0] = mask;
              quotient[1] = mask;   // x(when d1 = 0)
          }
          else{
              int d0 = dividend[0] << (32 - size);
              int d1 = divisor[0] << (32 - size);
              long int l0 = d0;
              long int l1 = d1;
              quotient[0] = (l0 / l1) & mask;
              quotient[1] = 0;
          }
      }
      else {
          if(dividend[1] || divisor[1]) {
              quotient[0] = mask;
              quotient[1] = mask;   // x
          }
          else if(!divisor[0]) {
              quotient[0] = mask;
              quotient[1] = mask;   // x(when d1 = 0)
          }
          else{
              int d0 = dividend[0] << (32 - size);
              int d1 = divisor[0] << (32 - size);
              long int l0 = d0;
              long int l1 = d1;
              quotient[0] = (l0 / l1) & mask;
              quotient[1] = 0;
          }
      }
    }else{
        const int divtmp_cnt =  ((size+0x1f) >> 5) << 1;
        const int cnt = divtmp_cnt >> 1;
        unsigned int * tmp_buffer = (unsigned int *)pint_mem_alloc(divtmp_cnt*sizeof(unsigned int)*4, SUB_T_CLEAR);

        unsigned int * remainder_tmp = tmp_buffer;
        unsigned int * inplace_tmp   = tmp_buffer + divtmp_cnt;
        unsigned int * dividend_tmp  = tmp_buffer + 2*divtmp_cnt;
        unsigned int * divisor_tmp   = tmp_buffer + 3*divtmp_cnt;
        pint_copy_int<_nxz>(dividend_tmp, dividend, size);
        pint_copy_int<_nxz>(divisor_tmp, divisor, size);        

        // Check for any XZ values ahead of time in a first pass. If
        // we find any, then force the entire result to be X and be
        // done.
        #ifdef IS_BINARY_DYNAMIC
        if(!is_binary_sqing(dividend,size) || !is_binary_sqing(divisor,size)) {
        #else
        if __constexpr(!_nxz) {
        #endif
            for (int idx = 0 ; idx < cnt; idx += 1) {
                unsigned int lval = dividend_tmp[cnt + idx];
                unsigned int rval = divisor_tmp[cnt + idx];

                if (lval || rval) {
                    pint_set_int<_nxz>(quotient, size, BIT_X);
#ifdef CPU_MODE
                    free(tmp_buffer);
#endif
                    return;
                }
            }
        }

        bool negate = false;
        unsigned int idx = size - 1;
        unsigned int addr = idx >> 5;  //addr = idx/PINT_INT_SIZE
        unsigned int mask = idx & 0x1f; //mask = idx%PINT_INT_SIZE

        if(dividend_tmp[addr] & (1UL << mask)) {
            pint_minus_int<_nxz>(inplace_tmp, dividend_tmp, size);
            pint_copy_int<_nxz>(dividend_tmp, inplace_tmp, size);
            negate = true;
        }
        if(divisor_tmp[addr] & (1UL << mask)) {
            pint_minus_int<_nxz>(inplace_tmp, divisor_tmp, size);
            pint_copy_int<_nxz>(divisor_tmp, inplace_tmp, size);
            negate = !negate;
        }        

        pint_div_mod_int_u<_nxz>(remainder_tmp, quotient, divisor_tmp, dividend_tmp, size);

        if(negate) {
            pint_minus_int<_nxz>(inplace_tmp, quotient, size);
            pint_copy_int<_nxz>(quotient, inplace_tmp, size);          
        }

#ifdef CPU_MODE
        free(tmp_buffer);
#endif
    }
}


//  Function: out = in0 % in1
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_modulob_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_modulob_char_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(54);
    unsigned char d0 = *in0;
    unsigned char d1 = *in1;
    unsigned char mask_l = (1 << size) -1;

    #ifdef IS_BINARY_DYNAMIC
    if(is_binary_sqing(in0,size) && is_binary_sqing(in1,size)) {
    #else
    if __constexpr(_nxz) {
    #endif
        if(!d1)
            *out = mask_l | (mask_l << 4);      // x(when d1 = 0)
        else
            *out = (d0 % d1) & mask_l;
    }
    else {
        if((d0 | d1) & 0xf0)
            *out = mask_l | (mask_l << 4);      // x
        else if(!d1)
            *out = mask_l | (mask_l << 4);      // x(when d1 = 0)
        else
            *out = (d0 % d1) & mask_l;
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_modulob_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_modulob_char_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(55);
    char d0 = *in0;
    char d1 = *in1;
    unsigned char mask_l = (1 << size) -1;
    #ifdef IS_BINARY_DYNAMIC
    if(is_binary_sqing(in0,size) && is_binary_sqing(in1,size)) {
    #else
    if __constexpr(_nxz) {
    #endif
        if(!d1)
            *out = mask_l | (mask_l << 4);      // x(when d1 = 0)
        else{
            char ret = (char)(d0 << (8 - size)) % (char)(d1 << (8 - size));
            *out = (unsigned char)ret >> (8 - size);
        }
    }
    else {
        if((d0 | d1) & 0xf0)
            *out = mask_l | (mask_l << 4);      // x
        else if(!d1)
            *out = mask_l | (mask_l << 4);      // x(when d1 = 0)
        else{
            char ret = (char)(d0 << (8 - size)) % (char)(d1 << (8 - size));
            *out = (unsigned char)ret >> (8 - size);
        }

    }

}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_modulob_int_u (unsigned int  *remainder, const unsigned int  *dividend,
                         const unsigned int  *divisor, unsigned int size){
    NCORE_PERF_MEASURE(pint_modulob_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(56);
    if(size <= 32){
        unsigned int mask = ~(0xfffffffe << (size -1));
        #ifdef IS_BINARY_DYNAMIC
        if(is_binary_sqing(dividend,size) && is_binary_sqing(divisor,size)) {
        #else
        if __constexpr(_nxz) {
        #endif
            if(!divisor[0]){
                remainder[0] = mask;
                remainder[1] = mask;   // x(when d1 = 0)
            }
            else{
                remainder[0] = (dividend[0] % divisor[0]) & mask;
                remainder[1] = 0;
            }
        }
        else {
            if(dividend[1] || divisor[1]){
                remainder[0] = mask;
                remainder[1] = mask;   // x
            } 
            else if(!divisor[0]){
                remainder[0] = mask;
                remainder[1] = mask;   // x(when d1 = 0)
            } 
            else{
                remainder[0] = (dividend[0] % divisor[0]) & mask;
                remainder[1] = 0;
            }
        }


    }else{
        // memset(remainder, 0x00, cnt_debug*sizeof(unsigned int)*2);

        const int divtmp_cnt =  ((size+0x1f) >> 5) << 1;
        const int cnt = divtmp_cnt >> 1;
        unsigned int * quotient_tmp = (unsigned int *)pint_mem_alloc(divtmp_cnt*sizeof(unsigned int), SUB_T_CLEAR);
        // Check for any XZ values ahead of time in a first pass. If
        // we find any, then force the entire result to be X and be
        // done.
        #ifdef IS_BINARY_DYNAMIC
        if(!is_binary_sqing(dividend,size) || !is_binary_sqing(divisor,size)) {
        #else
        if __constexpr(!_nxz) {
        #endif
            for (int idx = 0 ; idx < cnt; idx += 1) {
                unsigned int lval = dividend[cnt + idx];
                unsigned int rval = divisor[cnt + idx];

                if (lval || rval) {
                    pint_set_int<_nxz>(remainder, size, BIT_X);
#ifdef CPU_MODE
                    free(quotient_tmp);
#endif
                    return;
                }
            }

        }

        pint_div_mod_int_u<_nxz>(remainder, quotient_tmp, divisor, dividend, size);

#ifdef CPU_MODE
        free(quotient_tmp);
#endif
    }

}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_modulob_int_s (unsigned int  *remainder, const unsigned int  *dividend, 
                         const unsigned int  *divisor, unsigned int size){
    NCORE_PERF_MEASURE(pint_modulob_int_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(57);

    if(size <= 32){
        unsigned int mask = ~(0xfffffffe << (size -1));
        #ifdef IS_BINARY_DYNAMIC
        if(is_binary_sqing(dividend,size) && is_binary_sqing(divisor,size)) {
        #else
        if __constexpr(_nxz) {
        #endif
            if(!divisor[0]){
                remainder[0] = mask, remainder[1] = mask;   // x(when d1 = 0)
            } 
            else{
                int d0 = dividend[0] << (32 - size);
                int d1 = divisor[0] << (32 - size);
                long int l0 = d0;
                long int l1 = d1;
                remainder[0] = (unsigned int)(l0 % l1) >> (32 - size);  
                remainder[1] = 0;
            }
        }
        else {
            if(dividend[1] || divisor[1]){
                remainder[0] = mask, remainder[1] = mask;   // x
            } 
            else if(!divisor[0]) {
                remainder[0] = mask, remainder[1] = mask;   // x(when d1 = 0)
            } 
            else{
                int d0 = dividend[0] << (32 - size);
                int d1 = divisor[0] << (32 - size);
                long int l0 = d0;
                long int l1 = d1;
                remainder[0] = (unsigned int)(l0 % l1) >> (32 - size); 
                remainder[1] = 0;
            }
        }
    }else{
        const int divtmp_cnt =  ((size+0x1f) >> 5) << 1;
        const int cnt = divtmp_cnt >> 1;
        unsigned int * tmp_buffer = (unsigned int *)pint_mem_alloc(divtmp_cnt*sizeof(unsigned int)*4, SUB_T_CLEAR);
        // DBG_print_int_array(dividend, divtmp_cnt);
        // DBG_print_int_array(divisor, divtmp_cnt);

        unsigned int * quotient_tmp = tmp_buffer;
        unsigned int * inplace_tmp  = tmp_buffer + divtmp_cnt;
        unsigned int * dividend_tmp = tmp_buffer + 2*divtmp_cnt;
        unsigned int * divisor_tmp  = tmp_buffer + 3*divtmp_cnt;
        pint_copy_int<_nxz>(dividend_tmp, dividend, size);
        pint_copy_int<_nxz>(divisor_tmp, divisor, size);


        // Check for any XZ values ahead of time in a first pass. If
        // we find any, then force the entire result to be X and be
        // done.
        #ifdef IS_BINARY_DYNAMIC
        if(!is_binary_sqing(dividend,size) || !is_binary_sqing(divisor,size)) {
        #else
        if __constexpr(!_nxz) {
        #endif
            for (int idx = 0 ; idx < cnt; idx += 1) {
                unsigned int lval = dividend_tmp[cnt + idx];
                unsigned int rval = divisor_tmp[cnt + idx];

                if (lval || rval) {
                    pint_set_int<_nxz>(remainder, size, BIT_X);
    #ifdef CPU_MODE
                    free(tmp_buffer);
    #endif
                    return;
                }
            }         
        }

        bool negate = false;
        unsigned int idx = size - 1;
        unsigned int addr = idx >> 5;  //addr = idx/PINT_INT_SIZE
        unsigned int mask = idx & 0x1f; //mask = idx%PINT_INT_SIZE

        if(dividend_tmp[addr] & (1UL << mask)) {
            pint_minus_int<_nxz>(inplace_tmp, dividend_tmp, size);
            pint_copy_int<_nxz>(dividend_tmp, inplace_tmp, size);
            negate = true;
        }
        if(divisor_tmp[addr] & (1UL << mask)) {
            pint_minus_int<_nxz>(inplace_tmp, divisor_tmp, size);
            pint_copy_int<_nxz>(divisor_tmp, inplace_tmp, size);
        }        

        pint_div_mod_int_u<_nxz>(remainder, quotient_tmp, divisor_tmp, dividend_tmp, size);

        if(negate) {
            pint_minus_int<_nxz>(inplace_tmp, remainder, size);
            pint_copy_int<_nxz>(remainder, inplace_tmp, size);          
        }
#ifdef CPU_MODE
        free(tmp_buffer);
#endif
    } 
}

#if 1  // nxz
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
    NCORE_PERF_MEASURE(pint_power_u_common<_nxz>, 2);
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

// Function: out = -in0
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_minus_char(unsigned char *out, const unsigned char *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_minus_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(62);
    char d0 = *in0;
    unsigned char mask_l = (1 << size) -1;
    if __constexpr(_nxz) {
        *out = (~d0 + 0x01) & mask_l;
    }
    else {
        if(d0 & 0xf0) {
            *out = mask_l | (mask_l << 4);      // x
        }
        else {
            *out = (~d0 + 0x01) & mask_l;
        }
    }



}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_minus_int(unsigned int * out, const unsigned int *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_minus_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(63);
    if(size <= 32){
        unsigned int mask = ~(0xfffffffe << (size -1));
        if __constexpr(_nxz) {
            out[0] = (~(*in0) + 0x01) & mask;
            out[1] = 0x00;          
        }
        else {
            if(in0[1]){
                out[0] = mask;
                out[1] = mask;   // x
            }
            else{
                out[0] = (~(*in0) + 0x01) & mask;
                out[1] = 0x00;
            }
        }
    }else{
        const int cnt = (size+0x1f) >> 5;
        unsigned int * tmp_buf = (unsigned int *)pint_mem_alloc(cnt*sizeof(unsigned int)*4, SUB_T_CLEAR);
        memset(tmp_buf, 0x00, cnt*sizeof(unsigned int)*4);
        
        unsigned int * invert_out_tmp = (unsigned int *)&tmp_buf[0];
        unsigned int * add_tmp = (unsigned int *)&tmp_buf[2*cnt];
        add_tmp[0] = 0x01;
        pint_bitw_invert_int<_nxz>(invert_out_tmp, (unsigned int *)in0, size);
        pint_add_int_s<_nxz>((unsigned int *)out, invert_out_tmp, add_tmp, size);

#ifdef CPU_MODE
        free(tmp_buf);
#endif
    }
}

/************************************************** < End > **************************************************/

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_greater_than_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_greater_than_char_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(64);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned l_shift = 8 - size;
        unsigned char in0_pro = *in0 << l_shift;
        unsigned char in1_pro = *in1 << l_shift;
        *out = (char)in0_pro > (char)in1_pro;
    }
    else{
    unsigned int result;
    result = pint_compare_char_s(in0, in1,size);
    switch(result){
        case PINT_COMPARE_BIGGER:
        *out = BIT_1_CHAR;
        break;
        case PINT_COMPARE_X:
        *out = BIT_X_CHAR;
        break;
        default:
        *out = BIT_0_CHAR;
        break;
    }
}
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_greater_than_int_s(unsigned char *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_greater_than_int_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(65);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned cnt = (size + 31) / 32;
        unsigned highest_lshift = (32 - (size & 0x1f)) & 0x1f;
        unsigned highest_in0 = in0[cnt - 1] << highest_lshift;
        unsigned highest_in1 = in1[cnt - 1] << highest_lshift;

        if ((int)highest_in0 > (int)highest_in1){
            *out =  BIT_1_CHAR;
        }
        else if ((int)highest_in0 < (int)highest_in1){
            *out = BIT_0_CHAR;
        }
        else{
            for (int i = cnt - 2; i >= 0; i--){
                if (in0[i] > in1[i]){
                    *out =  BIT_1_CHAR;
                    return;
                }
                else if (in0[i] < in1[i]){
                    *out =  BIT_0_CHAR;
                    return;
                }
                else{
                }
            }
            *out =  BIT_0_CHAR;
        }
    }
    else{
    unsigned int result;
    result = pint_compare_int_s(in0, in1,size);
    switch(result){
        case PINT_COMPARE_BIGGER:
        *out = BIT_1_CHAR;
        break;
        case PINT_COMPARE_X:
        *out = BIT_X_CHAR;
        break;
        default:
        *out = BIT_0_CHAR;
        break;
    }
}
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_greater_than_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_greater_than_char_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(66);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        *out = *in0 > *in1;
    }
    else{
    unsigned int result;
    result = pint_compare_char_u(in0, in1,size);
    switch(result){
        case PINT_COMPARE_BIGGER:
        *out = BIT_1_CHAR;
        break;
        case PINT_COMPARE_X:
        *out = BIT_X_CHAR;
        break;
        default:
        *out = BIT_0_CHAR;
        break;
    }
}
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_greater_than_int_u(unsigned char *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_greater_than_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(67);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned cnt = (size + 31) / 32;
        for (int i = cnt - 1; i >= 0; i--){
            if (in0[i] > in1[i]){
                *out =  BIT_1_CHAR;
                return;
            }
            else if (in0[i] < in1[i]){
                *out =  BIT_0_CHAR;
                return;
            }
            else{
            }
        }
        *out =  BIT_0_CHAR;
    }
    else{
    unsigned int result;
    result = pint_compare_int_u(in0, in1,size);
    switch(result){
        case PINT_COMPARE_BIGGER:
        *out = BIT_1_CHAR;
        break;
        case PINT_COMPARE_X:
        *out = BIT_X_CHAR;
        break;
        default:
        *out = BIT_0_CHAR;
        break;
    }
}
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_greater_than_oreq_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_greater_than_oreq_char_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(68);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned l_shift = 8 - size;
        unsigned char in0_pro = *in0 << l_shift;
        unsigned char in1_pro = *in1 << l_shift;
        *out = (char)in0_pro >= (char)in1_pro;
    }
    else{
    unsigned int result;
    result = pint_compare_char_s(in0, in1,size);
    switch(result){
        case PINT_COMPARE_BIGGER:
        case PINT_COMPARE_EQUAL:
        *out = BIT_1_CHAR;
        break;
        case PINT_COMPARE_X:
        *out = BIT_X_CHAR;
        break;
        default:
        *out = BIT_0_CHAR;
        break;
    }
}
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_greater_than_oreq_int_s(unsigned char *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_greater_than_oreq_int_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(69);

    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned cnt = (size + 31) / 32;
        unsigned highest_lshift = (32 - (size & 0x1f)) & 0x1f;
        unsigned highest_in0 = in0[cnt - 1] << highest_lshift;
        unsigned highest_in1 = in1[cnt - 1] << highest_lshift;

        if ((int)highest_in0 > (int)highest_in1){
            *out =  BIT_1_CHAR;
        }
        else if ((int)highest_in0 < (int)highest_in1){
            *out = BIT_0_CHAR;
        }
        else{
            for (int i = cnt - 2; i >= 0; i--){
                if (in0[i] > in1[i]){
                    *out =  BIT_1_CHAR;
                    return;
                }
                else if (in0[i] < in1[i]){
                    *out =  BIT_0_CHAR;
                    return;
                }
                else{
                }
            }
            *out =  BIT_1_CHAR;
        }
    }
    else{
    unsigned int result;
    result = pint_compare_int_s(in0, in1,size);
    switch(result){
        case PINT_COMPARE_BIGGER:
        case PINT_COMPARE_EQUAL:
        *out = BIT_1_CHAR;
        break;
        case PINT_COMPARE_X:
        *out = BIT_X_CHAR;
        break;
        default:
        *out = BIT_0_CHAR;
        break;
    }
}
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_greater_than_oreq_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_greater_than_oreq_char_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(70);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        *out = *in0 >= *in1;
    }
    else{
    unsigned int result;
    result = pint_compare_char_u(in0, in1,size);
    switch(result){
        case PINT_COMPARE_BIGGER:
        case PINT_COMPARE_EQUAL:
        *out = BIT_1_CHAR;
        break;
        case PINT_COMPARE_X:
        *out = BIT_X_CHAR;
        break;
        default:
        *out = BIT_0_CHAR;
        break;
    }
}
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_greater_than_oreq_int_u(unsigned char *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_greater_than_oreq_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(71);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned cnt = (size + 31) / 32;

        for (int i = cnt - 1; i >= 0; i--){
            if (in0[i] > in1[i]){
                *out =  BIT_1_CHAR;
                return;
            }
            else if (in0[i] < in1[i]){
                *out =  BIT_0_CHAR;
                return;
            }
            else{
            }
        }
        *out =  BIT_1_CHAR;
    }
    else{
        unsigned int result;
    result = pint_compare_int_u(in0, in1,size);
    switch(result){
        case PINT_COMPARE_BIGGER:
        case PINT_COMPARE_EQUAL:
        *out = BIT_1_CHAR;
        break;
        case PINT_COMPARE_X:
        *out = BIT_X_CHAR;
        break;
        default:
        *out = BIT_0_CHAR;
        break;
    }
}
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_less_than_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_less_than_char_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(72);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned l_shift = 8 - size;
        unsigned char in0_pro = *in0 << l_shift;
        unsigned char in1_pro = *in1 << l_shift;
        *out = (char)in0_pro < (char)in1_pro;
    }
    else{
    unsigned int result;
    result = pint_compare_char_s(in0, in1,size);
    switch(result){
        case PINT_COMPARE_BIGGER:
        case PINT_COMPARE_EQUAL:
        *out = BIT_0_CHAR;
        break;
        case PINT_COMPARE_X:
        *out = BIT_X_CHAR;
        break;
        default:
        *out = BIT_1_CHAR;
        break;
    }
}
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_less_than_int_s(unsigned char *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_less_than_int_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(73);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned cnt = (size + 31) / 32;
        unsigned highest_lshift = (32 - (size & 0x1f)) & 0x1f;
        unsigned highest_in0 = in0[cnt - 1] << highest_lshift;
        unsigned highest_in1 = in1[cnt - 1] << highest_lshift;

        if ((int)highest_in0 < (int)highest_in1){
            *out =  BIT_1_CHAR;
        }
        else if ((int)highest_in0 > (int)highest_in1){
            *out = BIT_0_CHAR;
        }
        else{
            for (int i = cnt - 2; i >= 0; i--){
                if (in0[i] < in1[i]){
                    *out =  BIT_1_CHAR;
                    return;
                }
                else if (in0[i] > in1[i]){
                    *out =  BIT_0_CHAR;
                    return;
                }
                else{
                }
            }
            *out =  BIT_0_CHAR;
        }
    }
    else{
        unsigned int result;
        result = pint_compare_int_s(in0, in1,size);
        switch(result){
            case PINT_COMPARE_BIGGER:
            case PINT_COMPARE_EQUAL:
            *out = BIT_0_CHAR;
            break;
            case PINT_COMPARE_X:
            *out = BIT_X_CHAR;
            break;
            default:
            *out = BIT_1_CHAR;
            break;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_less_than_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_less_than_char_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(74);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        *out = *in0 < *in1;
    }
    else{
        unsigned int result;
        result = pint_compare_char_u(in0, in1,size);
        switch(result){
            case PINT_COMPARE_BIGGER:
            case PINT_COMPARE_EQUAL:
            *out = BIT_0_CHAR;
            break;
            case PINT_COMPARE_X:
            *out = BIT_X_CHAR;
            break;
            default:
            *out = BIT_1_CHAR;
            break;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_less_than_int_u(unsigned char *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_less_than_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(75);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned cnt = (size + 31) / 32;

        for (int i = cnt - 1; i >= 0; i--){
            if (in0[i] < in1[i]){
                *out =  BIT_1_CHAR;
                return;
            }
            else if (in0[i] > in1[i]){
                *out =  BIT_0_CHAR;
                return;
            }
            else{
            }
        }
        *out =  BIT_0_CHAR;
    }
    else{
        unsigned int result;
        result = pint_compare_int_u(in0, in1,size);
        switch(result){
            case PINT_COMPARE_BIGGER:
            case PINT_COMPARE_EQUAL:
            *out = BIT_0_CHAR;
            break;
            case PINT_COMPARE_X:
            *out = BIT_X_CHAR;
            break;
            default:
            *out = BIT_1_CHAR;
            break;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_less_than_oreq_char_s(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_less_than_oreq_char_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(76);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned l_shift = 8 - size;
        unsigned char in0_pro = *in0 << l_shift;
        unsigned char in1_pro = *in1 << l_shift;
        *out = (char)in0_pro <= (char)in1_pro;
    }
    else{
        unsigned int result;
        result = pint_compare_char_s(in0, in1,size);
        switch(result){
            case PINT_COMPARE_BIGGER:
            *out = BIT_0_CHAR;
            break;
            case PINT_COMPARE_X:
            *out = BIT_X_CHAR;
            break;
            default:
            *out = BIT_1_CHAR;
            break;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_less_than_oreq_int_s(unsigned char *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_less_than_oreq_int_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(77);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned cnt = (size + 31) / 32;
        unsigned highest_lshift = (32 - (size & 0x1f)) & 0x1f;
        unsigned highest_in0 = in0[cnt - 1] << highest_lshift;
        unsigned highest_in1 = in1[cnt - 1] << highest_lshift;

        if ((int)highest_in0 < (int)highest_in1){
            *out =  BIT_1_CHAR;
        }
        else if ((int)highest_in0 > (int)highest_in1){
            *out = BIT_0_CHAR;
        }
        else{
            for (int i = cnt - 2; i >= 0; i--){
                if (in0[i] < in1[i]){
                    *out =  BIT_1_CHAR;
                    return;
                }
                else if (in0[i] > in1[i]){
                    *out =  BIT_0_CHAR;
                    return;
                }
                else{
                }
            }
            *out =  BIT_1_CHAR;
        }
    }
    else{
        unsigned int result;
        result = pint_compare_int_s(in0, in1,size);
        switch(result){
            case PINT_COMPARE_BIGGER:
            *out = BIT_0_CHAR;
            break;
            case PINT_COMPARE_X:
            *out = BIT_X_CHAR;
            break;
            default:
            *out = BIT_1_CHAR;
            break;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_less_than_oreq_char_u(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_less_than_oreq_char_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(78);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        *out = *in0 <= *in1;
    }
    else{
        unsigned int result;
        result = pint_compare_char_u(in0, in1,size);
        switch(result){
            case PINT_COMPARE_BIGGER:
            *out = BIT_0_CHAR;
            break;
            case PINT_COMPARE_X:
            *out = BIT_X_CHAR;
            break;
            default:
            *out = BIT_1_CHAR;
            break;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_less_than_oreq_int_u(unsigned char *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_less_than_oreq_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(79);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned cnt = (size + 31) / 32;

        for (int i = cnt - 1; i >= 0; i--){
            if (in0[i] < in1[i]){
                *out =  BIT_1_CHAR;
                return;
            }
            else if (in0[i] > in1[i]){
                *out =  BIT_0_CHAR;
                return;
            }
            else{
            }
        }
        *out =  BIT_1_CHAR;
    }
    else{
        unsigned int result;
        result = pint_compare_int_u(in0, in1,size);
        switch(result){
            case PINT_COMPARE_BIGGER:
            *out = BIT_0_CHAR;
            break;
            case PINT_COMPARE_X:
            *out = BIT_X_CHAR;
            break;
            default:
            *out = BIT_1_CHAR;
            break;
        }
    }
}

/*void pint_logical_invert_char(unsigned char *out, unsigned char *in0, unsigned int size){
    if(*in0>>PINT_CHAR_OFFSET){
        *out=BIT_X_CHAR;
        return;
    }
    if(*in0){
        *out=0;
        return;
    }
    *out=BIT_1_CHAR;
}

void pint_logical_invert_int(unsigned char *out, unsigned int *in0, unsigned int size){
    unsigned int cnt,ii;
    
    cnt = ((size+PINT_INT_SIZE-1)/PINT_INT_SIZE);
    
    for(ii=0;ii<cnt;ii++){
        if(in0[ii+cnt]){
            *out=BIT_X_CHAR;
            return;
        }
    }
    for(ii=0;ii<cnt;ii++){
        if(in0[ii]){
            *out=BIT_0_CHAR;
            return;
        }
    }
    *out=BIT_1_CHAR;
}*/

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_logical_invert_char(unsigned char *out, const unsigned char *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_logical_invert_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(80);

    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        *out = (*in0 == 0);
    }
    else{
        unsigned char h_in0, l_in0, r0,t0;
        h_in0 = *in0>>PINT_CHAR_OFFSET;
        t0 = (1<<size) - 1;

        //h_in0 = h_in0 ^ t0;
        h_in0 = ~h_in0;
        l_in0 = *in0 & t0;
        r0 = h_in0 & l_in0;

        if(r0){
            *out=BIT_0_CHAR;
            return;
        }
        if(*in0==0){
            *out=BIT_1_CHAR;
            return;
        }

        *out=BIT_X_CHAR;
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_logical_invert_int(unsigned char *out, const unsigned int *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_logical_invert_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(81);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        unsigned int cnt = ((size+PINT_INT_SIZE-1)/PINT_INT_SIZE);

        for(int ii = 0; ii < cnt; ii++){
            if (in0[ii] > 0){
                *out = BIT_0_CHAR;
                return;
            }
        }
        *out = BIT_1_CHAR;
    }
    else{
        unsigned int cnt,ii,low,high,t0,r0,sum;
        
        cnt = ((size+PINT_INT_SIZE-1)/PINT_INT_SIZE);
        t0=0xFFFFFFFF;
        sum = 0;
        char flag_x = false;
        for(ii=0;ii<cnt;ii++)
        {
            low = in0[ii];
            high = in0[ii+cnt];
            if(high>0)
                flag_x = true;
            high = ~high;
            r0= high & low;
            //pint_printf("high=%d,low=%d,r0=%d\n",high,low,r0);
            sum += r0;
            if(r0)
            {
                *out=BIT_0_CHAR;
                return;
            }
        }

        //pint_printf("high=%d,low=%d,sum=%d\n",high,low,sum);
        if(flag_x)
        {
            *out=BIT_X_CHAR;
            return;
        }
        *out=BIT_1_CHAR;
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_logical_and_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_logical_and_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(82);

    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        *out = *in0 && *in1;
    }
    else{
        if (size == 1){
            unsigned char data0 = *in0;
            unsigned char data1 = *in1;
            *out = ((data0 && data1) == 0) ? BIT_0_CHAR : (((data0 == 1) && (data1 == 1)) ? BIT_1_CHAR : BIT_X_CHAR);
        }
        else{
            unsigned char h_in0, h_in1, l_in0, l_in1, r0, r1,t0;
            h_in0 = *in0>>PINT_CHAR_OFFSET;
            h_in1 = *in1>>PINT_CHAR_OFFSET;

            t0 = (1<<size) - 1;

            h_in0 = ~h_in0;
            h_in1 = ~h_in1;
            // h_in0 = h_in0 ^ t0;
            // h_in1 = h_in1 ^ t0;
            l_in0 = *in0 & t0;
            l_in1 = *in1 & t0;
            r0 = h_in0 & l_in0;
            r1 = h_in1 & l_in1;

            if(r0 && r1)
            {
                *out = BIT_1_CHAR;
                return;
            }

            l_in0 = ~l_in0;
            l_in1 = ~l_in1;

            r0 = h_in0 & l_in0;
            r1 = h_in1 & l_in1;

             if(((r0 | r1) & t0)==t0){
                *out=BIT_0_CHAR;
                return;
            }
           
            *out=BIT_X_CHAR;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_logical_and_int(unsigned char *out, const unsigned int *in0, const unsigned int *in1, unsigned int size)
{
    NCORE_PERF_MEASURE(pint_logical_and_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(83);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned cnt = ((size+PINT_INT_SIZE-1)/PINT_INT_SIZE);
        int in0_1 = 0;
        int in1_1 = 0;
        for(int ii = 0; ii < cnt; ii++)
        {
            if (in0[ii]){
                in0_1 = 1;
                break;
            }
        }

        for(int ii = 0; ii < cnt; ii++)
        {
            if (in1[ii]){
                in1_1 = 1;
                break;
            }
        }
        *out = in0_1 && in1_1; 
    }
    else{
    unsigned int cnt,ii,low0,high0,low1,high1,t0,r0,r1,sum0,sum1;
    
    cnt = ((size+PINT_INT_SIZE-1)/PINT_INT_SIZE);
    t0=0xFFFFFFFF;
    sum0 = 0;
    sum1 = 0;
    unsigned char zero_flag_sum = 0;
    for(ii=0;ii<cnt;ii++)
    {
        low0 = in0[ii];
        high0 = in0[ii+cnt];
        
        high0 = ~high0;
        r0= high0 & low0;

        low1 = in1[ii];
        high1 = in1[ii+cnt];
        high1 = ~high1;
        r1 = high1 & low1;

        //pint_printf("high=%d,low=%d,r0=%d\n",high,low,r0);
        sum0 += r0;
        sum1 += r1;
        if(r0 && r1)
        {
            *out=BIT_1_CHAR;
            return;
        }

        low0 = ~low0;
        low1 = ~low1;

        r0 = high0 & low0;
        r1 = high1 & low1;

        if(((r0 | r1) & t0)==t0)
        {
            zero_flag_sum += 1;
        }

    }

    //pint_printf("high=%d,low=%d,sum=%d\n",high,low,sum);
    if((sum0>0) && (sum1>0))
    {
        *out=BIT_1_CHAR;
        return;
    }
    if(zero_flag_sum==cnt)
    {
        *out=BIT_0_CHAR;
        return;
    }
    *out=BIT_X_CHAR;
}
}


template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_logical_or_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_logical_or_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(84);

    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        *out = *in0 || *in1;
    }
    else{
        unsigned char h_in0, h_in1, l_in0, l_in1, r0, r1,t0;
        if(*in0 == 0 && *in1 == 0)
        {
            *out=BIT_0_CHAR;
            return;
        }
        h_in0 = *in0>>PINT_CHAR_OFFSET;
        h_in1 = *in1>>PINT_CHAR_OFFSET;
        
        t0 = (1<<size) - 1;

        h_in0 = ~h_in0;
        h_in1 = ~h_in1;
       
        l_in0 = *in0 & t0;
        l_in1 = *in1 & t0;
        r0 = h_in0 & l_in0;
        r1 = h_in1 & l_in1;

        if(r0 || r1)
        {
            *out = BIT_1_CHAR;
            return;
        }

        *out = BIT_X_CHAR;
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_logical_or_int(unsigned char *out, const unsigned int *in0, const unsigned int *in1, unsigned int size)
{
    NCORE_PERF_MEASURE(pint_logical_or_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(85);
    unsigned int cnt,ii,low0,high0,low1,high1,t0,r0,r1;
    
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned cnt = ((size+PINT_INT_SIZE-1)/PINT_INT_SIZE);
        for(int ii = 0; ii < cnt; ii++)
        {
            if (in0[ii] || in1[ii]){
                *out = BIT_1_CHAR;
                return;
            }
        }
        *out = BIT_0_CHAR;
    }
    else{
    cnt = ((size+PINT_INT_SIZE-1)/PINT_INT_SIZE);
    t0=0xFFFFFFFF;
    char x_flag0 = false;
    char x_flag1 = false;
    for(ii=0;ii<cnt;ii++)
    {
        low0 = in0[ii];
        high0 = in0[ii+cnt];

        if(high0)
        {
            x_flag0 = true;
        }
        
        high0 = ~high0;
        r0= high0 & low0;

        low1 = in1[ii];
        high1 = in1[ii+cnt];

        if(high1)
        {
            x_flag1 = true;
        }
        high1 = ~high1;
        r1 = high1 & low1;

        if(r0 || r1)
        {
            *out = BIT_1_CHAR;
            return;
        }
    }
    if(x_flag0 || x_flag1)
    {
        *out = BIT_X_CHAR;
        return;
    }

    *out = BIT_0_CHAR;
}
}

/*__constexpr*/ forceinline 
unsigned int pint_compare_char_s(const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_compare_char_s, 2);
    NCORE_PERF_PINT_NET_SUMMARY(86);
    unsigned char mask_l,mask_h,in0_signed_bit,in1_signed_bit,in0_l,in0_h,in1_l,in1_h;
    mask_l = (1<<size) - 1;
    mask_h = mask_l<<PINT_CHAR_OFFSET;
    
    in0_h = *in0&mask_h;
    in1_h = *in1&mask_h;
    if((in0_h>0)||(in1_h>0))
    {
        return PINT_COMPARE_X;
    }

    in0_l = *in0&mask_l;
    in1_l = *in1&mask_l;
    in0_signed_bit = (in0_l>>(size-1))&1;
    in1_signed_bit = (in1_l>>(size-1))&1;

    if(!(in0_signed_bit^in1_signed_bit))
    {
        if(in0_l>in1_l)
        {
            return PINT_COMPARE_BIGGER;
        }
        else if(in0_l<in1_l)
        {
            return PINT_COMPARE_LESS;
        }
        else
        {
            return PINT_COMPARE_EQUAL;
        }
    }
    else if(in0_signed_bit>in1_signed_bit)
    {
        return PINT_COMPARE_LESS;
    }
    else //if(in0_signed_bit<in1_signed_bit)
    {
        return PINT_COMPARE_BIGGER;
    }
}

/*__constexpr*/ forceinline 
unsigned int pint_compare_char_u(const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_compare_char_u, 2);
    NCORE_PERF_PINT_NET_SUMMARY(87);
    unsigned char mask_l,mask_h,in0_signed_bit,in1_signed_bit,in0_l,in0_h,in1_l,in1_h;
    mask_l = (1<<size) -1;
    mask_h = mask_l<<PINT_CHAR_OFFSET;
    
    in0_h = *in0&mask_h;
    in1_h = *in1&mask_h;
    if((in0_h>0)||(in1_h>0))
    {
        return PINT_COMPARE_X;
    }

    in0_l = *in0&mask_l;
    in1_l = *in1&mask_l;

    if(in0_l>in1_l)
    {
        return PINT_COMPARE_BIGGER;
    }
    else if(in0_l<in1_l)
    {
        return PINT_COMPARE_LESS;
    }
    else
    {
        return PINT_COMPARE_EQUAL;
    }
}

/*__constexpr*/ forceinline 
unsigned int pint_compare_int_s(const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_compare_int_s, 2);
    NCORE_PERF_PINT_NET_SUMMARY(88);
    unsigned int in0_signed_bit,in1_signed_bit,in0_l,in1_l;
    unsigned int in_Hbit_start,cnt,mod;
    
    cnt = size/PINT_INT_SIZE;
    mod = size%PINT_INT_SIZE;
    in_Hbit_start = (size+PINT_INT_SIZE-1)/PINT_INT_SIZE;

    for(int i=0;i<cnt;i++)
    {
        if((in0[i+in_Hbit_start]>0)||(in1[i+in_Hbit_start]>0))
        {
            return PINT_COMPARE_X;
        }
    }
    unsigned int mask = (1<<mod)-1;
    if(mod>0)
    {        
        if(((in0[cnt+in_Hbit_start]&mask)>0)||((in1[cnt+in_Hbit_start]&mask)>0))
        {
            return PINT_COMPARE_X;
        }
        in0_signed_bit = (in0[cnt]>>(mod-1))&1;
        in1_signed_bit = (in1[cnt]>>(mod-1))&1;
    }
    else
    {
        in0_signed_bit = (in0[cnt-1]>>(PINT_INT_SIZE-1))&1;
        in1_signed_bit = (in1[cnt-1]>>(PINT_INT_SIZE-1))&1;
    }

    if(!(in0_signed_bit^in1_signed_bit))
    {
        if(mod>0)
        {
            in0_l = in0[cnt]&mask;
            in1_l = in1[cnt]&mask;
            if(in0_l>in1_l)
            {
                return PINT_COMPARE_BIGGER;
            }
            else if(in0_l<in1_l)
            {
                return PINT_COMPARE_LESS;
            }
        }
        for(int i=cnt-1;i>=0;i--)
        {
            in0_l = in0[i];
            in1_l = in1[i];
            if(in0_l>in1_l)
            {
                return PINT_COMPARE_BIGGER;
            }
            else if(in0_l<in1_l)
            {
                return PINT_COMPARE_LESS;
            }
        }
        return PINT_COMPARE_EQUAL;            
    }
    else if(in0_signed_bit>in1_signed_bit)
    {
        return PINT_COMPARE_LESS;
    }
    else //if(in0_signed_bit<in1_signed_bit)
    {
        return PINT_COMPARE_BIGGER;
    }
}

/*__constexpr*/ forceinline 
unsigned int pint_compare_int_u(const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_compare_int_u, 2);
    NCORE_PERF_PINT_NET_SUMMARY(89);

    unsigned int in0_signed_bit,in1_signed_bit,in0_l,in1_l;
    unsigned int in_Hbit_start,cnt,mod;
    
    cnt = size/PINT_INT_SIZE;
    mod = size%PINT_INT_SIZE;
    in_Hbit_start = (size+PINT_INT_SIZE-1)/PINT_INT_SIZE;

    for(int i=0;i<cnt;i++)
    {
        if((in0[i+in_Hbit_start]>0)||(in1[i+in_Hbit_start]>0))
        {
            return PINT_COMPARE_X;
        }
    }
    unsigned int mask = (1<<mod)-1;
    if(mod>0)
    {        
        if(((in0[cnt+in_Hbit_start]&mask)>0)||((in1[cnt+in_Hbit_start]&mask)>0))
        {
            return PINT_COMPARE_X;
        }
        in0_signed_bit = (in0[cnt]>>(mod-1))&1;
        in1_signed_bit = (in1[cnt]>>(mod-1))&1;
    }
    else
    {
        in0_signed_bit = (in0[cnt-1]>>(PINT_INT_SIZE-1))&1;
        in1_signed_bit = (in1[cnt-1]>>(PINT_INT_SIZE-1))&1;
    }

    
    if(mod>0)
    {
        in0_l = in0[cnt]&mask;
        in1_l = in1[cnt]&mask;
        if(in0_l>in1_l)
        {
            return PINT_COMPARE_BIGGER;
        }
        else if(in0_l<in1_l)
        {
            return PINT_COMPARE_LESS;
        }
    }
    for(int i=cnt-1;i>=0;i--)
    {
        in0_l = in0[i];
        in1_l = in1[i];
        if(in0_l>in1_l)
        {
            return PINT_COMPARE_BIGGER;
        }
        else if(in0_l<in1_l)
        {
            return PINT_COMPARE_LESS;
        }
    }
    return PINT_COMPARE_EQUAL;            
      
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_bitw_invert_char(unsigned char *out, const unsigned char *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_bitw_invert_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(90);
    if __constexpr(_nxz) {
        unsigned char mask = (1 << size) - 1;
        *out = ~(*in0) & mask;
    } else {
        /* 00->01 01->00 11->11 10->11 */
        if (size == 1){
            *out = (*in0 == 0) ? 1 : ((*in0 == 1) ? 0 : 0x11);
        }
        else{
            unsigned char mask_l = (1<<size) - 1;
            unsigned char in_h = *in0>>PINT_CHAR_OFFSET;
            unsigned char in_l = (~*in0) & mask_l;
            in_l |= in_h;
            *out = in_l | (in_h<<PINT_CHAR_OFFSET);
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_bitw_invert_int(unsigned int *out, const unsigned int *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_bitw_invert_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(91);
    if __constexpr(_nxz) {
        unsigned int count = (size + 0x1f) >> 5;
        unsigned int mod = size & 0x1f;
        for (size_t i = 0; i < count; ++i) {
            out[i] = ~(in0[i]);
        }
        if (mod > 0) {
            out[count - 1] &= (1 << mod) - 1;
        }
    } else {
        unsigned int cnt,mod,ii,mask;
        
        cnt = (size+0x1f) >> 5;
        mod = size & 0x1f;

        for (ii=0; ii<cnt; ii++) {
            out[ii+cnt] = in0[ii+cnt];
            out[ii] = (~in0[ii]) | in0[ii+cnt];
        }

        if (mod>0) {
            mask = ~(PINT_INT_MASK<<mod);
            out[cnt - 1] &= mask;
            out[2*cnt - 1] &= mask;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
unsigned char pint_bitw_invert_char_ret(const unsigned char *in0, unsigned int size) {
    NCORE_PERF_MEASURE(pint_bitw_invert_char_ret<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(92);
    unsigned char out;
    pint_bitw_invert_char(&out, in0, size);
    return out;
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_bitw_and_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_bitw_and_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(93);

    if __constexpr(_nxz) {
        *out = *in0 & *in1;
    } else {
        unsigned char bit_l0,bit_h0,bit_l1,bit_h1,bit_o;
        *out = 0;
        for (int i=0; i<size; i++)
        {
            bit_l0 = *in0>>i;
            bit_l0 &= 1;
            bit_h0 = *in0>>(i+PINT_CHAR_OFFSET);
            bit_h0 &= 1;

            bit_l1 = *in1>>i;
            bit_l1 &= 1;
            bit_h1 = *in1>>(i+PINT_CHAR_OFFSET);
            bit_h1 &= 1;

            if (((bit_l0 | bit_h0) > 0) && ((bit_l1 | bit_h1) > 0))
            {
                if((bit_l0==1)&&(bit_h0==0)&&(bit_l1==1)&&(bit_h1==0))
                {
                    bit_o = 1<<i;
                }
                else
                {
                    bit_o = (1<<i) | (1<<(i+PINT_CHAR_OFFSET));
                }
                *out = *out | bit_o;
            }
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_bitw_and_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_bitw_and_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(94);

    if __constexpr(_nxz) {
        unsigned int count = (size + 0x1f) >> 5;
        for (size_t i = 0; i < count; ++i) {
            out[i] = in0[i] & in1[i];
        }
    } else {
    unsigned int is_xz = 0;
    unsigned int count = (size + 0x1f) >> 5;
    for(size_t i = 0; i < count; ++i) {
      is_xz |= in0[count+i];
      is_xz |= in1[count+i];
    }

    if(!is_xz) {
      for(size_t i = 0; i < count; ++i) {
        out[i] = in0[i] & in1[i];
        out[i+count] = 0;
      }
    }

    else {
      unsigned int bit_l0,bit_h0,bit_l1,bit_h1,bit_o;
      unsigned int cnt,mod,out_Hbit_start;

      cnt = size >> 5;
      mod = size & 0x1f;
      out_Hbit_start = (size+0x1f) >> 5;

      for(int i=0; i<2*out_Hbit_start; i++)
      {
          out[i] = 0;
      }

      for(int i=0; i<=cnt; i++)
      {
          unsigned int max_bit = PINT_INT_SIZE;
          if(i==cnt)
          {
              max_bit = mod;
          }
          
          for(int j=0;j<max_bit;j++)
          {
              bit_l0 = in0[i]>>j;
              bit_l0 &= 1;
              bit_h0 = in0[i+out_Hbit_start]>>j;
              bit_h0 &= 1;

              bit_l1 = in1[i]>>j;
              bit_l1 &= 1;
              bit_h1 = in1[i+out_Hbit_start]>>j;
              bit_h1 &= 1;

              if(((bit_l0 | bit_h0)>0)&&((bit_l1 | bit_h1)>0))
              {
                  if((bit_l0==1)&&(bit_h0==0)&&(bit_l1==1)&&(bit_h1==0))
                  {
                      bit_o = 1<<j;
                      out[i] |= bit_o;
                  }
                  else
                  {
                      bit_o = (1<<j);
                      out[i] |= bit_o;
                      out[i+out_Hbit_start] |= bit_o;
                  }                
              }
          }
      }
  }
  }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_bitw_or_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_bitw_or_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(95);

    if __constexpr(_nxz) {
        *out = *in0 | *in1;
    } else {
        unsigned char bit_l0,bit_h0,bit_l1,bit_h1,bit_o;
        *out = 0;

        for(int i=0;i<size;i++)
        {
            bit_l0 = *in0>>i;
            bit_l0 &= 1;
            bit_h0 = *in0>>(i+PINT_CHAR_OFFSET);
            bit_h0 &= 1;

            bit_l1 = *in1>>i;
            bit_l1 &= 1;
            bit_h1 = *in1>>(i+PINT_CHAR_OFFSET);
            bit_h1 &= 1;

            if(((bit_l0 | bit_h0)>0) || ((bit_l1 | bit_h1)>0))
            {
                if(((bit_l0==1)&&(bit_h0==0)) || ((bit_l1==1)&&(bit_h1==0)))
                {
                    bit_o = 1<<i;
                }
                else
                {
                    bit_o = (1<<i) | (1<<(i+PINT_CHAR_OFFSET));
                }
                *out = *out | bit_o;
            }
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_bitw_or_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_bitw_or_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(96);

    if __constexpr(_nxz) {
        unsigned int count = (size + 0x1f) >> 5;
        for (size_t i = 0; i < count; ++i) {
            out[i] = in0[i] | in1[i];
        }
    } else {
    unsigned int is_xz = 0;
    unsigned int count = (size + 0x1f) >> 5;
    for(size_t i = 0; i < count; ++i) {
      is_xz |= in0[count+i];
      is_xz |= in1[count+i];
    }

    if(!is_xz) {
      for(size_t i = 0; i < count; ++i) {
        out[i] = in0[i] | in1[i];
        out[i+count] = 0;
      }
    }

    else {
      unsigned int bit_l0,bit_h0,bit_l1,bit_h1,bit_o;
      unsigned int cnt,mod,out_Hbit_start;

      cnt = size >> 5;
      mod = size & 0x1f;
      out_Hbit_start = (size+0x1f) >> 5;

      for(int i=0; i<2*out_Hbit_start; i++)
      {
          out[i] = 0;
      }

      for(int i=0;i<=cnt;i++)
      {
          unsigned int max_bit = PINT_INT_SIZE;
          if(i==cnt)
          {
              max_bit = mod;
          }
          
          for(int j=0;j<max_bit;j++)
          {
              bit_l0 = in0[i]>>j;
              bit_l0 &= 1;
              bit_h0 = in0[i+out_Hbit_start]>>j;
              bit_h0 &= 1;

              bit_l1 = in1[i]>>j;
              bit_l1 &= 1;
              bit_h1 = in1[i+out_Hbit_start]>>j;
              bit_h1 &= 1;

              if(((bit_l0 | bit_h0)>0) || ((bit_l1 | bit_h1)>0))
              {
                  if(((bit_l0==1)&&(bit_h0==0)) || ((bit_l1==1)&&(bit_h1==0)))
                  {
                      bit_o = 1<<j;
                      out[i] |= bit_o;
                  }
                  else
                  {
                      bit_o = (1<<j);
                      out[i] |= bit_o;
                      out[i+out_Hbit_start] |= bit_o;
                  }
              }
          }
      }
    }
	}
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_bitw_exclusive_or_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_bitw_exclusive_or_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(97);

    if __constexpr(_nxz) {
        *out = *in0 ^ *in1;
    } else {
        unsigned char bit_l0,bit_h0,bit_l1,bit_h1,bit_o;
        *out = 0;

        for(int i=0;i<size;i++)
        {
            bit_l0 = *in0>>i;
            bit_l0 &= 1;
            bit_h0 = *in0>>(i+PINT_CHAR_OFFSET);
            bit_h0 &= 1;

            bit_l1 = *in1>>i;
            bit_l1 &= 1;
            bit_h1 = *in1>>(i+PINT_CHAR_OFFSET);
            bit_h1 &= 1;

            if((bit_h1>0)||(bit_h0>0))
            {
                bit_o = (1<<i) | (1<<(i+PINT_CHAR_OFFSET));            
            }
            else
            {
                bit_o = bit_l0 ^ bit_l1;
                bit_o = bit_o<<i;
            }
            *out = *out | bit_o;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_bitw_exclusive_or_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_bitw_exclusive_or_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(98);

    if __constexpr(_nxz) {
        unsigned int count = (size + 0x1f) >> 5;
        for (size_t i = 0; i < count; ++i) {
            out[i] = in0[i] ^ in1[i];
        }
    } else {
        unsigned int bit_l0,bit_h0,bit_l1,bit_h1,bit_o;
        unsigned int cnt,mod,out_Hbit_start;

        cnt = size >> 5;
        mod = size & 0x1f;
        out_Hbit_start = (size+0x1f) >> 5;

        for(int i=0; i<2*out_Hbit_start; i++)
        {
            out[i] = 0;
        }

        for(int i=0;i<=cnt;i++)
        {
            unsigned int max_bit = PINT_INT_SIZE;
            if(i==cnt)
            {
                max_bit = mod;
            }

            for(int j=0;j<max_bit;j++)
            {
                bit_l0 = in0[i]>>j;
                bit_l0 &= 1;
                bit_h0 = in0[i+out_Hbit_start]>>j;
                bit_h0 &= 1;

                bit_l1 = in1[i]>>j;
                bit_l1 &= 1;
                bit_h1 = in1[i+out_Hbit_start]>>j;
                bit_h1 &= 1;

                if((bit_h1>0) || (bit_h0>0))
                {
                    bit_o = 1<<j;  
                    out[i] |= bit_o;
                    out[i+out_Hbit_start] |= bit_o;          
                }
                else
                {
                    bit_o = bit_l0 ^ bit_l1;
                    bit_o = bit_o<<j;
                    out[i] |= bit_o;
                }
            }
        }
    }
}

//  Function: a ~& b
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_bitw_nand_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_bitw_nand_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(99);

    if __constexpr(_nxz) {
        unsigned char mask = (1 << size) - 1;
        *out = ~(*in0 & *in1) & mask;
    } else {
        unsigned char d0 = *in0;
        unsigned char d1 = *in1;
        unsigned char ret = 0;
        unsigned char mask_h = 0x10;
        unsigned char mask_l = 0x01;
        int i;
        for(i=0; i < size; i++, mask_h <<= 1, mask_l <<= 1){
            if ((0 == (d0 & (mask_h | mask_l))) || (0 == (d1 & (mask_h | mask_l)))) {
                ret |= mask_l;
            } else {
                if (!(((d0 & mask_h) == 0 && (d0 & mask_l) != 0) && ((d1 & mask_h) == 0 && (d1 & mask_l) != 0))) {
                    ret |= mask_h | mask_l;
                }
            }
        }
        *out = ret;
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_bitw_nand_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_bitw_nand_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(100);

    if __constexpr(_nxz) {
        unsigned int count = (size + 0x1f) >> 5;
        unsigned int mod = size & 0x1f;
        for (size_t i = 0; i < count; ++i) {
            out[i] = ~(in0[i] & in1[i]);
        }
        if (mod > 0) {
            out[count - 1] &= (1 << mod) - 1;
        }
    } else {
        unsigned int cnt = (size + 0x1f) >> 5;
        unsigned int mod = size & 0x1f;
        unsigned int len = PINT_INT_SIZE;
        unsigned int ret_h, ret_l, hd0, hd1, ld0, ld1, mask;
        int i, j;
        for (i = 0; i < cnt; i++){
            if(i == cnt - 1 && mod > 0) len = mod;
            hd0 = in0[i + cnt];
            hd1 = in1[i + cnt];
            ld0 = in0[i];
            ld1 = in1[i];
            ret_h = 0;
            ret_l = 0;
            mask  = 0x1;
            for(j=0; j < len; j++, mask <<= 1){
                if ((0 == ((hd0 | ld0) & mask)) || (0 == ((hd1 | ld1) & mask))) {
                    ret_l |= mask;
                } else {
                    if (!(((hd0 & mask) == 0 && (ld0 & mask) != 0) && ((hd1 & mask) == 0 && (ld1 & mask) != 0))) {
                        ret_h |= mask;
                        ret_l |= mask;
                    }
                }
            }
            out[i] = ret_l;
            out[i+cnt] = ret_h;
        }
    }
}

//  Function: a ~| b
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_bitw_nor_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_bitw_nor_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(101);

    if __constexpr(_nxz) {
        unsigned char mask = (1 << size) - 1;
        *out = ~(*in0 | *in1) & mask;
    } else {
        unsigned char d0 = *in0;
        unsigned char d1 = *in1;
        unsigned char ret = 0;
        unsigned char mask_h = 0x10;
        unsigned char mask_l = 0x01;
        int i;
        for (i = 0; i < size; i++, mask_h <<= 1, mask_l <<= 1){
            if (!(((d0 & mask_h) == 0 && (d0 & mask_l) != 0) || ((d1 & mask_h) == 0 && (d1 & mask_l) != 0))) {
                if ((d0 & (mask_h | mask_l)) == 0 && (d1 & (mask_h | mask_l)) == 0) {
                    ret |= mask_l;
                }
                else {
                    ret |= mask_h | mask_l;
                }
            }
        }
        *out = ret;
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_bitw_nor_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_bitw_nor_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(102);

    if __constexpr(_nxz) {
        unsigned int count = (size + 0x1f) >> 5;
        unsigned int mod = (size & 0x1f);
        for (size_t i = 0; i < count; ++i) {
            out[i] = ~(in0[i] | in1[i]);
        }
        if (mod > 0) {
            out[count - 1] &= (1 << mod) - 1;
        }
    } else {
        unsigned int cnt = (size + 0x1f) >> 5;
        unsigned int mod = (size & 0x1f);
        unsigned int len = PINT_INT_SIZE;
        unsigned int ret_h, ret_l, hd0, hd1, ld0, ld1, mask;
        int i, j;
        for(i = 0; i < cnt; i++){
            if(i == cnt - 1 && mod > 0) len = mod;
            hd0 = in0[i + cnt];
            hd1 = in1[i + cnt];
            ld0 = in0[i];
            ld1 = in1[i];
            ret_h = 0;
            ret_l = 0;
            mask  = 0x1;
            for(j = 0; j < len; j++, mask <<= 1){
                if (!(((hd0 & mask) == 0 && (ld0 & mask) != 0) || ((hd1 & mask) == 0 && (ld1 & mask) != 0))) {
                    if ((0 == ((hd0 | ld0) & mask)) && (0 == ((hd1 | ld1) & mask))) {
                        ret_l |= mask;
                    } else {
                        ret_h |= mask;
                        ret_l |= mask;
                    }
                }
            }
            out[i] = ret_l;
            out[i+cnt] = ret_h;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_bitw_equivalence_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_bitw_equivalence_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(103);

    if __constexpr(_nxz) {
        unsigned char mask = (1 << size) - 1;
        *out = ~(*in0 ^ *in1) & mask;
    } else {
        unsigned char bit_l0,bit_h0,bit_l1,bit_h1,bit_o;
        unsigned char tmp = 0;

        for(int i=0; i<size; i++)
        {
            bit_l0 = *in0>>i;
            bit_l0 &= 1;
            bit_h0 = *in0>>(i+PINT_CHAR_OFFSET);
            bit_h0 &= 1;

            bit_l1 = *in1>>i;
            bit_l1 &= 1;
            bit_h1 = *in1>>(i+PINT_CHAR_OFFSET);
            bit_h1 &= 1;

            if((bit_h1>0) || (bit_h0>0)) {
                bit_o = (1<<i) | (1<<(i+PINT_CHAR_OFFSET));            
            }
            else{
                bit_o = ~(bit_l0 ^ bit_l1);
                bit_o &= 1;
                bit_o = bit_o<<i;
            }
            tmp |= bit_o;
        }
        *out = tmp;
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_bitw_equivalence_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_bitw_equivalence_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(104);

    if __constexpr(_nxz) {
        unsigned int count = (size + 0x1f) >> 5;
        unsigned int mod = size & 0x1f;
        for (size_t i = 0; i < count; ++i) {
            out[i] = ~(in0[i] ^ in1[i]);
        }
        if (mod > 0) {
            out[count - 1] &= (1 << mod) - 1;
        }
    } else {
        unsigned int bit_l0,bit_h0,bit_l1,bit_h1,bit_o;
        unsigned int cnt,mod,out_Hbit_start;

        cnt = size >> 5;
        mod = size & 0x1f;
        out_Hbit_start = (size+0x1f) >> 5;

        for(int i=0;i<2*out_Hbit_start;i++)
        {
            out[i] = 0;
        }

        for(int i=0;i<=cnt;i++)
        {
            unsigned int max_bit = PINT_INT_SIZE;
            if(i==cnt)
            {
                max_bit = mod;
            }

            for(int j=0;j<max_bit;j++)
            {
                bit_l0 = in0[i]>>j;
                bit_l0 &= 1;
                bit_h0 = in0[i+out_Hbit_start]>>j;
                bit_h0 &= 1;

                bit_l1 = in1[i]>>j;
                bit_l1 &= 1;
                bit_h1 = in1[i+out_Hbit_start]>>j;
                bit_h1 &= 1;

                if((bit_h1>0)||(bit_h0>0)) {
                    bit_o = 1<<j;
                    out[i] |= bit_o;
                    out[i+out_Hbit_start] |= bit_o;          
                }
                else{
                    bit_o = ~(bit_l0 ^ bit_l1);
                    bit_o &= 1;
                    bit_o = bit_o<<j;
                    out[i] |= bit_o;
                }
            }
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduction_and_char(unsigned char *out, const unsigned char *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_reduction_and_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(105);
    if __constexpr(_nxz) {
        unsigned char mask = (1 << size) - 1;
        if ((*in0 & mask) != mask) {
            *out = 0;
            return;
        }
        *out = 1;
    } else {
        unsigned char bit_l0,bit_h0,bit_o;
        *out = 0;
        for(int i=0; i<size; i++)
        {
            bit_l0 = *in0>>i;
            bit_l0 &= 1;
            bit_h0 = *in0>>(i+PINT_CHAR_OFFSET);
            bit_h0 &= 1;

            if((bit_l0==0)&&(bit_h0==0)){
                *out = 0;
                return;
            }
            else if((bit_l0==1)&&(bit_h0==0)){
                *out |= 1;
            }
            else if(bit_h0==1){
                *out = 0x11;
            }
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduction_and_int(unsigned char *out, const unsigned int *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_reduction_and_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(106);
    if __constexpr(_nxz) {
        unsigned int cnt = size >> 5;
        unsigned int mod = size & 0x1f;
        for (size_t i = 0; i < cnt; ++i) {
            if (in0[i] != PINT_INT_MASK) {
                *out = 0;
                return;
            }
        }
        if (mod > 0) {
            unsigned int mask = (1 << mod) - 1;
            if ((in0[cnt] & mask) != mask) {
                *out = 0;
                return;
            }
        }
        *out = 1;
    } else {
        unsigned int bit_l0,bit_h0,bit_o;
        unsigned int cnt,mod,out_Hbit_start;

        cnt = size >> 5;
        mod = size & 0x1f;
        out_Hbit_start = (size+0x1f) >> 5;

        *out=0;

        for(int i=0; i<=cnt; i++)
        {
            unsigned int max_bit = PINT_INT_SIZE;
            if(i==cnt)
            {
                max_bit = mod;
            }

            for(int j=0;j<max_bit;j++)
            {
                bit_l0 = in0[i]>>j;
                bit_l0 &= 1;
                bit_h0 = in0[i+out_Hbit_start]>>j;
                bit_h0 &= 1;

                if((bit_l0==0)&&(bit_h0==0))
                {
                    *out = 0;
                    return;
                }
                else if((bit_l0==1)&&(bit_h0==0)){
                    *out = *out | 1;
                }
                else if(bit_h0==1){
                    *out = 0x11;
                }
            }
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduction_nand_char(unsigned char *out, const unsigned char *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_reduction_nand_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(107);
    if __constexpr(_nxz) {
        unsigned char mask = (1 << size) - 1;
        if ((*in0 & mask) != mask) {
            *out = 1;
            return;
        }
        *out = 0;
    } else {
        unsigned char out_t;
        pint_reduction_and_char(&out_t, in0, size);
        pint_bitw_invert_char(out, &out_t, 1);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduction_nand_int(unsigned char *out, const unsigned int *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_reduction_nand_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(108);
    if __constexpr(_nxz) {
        unsigned int cnt = size >> 5;
        unsigned int mod = size & 0x1f;
        for (size_t i = 0; i < cnt; ++i) {
            if (in0[i] != PINT_INT_MASK) {
                *out = 1;
                return;
            }
        }
        if (mod > 0) {
            unsigned int mask = (1 << mod) - 1;
            if ((in0[cnt] & mask) != mask) {
                *out = 1;
                return;
            }
        }
        *out = 0;
    } else {
        unsigned char out_t;
        pint_reduction_and_int(&out_t, in0, size);
        pint_bitw_invert_char(out, &out_t, 1);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduction_or_char(unsigned char *out, const unsigned char *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_reduction_or_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(109);
    if __constexpr(_nxz) {
        unsigned char mask = (1 << size) - 1;
        if ((*in0 & mask) != 0) {
            *out = 1;
            return;
        }
        *out = 0;
    } else {
        unsigned char bit_l0,bit_h0,bit_o;
        *out = 0;
        for(int i=0; i<size; i++)
        {
            bit_l0 = *in0>>i;
            bit_l0 &= 1;
            bit_h0 = *in0>>(i+PINT_CHAR_OFFSET);
            bit_h0 &= 1;

            if((bit_l0==1)&&(bit_h0==0))
            {
                *out = 1;
                return;
            }
            else if(bit_h0==1){
                *out = 0x11;
            }
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduction_or_int(unsigned char *out, const unsigned int *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_reduction_or_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(110);
    if __constexpr(_nxz) {
        unsigned int cnt = size >> 5;
        unsigned int mod = size & 0x1f;
        for (size_t i = 0; i < cnt; ++i) {
            if (in0[i] != 0) {
                *out = 1;
                return;
            }
        }
        if (mod > 0) {
            unsigned int mask = (1 << mod) - 1;
            if ((in0[cnt] & mask) != 0) {
                *out = 1;
                return;
            }
        }
        *out = 0;
    } else {
        unsigned int bit_l0,bit_h0,bit_o;
        unsigned int cnt,mod,out_Hbit_start;

        cnt = size >> 5;
        mod = size & 0x1f;
        out_Hbit_start = (size+0x1f) >> 5;

        *out=0;

        for(int i=0;i<=cnt;i++)
        {
            unsigned int max_bit = PINT_INT_SIZE;
            if(i==cnt)
            {
                max_bit = mod;
            }

            for(int j=0;j<max_bit;j++)
            {
                bit_l0 = in0[i]>>j;
                bit_l0 &= 1;
                bit_h0 = in0[i+out_Hbit_start]>>j;
                bit_h0 &= 1;

                if((bit_l0==1)&&(bit_h0==0))
                {
                    *out = 1;
                    return;
                }
                else if(bit_h0==1){
                    *out = 0x11;
                }
            }
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduction_nor_char(unsigned char *out, const unsigned char *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_reduction_nor_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(111);
    if __constexpr(_nxz) {
        unsigned char mask = (1 << size) - 1;
        if ((*in0 & mask) != 0) {
            *out = 0;
            return;
        }
        *out = 1;
    } else {
        unsigned char out_t;
        pint_reduction_or_char(&out_t, in0, size);
        pint_bitw_invert_char(out, &out_t, 1);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduction_nor_int(unsigned char *out, const unsigned int *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_reduction_nor_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(112);
    if __constexpr(_nxz) {
        unsigned int cnt = size >> 5;
        unsigned int mod = size & 0x1f;
        for (size_t i = 0; i < cnt; ++i) {
            if (in0[i] != 0) {
                *out = 0;
                return;
            }
        }
        if (mod > 0) {
            unsigned int mask = (1 << mod) - 1;
            if ((in0[cnt] & mask) != 0) {
                *out = 0;
                return;
            }
        }
        *out = 1;
    } else {
        unsigned char out_t;
        pint_reduction_or_int(&out_t, in0, size);
        pint_bitw_invert_char(out, &out_t, 1);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduction_xor_char(unsigned char *out, const unsigned char *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_reduction_xor_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(113);
    if __constexpr(_nxz) {
        unsigned char bit_l0;    
        unsigned char v1_num = 0;
        for(int i=0;i<size;i++)
        {
            bit_l0 = *in0>>i;
            bit_l0 &= 1;
            if (bit_l0>0) ++v1_num;
        }

        if((v1_num%2)>0)
        {
            *out = 1;
        }
        else
        {
            *out = 0;
        }
    } else {
        unsigned char bit_l0,bit_h0;    
        unsigned char mask_l = (1<<size)-1;
        bit_h0 = *in0&(mask_l<<PINT_CHAR_OFFSET);
        if(bit_h0>0)
        {
            *out = 0x11;
            return;
        }
        else
        {
            unsigned char v1_num = 0;
            for(int i=0;i<size;i++)
            {
                bit_l0 = *in0>>i;
                bit_l0 &= 1;
                if (bit_l0>0) ++v1_num;
            }

            if((v1_num%2)>0)
            {
                *out = 1;
            }
            else
            {
                *out = 0;
            }
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduction_xor_int(unsigned char *out, const unsigned int *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_reduction_xor_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(114);
    if __constexpr(_nxz) {
        unsigned int bit_l0;
        unsigned int cnt,mod;

        cnt = size >> 5;
        mod = size & 0x1f;

        unsigned int v1_num = 0;
        for(int i=0; i<=cnt; i++)
        {
            unsigned int max_bit = PINT_INT_SIZE;
            if(i==cnt)
            {
                max_bit = mod;
            }

            for(int j=0;j<max_bit;j++)
            {
                bit_l0 = in0[i]>>j;
                bit_l0 &= 1;
                if (bit_l0>0) ++v1_num;
            }
        }

        if((v1_num%2)>0)
        {
            *out = 1;
        }
        else
        {
            *out = 0;
        }
    } else {
        unsigned int bit_l0,bit_h0;
        unsigned int cnt,mod,out_Hbit_start;

        cnt = size >> 5;
        mod = size & 0x1f;
        out_Hbit_start = (size+0x1f) >> 5;

        unsigned int v1_num = 0;
        for(int i=0; i<=cnt; i++)
        {
            if((i<cnt)&&(in0[i+out_Hbit_start]>0))
            {
                *out = 0x11;
                return;
            }
            else if((i==cnt)&&(mod>0))
            {
                if((in0[i+out_Hbit_start]&((1<<mod)-1))>0)
                {
                    *out = 0x11;
                    return;
                }
            }

            unsigned int max_bit = PINT_INT_SIZE;
            if(i==cnt)
            {
                max_bit = mod;
            }

            for(int j=0;j<max_bit;j++)
            {
                bit_l0 = in0[i]>>j;
                bit_l0 &= 1;
                if (bit_l0>0) ++v1_num;
            }
        }

        if((v1_num%2)>0)
        {
            *out = 1;
        }
        else
        {
            *out = 0;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduction_xnor_char(unsigned char *out, const unsigned char *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_reduction_xnor_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(115);
    if __constexpr(_nxz) {
        unsigned char bit_l0;    
        unsigned char v1_num = 0;
        for(int i=0;i<size;i++)
        {
            bit_l0 = *in0>>i;
            bit_l0 &= 1;
            if (bit_l0>0) ++v1_num;
        }

        if((v1_num%2)>0)
        {
            *out = 0;
        }
        else
        {
            *out = 1;
        }
    } else {
        unsigned char out_t;
        pint_reduction_xor_char(&out_t, in0, size);
        pint_bitw_invert_char(out, &out_t, 1);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_reduction_xnor_int(unsigned char *out, const unsigned int *in0, unsigned int size){
    NCORE_PERF_MEASURE(pint_reduction_xnor_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(116);
    if __constexpr(_nxz) {
        unsigned int bit_l0;
        unsigned int cnt,mod;

        cnt = size >> 5;
        mod = size & 0x1f;

        unsigned int v1_num = 0;
        for(int i=0; i<=cnt; i++)
        {
            unsigned int max_bit = PINT_INT_SIZE;
            if(i==cnt)
            {
                max_bit = mod;
            }

            for(int j=0;j<max_bit;j++)
            {
                bit_l0 = in0[i]>>j;
                bit_l0 &= 1;
                if (bit_l0>0) ++v1_num;
            }
        }

        if((v1_num%2)>0)
        {
            *out = 0;
        }
        else
        {
            *out = 1;
        }
    } else {
        unsigned char out_t;
        pint_reduction_xor_int(&out_t, in0, size);
        pint_bitw_invert_char(out, &out_t, 1);
    }
}

template<bool _nxz = false>  // _nxz = true
/*__constexpr*/ forceinline 
void pint_reduction_and_int_value(unsigned char *out, unsigned int in0, unsigned int size) {
    NCORE_PERF_MEASURE(pint_reduction_and_int_value<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(117);
    unsigned int mask = (1<<size) - 1;
    if ((in0 & mask) != mask) {
        *out = 0;
    } else {
        *out = 1;
    }
}

template<bool _nxz = false>  // _nxz = true
/*__constexpr*/ forceinline 
void pint_reduction_nand_int_value(unsigned char *out, unsigned int in0, unsigned int size) {
    NCORE_PERF_MEASURE(pint_reduction_nand_int_value<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(118);
    unsigned int mask = (1<<size) - 1;
    if ((in0 & mask) != mask) {
        *out = 1;
    } else {
        *out = 0;
    }
}

template<bool _nxz = false>  // _nxz = true
/*__constexpr*/ forceinline 
void pint_reduction_or_int_value(unsigned char *out, unsigned int in0, unsigned int size) {
    NCORE_PERF_MEASURE(pint_reduction_or_int_value<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(119);
    unsigned int mask = (1<<size) - 1;
    if ((in0 & mask) != 0) {
        *out = 1;
    } else {
        *out = 0;
    }
}

template<bool _nxz = false>  // _nxz = true
/*__constexpr*/ forceinline 
void pint_reduction_nor_int_value(unsigned char *out, unsigned int in0, unsigned int size) {
    NCORE_PERF_MEASURE(pint_reduction_nor_int_value<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(120);
    unsigned int mask = (1<<size) - 1;
    if ((in0 & mask) != 0) {
        *out = 0;
    } else {
        *out = 1;
    }
}

template<bool _nxz = false>  // _nxz = true
/*__constexpr*/ forceinline 
void pint_reduction_xor_int_value(unsigned char *out, unsigned int in0, unsigned int size) {
    NCORE_PERF_MEASURE(pint_reduction_xor_int_value<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(121);
    unsigned in[2];
    in[0] = in0;
    in[1] = 0;
    pint_reduction_xor_int(out, in, size);
}

template<bool _nxz = false>  // _nxz = true
/*__constexpr*/ forceinline 
void pint_reduction_xnor_int_value(unsigned char *out, unsigned int in0, unsigned int size) {
    NCORE_PERF_MEASURE(pint_reduction_xnor_int_value<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(122);
    unsigned in[2];
    in[0] = in0;
    in[1] = 0;
    pint_reduction_xnor_int(out, in, size);
}

// Function: "a << num" or "a <<< num"
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_lshift_char(unsigned char *out, const unsigned char *in0, unsigned int shift, unsigned int size){
    NCORE_PERF_MEASURE(pint_lshift_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(123);
    if __constexpr(!_nxz) {
        if (shift < size){
            unsigned char d0 = *in0;
            unsigned char mask = (0x1 << size) -1;
            mask |= mask << 4;
            *out = (((d0 << shift) & 0x0f) | ((d0 & 0xf0) << shift)) & mask;
        }else {
            if (shift == VALUE_XZ) {
                unsigned char mask = (1<<size) - 1;
                *out = mask | (mask << 4);
            }else{
                *out = 0;
            }
        }
    } else {
        if (shift < size) {
            unsigned char mask = (0x01 << size) - 1;
            *out = (*in0 << shift) & mask;
        } else {
            *out = 0;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_lshift_int(unsigned int *out, const unsigned int *in0, unsigned int shift, unsigned int size){
    NCORE_PERF_MEASURE(pint_lshift_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(124);
    if __constexpr(!_nxz) {
        unsigned int cnt = (size + 0x1f) >> 5;  //(size + (32-1)) / 32
        unsigned int mod = size & 0x1f;  //size % 32
        unsigned int mask = (0x01 << mod) - 1;
        int i;
        if (shift < size) {
            if(shift < 32){
                unsigned int hd0, ld0;
                for(i = cnt-1; i >= 0; i--){
                    ld0 = in0[i];
                    hd0 = in0[i+cnt];
                    out[i]     = ld0 << shift;
                    out[i+cnt] = hd0 << shift;
                    if ((shift > 0) && (i < cnt-1)) {
                        out[i+1]     |= ld0 >> (32 - shift);
                        out[i+1+cnt] |= hd0 >> (32 - shift);
                    }
                }
            }else{
                unsigned int nsh, ish, hd0, ld0;
                nsh = shift >> 5;
                ish = shift & 0x1f;
                for (i = cnt-nsh-1; i >= 0; i--) {
                    ld0 = in0[i];
                    hd0 = in0[i+cnt];
                    out[i+nsh]     = ld0 << ish;
                    out[i+nsh+cnt] = hd0 << ish;
                    if ((ish > 0) && (i+nsh < cnt-1)) {
                        out[i+nsh+1]     |= ld0 >> (32 - ish);
                        out[i+nsh+1+cnt] |= hd0 >> (32 - ish);
                    }
                }
                for (i = nsh-1; i >= 0; i--) {
                    out[i] = 0;
                    out[i+cnt] = 0;
                }
            }
            if (mod > 0) {
                out[cnt-1] &= mask;
                out[2*cnt-1] &= mask;
            }
        } else {
            for (i = 0; i < cnt; i++) {
                out[i] = 0;
                out[i+cnt] = 0;
            }
        }
    } else {
        unsigned int cnt = (size + 0x1f) >> 5;  //(size + (32-1)) / 32
        unsigned int mod = size & 0x1f;  //size % 32
        unsigned int mask = (0x01 << mod) - 1;
        int i;
        if (shift < size) {
            if(shift < 32){
                unsigned int ld0;
                for(i = cnt-1; i >= 0; i--){
                    ld0 = in0[i];
                    out[i] = ld0 << shift;
                    if ((shift > 0) && (i < cnt-1)) {
                        out[i+1] |= ld0 >> (32 - shift);
                    }
                }
            }else{
                unsigned int nsh, ish, ld0;
                nsh = shift >> 5;
                ish = shift & 0x1f;
                for (i = cnt-nsh-1; i >= 0; i--) {
                    ld0 = in0[i];
                    out[i+nsh] = ld0 << ish;
                    if ((ish > 0) && (i+nsh < cnt-1)) {
                        out[i+nsh+1] |= ld0 >> (32 - ish);
                    }
                }
                for (i = nsh-1; i >= 0; i--) {
                    out[i] = 0;
                }
            }
            if (mod > 0) {
                out[cnt-1] &= mask;
            }
        } else {
            for (i = 0; i < cnt; i++) {
                out[i] = 0;
            }
        }
    }
}

// Function: "a >> b" or "a >>> b"
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_rshift_char_u(unsigned char *out, const unsigned char *in0, unsigned int shift, unsigned int size){
    NCORE_PERF_MEASURE(pint_rshift_char_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(125);
    if __constexpr(!_nxz) {
        if (shift < size){
            unsigned char mask = (0x1 << size) -1;
            mask |= mask << 4;
            unsigned char d0 = *in0 & mask;
            *out = ((d0 >> shift) & 0xf0) | ((d0 & 0x0f) >> shift);
        }else {
            if (shift == VALUE_XZ) {
                unsigned char mask = (1<<size) - 1;
                *out = mask | (mask << 4);
            } else{
                *out = 0;
            }
        }
    } else {
        if (shift < size){
            unsigned char mask = (0x1 << size) - 1;
            *out = (*in0 & mask) >> shift;
        }else {
            *out = 0;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_rshift_int_u(unsigned int *out, const unsigned int *in0, unsigned int shift, unsigned int size){
    NCORE_PERF_MEASURE(pint_rshift_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(126);
    if __constexpr(!_nxz) {
        unsigned int cnt = (size + (PINT_INT_SIZE-1)) >> 5;  //(size + (32-1)) / 32
        int i;
        if (shift < size) {
            if(shift < PINT_INT_SIZE){
                unsigned int hd0, ld0;
                for (i = 0; i <= cnt-1; i++) {
                    ld0 = in0[i];
                    hd0 = in0[i+cnt];
                    out[i]     = ld0 >> shift;
                    out[i+cnt] = hd0 >> shift;
                    if ((shift > 0) && (i > 0)) {
                        out[i-1]     |= ld0 << (PINT_INT_SIZE - shift);
                        out[i-1+cnt] |= hd0 << (PINT_INT_SIZE - shift);
                    }
                }
            }else{
                unsigned int nsh, ish, hd0, ld0;
                nsh = shift >> 5;
                ish = shift & 0x1f;
                for (i = nsh; i <= cnt-1; i++) {
                    ld0 = in0[i];
                    hd0 = in0[i+cnt];
                    out[i-nsh]     = ld0 >> ish;
                    out[i-nsh+cnt] = hd0 >> ish;
                    if ((ish > 0) && (i-nsh > 0)) {
                        out[i-nsh-1]     |= ld0 << (PINT_INT_SIZE - ish);
                        out[i-nsh-1+cnt] |= hd0 << (PINT_INT_SIZE - ish);
                    }
                }
                for (i = cnt-nsh; i <= cnt-1; i++) {
                    out[i] = 0;
                    out[i+cnt] = 0;
                }
            }
        } else {
            for(i=0; i < cnt; i++){
                out[i] = 0;
                out[i+cnt] = 0;
            }
        }
    } else {
        unsigned int cnt = (size + (PINT_INT_SIZE-1)) >> 5;  //(size + (32-1)) / 32
        int i;
        if (shift < size) {
            if (shift < PINT_INT_SIZE) {
                unsigned int ld0;
                for (i = 0; i <= cnt-1; i++) {
                    ld0 = in0[i];
                    out[i] = ld0 >> shift;
                    if ((shift > 0) && (i > 0)) {
                        out[i-1] |= ld0 << (PINT_INT_SIZE - shift);
                    }
                }
            } else {
                unsigned int nsh, ish, ld0;
                nsh = shift >> 5;
                ish = shift & 0x1f;
                for (i = nsh; i <= cnt-1; i++) {
                    ld0 = in0[i];
                    out[i-nsh] = ld0 >> ish;
                    if ((ish > 0) && (i-nsh > 0)) {
                        out[i-nsh-1] |= ld0 << (PINT_INT_SIZE - ish);
                    }
                }
                for (i = cnt-nsh; i <= cnt-1; i++) {
                    out[i] = 0;
                }
            }
        } else {
            for(i=0; i < cnt; i++){
                out[i] = 0;
            }
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_rshift_char_s(unsigned char *out, const unsigned char *in0, unsigned int shift, unsigned int size){
    NCORE_PERF_MEASURE(pint_rshift_char_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(127);
    if __constexpr(!_nxz) {
        unsigned char mask = (0x1 << size) - 1;
        mask |= mask << 4;
        if(shift < size){
            unsigned int shift_l = 4 - size;
            unsigned int shift_r = shift + shift_l;

            char d0  = (char)((*in0 & mask) << shift_l);
            char ld0 = d0 << 4;
            *out = ((d0 >> shift_r) & 0xf0) | ((unsigned char)(ld0 >> shift_r) >> 4);
        }else {
            if (shift == VALUE_XZ) {
                *out = mask;
            } else {
                *out = 0;
                if (*in0 & (0x01 << (size-1))) {
                    *out |= 0x0f & mask;
                }
                if (*in0 & (0x10 << (size-1))) {
                    *out |= 0xf0 & mask;
                }
            }
        }
    } else {
        unsigned char mask = (0x1 << size) - 1;
        if(shift < size){
            // unsigned int shift_l = 4 - size;
            // unsigned int shift_r = shift + shift_l;

            // char d0  = (char)((*in0 & mask) << shift_l);
            // char ld0 = d0 << 4;
            // *out = (unsigned char)(ld0 >> shift_r) >> 4;

            unsigned int shift_l = 8 - size;
            unsigned int shift_r = shift + shift_l;
            char ld0  = (char)(*in0 << shift_l);
            *out = (unsigned char)(ld0 >> shift_r);
        }else {
            if (*in0 & (0x01 << (size-1))) {
                *out = mask;
            } else {
                *out = 0;
            }
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_rshift_int_s(unsigned int *out, const unsigned int *in0, unsigned int shift, unsigned int size){
    NCORE_PERF_MEASURE(pint_rshift_int_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(128);
    if __constexpr(!_nxz) {
        unsigned int cnt = (size + (PINT_INT_SIZE-1)) >> 5;  //(size + (32-1)) / 32
        unsigned int mod = size & (PINT_INT_SIZE-1);  //size % 32
        unsigned int mask = (0x01 << mod) - 1;
        unsigned int lsign = in0[cnt-1]   & (0x01 << ((size-1)&0x1f));
        unsigned int hsign = in0[2*cnt-1] & (0x01 << ((size-1)&0x1f));
        int i;

        if (shift < size) {
            unsigned int size_remain = size - shift;
            unsigned int cnt_remain  = (size_remain + 0x1f) >> 5;
            unsigned int mod_remain  = size_remain & 0x1f;

            if(shift < PINT_INT_SIZE){
                unsigned int hd0, ld0;
                for (i = 0; i <= cnt-1; i++){
                    ld0 = in0[i];
                    hd0 = in0[i+cnt];
                    out[i]     = ld0 >> shift;
                    out[i+cnt] = hd0 >> shift;
                    if ((shift > 0) && (i > 0)) {
                        out[i-1]     |= ld0 << (PINT_INT_SIZE - shift);
                        out[i-1+cnt] |= hd0 << (PINT_INT_SIZE - shift);
                    }
                }
            }else{
                unsigned int nsh, ish, hd0, ld0;
                nsh = shift >> 5;
                ish = shift & 0x1f;
                for(i = nsh; i <= cnt-1; i++){
                    ld0 = in0[i];
                    hd0 = in0[i+cnt];
                    out[i-nsh]     = ld0 >> ish;
                    out[i-nsh+cnt] = hd0 >> ish;
                    if ((ish > 0) && (i-nsh > 0)) {
                        out[i-nsh-1]     |= ld0 << (PINT_INT_SIZE - ish);
                        out[i-nsh-1+cnt] |= hd0 << (PINT_INT_SIZE - ish);
                    }
                }
            }

            if (lsign) {
                if (mod_remain > 0) {
                    out[cnt_remain-1] |= (PINT_INT_MASK>>mod_remain)<<mod_remain;
                }
                for (i = cnt_remain; i <= cnt-1; i++) {
                    out[i] = PINT_INT_MASK;
                }
            } else {
                for (i = cnt_remain; i <= cnt-1; i++) {
                    out[i] = 0;
                }
            }
            if (hsign) {
                if (mod_remain > 0) {
                    out[cnt_remain-1+cnt] |= (PINT_INT_MASK>>mod_remain)<<mod_remain;
                }
                for (i = cnt_remain; i <= cnt-1; i++) {
                    out[i+cnt] = PINT_INT_MASK;
                }
            } else {
                for (i = cnt_remain; i <= cnt-1; i++) {
                    out[i+cnt] = 0;
                }
            }
        } else {
            if (lsign) {
                for (i = 0; i < cnt; i++) {
                    out[i] = PINT_INT_MASK;
                }
            } else {
                for (i = 0; i < cnt; i++) {
                    out[i] = 0;
                }
            }
            if (hsign) {
                for (i = 0; i < cnt; i++) {
                    out[i+cnt] = PINT_INT_MASK;
                }
            } else {
                for (i = 0; i < cnt; i++) {
                    out[i+cnt] = 0;
                }
            }
        }

        if (mod > 0) {
            out[cnt-1] &= mask;
            out[2*cnt-1] &= mask;
        }
    } else {
        unsigned int cnt = (size + (PINT_INT_SIZE-1)) >> 5;  //(size + (32-1)) / 32
        unsigned int mod = size & (PINT_INT_SIZE-1);  //size % 32
        unsigned int mask = (0x01 << mod) - 1;
        unsigned int lsign = in0[cnt-1]   & (0x01 << ((size-1)&0x1f));
        int i;

        if (shift < size) {
            unsigned int size_remain = size - shift;
            unsigned int cnt_remain  = (size_remain + 0x1f) >> 5;
            unsigned int mod_remain  = size_remain & 0x1f;

            if(shift < PINT_INT_SIZE){
                unsigned int hd0, ld0;
                for (i = 0; i <= cnt-1; i++){
                    ld0 = in0[i];
                    out[i] = ld0 >> shift;
                    if ((shift > 0) && (i > 0)) {
                        out[i-1] |= ld0 << (PINT_INT_SIZE - shift);
                    }
                }
            }else{
                unsigned int nsh, ish, hd0, ld0;
                nsh = shift >> 5;
                ish = shift & 0x1f;
                for(i = nsh; i <= cnt-1; i++){
                    ld0 = in0[i];
                    out[i-nsh] = ld0 >> ish;
                    if ((ish > 0) && (i-nsh > 0)) {
                        out[i-nsh-1] |= ld0 << (PINT_INT_SIZE - ish);
                    }
                }
            }

            if (lsign) {
                if (mod_remain > 0) {
                    out[cnt_remain-1] |= (PINT_INT_MASK>>mod_remain)<<mod_remain;
                }
                for (i = cnt_remain; i <= cnt-1; i++) {
                    out[i] = PINT_INT_MASK;
                }
            } else {
                for (i = cnt_remain; i <= cnt-1; i++) {
                    out[i] = 0;
                }
            }
        } else {
            if (lsign) {
                for (i = 0; i < cnt; i++) {
                    out[i] = PINT_INT_MASK;
                }
            } else {
                for (i = 0; i < cnt; i++) {
                    out[i] = 0;
                }
            }
        }

        if (mod > 0) {
            out[cnt-1] &= mask;
        }
    }
}


template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_logic_lshift_char(unsigned char *out, unsigned char *in0,unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_logic_lshift_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(129);
    pint_lshift_char<_nxz>(out, in0, *in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_arithmetic_lshift_char(unsigned char *out, const unsigned char *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_arithmetic_lshift_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(130);
    pint_lshift_char<_nxz>(out, in0, *in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_c_logic_lshift_char(unsigned char *out, const unsigned char *in0,unsigned int in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_c_logic_lshift_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(131);
    pint_lshift_char<_nxz>(out, in0, in1, size);
}

/*void pint_logical_equality_char(unsigned char *out, unsigned char *in0,unsigned char *in1, unsigned int size){
    unsigned char mask_h,mask_l,in0_l,in0_h,in1_l,in1_h;
    mask_l = (1<<size)-1;
    mask_h = mask_l<<PINT_CHAR_OFFSET;

    in0_h = *in0&mask_h;
    in1_h = *in1&mask_h;
    if((in0_h>0)||(in1_h>0))
    {
        *out=0x11;
        return;
    }

    in0_l = *in0&mask_l;
    in1_l = *in1&mask_l;
    if(in0_l==in1_l)
    {
        *out = 1;
    }
    else{
        *out = 0;
    }
    
}

void pint_logical_equality_int(unsigned char *out, unsigned int *in0, unsigned int *in1, unsigned int size){

    unsigned int cnt,mod,in0_ml,in0_mh,in1_mh,in1_ml;
    cnt = size/PINT_INT_SIZE;
    mod = size%PINT_INT_SIZE;

    unsigned int out_Hbit_start = (size+PINT_INT_SIZE-1)/PINT_INT_SIZE;
    *out = 1;
    for(int i=0;i<cnt;i++)
    {
        if((in0[i+out_Hbit_start]>0)||(in1[i+out_Hbit_start]>0))
        {
            *out=0x11;
            return;
        }
    }
    if(mod>0)
    {
        in0_mh = in0[cnt+out_Hbit_start]&((1<<mod)-1);
        in1_mh = in1[cnt+out_Hbit_start]&((1<<mod)-1);
        if((in0_mh>0)||(in1_mh>0))
        {
            *out=0x11;
            return;
        }
    }
    for(int i=0;i<cnt;i++)
    {
        if(in0[i]!=in1[i])
        {
            *out=0;
            return;
        }
    }
    if(mod>0)
    {
        in0_ml = in0[cnt]&((1<<mod)-1);
        in1_ml = in1[cnt]&((1<<mod)-1);
        if((in0_ml!=in1_ml))
        {
            *out=0;
        }
    }
}*/
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_logical_equality_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_logical_equality_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(132);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        *out = (*in0 == *in1);
    }
    else{
        if (size == 1){
        unsigned char data0 = *in0;
        unsigned char data1 = *in1;
        *out = (data0 < 0x10) && (data1 < 0x10) ? (data0 == data1) : 0x11;
    }
    else{
        unsigned char nor_l,and_h, mask_l,in0_l,in0_h,in1_l,in1_h,in0_h_inv,in1_h_inv;
        mask_l = (1<<size)-1;
        //mask_h = mask_l<<PINT_CHAR_OFFSET;

        in0_h = *in0>>PINT_CHAR_OFFSET;
        in1_h = *in1>>PINT_CHAR_OFFSET;


        in0_h_inv = (~in0_h) & mask_l;
        in1_h_inv = (~in1_h) & mask_l;

        and_h = in1_h_inv & in0_h_inv;

        in0_l = *in0 & mask_l;
        in1_l = *in1 & mask_l;

        nor_l = in0_l ^ in1_l;

        if(and_h & nor_l)
        {
            *out = 0;
            return;
        }

        if((in0_h>0)||(in1_h>0))
        {
            *out=0x11;
            return;
        }

        *out = 1;
    }
}
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_logical_equality_int(unsigned char *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_logical_equality_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(133);

    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned int cnt = (size+PINT_INT_SIZE-1)/PINT_INT_SIZE;

        for (int i = 0; i < cnt; i++){
            if (in0[i] != in1[i]){
                *out = 0;
                return;
            }
        }
        *out = 1;
    }
    else{
        unsigned int cnt,mod,in0_l,in0_h,in1_h,in1_l,and_h,nor_l,t_val;
        cnt = size/PINT_INT_SIZE;
    mod = size%PINT_INT_SIZE;

    unsigned int out_Hbit_start = (size+PINT_INT_SIZE-1)/PINT_INT_SIZE;
    *out = 1;
    bool x_flag = false;
    for(int i=0;i<cnt;i++)
    {

        in0_h = in0[i+out_Hbit_start];
        in1_h = in1[i+out_Hbit_start];

        in0_h = ~in0_h;
        in1_h = ~in1_h;
        and_h = in0_h & in1_h;

        in0_l = in0[i];
        in1_l = in1[i];
        nor_l = in0_l ^ in1_l;

        if(and_h & nor_l)
        {
            *out = 0;
            return;
        }


        if((in0[i+out_Hbit_start]>0)||(in1[i+out_Hbit_start]>0))
        {
            x_flag = true;
        }
    }
    if(mod>0)
    {
        t_val = (1<<mod)-1;

        in0_h = in0[cnt+out_Hbit_start] & t_val;
        in1_h = in1[cnt+out_Hbit_start] & t_val;

        in0_h = ~in0_h;
        in1_h = ~in1_h;
        and_h = in0_h & in1_h;

        in0_l = in0[cnt] & t_val;
        in1_l = in1[cnt] & t_val;
        nor_l = in0_l ^ in1_l;

        if(and_h & nor_l)
        {
            *out = 0;
            return;
        }


        if((in0[cnt+out_Hbit_start]>0)||(in1[cnt+out_Hbit_start]>0))
        {
            x_flag = true;
        }
    }

    if(x_flag)
    {
        *out=0x11;
        return;
    }

    *out = 1;
}
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
bool pint_logical_equality_int_ret(unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_logical_equality_int_ret<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(134);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned int cnt = (size+PINT_INT_SIZE-1)/PINT_INT_SIZE;

        for (int i = 0; i < cnt; i++){
            if (in0[i] != in1[i]){
                return 0;
            }
        }
        return 1;
    }
    else{
        unsigned char out_t;
        pint_logical_equality_int(&out_t, in0, in1, size);
        if(out_t==1)
        {
            return 1;
        }
        else{
            return 0;
        }    
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
bool pint_logical_equality_int_value_ret(unsigned int *in0, unsigned int in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_logical_equality_int_value_ret<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(135);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        return *in0 == in1;
    }
    else{
        unsigned in[2];
        in[0] = in1;
        in[1] = 0;
        return pint_logical_equality_int_ret(in0, in, size);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_case_equality_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_case_equality_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(136);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        *out = (*in0 == *in1);
    }
    else{
        unsigned char in0_l,in0_h,in1_l,in1_h,mask_l,mask_h;
        mask_l = (1<<size) - 1;
        mask_h = mask_l << PINT_CHAR_OFFSET;
        in0_l = *in0&mask_l;
        in0_h = *in0&mask_h;
        in1_l = *in1&mask_l;
        in1_h = *in1&mask_h;
        if((in0_l==in1_l)&&(in0_h==in1_h))
        {
            *out = 1;
        }
        else{
            *out = 0;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_case_inquality_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_case_inquality_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(137);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        *out = (*in0 != *in1);
    }
    else{
        unsigned char out_t;
        pint_case_equality_char(&out_t,in0,in1,size);
        *out = !out_t;
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_case_equality_int(unsigned char *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_case_equality_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(138);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned int cnt = (size+PINT_INT_SIZE-1)/PINT_INT_SIZE;

        for (int i = 0; i < cnt; i++){
            if (in0[i] != in1[i]){
                *out = 0;
                return;
            }
        }
        *out = 1;
    }
    else{
        unsigned int cnt = ((size + 0x1f) >> 5) *2;
        int i;
        for(i=0; i < cnt; i++){
            if(in0[i] != in1[i]){
                *out = 0;
                return;
            }
        }
        *out = 1;
        return;
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
bool pint_case_equality_int_ret(const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_case_equality_int_ret<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(139);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned int cnt = (size+PINT_INT_SIZE-1)/PINT_INT_SIZE;

        for (int i = 0; i < cnt; i++){
            if (in0[i] != in1[i]){
                return 0;
            }
        }
        return 1;
    }
    else{
        unsigned int cnt = ((size + 0x1f) >> 5) *2;
        int i;
        for(i=0; i < cnt; i++){
            if(in0[i] != in1[i])    return 0;
        }
        return 1;
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
bool pint_case_equality_int_value_ret(const unsigned int *in0, unsigned int in1, unsigned int size) {
    NCORE_PERF_MEASURE(pint_case_equality_int_value_ret<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(140);
    unsigned int cnt = ((size + 0x1f) >> 5) *2;
    int i;
    if (in0[0] != in1) {
        return 0;
    } else {
        for(i=1; i < cnt; i++){
            if(in0[i])return 0;
        }
    }
    return 1;
}

/*__constexpr*/ forceinline 
bool pint_equality_ignore_z_char_ret(const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_equality_ignore_z_char_ret, 2);
    NCORE_PERF_PINT_NET_SUMMARY(141);
    if __constexpr(0){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        return (*in0 == *in1);
    }
    else{
        unsigned char dat0 = *in0;
        unsigned char dat1 = *in1;
        if(dat0 != dat1){
            unsigned char mask_l = 1;
            unsigned char mask_h = 0x10;
            unsigned char diff   = dat0 ^ dat1;
            for(int i=0; i < size; i++, mask_l <<= 1, mask_h <<= 1){
                if(diff & (mask_l | mask_h)){
                    if((dat0 & mask_h) && (!(dat0 & mask_l)))   continue;
                    if((dat1 & mask_h) && (!(dat1 & mask_l)))   continue;
                    return 0;
                }
            }
        }
        return 1;
    }
}

/*__constexpr*/ forceinline 
bool pint_equality_ignore_xz_char_ret(const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_equality_ignore_xz_char_ret, 2);
    NCORE_PERF_PINT_NET_SUMMARY(142);
    if __constexpr(0){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        return (*in0 == *in1);
    }
    else{
        unsigned char dat0 = *in0;
        unsigned char dat1 = *in1;
        if(dat0 != dat1){
            unsigned char mask_l = 1;
            unsigned char mask_h = 0x10;
            unsigned char diff   = dat0 ^ dat1;
            for(int i=0; i < size; i++, mask_l <<= 1, mask_h <<= 1){
                if(diff & mask_l){
                    if((dat0 | dat1) & mask_h)  continue;
                    return 0;
                }
            }
        }
        return 1;   
    }
}

/*__constexpr*/ forceinline 
bool pint_equality_ignore_z_int_ret(const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_equality_ignore_z_int_ret, 2);
    NCORE_PERF_PINT_NET_SUMMARY(143);
    if __constexpr(0){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned int cnt = (size+PINT_INT_SIZE-1)/PINT_INT_SIZE;

        for (int i = 0; i < cnt; i++){
            if (in0[i] != in1[i]){
                return 0;
            }
        }
        return 1;
    }
    else{
    unsigned int cnt = size >> 5;
    unsigned int len = 32;
    if(size & 0x1f) cnt ++;
    unsigned int ld0, hd0, ld1, hd1, mask, diff;
    for(int i=0; i < cnt; i++){
        ld0 = in0[i];
        ld1 = in1[i];
        hd0 = in0[i+cnt];
        hd1 = in1[i+cnt];
        if((ld0 != ld1) || (hd0 != hd1)){
            mask = 1;
            if(i == cnt -1) len = size & 0x1f;
            diff = (ld0 ^ ld1) | (hd0 ^ hd1);
            for(int j=0; j < len; j++, mask <<= 1){
                if(diff & mask){
                    if((hd0 & mask) && (!(ld0 & mask))) continue;
                    if((hd1 & mask) && (!(ld1 & mask))) continue;
                    return 0;
                }
            }
        }
    }
    return 1;
}
}

/*__constexpr*/ forceinline 
bool pint_equality_ignore_xz_int_ret(const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_equality_ignore_xz_int_ret, 2);
    NCORE_PERF_PINT_NET_SUMMARY(144);
    if __constexpr(0){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned int cnt = (size+PINT_INT_SIZE-1)/PINT_INT_SIZE;
        for (int i = 0; i < cnt; i++){
            if (in0[i] != in1[i]){
                return 0;
            }
        }
        return 1;
    }
    else{
        unsigned int cnt = size >> 5;
        unsigned int len = 32;
        if(size & 0x1f) cnt ++;
        unsigned int ld0, hd0, ld1, hd1, mask, diff;
        for(int i=0; i < cnt; i++){
            ld0 = in0[i];
            ld1 = in1[i];
            if(ld0 != ld1){
                hd0 = in0[i+cnt];
                hd1 = in1[i+cnt];
                mask = 1;
                if(i == cnt -1) len = size & 0x1f;
                diff = ld0 ^ ld1;
                for(int j=0; j < len; j++, mask <<= 1){
                    if(diff & mask){
                        if((hd0 | hd1) & mask)  continue;
                        return 0;
                    }
                }
            }
        }
        return 1;   
    }
}

//  Function: "a!=b"
/*void pint_logical_inquality_char(unsigned char *out, unsigned char *in0, unsigned char *in1, unsigned int size){
    unsigned char d0 = *in0;
    unsigned char d1 = *in1;
    if((d0 | d1) & 0xf0)    *out = 0x11;    // x
    else if(d0 != d1)       *out = 1;       // 1
    else                    *out = 0;       // 0
}
void pint_logical_inquality_int(unsigned char *out, unsigned int *in0, unsigned int *in1, unsigned int size){
    unsigned int cnt = size >> 5;
    if(size & 0x1f) cnt ++;
    int i;
    for(i=0; i < cnt; i++){
        if(in0[i+cnt] | in1[i +cnt]){
            *out = 0x11;
            return;
        }
    }
    for(i =0; i < cnt; i++){
        if(in0[i] != in1[i]){
            *out = 1;
            return;
        }
    }
    *out = 0;
}*/
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_logical_inquality_char(unsigned char *out, const unsigned char *in0, const unsigned char *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_logical_inquality_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(145);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        *out = (*in0 != *in1);
    }
    else{
        unsigned char t_out;
        pint_logical_equality_char(&t_out, in0,in1, size);

        if(t_out == 0x11)    
            *out = 0x11;   
        else if(t_out == 0)       
            *out = 1;     
        else                    
            *out = 0;   
        //printf("inquality_char: t_out=%d,out=%d",t_out,*out);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_logical_inquality_int(unsigned char *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_logical_inquality_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(146);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size);
        check_is_xz(in1, size);
        unsigned int cnt = (size+PINT_INT_SIZE-1)/PINT_INT_SIZE;

        for (int i = 0; i < cnt; i++){
            if (in0[i] != in1[i]){
                *out = 1;
                return;
            }
        }
        *out = 0;
    }
    else{
        unsigned char t_out;
        pint_logical_equality_int(&t_out,in0,in1,size);
        if(t_out == 0x11)    
            *out = 0x11;   
        else if(t_out == 0)       
            *out = 1;     
        else                    
            *out = 0;   
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_case_inquality_int(unsigned char *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_case_inquality_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(147);
    unsigned char out_t;
    pint_case_equality_int(&out_t,in0,in1,size);
    *out = !out_t;
}

//  Function: "out = sel ? in1 : in0"
/*void pint_mux_char_char(unsigned char* out, unsigned char* in0, unsigned char* in1, unsigned char* sel, unsigned int size_d, unsigned int size_s){
    unsigned char s0 = *sel;
    if(s0 & 0xf0){
        unsigned char d0 = *in0;
        unsigned char d1 = *in1;
        unsigned char ret = 0;
        unsigned char mask = 0x11;
        int i;
        for(i=0; i < size_d; i++, mask <<= 1){
            if((d0 ^ d1) & mask)    ret |= mask;
            else    ret |= d0 & mask;
        }
        *out = ret;
    }else{
        if(s0)  *out = *in1;
        else    *out = *in0;
    }
}*/

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_mux_char_char(unsigned char* out, const unsigned char* in0, const unsigned char* in1, const unsigned char* sel, unsigned int size_d, unsigned int size_s){
    NCORE_PERF_MEASURE(pint_mux_char_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(148);

    if __constexpr(_nxz == true){
        check_is_xz(in0, size_d);
        check_is_xz(in1, size_d);
        check_is_xz(sel, size_s);
        *out = *sel ? *in0 : *in1;
    }
    else{
    unsigned char mask_l,in0_l,in0_h,in0_h_inv;
    mask_l = 0x0f;
    unsigned char s0 = *sel;
    //mask_h = mask_l<<PINT_CHAR_OFFSET;

    in0_h = s0>>PINT_CHAR_OFFSET;
    in0_h_inv = (~in0_h) & mask_l;
   
    
    in0_l = s0 & mask_l;
    if(in0_l & in0_h_inv)
    {
        *out = *in1;
    }
    else 
    {
        if(s0 & 0xf0)
        {
            unsigned char d0 = *in0;
            unsigned char d1 = *in1;
            unsigned char ret = 0;
            unsigned char mask = 0x11;
            int i;
            for(i=0; i < size_d; i++, mask <<= 1){
                if((d0 ^ d1) & mask)    ret |= mask;
                else    ret |= d0 & mask;
            }
            *out = ret;
        }
        else
        {
            *out = *in0;
        }
    }
}
}
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_mux_char_int(unsigned char* out, const unsigned char* in0, const unsigned char* in1, const unsigned int* sel, unsigned int size_d, unsigned int size_s){
    NCORE_PERF_MEASURE(pint_mux_char_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(149);
    if __constexpr(0){
        check_is_xz(in0, size_d);
        check_is_xz(in1, size_d);
        check_is_xz(sel, size_s);
        unsigned sel_cnt = (size_s + 31) / 32;

        for (int i = 0; i < sel_cnt; i++){
            if (sel[i]){
                *out = *in0;
                return;
            }
        }
        
        *out = *in1;
    }
    else{
        unsigned int scnt = size_s >> 5;
        if(size_s & 0x1f) scnt ++;
        int sv =0;  // The value of sel; [1], is_xz; [0], is_true
        int i;
        for(i=0; i < scnt; i++){
            if(sel[i +scnt]){
                sv = 2;
                break;
            }
        }
        if(sv & 2){
            unsigned char d0 = *in0;
            unsigned char d1 = *in1;
            unsigned char ret = 0;
            unsigned char mask = 0x11;
            for(i=0; i < size_d; i++, mask <<= 1){
                if((d0 ^ d1) & mask)    ret |= mask;
                else    ret |= d0 & mask;
            }
            *out = ret;        
        }else{
            for(i=0; i < scnt; i++){
                if(sel[i]){
                    sv = 1;
                    break;
                }
            }
            if(sv)  *out = *in1;
            else    *out = *in0;
        }
    }
}
/*void pint_mux_int_char(unsigned int* out, unsigned int* in0, unsigned int* in1, unsigned char* sel, unsigned int size_d, unsigned int size_s){
    NCORE_PERF_MEASURE(pint_mux_int_char, 2);
    unsigned char s0 = *sel;
    if(s0 & 0xf0){
        unsigned int cnt = size_d >> 5;
        if(size_d & 0x1f)  cnt++;
        unsigned int len = 32;
        unsigned int ret, d0, d1, mask;
        int i, j;
        for(i=0; i < cnt; i++){
            d0 = in0[i];
            d1 = in1[i];
            ret = 0;
            mask = 0x11;
            if(i == cnt -1) len = size_d & 0x1f;
            for(j=0; j < len; j++, mask <<= 1){
                if((d0 ^ d1) & mask)    ret |= mask;    // x
                else    ret |= d0 & mask;
            }
            out[i] = ret;
        }
    }else{
        if(s0)  pint_copy_int(out, in1, size_d);
        else    pint_copy_int(out, in0, size_d);
    }
}*/

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_mux_int_char(unsigned int* out, const unsigned int* in0, const unsigned int* in1, const unsigned char* sel, unsigned int size_d, unsigned int size_s){
    NCORE_PERF_MEASURE(pint_mux_int_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(150);
    if __constexpr(_nxz == true){
        check_is_xz(in0, size_d);
        check_is_xz(in1, size_d);
        check_is_xz(sel, size_s);
        unsigned data_cnt = (size_d + 31) / 32;
        unsigned int* data_p = *sel ? in0 : in1;
        
        for (int i = 0; i < data_cnt * LYQ_TWO; i++){
            out[i] = data_p[i];
        }
    }
    else{
    unsigned char s0 = *sel;

    unsigned char mask_l,s_l,s_h,s_h_inv;
    mask_l = 0x0f;
   

    s_h = s0>>PINT_CHAR_OFFSET;
    s_h_inv = (~s_h) & mask_l;
   
    
    s_l = s0 & mask_l;
    if(s_l & s_h_inv)
    {
        pint_copy_int(out, in1, size_d);
    }
    else
    {
        if(s0 & 0xf0)
        {
            //printf("s0=%d\n",s0);
            unsigned int cnt = size_d >> 5;
            if(size_d & 0x1f)  cnt++;
            unsigned int len = 32;
            unsigned int ret_l, ret_h,d0_l, d0_h, d1_l, d1_h,mask;
            int i, j;
            for(i=0; i < cnt; i++)
            {
                d0_l = in0[i];
                d0_h = in0[i+cnt];
                d1_l = in1[i];
                d1_h = in1[i+cnt];
                ret_l = 0;
                ret_h = 0;
                mask = 0x1;
                if((i == cnt -1) && (size_d & 0x1f))
                    len = size_d & 0x1f;
                for(j=0; j < len; j++, mask <<= 1)
                {
                    if(((d0_l ^ d1_l)  | (d0_h ^ d1_h)) & mask)  
                    {
                        ret_l |= mask;    // x
                        ret_h |= mask;    // x
                    }  
                    else
                    {
                        ret_l |= (d0_l & mask);
                        ret_h |= (d0_h & mask);
                    }    
                        
                }
                //printf("ret_l=%d, ret_h=%d\n", ret_l, ret_h);
                out[i] = ret_l;
                out[i+cnt] = ret_h;
            }
        }
        else
        {
            pint_copy_int(out, in0, size_d);
        }
    }
}
}
template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_mux_int_int(unsigned int* out, const unsigned int* in0, const unsigned int* in1, const unsigned int* sel, unsigned int size_d, unsigned int size_s){
    NCORE_PERF_MEASURE(pint_mux_int_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(151);
    if __constexpr(0){
        check_is_xz(in0, size_d);
        check_is_xz(in1, size_d);
        check_is_xz(sel, size_s);
        unsigned data_cnt = (size_d + 31) / 32;
        unsigned sel_cnt = (size_s + 31) / 32;
        int sel_1 = 0;
        for (int i = 0; i < sel_cnt; i++){
            if (sel[i]){
                sel_1 = 1;
                break;
            }
        }
        unsigned int* data_p = sel_1 ? in0 : in1;
        
        for (int i = 0; i < data_cnt * LYQ_TWO; i++){
            out[i] = data_p[i];
        }
    }
    else{
    unsigned int scnt = size_s >> 5;
    if(size_s & 0x1f) scnt ++;
    int sv =0;  // The value of sel; [1], is_xz; [0], is_true
    int i;
    for(i=0; i < scnt; i++){
        if(sel[i +scnt]){
            sv = 2;
            break;
        }
    }
    if(sv & 2){     // sel = x/z
        scnt = size_d >> 5;     // used as cnt  
        if(size_d & 0x1f)  scnt++;
        unsigned int len = 32;
        unsigned int d0, d1, mask;
        int j;
        for(i=0; i < scnt; i++){
            d0 = in0[i];
            d1 = in1[i];
            sv = 0;             // used as ret
            mask = 0x11;
            if((i == scnt -1)&&(size_d & 0x1f)) len = size_d & 0x1f;
            for(j=0; j < len; j++, mask <<= 1){
                if((d0 ^ d1) & mask)    sv |= mask;    // x
                else    sv |= d0 & mask;
            }
            out[i] = sv;
        }       
    }else{          // sel = 0/1
        for(i=0; i < scnt; i++){
            if(sel[i]){
                sv = 1;
                break;
            }
        }
        if(sv)  pint_copy_int(out, in1, size_d);
        else    pint_copy_int(out, in0, size_d);
        }
    }
}


template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_c_arithmetic_lshift_char(unsigned char *out, const unsigned char *in0,unsigned int in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_c_arithmetic_lshift_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(152);
    pint_lshift_char<_nxz>(out, in0, in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_logic_rshift_char(unsigned char *out, const unsigned char *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_logic_rshift_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(153);
    pint_rshift_char_u<_nxz>(out, in0, *in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_c_logic_rshift_char(unsigned char *out, const unsigned char *in0,unsigned int in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_c_logic_rshift_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(154);
    pint_rshift_char_u<_nxz>(out, in0, in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_arithmetic_rshift_char_u(unsigned char *out, const unsigned char *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_arithmetic_rshift_char_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(155);
    pint_rshift_char_u<_nxz>(out, in0, *in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_c_arithmetic_rshift_char_u(unsigned char *out, const unsigned char *in0,unsigned int in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_c_arithmetic_rshift_char_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(156);
    pint_rshift_char_u<_nxz>(out, in0, in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_arithmetic_rshift_char_s(unsigned char *out, const unsigned char *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_arithmetic_rshift_char_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(157);
    pint_rshift_char_s<_nxz>(out, in0, *in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_c_arithmetic_rshift_char_s(unsigned char *out, const unsigned char *in0, unsigned int in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_c_arithmetic_rshift_char_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(158);
    pint_rshift_char_s<_nxz>(out, in0, in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_logic_lshift_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_logic_lshift_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(159);
    pint_lshift_int<_nxz>(out, in0, *in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_arithmetic_lshift_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_arithmetic_lshift_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(160);
    pint_lshift_int<_nxz>(out, in0, *in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_c_logic_lshift_int(unsigned int *out, const unsigned int *in0, unsigned int in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_c_logic_lshift_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(161);
    pint_lshift_int<_nxz>(out, in0, in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_c_arithmetic_lshift_int(unsigned int *out, const unsigned int *in0, unsigned int in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_c_arithmetic_lshift_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(162);
    pint_lshift_int<_nxz>(out, in0, in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_logic_rshift_int(unsigned int *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_logic_rshift_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(163);
    pint_rshift_int_u<_nxz>(out, in0, *in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_c_logic_rshift_int(unsigned int *out, const unsigned int *in0, unsigned int in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_c_logic_rshift_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(164);
    pint_rshift_int_u<_nxz>(out, in0, in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_arithmetic_rshift_int_u(unsigned int *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_arithmetic_rshift_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(165);
    pint_rshift_int_u<_nxz>(out, in0, *in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_c_arithmetic_rshift_int_u(unsigned int *out, const unsigned int *in0, unsigned int in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_c_arithmetic_rshift_int_u<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(166);
    pint_rshift_int_u<_nxz>(out, in0, in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_arithmetic_rshift_int_s(unsigned int *out, const unsigned int *in0, const unsigned int *in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_arithmetic_rshift_int_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(167);
    pint_rshift_int_s<_nxz>(out, in0, *in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_c_arithmetic_rshift_int_s(unsigned int *out, const unsigned int *in0, unsigned int in1, unsigned int size){
    NCORE_PERF_MEASURE(pint_c_arithmetic_rshift_int_s<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(168);
    pint_rshift_int_s<_nxz>(out, in0, in1, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_concat_repeat(void* out, const void* in0, unsigned size_out, unsigned size_in, unsigned offset, unsigned repeat)
{
    unsigned int this_offset = offset;
    int re;

    if (size_out <= 4) {
        for (re = 0; re < repeat; re++) {
            unsigned char in_data = *(unsigned char*)in0;
            *(unsigned char*)out |= in_data << this_offset;
            this_offset += size_in;
        }
    } else if (size_in <= 32) {
        if __constexpr(!_nxz) {
            int out_half_word_num = (size_out + 31) >> 5;
            for (re = 0; re < repeat; re++) {
                int start_word_idx = this_offset >> 5;
                int start_bit_idx = this_offset & 0x1f;
                unsigned bits_low;
                unsigned bits_high;
                if (size_in <= 4){
                    unsigned in_data = *(unsigned char*)in0;
                    bits_low = in_data & 0xf;
                    bits_high = in_data >> 4;
                }
                else{
                    bits_low = ((unsigned*)in0)[0];
                    bits_high = ((unsigned*)in0)[1];
                }
                if ((start_bit_idx + size_in) <= 32){
                    ((unsigned int*)out)[start_word_idx] |= bits_low << start_bit_idx;
                    ((unsigned int*)out)[out_half_word_num + start_word_idx] |= bits_high << start_bit_idx;
                }
                else{
                    int first_sec_bit_num = 32 - start_bit_idx;
                    ((unsigned int*)out)[start_word_idx] |= bits_low << start_bit_idx;
                    ((unsigned int*)out)[out_half_word_num + start_word_idx] |= bits_high << start_bit_idx;
                
                    int second_sec_bit_num = size_in - first_sec_bit_num;
                    ((unsigned int*)out)[start_word_idx + 1] |= bits_low >> first_sec_bit_num;
                    ((unsigned int*)out)[out_half_word_num + start_word_idx + 1] |= bits_high >> first_sec_bit_num;
                }
                this_offset += size_in;
            }
        } else {
            for (re = 0; re < repeat; re++) {
                int start_word_idx = this_offset >> 5;
                int start_bit_idx = this_offset & 0x1f;
                unsigned bits_low;
                if (size_in <= 4){
                    unsigned in_data = *(unsigned char*)in0;
                    bits_low = in_data;
                }
                else{
                    bits_low = ((unsigned*)in0)[0];
                }
                if ((start_bit_idx + size_in) <= 32){
                    ((unsigned int*)out)[start_word_idx] |= bits_low << start_bit_idx;
                }
                else{
                    int first_sec_bit_num = 32 - start_bit_idx;
                    ((unsigned int*)out)[start_word_idx] |= bits_low << start_bit_idx;
                
                    int second_sec_bit_num = size_in - first_sec_bit_num;
                    ((unsigned int*)out)[start_word_idx + 1] |= bits_low >> first_sec_bit_num;
                }
                this_offset += size_in;
            }
        }
    } else {
        for (re = 0; re < repeat; re++) {
            pint_set_subarray_int<_nxz>((unsigned int*)out, (unsigned int*)in0, this_offset, size_in, size_out);
            this_offset += size_in;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_concat(void* out, const void* in0, unsigned size_out, unsigned size_in, unsigned offset)
{
    NCORE_PERF_MEASURE(pint_concat<_nxz>, 2);

    if (size_out <= 4) {
        unsigned char in_data = *(unsigned char*)in0;
        *(unsigned char*)out |= in_data << offset;
    } else if (size_in <= 32) {
        if __constexpr(!_nxz) {
            int out_half_word_num = (size_out + 31) >> 5;
            int start_word_idx = offset >> 5;
            int start_bit_idx = offset & 0x1f;
            unsigned bits_low;
            unsigned bits_high;
            if (size_in <= 4){
                unsigned in_data = *(unsigned char*)in0;
                bits_low = in_data & 0xf;
                bits_high = in_data >> 4;
            }
            else{
                bits_low = ((unsigned*)in0)[0];
                bits_high = ((unsigned*)in0)[1];
            }
            if ((start_bit_idx + size_in) <= 32){
                ((unsigned int*)out)[start_word_idx] |= bits_low << start_bit_idx;
                ((unsigned int*)out)[out_half_word_num + start_word_idx] |= bits_high << start_bit_idx;
            }
            else{
                int first_sec_bit_num = 32 - start_bit_idx;
                ((unsigned int*)out)[start_word_idx] |= bits_low << start_bit_idx;
                ((unsigned int*)out)[out_half_word_num + start_word_idx] |= bits_high << start_bit_idx;
            
                int second_sec_bit_num = size_in - first_sec_bit_num;
                ((unsigned int*)out)[start_word_idx + 1] |= bits_low >> first_sec_bit_num;
                ((unsigned int*)out)[out_half_word_num + start_word_idx + 1] |= bits_high >> first_sec_bit_num;
            }
        } else {
            int start_word_idx = offset >> 5;
            int start_bit_idx = offset & 0x1f;
            unsigned bits_low;
            if (size_in <= 4){
                unsigned in_data = *(unsigned char*)in0;
                bits_low = in_data;
            }
            else{
                bits_low = ((unsigned*)in0)[0];
            }
            if ((start_bit_idx + size_in) <= 32){
                ((unsigned int*)out)[start_word_idx] |= bits_low << start_bit_idx;
            }
            else{
                int first_sec_bit_num = 32 - start_bit_idx;
                ((unsigned int*)out)[start_word_idx] |= bits_low << start_bit_idx;
            
                int second_sec_bit_num = size_in - first_sec_bit_num;
                ((unsigned int*)out)[start_word_idx + 1] |= bits_low >> first_sec_bit_num;
            }
        }
    } else {
        pint_set_subarray_int<_nxz>((unsigned int*)out, (unsigned int*)in0, offset, size_in, size_out);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_concat_value_repeat(void* out, unsigned int in0, unsigned size_out, unsigned size_in, unsigned offset, unsigned repeat)
{
    unsigned int this_offset = offset;
    int re;

    if (size_out <= 4) {
        for (re = 0; re < repeat; re++) {
            *(unsigned char*)out |= in0 << this_offset;
            this_offset += size_in;
        }
    } else if (size_in <= 32) {
        for (re = 0; re < repeat; re++) {
            int start_word_idx = this_offset >> 5;
            int start_bit_idx = this_offset & 0x1f;
            if ((start_bit_idx + size_in) <= 32){
                ((unsigned int*)out)[start_word_idx] |= in0 << start_bit_idx;
            }
            else{
                int first_sec_bit_num = 32 - start_bit_idx;
                ((unsigned int*)out)[start_word_idx] |= in0 << start_bit_idx;
            
                int second_sec_bit_num = size_in - first_sec_bit_num;
                ((unsigned int*)out)[start_word_idx + 1] |= in0 >> first_sec_bit_num;
            }
            this_offset += size_in;
        }
    } else {
        for (re = 0; re < repeat; re++) {
            pint_set_subarray_int_value<_nxz>((unsigned int*)out, (unsigned int)in0, this_offset, size_in, size_out);
            this_offset += size_in;
        }
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_concat_value(void* out, unsigned int in0, unsigned size_out, unsigned size_in, unsigned offset)
{
    NCORE_PERF_MEASURE(pint_concat_value<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(170);

    if (size_out <= 4) {
        *(unsigned char*)out |= in0 << offset;
    } else if (size_in <= 32) {
        int start_word_idx = offset >> 5;
        int start_bit_idx = offset & 0x1f;
        if ((start_bit_idx + size_in) <= 32){
            ((unsigned int*)out)[start_word_idx] |= in0 << start_bit_idx;
        }
        else{
            int first_sec_bit_num = 32 - start_bit_idx;
            ((unsigned int*)out)[start_word_idx] |= in0 << start_bit_idx;
        
            int second_sec_bit_num = size_in - first_sec_bit_num;
            ((unsigned int*)out)[start_word_idx + 1] |= in0 >> first_sec_bit_num;
        }
    } else {
        pint_set_subarray_int_value<_nxz>((unsigned int*)out, (unsigned int)in0, offset, size_in, size_out);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_sig_to_real_char(float* out, const unsigned char *in, unsigned size){
    NCORE_PERF_MEASURE(pint_sig_to_real_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(171);
    *out = (float)pint_get_value_char_u(in, size);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_sig_to_real_int(float* out, const unsigned int* in, unsigned size){
    NCORE_PERF_MEASURE(pint_sig_to_real_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(172);
    *out = (float)pint_get_value_int_u(in, size);
}

template<bool _nxz = false>  // _nxz = true
/*__constexpr*/ forceinline 
void pint_sig_to_real_value(float* out, unsigned int in, unsigned size){
    NCORE_PERF_MEASURE(pint_sig_to_real_value<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(173);
    unsigned int mask = (1<<size) - 1;
    *out = (float)(in & mask);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_real_to_sig_char(unsigned char* out, const float *in, unsigned size){
    NCORE_PERF_MEASURE(pint_real_to_sig_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(174);
    pint_assert(size <= 4, "%u", size);
    int value = (int)in[0];
    unsigned int mask = (1<<size) - 1;
    *out = value & mask;
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_real_to_sig_int(unsigned int* out, const float* in, unsigned size){
    NCORE_PERF_MEASURE(pint_real_to_sig_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(175);
    int value = (int)in[0];
    unsigned cnt = (size + 0x1f) >> 5;
    unsigned int mask = 0xFFFFFFFF;
    if (size < 32) {
        mask = (1<<size) - 1;
    }

    out[0] = value & mask;
    for (int i = 1; i < 2*cnt; i++) {
        out[i] = 0;
    }
}

/*__constexpr*/ forceinline 
unsigned copy_bit_tmp(unsigned char * src, unsigned src_off, unsigned char* dst, unsigned dst_off){
    NCORE_PERF_MEASURE(copy_bit_tmp, 2);
    NCORE_PERF_PINT_NET_SUMMARY(176);
    unsigned src_byte_off = src_off / 8;
    unsigned src_bit_off = src_off % 8;
    unsigned dst_byte_off = dst_off / 8;
    unsigned dst_bit_off = dst_off % 8;

    unsigned src_bit = (src[src_byte_off] >> src_bit_off) & 1;
    unsigned dst_bit = (dst[dst_byte_off] >> dst_bit_off) & 1;
    dst[dst_byte_off] &= ~(1 << dst_bit_off);
    dst[dst_byte_off] |= (src_bit << dst_bit_off);
    return src_bit != dst_bit;
}

/*__constexpr*/ forceinline 
unsigned get_bit_tmp(unsigned char * src, unsigned src_off, unsigned char* dst, unsigned dst_off){
    NCORE_PERF_MEASURE(get_bit_tmp, 2);
    NCORE_PERF_PINT_NET_SUMMARY(177);
    unsigned src_byte_off = src_off / 8;
    unsigned src_bit_off = src_off % 8;
    unsigned dst_byte_off = dst_off / 8;
    unsigned dst_bit_off = dst_off % 8;

    unsigned src_bit = (src[src_byte_off] >> src_bit_off) & 1;
    unsigned dst_bit = (dst[dst_byte_off] >> dst_bit_off) & 1;
    return src_bit + (dst_bit << 16);
}

/*__constexpr*/ forceinline 
int pint_copy_signal_common(const void* src, unsigned src_width, unsigned src_base, 
                                       void* dst, unsigned dst_width, unsigned dst_base, unsigned count){
    NCORE_PERF_MEASURE(pint_copy_signal_common, 2);
    NCORE_PERF_PINT_NET_SUMMARY(178);
    pint_assert((src_base < src_width)&&(src_base + count <= src_width), "%d, %d, %d", src_base, count, src_width);
    pint_assert((dst_base < dst_width)&&(dst_base + count <= dst_width), "%d, %d, %d", dst_base, count, dst_width);

    unsigned char* src_lbits_addr = (unsigned char*)src;
    unsigned char* src_hbits_addr = (unsigned char*)src + (src_width + 31) / 32 * 4;
    unsigned char* dst_lbits_addr = (unsigned char*)dst;
    unsigned char* dst_hbits_addr = (unsigned char*)dst + (dst_width + 31) / 32 * 4;
    int i = 0;
    int change_flag = 0;
    unsigned edge_tmp0 = 0;
    unsigned edge_tmp1 = 0;
    if (src_width <= 4){
        if (dst_width <= 4){
            edge_tmp0 = get_bit_tmp(src_lbits_addr, src_base, dst_lbits_addr, dst_base);
            edge_tmp1 = get_bit_tmp(src_lbits_addr, src_base + 4, dst_lbits_addr, dst_base + 4);
            for (i = 0; i < count; i++){
                change_flag |= copy_bit_tmp(src_lbits_addr, i + src_base, dst_lbits_addr, i + dst_base);
                change_flag |= copy_bit_tmp(src_lbits_addr, i + src_base + 4, dst_lbits_addr, i + dst_base + 4);
            }
        }
        else{
            edge_tmp0 = get_bit_tmp(src_lbits_addr, src_base, dst_lbits_addr, dst_base);
            edge_tmp1 = get_bit_tmp(src_lbits_addr, src_base + 4, dst_hbits_addr, dst_base);
            for (i = 0; i < count; i++){
                change_flag |= copy_bit_tmp(src_lbits_addr, i + src_base, dst_lbits_addr, i + dst_base);
                change_flag |= copy_bit_tmp(src_lbits_addr, i + src_base + 4, dst_hbits_addr, i + dst_base);
            }
        }
    }
    else{
        if (dst_width <= 4){
            edge_tmp0 = get_bit_tmp(src_lbits_addr, src_base, dst_lbits_addr, dst_base);
            edge_tmp1 = get_bit_tmp(src_hbits_addr, src_base, dst_lbits_addr, dst_base + 4);
            for (i = 0; i < count; i++){
                change_flag |= copy_bit_tmp(src_lbits_addr, i + src_base, dst_lbits_addr, i + dst_base);
                change_flag |= copy_bit_tmp(src_hbits_addr, i + src_base, dst_lbits_addr, i + dst_base + 4);
            }
        }
        else{
            edge_tmp0 = get_bit_tmp(src_lbits_addr, src_base, dst_lbits_addr, dst_base);
            edge_tmp1 = get_bit_tmp(src_hbits_addr, src_base, dst_hbits_addr, dst_base);
            for (i = 0; i < count; i++){
                change_flag |= copy_bit_tmp(src_lbits_addr, i + src_base, dst_lbits_addr, i + dst_base);
                change_flag |= copy_bit_tmp(src_hbits_addr, i + src_base, dst_hbits_addr, i + dst_base);
            }
        }
    }

    unsigned bitsrc = (edge_tmp0 & 0xffff) + ((edge_tmp1 & 0xffff) << 16);
    unsigned bitdst = ((edge_tmp0 & 0xffff0000) >> 16) + (edge_tmp1 & 0xffff0000);

    //printf("bitsrc = 0x%x, bitdst = 0x%x\n", bitsrc, bitdst);
    if (((bitsrc == 0)&&(bitdst == 1))||
        ((bitsrc == 0)&&(bitdst == 0x10001))||
        ((bitsrc == 0)&&(bitdst == 0x10000))||
        ((bitsrc == 0x10000)&&(bitdst == 1))||
        ((bitsrc == 0x10001)&&(bitdst == 1))){
        return pint_negedge;
    }
    else if (((bitsrc == 1)&&(bitdst == 0))||
            ((bitsrc == 1)&&(bitdst == 0x10001))||
            ((bitsrc == 1)&&(bitdst == 0x10000))||
            ((bitsrc == 0x10001)&&(bitdst == 0))||
            ((bitsrc == 0x10000)&&(bitdst == 0))){
        return pint_posedge;
    }
    else if (change_flag){
        return 3;
    }
    else{
        return 0;
    }
}

#if 1  // nxz
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

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_copy_char_char(const void* src, unsigned src_base, 
    void*dst, unsigned dst_base, unsigned count){
    NCORE_PERF_MEASURE(pint_copy_char_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(179);
    unsigned char src_value = *(unsigned char*)src;
    char to = ((src_value & 0xf) >> src_base) + ((src_value & 0xf0) >> src_base);
    pint_set_subarray_char((unsigned char*)dst, (unsigned char*)&to, dst_base, count, 4);
}


template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_copy_int_int(const void* src, unsigned src_width, unsigned src_base, 
    void*dst, unsigned dst_width, unsigned dst_base, unsigned count){
    NCORE_PERF_MEASURE(pint_copy_int_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(180);
    unsigned tmp_sig_half_word_num = (count + 31) / 32;
    unsigned tmp_p_src[tmp_sig_half_word_num * 2];
    pint_get_subarray_int_int(tmp_p_src, (unsigned*)src, src_base, count, src_width);
    
    if (count == dst_width) {
        pint_copy_int((unsigned*)dst, tmp_p_src, count);
    } else {
        pint_set_subarray_int((unsigned*)dst, tmp_p_src, dst_base, count, dst_width);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_copy_int_char(const void* src, unsigned src_width, unsigned src_base, 
    void*dst, unsigned dst_base, unsigned count){
    NCORE_PERF_MEASURE(pint_copy_int_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(181);
    pint_assert((dst_base < 4)&&(dst_base + count <= 4), "%d, %d", dst_base, count);
    
    unsigned tmp_value[2];
    pint_get_subarray_int_int(tmp_value, (unsigned*)src, src_base, count, src_width);
    
    unsigned mask = (1<<count) - 1;
    unsigned char to = (tmp_value[0] & mask) + ((tmp_value[1] & mask) << 4);
    pint_set_subarray_char((unsigned char*)dst, &to, dst_base, count, 4);
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_copy_char_int(const void* src, unsigned src_base, 
    void*dst, unsigned dst_width, unsigned dst_base, unsigned count){
    NCORE_PERF_MEASURE(pint_copy_char_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(182);
    pint_assert((src_base < 4)&&(src_base + count <= 4), "%d, %d", src_base, count);
    
    unsigned char mask_l = (1 << count) - 1;
    unsigned char mask_h = mask_l << 4;
    unsigned char src_value = *(unsigned char*)src;
    unsigned char to = ((src_value & 0xf0) >> src_base) | ((src_value & 0xf) >> src_base);
    unsigned src_value_int[2] = {(unsigned)(to&mask_l), (unsigned)((to&mask_h)>>4)};
    pint_set_subarray_int((unsigned int*)dst, src_value_int, dst_base, count, dst_width);
}

#if 1  // nxz
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

// width <= 16
template<bool _nxz = false>
forceinline
unsigned int pint_concat_ptr_to_int(const unsigned int *in)
{
    NCORE_PERF_MEASURE(pint_concat_ptr_to_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(183);
    unsigned int result;
    if __constexpr(!_nxz) {
        result = (in[0] & 0xFFFF);
        result = result | (in[1]  << 16);
    } else {
        result = (in[0] & 0xFFFF);
    }
    //printf("result=%u\n",result);
    return result;
}

//  Function:   get_value(exclude x, z);
//  Character:	3'b01z => 0, 3'b01x => 0;
template<bool _nxz = false>
/*__constexpr*/ forceinline 
int  pint_get_value_char_neg(unsigned char* in,unsigned char size){
    NCORE_PERF_MEASURE(pint_get_value_char_neg, 2);
    NCORE_PERF_PINT_NET_SUMMARY(184);
    if __constexpr(_nxz) {
        return 0;
    } else {
        unsigned char   d0 = *in;
        if(d0 & 0xf0)   return 0;
        else            return (int)d0 - (1 << size);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline 
int pint_get_value_int_neg(unsigned int* in,unsigned int size){
    NCORE_PERF_MEASURE(pint_get_value_int_neg, 2);
NCORE_PERF_PINT_NET_SUMMARY(185);
#ifdef CPU_MODE
    assert(size <= PINT_INT_SIZE);
#endif
    pint_assert(size <= PINT_INT_SIZE, "size is not valid");
    if __constexpr(_nxz) {
        return (size == 32 ? (int)in[0] : ((int)in[0] - (1 << size)));
    } else {
        return (in[1]>0)?0:(size == 32 ? (int)in[0] : ((int)in[0] - (1 << size)));
    }
}


template<bool _nxz = false>
/*__constexpr*/ forceinline 
void pint_sig_to_string_char(char* out, unsigned char in0, unsigned int size)
{
    unsigned char used_val = in0 & ((1<<size) -1);
    out[0] = used_val;
    out[1] = 0; //'\0'
}

template<bool _nxz = false>
/*__constexpr*/ forceinline
void pint_sig_to_string_int(char* out, unsigned int* in0, unsigned int size)
{
    unsigned int mask_in_l,tmp_val,cur_val,t_count;
    unsigned int int_cnt = (size + 0x1f) >> 5;
    unsigned int out_cnt = (size + 0x07) >> 3;
    unsigned int left_bit = size % 0x20;
    unsigned int i, j;

    t_count = (left_bit + 0x07) >> 3;
    if(left_bit>0){
        mask_in_l = (1<<left_bit)-1;
        tmp_val = (in0[int_cnt - 1] & mask_in_l);
        cur_val = tmp_val;
        for(int ii=0; ii<t_count; ii++){
            cur_val = tmp_val & 0xff;
            out[t_count-ii-1] = (unsigned char)cur_val;
            tmp_val = (tmp_val >> 8);
        }
    }
    //X/Z
    for (i = int_cnt; i < 2*int_cnt-1; i++) {
        if (in0[i]) {
            break;
        }
    }
    //'0'
    i = i - int_cnt;
    for (j = 0; j < i; j++) {
        if (in0[j] == 0) {
            break;
        }
    }
    for(; j>0; j--){
        int idx = 0;
        tmp_val = in0[j - 1];
        for (int i = 0; i < 4; i++) {
            cur_val = (tmp_val >> ((3-i)*8)) & 0xff;
            if (cur_val) {
                out[t_count+idx] = (unsigned char)cur_val;
                idx++;
            }
        }
        t_count += idx;        
    }
    out[t_count] = 0; //'\0'
}


template<bool _nxz = false>
/*__constexpr*/ forceinline
void pint_get_subarray_char_char(unsigned char* out, const unsigned char* in0, int base, int size, int size_in){
    NCORE_PERF_MEASURE(pint_get_subarray_char_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(9);
    if ((base >= 0)&&(base + size <= size_in)){
        if __constexpr(_nxz) {
            unsigned char mask = (1<<size) - 1;
            *out = (*in0 & (mask << base)) >> base;
        } else {
            unsigned char mask = (1<<size) - 1;
            mask = ((mask << PINT_CHAR_OFFSET) + mask)<<base;
            *out=(*in0&mask)>>base;
        }
    }
    else{
        pint_copy_bits_common(out, in0, base, size, size_in);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline
void pint_get_subarray_int_char(unsigned char* out, const unsigned int* in0, int base, int size, int size_in){  
    NCORE_PERF_MEASURE(pint_get_subarray_int_char<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(10);
    if ((base >= 0)&&(base + size <= size_in)){
        unsigned int cnt_in = (size_in + 0x1f) >> 5;
        unsigned int cnt_b = base >> 5;
        unsigned int mod_b = base & 0x1f;
        unsigned int mod = mod_b + size;
        unsigned int mask ,ret;
        mask = (1 << size) -1;
        ret  = (in0[cnt_b] >> mod_b) & mask;
        if __constexpr(!_nxz) {
            ret |= ((in0[cnt_b + cnt_in] >> mod_b) & mask) << 4;
        }
        if(mod > 32){
            mask = (1 << (mod & 0x1f)) -1;
            ret |= (in0[cnt_b +1] & mask) << (32 -mod_b);
            if __constexpr(!_nxz) {
                ret |= ((in0[cnt_b + 1 + cnt_in]) & mask) << (36 - mod_b);
            }
        }
        *out = ret;
    }
    else{
        pint_copy_bits_common(out, in0, base, size, size_in);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline
void pint_get_subarray_char_int(unsigned int* out, const unsigned char* in0, int base, int size, int size_in){  
    NCORE_PERF_PINT_NET_SUMMARY(5);
    pint_copy_bits_common(out, in0, base, size, size_in);
}


template<bool _nxz = false>
/*__constexpr*/ forceinline
void pint_get_subarray_int_int(unsigned int* out, const unsigned int* in0, int base, int size, int size_in) {
    NCORE_PERF_MEASURE(pint_get_subarray_int_int<_nxz>, 2);
    NCORE_PERF_PINT_NET_SUMMARY(11);
    if ((base >= 0)&&(base + size <= size_in)){
        unsigned int cnt_base, mod_base, cnt_size, mod_size, words_in, words_out;
        unsigned int endbit = base + size;
        unsigned mask;
        unsigned ii = 0;
        unsigned i = 0;

        if (endbit <= PINT_INT_SIZE) {
            mask = ((size == 32) ? PINT_INT_MASK : ((1<<size) - 1));
            words_in = (size_in + 0x1f) >> 5;
            out[0] = (in0[0]>>base) & mask;
            if __constexpr(!_nxz) {
                out[1] = (in0[words_in]>>base) & mask;
            }
        } else {
            cnt_base = base >> 5;
            mod_base = base & 0x1f;
            words_out = (size + 0x1f) >> 5;
            words_in = (size_in + 0x1f) >> 5;

            if (0 == mod_base) {
                if (1 == words_out) {
                    mask = ((size == 32) ? PINT_INT_MASK : ((1<<size) - 1));
                    out[0] = (in0[cnt_base]) & mask;
                    if __constexpr(!_nxz) {
                        out[1] = (in0[cnt_base + words_in]) & mask;
                    }
                } else {
                    for (i = 0; i < words_out - 1; i++) {
                        out[i] = in0[cnt_base + i];
                        if __constexpr(!_nxz) {
                            out[i + words_out] = in0[cnt_base + i + words_in];
                        }
                    }
                    unsigned size_mod = size & 0x1f;
                    mask = (size_mod ? (1 << size_mod) - 1 : PINT_INT_MASK);
                    out[i] = in0[cnt_base + i] & mask;
                    if __constexpr(!_nxz) {
                        out[i+words_out] = in0[cnt_base + i + words_in] & mask;
                    }
                }
            } else {
                if (1 == words_out) {
                    mask = ((size == 32) ? PINT_INT_MASK : ((1<<size) - 1));
                    out[0] = (in0[cnt_base]>>mod_base | (in0[cnt_base+1] << (PINT_INT_SIZE - mod_base))) & mask;
                    if __constexpr(!_nxz) {
                        out[1] = (in0[cnt_base+words_in]>>mod_base | (in0[cnt_base+1+words_in] << (PINT_INT_SIZE - mod_base))) & mask;
                    }
                } else {
                    for (i = 0; i < words_out - 1; i++) {
                        out[i] = in0[cnt_base+i]>>mod_base | (in0[cnt_base+i+1] << (PINT_INT_SIZE - mod_base));
                        if __constexpr(!_nxz) {
                            out[i+words_out] = in0[cnt_base+i+words_in]>>mod_base | (in0[cnt_base+i+1+words_in] << (PINT_INT_SIZE - mod_base));
                        }
                    }
                    unsigned size_mod = size & 0x1f;
                    mask = (size_mod ? (1 << size_mod) - 1 : PINT_INT_MASK);
                    if (size_mod > 0 && (size_mod + mod_base) <= 32) {
                        out[i] = (in0[cnt_base+i]>>mod_base) & mask;
                        if __constexpr(!_nxz) {
                            out[i+words_out] = (in0[cnt_base + i + words_in]>>mod_base) & mask;
                        }
                    } else {
                        out[i] = (in0[cnt_base+i]>>mod_base | (in0[cnt_base+i+1] << (PINT_INT_SIZE - mod_base))) & mask;
                        if __constexpr(!_nxz) {
                            out[i+words_out] = (in0[cnt_base+i+words_in]>>mod_base | (in0[cnt_base+i+1+words_in] << (PINT_INT_SIZE - mod_base))) & mask;
                        }
                    }
                }
            }
        }
    }else{
        pint_copy_bits_common(out, in0, base, size, size_in);
    }
}

template<bool _nxz = false>
/*__constexpr*/ forceinline
void pint_set_sig_zero(void* buf, unsigned width)
{
    if (width <= 4){
        *(unsigned char*)buf = 0;
    }
    else{
        int word_num = (width + 31) / 32 * 2;
        unsigned* int_p = (unsigned*)buf;

        for (int i = 0; i < word_num; i++){
            int_p[i] = 0;
        }
    }
}


