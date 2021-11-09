#include "../common/perf.h"
#include "../inc/pint_simu.h"
#include "../inc/pint_net.h"
#include "../inc/pint_vpi.h"
#include "../common/pint_perf_counter.h"
#include "../common/sys_args.h"

extern "C" void *memcpy(void *dest, const void *src, size_t len) {
    if (dest == nullptr || src == nullptr)
        return nullptr;
    void *res = dest;
    if (dest <= src || (char *)dest >= (char *)src + len) {
        while (len--) {
            *(char *)dest = *(char *)src;
            dest = (char *)dest + 1;
            src = (char *)src + 1;
        }
    } else {
        src = (char *)src + len - 1;
        dest = (char *)dest + len - 1;
        while (len--) {
            *(char *)dest = *(char *)src;
            dest = (char *)dest - 1;
            src = (char *)src - 1;
        }
    }
    return res;
}

int edge(char from, char to) {
    NCORE_PERF_MEASURE(edge, 12);
    NCORE_PERF_PINT_NET_SUMMARY(192);

    unsigned char f = from & 0x3;
    unsigned char t = to & 0x3;

    if (f == t) {
        return pint_nonedge;
    } else if ((f == BIT4_1) && (t == BIT4_0)) {
        return pint_negedge;
    } else if ((f == BIT4_0) && (t == BIT4_1)) {
        return pint_posedge;
    } else {
        if ((f == BIT4_0) && (t == BIT4_X) || (f == BIT4_0) && (t == BIT4_Z) ||
            (f == BIT4_X) && (t == BIT4_1) || (f == BIT4_Z) && (t == BIT4_1)) {
            return pint_posedge;
        } else if ((f == BIT4_1) && (t == BIT4_X) || (f == BIT4_1) && (t == BIT4_Z) ||
                   (f == BIT4_X) && (t == BIT4_0) || (f == BIT4_Z) && (t == BIT4_0)) {
            return pint_negedge;
        } else {
            return pint_nonedge;
        }
    }
}

int edge_char(char from, char to) {
    if (to == from) {
        return pint_nonedge;
    } else {
        to &= 0x11;
        from &= 0x11;
        if ((to > 0) && (from == 0)) {
            return pint_posedge;
        } else if ((to == 0) && (from > 0)) {
            return pint_negedge;
        } else if ((to != 1) && (from == 1)) {
            return pint_negedge;
        } else if ((to == 1) && (from != 1)) {
            return pint_posedge;
        } else {
            return pint_otherchange;
        }
    }
}

int edge_1bit(char from, char to) {
    if ((to > 0) && (from == 0)) {
        return pint_posedge;
    } else if ((to == 0) && (from > 0)) {
        return pint_negedge;
    } else if ((to != 1) && (from == 1)) {
        return pint_negedge;
    } else if ((to == 1) && (from != 1)) {
        return pint_posedge;
    } else if (to == from) {
        return pint_nonedge;
    } else {
        return pint_otherchange;
    }
}

int edge_int(unsigned *from, unsigned *to, unsigned size) {
    NCORE_PERF_MEASURE(edge_int, 12);
    NCORE_PERF_PINT_NET_SUMMARY(193);
    unsigned words = size / 32 + (size % 32 ? 1 : 0);
    unsigned f = (from[0] & 0x01) + ((from[words] & 0x01) << 1);
    unsigned t = (to[0] & 0x01) + ((to[words] & 0x01) << 1);
    if ((f == BIT4_0) && (t == BIT4_1) || (f == BIT4_0) && (t == BIT4_X) ||
        (f == BIT4_0) && (t == BIT4_Z) || (f == BIT4_X) && (t == BIT4_1) ||
        (f == BIT4_Z) && (t == BIT4_1)) {
        return pint_posedge;
    } else if ((f == BIT4_1) && (t == BIT4_0) || (f == BIT4_1) && (t == BIT4_X) ||
               (f == BIT4_1) && (t == BIT4_Z) || (f == BIT4_X) && (t == BIT4_0) ||
               (f == BIT4_Z) && (t == BIT4_0)) {
        return pint_negedge;
    } else {
        return pint_nonedge;
    }
}

#ifndef CPU_MODE

unsigned pint_enqueue_flag_set_any(unsigned short *buf, unsigned int func_id) // func_id bit  1
{
    NCORE_PERF_MEASURE(pint_enqueue_flag_set_any, 6);
    NCORE_PERF_PINT_NET_SUMMARY(195);
    unsigned int rs2;
    unsigned short val;
    pint_wait_write_done();

    if (func_id < 16) {
        val = 1 << func_id; //
        rs2 = (val << 16) | val;
        asm volatile("uap.xhum\t%1, %0"
                     : "+m"(buf[0]), "+r"(rs2)); // short  atomic flag confuluence
    } else {
        unsigned half_offset = func_id % 16;
        unsigned int offset = func_id >> 4;
        val = 1 << half_offset;
        rs2 = (val << 16) | val;
        asm volatile("uap.xhum\t%1, %0" : "+m"(buf[offset]), "+r"(rs2));
    }
    return rs2 & val;
}

void pint_enqueue_flag_clear_any(unsigned short *buf, unsigned int func_id) // bit 0
{
    NCORE_PERF_MEASURE(pint_enqueue_flag_clear_any, 6);
    NCORE_PERF_PINT_NET_SUMMARY(196);
    unsigned int rs2;
    if (func_id < 16) {
        rs2 = (1 << func_id) << 16;
        asm volatile("uap.xhum\t%1, %0" : "+m"(buf[0]), "+r"(rs2));
    } else {
        unsigned half_offset = func_id % 16;
        unsigned int offset = func_id >> 4;
        rs2 = (1 << half_offset) << 16;
        asm volatile("uap.xhum\t%1, %0" : "+m"(buf[offset]), "+r"(rs2));
    }
    pint_wait_write_done();
}
#endif

/*
void pint_add_list_to_active(pint_thread_t list, unsigned thread_num)
{
    NCORE_PERF_MEASURE(pint_add_list_to_active, 7);
    for (int i = 0; i < thread_num*2; i += 2)
    {
        if (!pint_enqueue_flag_set((unsigned int)list[i])) {
            pint_enqueue_thread(list[i+1], ACTIVE_Q);
        }
    }
}
*/

void pint_add_list_to_inactive(pint_thread_t list, unsigned thread_num) {
    NCORE_PERF_MEASURE(pint_add_list_to_inactive, 7);
    NCORE_PERF_PINT_NET_SUMMARY(197);
    if (thread_num == 1) {
        pint_wait_write_done();
        if (!pint_enqueue_flag_set((unsigned int)list[0])) {
            int enq_idx = pint_atomic_add(&pint_q_head[INACTIVE_Q], 1);
            pint_q_base[INACTIVE_Q][enq_idx] = list[1];
        }
    } else {
        pint_thread thread[thread_num];
        int cnt = 0;
        pint_wait_write_done();
        for (int i = 0; i < thread_num * 2; i += 2) {
            if (!pint_enqueue_flag_set((unsigned int)list[i])) {
                thread[cnt++] = list[i + 1];
            }
        }
        int enq_idx = pint_atomic_add(&pint_q_head[INACTIVE_Q], cnt);
        for (int i = 0; i < cnt; i++) {
            pint_q_base[INACTIVE_Q][enq_idx++] = thread[i];
        }
    }
}

void pint_add_arr_list_to_inactive(pint_thread_t list, unsigned thread_num, unsigned word) {
    NCORE_PERF_MEASURE(pint_add_arr_list_to_inactive, 7);
    NCORE_PERF_PINT_NET_SUMMARY(205);
    pint_thread thread[thread_num];
    int cnt = 0;
    pint_wait_write_done();

    for (int i = 0; i < thread_num * 3; i += 3) {
        if ((list[i + 1] == word || list[i + 1] == 0xffffffff) &&
            !pint_enqueue_flag_set((unsigned int)list[i])) {
            thread[cnt++] = list[i + 2];
        }
    }
    int enq_idx = pint_atomic_add(&pint_q_head[INACTIVE_Q], cnt);
    for (int i = 0; i < cnt; i++) {
        pint_q_base[INACTIVE_Q][enq_idx++] = thread[i];
    }
}

void pint_set_event_flag(unsigned *list) {
    // unsigned num = *list;
    // unsigned *flag = (unsigned *)(list + 1);
    // for (int i = 0; i < num; i++)
    // {
    //     *(unsigned*)flag[i] = 0x1;
    // }
    // pint_wait_write_done();
    unsigned num = *list;
    unsigned *flag = list + 1;
    for (int i = 0; i < num; i++) {
        pint_event_flag[flag[i]] = 1;
    }
}

void pint_handle_future_nb(pint_future_nbassign_t nb, unsigned nb_id) {
    pint_copy_sig_out_range_noedge(nb->src, nb->width, nb->base, nb->dst, nb->width, nb->base,
                                   nb->assign_width);
    if (nb->array_idx < 0) {
        pint_enqueue_nb_thread_by_idx(&pint_nb_list[nb_id]);
    } else {
        pint_enqueue_nb_thread_array_by_idx(nb_id, nb->array_idx);
    }
    pint_mem_free((unsigned int *)nb->src);
}

void pint_enqueue_nb_thread_by_idx(pint_nbassign_t nb) {
    // pint_nbassign_t nb = &pint_nb_list[id];
    unsigned nb_id = nb->nbflag_id;
    if (!pint_enqueue_flag_set(nb_id)) {
#ifndef CPU_MODE
        int enq_idx = pint_atomic_add(&pint_q_head[NBASSIGN_Q], 1);
        pint_nbq_base[enq_idx] = (pint_nbassign_t)nb;
#else
        pint_nbq_base[pint_q_head[NBASSIGN_Q]++] = (pint_nbassign_t)nb;
#endif
    }
}

void pint_enqueue_nb_thread_array_by_idx(unsigned id, unsigned array_idx) {
    pint_nbassign_array_t nb = &pint_nb_array_list[id];
    unsigned nb_id = nb->nbflag_id;

    int updt_flag = 1;
    for (int i = 0; i < nb->updt_cnt; i++) {
        if (nb->updt_idx[i] == array_idx) {
            updt_flag = 0;
            break;
        }
    }

    if ((updt_flag) && (nb->updt_cnt < 16)) {
        nb->updt_idx[nb->updt_cnt++] = array_idx;
    }

    if (!pint_enqueue_flag_set(nb_id)) {
#ifndef CPU_MODE
        int enq_idx = pint_atomic_add(&pint_q_head[NBASSIGN_Q], 1);
        pint_nbq_base[enq_idx] = (pint_nbassign_t)nb;
#else
        pint_nbq_base[pint_q_head[NBASSIGN_Q]++] = (pint_nbassign_t)nb;
#endif
    }
}

pint_nbassign_t pint_dequeue_nb_thread(void) {
    NCORE_PERF_MEASURE(pint_dequeue_nb_thread, 8);
    NCORE_PERF_PINT_NET_SUMMARY(199);
    int deq_idx = pint_atomic_add(&pint_q_head[NBASSIGN_Q], -1);
    if (deq_idx > 0) {
        return pint_nbq_base[deq_idx - 1];
    } else {
        return NULL;
    }
}

/**
 * e_list has two types:
 * 1. signal array size is 1, e_list = [s_list, pos_num, any_num, neg_num, move_event_xxx...]
 * 2. array signal, e_list =  [pos_num, any_num, neg_num, sen_num, move_event_xxx...]
 */
void pint_process_event_list(pint_thread_t e_list, unsigned e_type) {
    NCORE_PERF_MEASURE(pint_process_event_list, 9);
    NCORE_PERF_PINT_NET_SUMMARY(200);
    if (e_list[0]) {
        pint_thread_t s_list = (pint_thread_t)e_list[0];
        unsigned thread_num;
        unsigned static_table_size;
        pint_thread_t static_table;
        pint_thread_t list = s_list + 3;
        if (e_type == pint_posedge) {
            thread_num = (unsigned int)s_list[0] + (unsigned int)s_list[1]; // pos_num + any_num ?
        } else if (e_type == pint_negedge) {
            list += ((unsigned int)s_list[0] * 2);
            thread_num = (unsigned int)s_list[1] + (unsigned int)s_list[2];
        } else {
            // any
            list += ((unsigned int)s_list[0] * 2);
            thread_num = (unsigned int)s_list[1];
        }
        if (thread_num) {
#ifndef CPU_MODE
            int enq_idx =
                (pint_atomic_add(&static_event_list_table_num, (thread_num << 12) + 1)) & 0xfff;
            static_event_list_table[enq_idx] = list;
            static_event_list_table_size[enq_idx] = thread_num;
#else
            static_event_list_table[static_event_list_table_num] = list;
            static_event_list_table_size[static_event_list_table_num] = thread_num;
            static_event_list_table_num++;
#endif
        }
    }

    if (e_list[1]) {
        unsigned thread_num;
        pint_thread_t list;
        if (e_type == pint_posedge) {
            list = e_list + 5;
            thread_num = (unsigned int)e_list[2] + (unsigned int)e_list[3]; // pos_num + any_num ?
        } else if (e_type == pint_negedge) {
            list = e_list + 5 + ((unsigned int)e_list[2]);
            thread_num = (unsigned int)e_list[3] + (unsigned int)e_list[4];
        } else {
            // any
            list = e_list + 5 + ((unsigned int)e_list[2]);
            thread_num = (unsigned int)e_list[3];
        }
        for (int i = 0; i < thread_num; i++) {
            list[i]();
        }
    }
}

void pint_process_arr_event_list(pint_thread_t e_list, unsigned e_type, unsigned cnt2) {
    NCORE_PERF_MEASURE(pint_process_arr_event_list, 9);
    NCORE_PERF_PINT_NET_SUMMARY(206);
    unsigned thread_num;
    pint_thread_t list;
    if (e_type == pint_posedge) {
        list = e_list + 3;
        thread_num = (unsigned int)e_list[0] + (unsigned int)e_list[1]; // pos_num + any_num ?
    } else if (e_type == pint_negedge) {
        list = e_list + 3 + ((unsigned int)e_list[0]) * 2;
        thread_num = (unsigned int)e_list[1] + (unsigned int)e_list[2];
    } else {
        // any
        list = e_list + 3 + ((unsigned int)e_list[0]) * 2;
        thread_num = (unsigned int)e_list[1];
    }
    for (int i = 0; i < thread_num * 2; i += 2) {
        if (list[i] == cnt2 || list[i] == 0xffffffff) {
            list[i + 1]();
        }
    }
}

void pint_nb_assign(pint_nbassign_t nb) {
    NCORE_PERF_MEASURE(pint_nb_assign, 10);
    NCORE_PERF_PINT_NET_SUMMARY(201);
    pint_enqueue_flag_clear(nb->nbflag_id);

    if (nb->size <= 4) { // uap.xw
        unsigned char src = *nb->src;
        unsigned char dst = *nb->dst;
        if (src != dst) {
            if (nb->p_list) {
                pint_set_event_flag(nb->p_list);
            }
            if (nb->e_list) {
                pint_process_event_list(nb->e_list, edge((dst & 0x1) + ((dst & 0x10) >> 3),
                                                         (src & 0x1) + ((src & 0x10) >> 3)));
            }

            *(unsigned char *)nb->dst = src;

            if (nb->l_list) {
                pint_add_list_to_inactive(nb->l_list + 1, (unsigned int)nb->l_list[0]);
            }

            if (nb->vcd_dump_symbol) {
                pint_vcddump_char(src, nb->size, (const char *)nb->vcd_dump_symbol);
            }
#ifdef PINT_PLI_MODE
            if (nb->is_output_interface) {
                pint_update_output_value_change(nb->output_port_id);
            }
#endif
        }
    } else { // ptr
        if (!pint_case_equality_int_ret((unsigned *)nb->src, (unsigned *)nb->dst, nb->size)) {
            unsigned words = nb->size / 32 + (nb->size % 32 ? 1 : 0);
            if (nb->p_list) {
                pint_set_event_flag(nb->p_list);
            }
            if (nb->e_list) {
                pint_process_event_list(
                    nb->e_list,
                    edge(((*(unsigned *)nb->dst) & 0x1) + (((unsigned *)nb->dst)[words] & 0x01) * 2,
                         ((*(unsigned *)nb->src) & 0x1) +
                             (((unsigned *)nb->src)[words] & 0x01) * 2));
            }
            pint_copy_int((unsigned *)nb->dst, (unsigned *)nb->src, nb->size);
            if (nb->l_list) {
                pint_add_list_to_inactive(nb->l_list + 1, (unsigned int)nb->l_list[0]);
            }

            if (nb->vcd_dump_symbol) {
                pint_vcddump_int((unsigned *)nb->dst, nb->size, (const char *)nb->vcd_dump_symbol);
            }
#ifdef PINT_PLI_MODE
            if (nb->is_output_interface) {
                pint_update_output_value_change(nb->output_port_id);
            }
#endif
        }
    }
}

void pint_nb_array_process(pint_nbassign_array_t nb, unsigned word) {
    NCORE_PERF_MEASURE(pint_nb_array_process, 11);
    NCORE_PERF_PINT_NET_SUMMARY(202);
    if (nb->size <= 4) { // uap.xw
        unsigned char src = (nb->src)[word];
        unsigned char dst = (nb->dst)[word];
        if (src != dst) {
            if (nb->p_list) {
                pint_set_event_flag(nb->p_list);
            }

            if (nb->e_list) {
                pint_process_arr_event_list(nb->e_list, edge((dst & 0x1) + ((dst & 0x10) >> 3),
                                                             (src & 0x1) + ((src & 0x10) >> 3)),
                                            word);
            }
            (nb->dst)[word] = src;

            if (nb->l_list) {
                pint_add_arr_list_to_inactive(nb->l_list + 1, (unsigned int)nb->l_list[0], word);
            }
        }
    } else { // ptr
        unsigned cnt2 = ((nb->size + 31) / 32) * 2;
        unsigned index = word * cnt2;
        if (!pint_case_equality_int_ret(&(((unsigned *)nb->dst)[index]),
                                        &(((unsigned *)nb->src)[index]), nb->size)) {
            unsigned words = nb->size / 32 + (nb->size % 32 ? 1 : 0);
            if (nb->p_list) {
                pint_set_event_flag(nb->p_list);
            }
            if (nb->e_list) {
                pint_process_arr_event_list(
                    nb->e_list, edge((((unsigned *)nb->dst)[index] & 0x1) +
                                         ((((unsigned *)nb->dst)[index + words] & 0x01) * 2),
                                     (((unsigned *)nb->src)[index] & 0x1) +
                                         ((((unsigned *)nb->src)[index + words] & 0x01) * 2)),
                    word);
            }
            pint_copy_int(&(((unsigned *)nb->dst)[index]), &(((unsigned *)nb->src)[index]),
                          nb->size);
            if (nb->l_list) {
                pint_add_arr_list_to_inactive(nb->l_list + 1, (unsigned int)nb->l_list[0], word);
            }
        }
    }
}

void pint_nb_assign_array(pint_nbassign_array_t nb) {
    NCORE_PERF_MEASURE(pint_nb_assign_array, 10);
    NCORE_PERF_PINT_NET_SUMMARY(203);
    unsigned updt_cnt = nb->updt_cnt;
    nb->updt_cnt = 0;
    pint_enqueue_flag_clear(nb->nbflag_id);
    // update all elements
    if ((updt_cnt >= 16) || (updt_cnt == nb->array_size)) {
        for (unsigned i = 0; i < nb->array_size; i++) {
            pint_nb_array_process(nb, i);
        }
    } else {
        for (unsigned i = 0; i < updt_cnt; i++) {
            pint_nb_array_process(nb, nb->updt_idx[i]);
        }
    }
}

void execute_nbassign_q_thread(void) {
#ifdef PROF_ON
    PROF_BEGIN(execute_nbassign_q_thread, 187);
#endif
    pint_nbassign_t nb = pint_dequeue_nb_thread();
    while (NULL != nb) {
        if (nb->type == NB_SIGNAL) {
            pint_nb_assign(nb);
        } else {
            pint_nb_assign_array((pint_nbassign_array_t)nb);
        }
        nb = pint_dequeue_nb_thread();
    }
#ifdef PROF_ON
    PROF_END();
#endif
    // #ifdef PROF_ON
    //   }
    // #endif
    pint_dcache_discard();
}

#ifdef CPU_MODE
void execute_nbassign_q_thread_cpu(void) {
    unsigned nb_size = pint_q_head[NBASSIGN_Q];
    for (int i = 0; i < nb_size; i++) {
        pint_nbassign_t nb = pint_nbq_base[i];
        if (NULL != nb) {
            if (nb->type == NB_SIGNAL) {
                pint_nb_assign(nb);
            } else {
                pint_nb_assign_array((pint_nbassign_array_t)nb);
            }
        }
    }
}
#endif

#ifndef CPU_MODE
extern "C" void *memset(void *s, int c, size_t n) {
    char *func = (char *)s;
    if (s == NULL || n < 0)
        return NULL;
    while (n--) {
        *func++ = c;
    }
    return s;
}
#endif

inline void array_init(const int total_cores, const int gtid) {
    for (int i = gtid; i < pint_array_init_num; i += total_cores) {
        unsigned bit_l = array_info[i].bit_val & 0x01;
        unsigned bit_h = array_info[i].bit_val & 0x02;
        unsigned width = array_info[i].width;
        unsigned size = array_info[i].size;
        unsigned val = array_info[i].bit_val;

        if (width <= 4) {
            unsigned char *sig_ch = (unsigned char *)array_info[i].sig;
            unsigned char char_mask = (0x01 << width) - 1;
            unsigned char char_val;
            switch (val) {
            case 3:
                char_val = char_mask | char_mask << 4;
                break;
            case 2:
                char_val = char_mask << 4;
                break;
            case 1:
                char_val = char_mask;
                break;
            case 0:
                char_val = 0;
                break;
            default:
                char_val = 0;
            }
            for (int j = 0; j < size; j++) {
                sig_ch[j] = char_val;
            }
        } else if (width <= 32) {
            unsigned *sig_int = (unsigned *)array_info[i].sig;
            unsigned mask = width == 32 ? 0xffffffff : ((0x01 << width) - 1);
            unsigned int_val_l = bit_l ? mask : 0;
            unsigned int_val_h = bit_h ? mask : 0;
            unsigned max = size * 2;
            for (int j = 0; j < max; j += 2) {
                sig_int[j] = int_val_l;
                sig_int[j + 1] = int_val_h;
            }
        } else if (width % 32 == 0) {
            unsigned *sig_int = (unsigned *)array_info[i].sig;
            unsigned mask = width == 32 ? 0xffffffff : ((0x01 << width) - 1);
            unsigned int_val_l = bit_l ? mask : 0;
            unsigned int_val_h = bit_h ? mask : 0;
            unsigned cnt = (width + 31) >> 5;
            unsigned len = cnt * 2;
            for (int k = 0; k < size; k++) {
                for (int j = 0; j < cnt; j++) {
                    sig_int[j] = int_val_l;
                    sig_int[j + cnt] = int_val_h;
                }
                sig_int = sig_int + len;
            }
        } else {
            unsigned *sig_int = (unsigned *)array_info[i].sig;
            unsigned sig_cnt = (width + 31) >> 5;
            unsigned end = width - 1;
            unsigned end_mod = end & 0x1f;
            unsigned end_mask = ((unsigned long long)0x01 << (end_mod + 1)) - 1;
            unsigned whole_cnt = sig_cnt;

            if (end_mask != 0xffffffff) {
                unsigned int_val_l = bit_l ? end_mask : 0;
                unsigned int_val_h = bit_h ? end_mask : 0;
                unsigned *sig_int_this = sig_int + sig_cnt - 1;
                for (int j = sig_cnt - 1; j < size; j++) {
                    sig_int_this[0] = int_val_l;
                    sig_int_this += sig_cnt;
                    sig_int_this[0] = int_val_h;
                    sig_int_this += sig_cnt;
                }
                whole_cnt -= 1;
            }

            if (whole_cnt > 0) {
                unsigned int_val_l = bit_l ? 0xffffffff : 0;
                unsigned int_val_h = bit_h ? 0xffffffff : 0;
                unsigned *sig_int_this = sig_int;
                for (int j = 0; j < size; j++) {
                    for (int k = 0; k < whole_cnt; k++) {
                        sig_int_this[k] = int_val_l;
                    }
                    sig_int_this += sig_cnt;
                    for (int k = 0; k < whole_cnt; k++) {
                        sig_int_this[k] = int_val_h;
                    }
                    sig_int_this += sig_cnt;
                }
            }
        }
    }
}

void pint_array_init(void) { array_init(1, 0); }

void pint_array_init_parallel(void) {
    const int total_cores = pint_core_num();
    const int gtid = pint_core_id();
    array_init(total_cores, gtid);
    pint_dcache_discard();
}

extern int pint_sys_args_num;
extern int *pint_sys_args_off;
extern char *pint_sys_args_buff;

int pint_get_test_plusargs(const char *str) {
    int match_flag;
    int j;
    int i;
    char *arg_str;
    int sys_args_num = pint_sys_args_num;
    for (i = 0; i < sys_args_num; i++) {
        arg_str = pint_sys_args_buff + pint_sys_args_off[i];
        j = 0;
        match_flag = 1;
        while (str[j] != 0) {
            if (str[j] != arg_str[j]) {
                match_flag = 0;
                break;
            }
            j++;
        }
        if (match_flag) {
            return 1;
        }
    }
    return 0;
}

int pint_get_value_plusargs(const char *str, char format, void *dst, unsigned width) {
    int match_flag;
    int j;
    int i;
    char tmp_char;
    char *arg_str;
    int sys_args_num = pint_sys_args_num;
    for (i = 0; i < sys_args_num; i++) {
        arg_str = pint_sys_args_buff + pint_sys_args_off[i];
        j = 0;
        match_flag = 1;
        while (str[j] != 0) {
            if (str[j] != arg_str[j]) {
                match_flag = 0;
                break;
            }
            j++;
        }
        if (match_flag) {
            switch (format) {
            case 'd':
            case 'o':
            case 'h': {
                int scale = (format == 'd') ? 10 : ((format == 'h') ? 16 : 8);
                unsigned long long data = 0;

                if ((arg_str[j] == '0') && ((arg_str[j + 1] == 'x') || (arg_str[j + 1] == 'X'))) {
                    j += 2;
                }

                while (arg_str[j] != 0) {
                    tmp_char = arg_str[j];
                    if ((tmp_char >= '0') && (tmp_char <= '9')) {
                        data = data * scale + arg_str[j] - '0';
                    } else if ((tmp_char >= 'a') && (tmp_char <= 'f')) {
                        data = data * scale + arg_str[j] - 'a' + 10;
                    } else if ((tmp_char >= 'A') && (tmp_char <= 'F')) {
                        data = data * scale + arg_str[j] - 'A' + 10;
                    } else {
                        printf("Error: Get unexpected char when execute $value$plusargs!\n");
                    }
                    j++;
                }

                if (width <= 4) {
                    unsigned mask = (1 << width) - 1;
                    data &= mask;
                    ((unsigned char *)dst)[0] = data;
                } else if (width <= 32) {
                    unsigned mask = (width == 32) ? 0xffffffff : ((1 << width) - 1);
                    data &= mask;
                    ((unsigned *)dst)[0] = *(unsigned *)&data;
                    ((unsigned *)dst)[1] = 0;
                } else {
                    unsigned mask = (width == 64) ? 0xffffffff : ((1 << (width - 32)) - 1);
                    int word_num = (width + 31) / 32 * 2;
                    ((unsigned *)dst)[0] = data & 0xffffffff;
                    ((unsigned *)dst)[1] = (data >> 32) & mask;

                    for (int word_i = 2; word_i < word_num; word_i++) {
                        ((unsigned *)dst)[word_i] = 0;
                    }
                }
                break;
            }
            case 'b': {
            }
            case 'e': {
            }
            case 'f': {
            }
            case 'g': {
            }
            case 's': {
                int char_num = 0;
                char *char_walk = &arg_str[j];
                while (*char_walk != 0) {
                    char_num++;
                    char_walk += 1;
                }

                if (char_num == 0) {
                    break;
                }
                char_walk = &arg_str[j];

                if (width > 4) {
                    int word_num = (width + 31) / 32;
                    int char_i;
                    for (int int_i = 0; int_i < word_num * 2; int_i++) {
                        ((int *)dst)[int_i] = 0;
                    }

                    if (width >= char_num * 8) {
                        for (char_i = 0; char_i < char_num; char_i++) {
                            ((char *)dst)[char_i] = char_walk[char_num - 1 - char_i];
                        }
                    } else {
                        int copy_char_num = width / 8;
                        int copy_bit_num = width % 8;

                        for (char_i = 0; char_i < copy_char_num; char_i++) {
                            ((char *)dst)[char_i] = char_walk[char_num - 1 - char_i];
                        }
                        if (copy_bit_num) {
                            unsigned mask = (1 << copy_bit_num) - 1;
                            ((char *)dst)[char_i] = char_walk[char_num - 1 - copy_char_num] & mask;
                        }
                    }
                } else {
                    unsigned mask = (1 << width) - 1;
                    ((char *)dst)[0] = char_walk[0] & mask;
                }
                break;
            }
            default: {
                printf("Error: $value$plusargs Not support this format %c yet!\n", format);
                break;
            }
            }
            break;
        }
    }

    return i < pint_sys_args_num;
}

//-----------------------pint memory management-----------------------
#ifndef CPU_MODE
struct pint_mem_alloc_tab_s *_mem_alloc_tab_base[2];
unsigned int *_mem_chunk_base[2];
unsigned int *_mem_subt_chunk_base;

int _mem_alloc_tab_head[2] __attribute__((section(".builtin_simu_buffer")));
int _mem_chunk_head[2] __attribute__((section(".builtin_simu_buffer")));
int _mem_cur_ind;
int _mem_subt_chunk_head __attribute__((section(".builtin_simu_buffer")));
int _mem_cross_t_count __attribute__((section(".builtin_simu_buffer")));

unsigned int _mem_tab_total_size;
unsigned int _mem_chunk_total_size;
unsigned int _mem_subt_chunk_total_size;
unsigned int pint_mem_compact_count;

unsigned int pint_align_cache_line(unsigned int addr) {
    return (addr + PINT_CACHE_LINE_SIZE - 1) / PINT_CACHE_LINE_SIZE * PINT_CACHE_LINE_SIZE;
}

/*
binary--->|---tab0---|---tab1---|---chunk0---|---chunk1---|---subt_chunk---|<---stack(3G+512M)
*/
void pint_mem_init() {
    _mem_cur_ind = 0;
    _mem_cross_t_count = 0;
    pint_mem_compact_count = 0;

    unsigned binary_len = *(unsigned int *)PINT_BINARY_LEN_ADDR;
    unsigned align_binary_len = pint_align_cache_line(binary_len);
    _mem_alloc_tab_base[0] = (struct pint_mem_alloc_tab_s *)align_binary_len;
    _mem_tab_total_size = PINT_MEM_TAB_MIN_SIZE;
    if (pint_block_size * sizeof(struct pint_mem_alloc_tab_s) > _mem_tab_total_size) {
        _mem_tab_total_size = pint_block_size * sizeof(struct pint_mem_alloc_tab_s);
    }
    _mem_alloc_tab_base[1] = (struct pint_mem_alloc_tab_s *)pint_align_cache_line(
        (unsigned int)_mem_alloc_tab_base[0] + _mem_tab_total_size);

    _mem_chunk_base[0] = (unsigned int *)pint_align_cache_line(
        (unsigned int)_mem_alloc_tab_base[1] + _mem_tab_total_size);
    _mem_chunk_total_size =
        (PINT_MEM_ALLOC_END - (unsigned int)_mem_chunk_base[0] - 4 * PINT_CACHE_LINE_SIZE) / 4;
    _mem_chunk_base[1] = (unsigned int *)pint_align_cache_line((unsigned int)_mem_chunk_base[0] +
                                                               _mem_chunk_total_size);
    _mem_subt_chunk_base = (unsigned int *)pint_align_cache_line((unsigned int)_mem_chunk_base[1] +
                                                                 _mem_chunk_total_size);
    _mem_subt_chunk_total_size = _mem_chunk_total_size * 2;
    printf("dmem: tab[%u] chunk[%u] block[%u]\n", _mem_tab_total_size, _mem_chunk_total_size,
           pint_block_size);

    for (int i = 0; i < 2; i++) {
        _mem_alloc_tab_head[i] = 0;
        _mem_chunk_head[i] = 0;
    }
    _mem_subt_chunk_head = 0;
}

unsigned int *pint_mem_alloc_subt(unsigned int alloc_byte) {
    unsigned int *p;
    int chunk_offset;

    if (alloc_byte > PINT_MEM_SINGLE_ALLOC_MAX_SIZE) {
        printf("ERROR:subt alloc_byte[%u] too large\n", alloc_byte);
        p = _mem_subt_chunk_base;
        return p;
    }

    chunk_offset = pint_atomic_add(&_mem_subt_chunk_head, alloc_byte);
    if (_mem_subt_chunk_head > _mem_subt_chunk_total_size) {
        printf("ERROR: malloc subt[chunk] out of memory head:%u base:%u\n",
               (unsigned int)_mem_subt_chunk_head, (unsigned int)_mem_subt_chunk_base);
        chunk_offset = 0;
    }
    p = (unsigned int *)((unsigned int)_mem_subt_chunk_base + chunk_offset);

    return p;
}

/*  --------
    | tab  |
    --------
    | ref  |
    --------
    | data |
    --------
*/
unsigned int *pint_mem_alloc(unsigned int n_byte, alloc_type type) {
    unsigned int *p;
    int chunk_offset;
    struct pint_mem_alloc_tab_s *tab;
    int tab_offset;
    unsigned int alloc_byte = ((n_byte + 3) / 4) * 4; // allign to 4byte

    if (SUB_T_CLEAR == type) {
        return pint_mem_alloc_subt(alloc_byte);
    }

    alloc_byte += 8; // tab addr + ref addr = 8byte
    if (alloc_byte > PINT_MEM_SINGLE_ALLOC_MAX_SIZE) {
        printf("ERROR:pint_mem_alloc alloc_byte[%u] too large, n_byte[%u]\n", alloc_byte, n_byte);
        p = _mem_chunk_base[_mem_cur_ind];
        return (p + 2);
    }

    chunk_offset = pint_atomic_add(&_mem_chunk_head[_mem_cur_ind], alloc_byte);
    if (_mem_chunk_head[_mem_cur_ind] > _mem_chunk_total_size) {
        printf("ERROR:pint_mem_alloc[chunk] out of memory head:%u base:%u\n",
               (unsigned int)_mem_chunk_head[_mem_cur_ind],
               (unsigned int)_mem_chunk_base[_mem_cur_ind]);
        chunk_offset = 0;
    }
    p = (unsigned int *)((unsigned int)_mem_chunk_base[_mem_cur_ind] + chunk_offset);

    tab_offset =
        pint_atomic_add(&_mem_alloc_tab_head[_mem_cur_ind], sizeof(struct pint_mem_alloc_tab_s));
    if (_mem_alloc_tab_head[_mem_cur_ind] > _mem_tab_total_size) {
        printf("ERROR:pint_mem_alloc[tab] out of memory head:%u base:%u\n",
               (unsigned int)_mem_alloc_tab_head[_mem_cur_ind],
               (unsigned int)_mem_alloc_tab_base[_mem_cur_ind]);
        tab_offset = 0;
    }
    tab = (struct pint_mem_alloc_tab_s *)((unsigned int)_mem_alloc_tab_base[_mem_cur_ind] +
                                          tab_offset);

    if (CROSS_T == type) {
        pint_atomic_add(&_mem_cross_t_count, 1);
    }

    tab->type = type;
    tab->n_byte = alloc_byte;
    tab->magic = PINT_MEM_MAGIC_IND;
    tab->p = p;

    *p = (unsigned int)(tab);
    *(p + 1) = 0;
    return (p + 2);
}

void pint_mem_free(unsigned int *p) {
    if (p >= _mem_chunk_base[_mem_cur_ind] &&
        p < (_mem_chunk_base[_mem_cur_ind] + _mem_chunk_head[_mem_cur_ind])) {
        if (CROSS_T == ((struct pint_mem_alloc_tab_s *)(*(p - 2)))->type) {
            pint_atomic_add(&_mem_cross_t_count, -1);
            ((struct pint_mem_alloc_tab_s *)(*(p - 2)))->type = 0;
        }
    }
}

void pint_mem_set_ref(unsigned int *p, unsigned int *ref) { *(p - 1) = (unsigned int)ref; }

void pint_mem_compact_kernel() {
    struct pint_mem_alloc_tab_s *old_tab;
    unsigned int *p;
    unsigned int *ref;

    int old = 1 - _mem_cur_ind;
    unsigned int tab_num = _mem_alloc_tab_head[old] / sizeof(struct pint_mem_alloc_tab_s);
    unsigned int core_id = pint_core_id();
    unsigned int mod = tab_num & 0xff;     // pint core num = 256, 0xff = 255
    unsigned int loop_size = tab_num >> 8; // pint core num = 256, >> 8 = / 256
    unsigned int offset;
    if (core_id < mod) {
        loop_size++;
        offset = core_id * loop_size;
    } else {
        offset = core_id * loop_size + mod;
    }

    for (int j = 0; j < loop_size; j++) {
        old_tab = &_mem_alloc_tab_base[old][offset + j];
        if ((old_tab->type == CROSS_T) && (old_tab->n_byte > 8) &&
            (old_tab->magic == PINT_MEM_MAGIC_IND)) {
            p = pint_mem_alloc(old_tab->n_byte - 8, CROSS_T);
            for (int i = 0; i < (old_tab->n_byte - 8) / 4; i++) {
                *(p + i) = *(old_tab->p + 2 + i);
            }
            ref = (unsigned int *)(*(old_tab->p + 1));
            if (ref) {
                pint_mem_set_ref(p, ref);
                *ref = (unsigned int)p;
            }
        }
    }

    pint_dcache_discard();
}

void pint_mem_compact() {
    if (0 == _mem_cross_t_count) {
        _mem_alloc_tab_head[_mem_cur_ind] = 0;
        _mem_chunk_head[_mem_cur_ind] = 0;
        return;
    }

    if (((_mem_chunk_head[_mem_cur_ind] << 1) < _mem_chunk_total_size) &&
        ((_mem_alloc_tab_head[_mem_cur_ind] << 1) < _mem_tab_total_size)) {
        return;
    }

    pint_mem_compact_count++;
    int old = _mem_cur_ind;
    _mem_cur_ind = 1 - _mem_cur_ind;
    _mem_cross_t_count = 0;
    pint_parallel_start((addr_t)pint_mem_compact_kernel);
    pint_dcache_discard();

    _mem_alloc_tab_head[old] = 0;
    _mem_chunk_head[old] = 0;
}

#if 0
void pint_print_mem()
{
    int i,j,k;
    printf("\n***********************************************\n");
    printf("subt_chunk       : 0x%x\n", (unsigned int)_mem_subt_chunk_base);
    printf("subt_chunk_head  : 0x%x\n\n", _mem_subt_chunk_head);
    unsigned int *p;
    for (i=0; i<2; i++) {
        printf("tab         : 0x%x\n", (unsigned int)_mem_alloc_tab_base[i]);
        printf("chunk       : 0x%x\n", (unsigned int)_mem_chunk_base[i]);
        printf("tab_head    : 0x%x\n", _mem_alloc_tab_head[i]);
        printf("chunk_head  : 0x%x\n", _mem_chunk_head[i]);
        for (j=0; (unsigned int)&_mem_alloc_tab_base[i][j] < ((unsigned int)_mem_alloc_tab_base[i] + _mem_alloc_tab_head[i]); j++) {
            printf("p:0x%x\tn_byte:%d\ttype:%d\n",
                   _mem_alloc_tab_base[i][j].p, _mem_alloc_tab_base[i][j].n_byte, _mem_alloc_tab_base[i][j].type);
            p = _mem_alloc_tab_base[i][j].p;
            printf("chunk tab:0x%x\tref:0x%x\t0x%x\t0x%x\t...0x%x\n",
                   *p, *(p+1), *(p+2), *(p+3), *(p+(_mem_alloc_tab_base[i][j].n_byte-8)/4-1));
        }        
        printf("\n-----------------------------------------------\n");
    }

    for (k = 0; k < _mem_subt_chunk_head / 4; k++) {
        p = _mem_subt_chunk_base + k;
        printf("subt chunk data[%u]: 0x%x\n", k, *p);
    }
}

void pint_mem_set_value(unsigned int *p, int n_byte, int value)
{
    for (int i=0; i<n_byte/4; i++) {
        *(p+i) = value;
    }
}

unsigned int *q;
void pint_mem_test()
{
    pint_print_mem();

    unsigned int *p;
    p = pint_mem_alloc(20, SUB_T_CLEAR);
    pint_mem_set_value(p, 20, 0xa0a0a0a0);
    pint_print_mem();

    p = pint_mem_alloc(32, T_CLEAR);
    pint_mem_set_value(p, 32, 0xd0d0d0d0);
    pint_print_mem();

    p = pint_mem_alloc(40, SUB_T_CLEAR);
    pint_mem_set_value(p, 40, 0xb0b0b0b0);
    pint_print_mem();

    p = pint_mem_alloc(48, CROSS_T);
    pint_mem_set_value(p, 48, 0xc0c0c0c0);
    q = p;
    pint_mem_set_ref(p, (unsigned int *)&q);
    pint_print_mem();
    
    pint_mem_compact();
    pint_print_mem();
    pint_mem_free(q);
    pint_print_mem();
    pint_mem_compact();
    pint_print_mem();
}
#endif
#else
int _mem_subt_chunk_head;
unsigned int pint_mem_compact_count = 0;

unsigned int *pint_mem_alloc(unsigned int n_byte, alloc_type type) {
    return (unsigned int *)malloc(n_byte);
}

void pint_mem_set_ref(unsigned int *p, unsigned int *ref) { return; }

void pint_mem_free(unsigned int *p) { free(p); }
#endif
