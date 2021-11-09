#ifdef CPU_MODE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include "../inc/cpu_adapt.h"
#else
#include "pintdev.h"
#define PINT_SET_RO
#define PINT_SET_RO_MCORE
#define PINT_PARALLEL_MODE

extern "C" {
void *__dso_handle = 0;
}
extern "C" {
void *__cxa_atexit = 0;
}
extern "C" {
void *_Unwind_Resume = 0;
}
extern "C" {
void *__gxx_personality_v0 = 0;
}
#endif

#include "../common/perf.h"
#include "../inc/pint_simu.h"
#include "../inc/pint_vpi.h"
#include "../common/pint_perf_counter.h"

unsigned pint_ro_start = 0x10000;
#ifdef PINT_PLI_MODE
simu_time_t T __attribute__((section(".builtin_simu_buffer")));
unsigned int pint_stop __attribute__((section(".builtin_simu_buffer")));
#else
simu_time_t T __attribute__((section(".mcore_visit_buffer")));
unsigned int pint_stop __attribute__((section(".mcore_visit_buffer")));
#endif
unsigned int pint_dump_enable __attribute__((section(".mcore_visit_buffer"))) = 0;
pint_thread monitor_func_ptr __attribute__((section(".mcore_visit_buffer"))) = NULL;
unsigned monitor_on __attribute__((section(".mcore_visit_buffer"))) = 1;
unsigned random_num __attribute__((section(".mcore_visit_buffer"))) = 0;
pint_thread_t pint_q_base[PINT_Q_NUM] __attribute__((section(".mcore_visit_buffer")));
pint_thread ncore_thread_list[PINT_CORE_NUM][PINT_NCORE_LIST_SIZE];

#ifdef PINT_PLI_MODE
unsigned pint_init_done __attribute__((section(".builtin_simu_buffer"))) = 0;
unsigned pint_start_nba __attribute__((section(".builtin_simu_buffer"))) = 0;
volatile unsigned pint_start_postpone __attribute__((section(".builtin_simu_buffer"))) = 0;
volatile unsigned pint_output_value_change __attribute__((section(".builtin_simu_buffer"))) = 0;

volatile unsigned pint_need_update_ctrl __attribute__((section(".simu.dut.output")));
int pint_output_port_change_num __attribute__((section(".simu.dut.output"))) = 0;

int pint_input_port_change_num __attribute__((section(".simu.dut.input"))) = 0;
#endif

extern pint_thread_t static_event_list_table[];
extern int static_event_list_table_size[];
extern int static_event_list_table_num;

//#define COUNT_PARALLEL_RATIO_ENABLE

#ifdef COUNT_PARALLEL_RATIO_ENABLE
#define START_PARALLEL_WITH_CYCLE_COUNT(thread, cycle_begin, cycle_end, cycle_total)               \
    do {                                                                                           \
        pint_mcore_cycle_get((unsigned int *)&cycle_begin);                                        \
        pint_parallel_start((addr_t)thread);                                                       \
        pint_mcore_cycle_get((unsigned int *)&cycle_end);                                          \
        cycle_total += (cycle_end - cycle_begin);                                                  \
        pint_dcache_discard();                                                                     \
    } while (0)
#else
#define START_PARALLEL_WITH_CYCLE_COUNT(thread, cycle_begin, cycle_end, cycle_total)               \
    do {                                                                                           \
        pint_parallel_start((addr_t)thread);                                                       \
        pint_dcache_discard();                                                                     \
    } while (0)

#endif

int static_tables_item_num __attribute__((section(".builtin_simu_buffer")));
int pint_q_head[PINT_Q_NUM] __attribute__((section(".builtin_simu_buffer")));
int pint_future_q_head __attribute__((section(".builtin_simu_buffer")));
// dlpm
int pint_lpm_future_q_head __attribute__((section(".builtin_simu_buffer")));

void pint_stop_simu(void) { pint_stop = 1; }

#ifdef PINT_PLI_MODE
void pint_update_output_value_change(unsigned int port_id) {
    if (!pint_enqueue_flag_set_any(pint_output_port_id_flag, port_id)) {
        pint_output_value_change = 1;
        int change_idx = pint_atomic_add(&pint_output_port_change_num, 1);
        pint_output_port_change_id[change_idx] = port_id;
    }
}
#endif

int pint_enqueue_thread(pint_thread thread, enum pint_queue_type q_type) {
    // NCORE_PERF_MEASURE(pint_enqueue_thread, 4); // may call from mcore
    NCORE_PERF_PINT_NET_SUMMARY(191);

    int enq_idx = pint_atomic_add(&pint_q_head[q_type], 1);
    pint_q_base[q_type][enq_idx] = thread;
    return 0;
}

int pint_enqueue_thread_batched(pint_thread_t thread_list, // function address
                                enum pint_queue_type q_type, unsigned int thread_num) {
    int enq_idx = pint_atomic_add(&pint_q_head[q_type], thread_num);
    pint_thread_t p_que = pint_q_base[q_type] + enq_idx;

    for (int i = 0; i < thread_num; i++) {
        p_que[i] = thread_list[i];
    }
    return 0;
}

inline int pint_dequeue_thread_batched(enum pint_queue_type q_type, pint_thread *ret_threads,
                                       int fetch_num, int &left_thread_num) {
    NCORE_PERF_MEASURE(pint_dequeue_thread_batched, 4);

    // deq_idx is old value
    int deq_idx = pint_atomic_add(&pint_q_head[q_type], 0 - fetch_num);
    int real_fetch_num;
    if (deq_idx > fetch_num) {
        real_fetch_num = fetch_num;
        left_thread_num = deq_idx - fetch_num;
    } else // deq_idx <= fetch_num
    {
        real_fetch_num = _MAX(0, deq_idx);
        left_thread_num = 0;
    }

    for (int i = 0; i < real_fetch_num; i++) {
        ret_threads[i] = pint_q_base[q_type][deq_idx - 1 - i];
    }
    return real_fetch_num;
}

// dlpm
int pint_lpm_enqueue_thread_future(pint_future_thread_t fthread, enum pint_queue_type q_type,
                                   int delay_lpm_idx) {
    NCORE_PERF_MEASURE(pint_lpm_enqueue_thread_future, 4);
    NCORE_PERF_PINT_NET_SUMMARY(190);

    if (pint_lpm_future_q[delay_lpm_idx] == 0) {
        int enq_idx = pint_atomic_add(&pint_lpm_future_q_head, 1);
        pint_lpm_future_q_idx_active[enq_idx] = delay_lpm_idx;
    }
    pint_lpm_future_q[delay_lpm_idx] = fthread;
    return 0;
}

pint_future_thread_t pint_get_lpm_future_queue_thread(int delay_lpm_idx) {
    pint_future_thread_t fthread = pint_lpm_future_q[delay_lpm_idx];
    return fthread;
}

int pint_clear_lpm_future_queue_thread(int delay_lpm_idx) {
    pint_lpm_future_q[delay_lpm_idx] = 0;
    return 0;
}

int pint_enqueue_thread_future(pint_future_thread_t fthread) {
    NCORE_PERF_MEASURE(pint_enqueue_thread_future, 4);
    NCORE_PERF_PINT_NET_SUMMARY(190);

    int enq_idx = pint_atomic_add(&pint_future_q_head, 1);
    pint_future_q[enq_idx] = fthread;
    return 0;
}

void pint_enqueue_nb_future(void *src, void *nb_dst, unsigned sig_width, int base,
                            unsigned copy_width, int array_idx, unsigned delay_idx, unsigned delay,
                            unsigned nb_id) {
    pint_future_nbassign_t nb_future_assign_info =
        (pint_future_nbassign_t)((char *)src + (sig_width + 31) / 32 * 4 * 2);

    if (nb_future_assign_info->enqueue_flag == 0) {
        nb_future_assign_info->enqueue_flag = 1;
    } else {
        return;
    }

    nb_future_assign_info->width = sig_width;
    nb_future_assign_info->src = src;
    nb_future_assign_info->dst = nb_dst;
    nb_future_assign_info->base = base;
    nb_future_assign_info->assign_width = copy_width;
    nb_future_assign_info->array_idx = array_idx;

    if (delay > 0) {
        pint_future_thread[delay_idx].T = T + delay;
        pint_future_thread[delay_idx].type = FUTURE_NBASSIGN_TYPE;
        pint_future_thread[delay_idx].nb = nb_future_assign_info;
        pint_future_thread[delay_idx].nb_id = nb_id;
        pint_enqueue_thread_future(&pint_future_thread[delay_idx]);
        pint_mem_set_ref((unsigned int *)src, (unsigned int *)&nb_future_assign_info->src);
    } else {
        pint_handle_future_nb(nb_future_assign_info, nb_id);
    }
}
pint_future_thread_t pint_get_future_queue_thread(int idx) {
    pint_future_thread_t fthread = pint_future_q[idx];
    return fthread;
}

void pint_set_future_queue_thread(int idx, pint_future_thread_t fthread) {
    pint_future_q[idx] = fthread;
}

inline int pint_lock(int &lock) {
    int swap_data = 0;
    do {
        pint_atomic_swap(&lock, swap_data);
    } while (swap_data == 0);
    return swap_data;
}

inline void pint_unlock(int &lock, int swap_data) { pint_atomic_swap(&lock, swap_data); }

int pint_wait_on_event(pint_wait_event_t wait_event, pint_thread thread) {
    NCORE_PERF_MEASURE(pint_wait_on_event, 5);
    NCORE_PERF_PINT_NET_SUMMARY(189);

    int thread_num = pint_lock(wait_event->thread_num);
    int idx = pint_atomic_add(&wait_event->thread_head, 1);
    wait_event->thread_list[idx] = thread;
    pint_wait_write_done();

    pint_unlock(wait_event->thread_num, thread_num);
    return 0;
}

int pint_move_event_thread_list(pint_wait_event_t wait_event) {
    NCORE_PERF_MEASURE(pint_move_event_thread_list, 3);
    NCORE_PERF_PINT_NET_SUMMARY(188);
    int enq_idx;

    int head = wait_event->thread_head;
    if (head > 0) {
        // lock
        int thread_num = pint_lock(wait_event->thread_num);

        wait_event->thread_head = 0;
        enq_idx = pint_atomic_add(&pint_q_head[INACTIVE_Q], head);
        pint_wait_write_done();

        for (int i = 0; i < head; i++) {
            pint_q_base[INACTIVE_Q][enq_idx++] = wait_event->thread_list[i];
        }

        // unlock
        pint_unlock(wait_event->thread_num, thread_num);
    }

    return 0;
}

int pint_move_event_thread_list_with_static_table(pint_wait_event_t wait_event, unsigned type,
                                                  pint_thread_t static_table) {
    NCORE_PERF_MEASURE(pint_move_event_thread_list_with_static_table, 3);
    NCORE_PERF_PINT_NET_SUMMARY(188);
    int enq_idx;

    if (type & 0x80000000) {
        int head = wait_event->thread_head;
        if (head > 0) {
            // lock
            int thread_num = pint_lock(wait_event->thread_num);

            wait_event->thread_head = 0;
            enq_idx = pint_atomic_add(&pint_q_head[INACTIVE_Q], head);
            pint_wait_write_done();

            for (int i = 0; i < head; i++) {
                pint_q_base[INACTIVE_Q][enq_idx++] = wait_event->thread_list[i];
            }

            // unlock
            pint_unlock(wait_event->thread_num, thread_num);
        }
    }

    unsigned static_table_size = type & 0x7fffffff;
    if (static_table_size) {
#ifndef CPU_MODE
        enq_idx = pint_atomic_add(&static_event_list_table_num, (static_table_size << 12) + 1);
        static_event_list_table[enq_idx & 0xfff] = static_table;
        static_event_list_table_size[enq_idx & 0xfff] = static_table_size;
#else
        static_event_list_table[static_event_list_table_num] = static_table;
        static_event_list_table_size[static_event_list_table_num] = static_table_size;
        static_event_list_table_num++;
#endif
    }
    return 0;
}

void pint_process_s_list(pint_thread_t s_list, unsigned e_type) {
    NCORE_PERF_MEASURE(pint_process_s_list, 9);
    NCORE_PERF_PINT_NET_SUMMARY(207);
    unsigned thread_num;
    unsigned static_table_size;
    pint_thread_t static_table;
    pint_thread_t list;
    if (e_type == pint_posedge) {
        list = s_list + 3;
        thread_num = (unsigned int)s_list[0] + (unsigned int)s_list[1]; // pos_num + any_num ?
    } else if (e_type == pint_negedge) {
        list = s_list + 3 + ((unsigned int)s_list[0]);
        thread_num = (unsigned int)s_list[1] + (unsigned int)s_list[2];
    } else {
        // any
        list = s_list + 3 + ((unsigned int)s_list[0]);
        thread_num = (unsigned int)s_list[1];
    }

    if (thread_num > 0) {
#ifndef CPU_MODE
        static_table_size = thread_num;
        static_table = list;
        unsigned enq_idx =
            pint_atomic_add(&static_event_list_table_num, (static_table_size << 12) + 1);
        static_event_list_table[enq_idx & 0xfff] = static_table;
        static_event_list_table_size[enq_idx & 0xfff] = static_table_size;
#else
        static_event_list_table[static_event_list_table_num] = list;
        static_event_list_table_size[static_event_list_table_num] = thread_num;
        static_event_list_table_num++;
#endif
    }
}

#ifdef PINT_PARALLEL_MODE
void execute_active_q_thread() {
#ifdef PROF_ON
    PROF_BEGIN(execute_active_q_thread, 186);
#endif

    pint_thread thread = pint_dequeue_thread(ACTIVE_Q);
    while (NULL != thread) {
        {
            NCORE_PERF_MEASURE(thread, 1);
            thread();
        }
        pint_dcache_discard();
        thread = pint_dequeue_thread(ACTIVE_Q);
    }
#ifdef PROF_ON
    PROF_END();
#endif
}
#endif

inline void execute_static_by_static_alloc(unsigned off, unsigned num) {
    pint_thread_t thread_list;
    pint_thread thread;
    unsigned char flag;
    unsigned q_flag_idx;
    unsigned i;
    unsigned table_idx = 0;
    unsigned table_size = static_event_list_table_size[0];
    while (table_size <= off) { //  find first dst
        off -= table_size;
        table_size = static_event_list_table_size[++table_idx];
    }
    thread_list = static_event_list_table[table_idx];
    for (i = 0; i < num; i++, off += PINT_CORE_NUM) {
        if (table_size <= off) { //  correct location
            do {
                off -= table_size;
                table_size = static_event_list_table_size[++table_idx];
            } while (table_size <= off);
            thread_list = static_event_list_table[table_idx];
        }
        q_flag_idx = thread_list[2 * off + 1];
        flag = pint_event_flag[q_flag_idx];
        if (flag) {
            if (flag == 1) {
                pint_event_flag[q_flag_idx] = 0;
            }
            thread = thread_list[2 * off];
            NCORE_PERF_MEASURE(thread, 1);
            thread();
        }
    }
}

inline pint_thread screen_valid_static_task(unsigned off, unsigned num, pint_thread_t dst) {
    //  return first valid thread(not enqueue)
    pint_thread_t thread_list;
    pint_thread thread;
    unsigned char flag;
    unsigned q_flag_idx;
    unsigned i;
    unsigned valid_thread_num = 0;
    unsigned table_idx = 0;
    unsigned table_size = static_event_list_table_size[0];
    while (table_size <= off) { //  find first dst
        off -= table_size;
        table_size = static_event_list_table_size[++table_idx];
    }
    thread_list = static_event_list_table[table_idx];
    if (num <= PINT_NCORE_LIST_SIZE) {
        for (i = 0; i < num; i++, off++) {
            if (table_size <= off) { //  find location
                do {
                    off -= table_size;
                    table_size = static_event_list_table_size[++table_idx];
                } while (table_size <= off);
                thread_list = static_event_list_table[table_idx];
            }
            q_flag_idx = thread_list[2 * off + 1];
            flag = pint_event_flag[q_flag_idx];
            switch (flag) {
            case 0x1:
                pint_event_flag[q_flag_idx] = 0;
            case 0x55:
                thread = thread_list[2 * off];
                dst[valid_thread_num++] = thread;
                break;
            default:
                break;
            }
        }
    } else {
        for (i = 0; i < num; i++, off++) {
            if (table_size <= off) { //  find location
                do {
                    off -= table_size;
                    table_size = static_event_list_table_size[++table_idx];
                } while (table_size <= off);
                thread_list = static_event_list_table[table_idx];
            }
            q_flag_idx = thread_list[2 * off + 1];
            flag = pint_event_flag[q_flag_idx];
            switch (flag) {
            case 0x1:
                pint_event_flag[q_flag_idx] = 0;
            case 0x55:
                thread = thread_list[2 * off];
                dst[valid_thread_num++] = thread;
                if (valid_thread_num == PINT_NCORE_LIST_SIZE) {
                    unsigned loc = pint_atomic_add(&pint_q_head[ACTIVE_Q], PINT_NCORE_LIST_SIZE);
                    for (unsigned k = 0; k < PINT_NCORE_LIST_SIZE; k++) {
                        pint_q_base[ACTIVE_Q][loc++] = dst[k];
                    }
                    valid_thread_num = 0;
                }
                break;
            default:
                break;
            }
        }
    }
    switch (valid_thread_num) {
    case 0:
        return NULL;
    case 1:
        return dst[0];
    default: {
        unsigned loc = pint_atomic_add(&pint_q_head[ACTIVE_Q], valid_thread_num - 1);
        for (unsigned k = 1; k < valid_thread_num; k++) {
            pint_q_base[ACTIVE_Q][loc++] = dst[k];
        }
        return dst[0];
    }
    }
}

void execute_static_q_thread_cpu() {
    pint_thread thread;
    for (int i = 0; i < static_event_list_table_num; i++) {
        unsigned num = static_event_list_table_size[i];
        pint_thread_t list = static_event_list_table[i];
        for (int j = 0; j < num; j++) {
            unsigned q_flag_idx = list[2 * j + 1];
            unsigned flag = pint_event_flag[q_flag_idx];
            switch (flag) {
            case 0x1:
                pint_event_flag[q_flag_idx] = 0;
            case 0x55:
                thread = list[2 * j];
                thread();
                break;
            default:
                break;
            }
        }
    }
}

void execute_static_q_thread() {
#ifdef PROF_ON
    PROF_BEGIN(execute_static_q_thread, 204);
#endif
    pint_dcache_discard();
    unsigned total_thread_num = static_event_list_table_num >> 12;
#ifndef PROF_ON
    unsigned core_id = pint_core_id();
#else
    unsigned core_id = pint_core_id() >> 4;
#endif
    unsigned div = total_thread_num / PINT_CORE_NUM;
    unsigned mod = total_thread_num % PINT_CORE_NUM;
    unsigned num = div + ((mod > core_id) ? 1 : 0);
    if (total_thread_num <= STATIC_SWITCH_MODE_SIZE) {
        if (num) {
            execute_static_by_static_alloc(core_id, num);
        }
    } else {
        unsigned off = div * core_id + ((mod > core_id) ? core_id : mod);
        pint_thread thread = screen_valid_static_task(off, num, ncore_thread_list[core_id]);
#ifndef CPU_MODE
        // pint_wait_write_done();
        asm volatile("uap.sync");
#endif

        if (thread) {
            NCORE_PERF_MEASURE(thread, 1);
            thread();
        }
        thread = pint_dequeue_thread(ACTIVE_Q);
        while (thread) {
            NCORE_PERF_MEASURE(thread, 1);
            thread();
            // pint_dcache_discard();
            thread = pint_dequeue_thread(ACTIVE_Q);
        }
    }
    pint_dcache_discard();
#ifdef PROF_ON
    PROF_END();
#ifndef CPU_MODE
    if ((static_event_list_table_num >> 12) > STATIC_SWITCH_MODE_SIZE && (core_id & 0xf)) {
        asm volatile("uap.sync");
    }
#endif
#endif
}

void execute_monitor_q_thread() {
#ifdef PROF_ON
    int core_id = pint_core_id();
    if ((0 == (core_id & 15)) && (core_id < (gperf_ncore_num_to_perf << 4))) {
        NCORE_PERF_MEASURE(execute_monitor_q_thread, 0);
#endif

#ifdef PINT_SET_RO
// pint_ro_seg_set(0x10000, 0xC0000000);
#endif
        pint_thread thread = pint_dequeue_thread(MONITOR_Q);
        while (NULL != thread) {
            {
                NCORE_PERF_MEASURE(thread, 1);
                thread();
                pint_dcache_discard();
                thread = pint_dequeue_thread(MONITOR_Q);
            }
        }
#ifdef PINT_SET_RO
// pint_ro_seg_set(1, 0);
// pint_dcache_discard();
#endif

#ifdef PROF_ON
    } else {
        pint_sync_retire_enable();
    }
#endif
}

void execute_q_thread_mcore(enum pint_queue_type type) {
#if 0    
    int thread_num = pint_q_head[type];

    for (int i = 0; i < thread_num; i++)
    {
#ifdef CPU_MODE
        pint_thread thread = pint_q_base[type][i]; //pint_dequeue_thread(type);
#else
        pint_thread thread = pint_dequeue_thread(type);
#endif
        if (NULL != thread)
        {
            // printf("---i[%d] thread addr[0x%lx] head[%d]\n", i, thread, pint_q_head[type]);
            thread();
        }
        else
        {
            printf("execute_q_thread_mcore:type:%d thread_num:%d i:%d thread is NULL.\n", type, thread_num, i);
        }
    }
#else
    int thread_num = pint_q_head[type];
    for (int i = 0; i < thread_num; i++) {
        (pint_q_base[type][i])();
    }
#endif
}

int pint_swap_active_and_inactive_queue() {
    int active_head = pint_q_head[INACTIVE_Q];
    pint_thread_t pint_q_tmp = pint_q_base[ACTIVE_Q];
    pint_q_base[ACTIVE_Q] = pint_q_base[INACTIVE_Q];
    pint_q_head[ACTIVE_Q] = active_head;
    pint_q_base[INACTIVE_Q] = pint_q_tmp;
    pint_q_head[INACTIVE_Q] = 0;
    return active_head;
}

void pint_queue_init() {
    pint_q_base[ACTIVE_Q] = pint_active_q;
    pint_q_head[ACTIVE_Q] = 0;

    pint_q_base[INACTIVE_Q] = pint_inactive_q;
    pint_q_head[INACTIVE_Q] = 0;

    // pint_q_base[NBASSIGN_Q] = pint_nbassign_q;
    pint_q_head[NBASSIGN_Q] = 0;

    pint_q_base[DELAY0_Q] = pint_delay0_q;
    pint_q_head[DELAY0_Q] = 0;

    pint_q_base[MONITOR_Q] = pint_monitor_q;
    pint_q_head[MONITOR_Q] = 0;

    pint_future_q_head = 0;
    // dlpm
    pint_lpm_future_q_head = 0;
    static_tables_item_num = 0;
}

#ifdef CPU_MODE
extern void execute_nbassign_q_thread_cpu(void);
#endif

void set_ro_ncore() {
#ifdef PINT_SET_RO
    pint_ro_seg_set(pint_ro_start, PINT_STACK_END);
#endif
}

#ifndef CPU_MODE
int main()
#else
int main(int argc, char *argv[])
#endif
{
#ifndef CPU_MODE
    // get binary info
    unsigned binary_len = *(unsigned int *)PINT_BINARY_LEN_ADDR;
    unsigned text_base = *(unsigned int *)PINT_TEXT_BASE_ADDR;
    unsigned text_size = *(unsigned int *)PINT_TEXT_SIZE_ADDR + text_base;
    unsigned data = binary_len - text_size;
    printf("------Binary[%u] Text[%u] Data[%u]\n", binary_len, text_size, data);
    pint_mem_init();
    // pint_mem_test();
    pint_printf("------pint verilog simulation start\n");
#else
    pint_printf("------pint verilog simulation start\n");
    host_malloc_sys_args();
    parse_sys_args(argc, argv);
#endif

    unsigned int cycles_1[2];
    unsigned int cycles_2[2];

    unsigned int cycles_3[2];
    unsigned int cycles_4[2];

    unsigned int parallel_cnt = 0;
    unsigned int parallel_cycles = 0;

    unsigned long long cycles_parallel_begin = 0;
    unsigned long long cycles_parallel_end = 0;
    unsigned long long cycles_parallel_total = 0;
    unsigned long long total_thread = 0;
    unsigned total_thread_h = 0;
    unsigned total_thread_l = 0;

    unsigned time_steps = 0;
    unsigned static_thread_num;
    unsigned executed_thread_num = 0;
    unsigned int nbassign_num;
    unsigned int delay0_num;
    int active_head;
    int static_table_num;

#ifdef PROF_ON
    // flush scache and discard scache
    pint_mcore_flush_all();
    pint_mcore_discard_all();
    PerfBegin();
#endif

    pint_mcore_cycle_get(cycles_1);

#ifdef PROF_SUMMARY_ON
    func_summary_array = device_func_summary_array;
#endif

#ifndef CPU_MODE
    pint_ro_start = text_base + 0x40;
#ifdef PINT_SET_RO_MCORE
    pint_ro_seg_set(pint_ro_start, PINT_STACK_END);
#endif
    START_PARALLEL_WITH_CYCLE_COUNT(set_ro_ncore, cycles_parallel_begin, cycles_parallel_end,
                                    cycles_parallel_total);
    START_PARALLEL_WITH_CYCLE_COUNT(pint_array_init_parallel, cycles_parallel_begin,
                                    cycles_parallel_end, cycles_parallel_total);
#else
    pint_array_init();
#endif

    T = 0;
    pint_stop = 0;
    pint_queue_init();
    pint_global_init();

    if (pint_q_head[ACTIVE_Q] == 0) {
        pint_swap_active_and_inactive_queue();
    }

    while (!pint_stop) {
        {
#ifndef PROF_NCORE_OCCUPANCY
            MCORE_PERF_MEASURE(main, 0);
#endif
            active_head = pint_q_head[ACTIVE_Q];
            do {
                // 1.process active queue and inactive queue
                while (1) {
                    while (active_head > 0) {
                        executed_thread_num += active_head;
#ifdef PINT_PARALLEL_MODE
                        parallel_cnt++;
                        {
                            MCORE_PERF_MEASURE_PRINT(pint_q_head[ACTIVE_Q], parallel_cnt);
                            if (active_head > 1) {
                                MCORE_PERF_MEASURE(execute_active_q_thread, 0);
                                MCORE_PERF_SUMMARY(210);
                                START_PARALLEL_WITH_CYCLE_COUNT(
                                    execute_active_q_thread, cycles_parallel_begin,
                                    cycles_parallel_end, cycles_parallel_total);
                            } else {
                                MCORE_PERF_MEASURE(pint_q_base[ACTIVE_Q][0], 0);
                                pint_q_base[ACTIVE_Q][0]();
                            }
                        }
#else
                        execute_q_thread_mcore(ACTIVE_Q);
#endif
                        active_head = pint_swap_active_and_inactive_queue();
                    }
                    static_table_num = static_event_list_table_num;
                    if (static_table_num) {
                        unsigned total_thread_num = static_table_num >> 12;
#ifdef PINT_PARALLEL_MODE
                        parallel_cnt++;
                        MCORE_PERF_MEASURE(execute_static_q_thread, 0);
                        MCORE_PERF_SUMMARY(209);
                        START_PARALLEL_WITH_CYCLE_COUNT(execute_static_q_thread,
                                                        cycles_parallel_begin, cycles_parallel_end,
                                                        cycles_parallel_total);
#else
#ifndef CPU_MODE
                        execute_static_q_thread();
#else
                        execute_static_q_thread_cpu();
#endif
#endif
                        static_tables_item_num = 0;
                        static_event_list_table_num = 0;
                        executed_thread_num += total_thread_num;
                        active_head = pint_swap_active_and_inactive_queue();
                    } else {
                        break;
                    }
                }

                // 2.process non blocking assignment update queue
                if (pint_q_head[NBASSIGN_Q] > 0) {
                    nbassign_num = pint_q_head[NBASSIGN_Q];
                    executed_thread_num += nbassign_num;

#ifdef PINT_PARALLEL_MODE
                    parallel_cnt++;
                    // sub_t measure enable
                    {
                        MCORE_PERF_MEASURE_PRINT(pint_q_head[NBASSIGN_Q], parallel_cnt);
                        MCORE_PERF_SUMMARY(211);
                        MCORE_PERF_MEASURE(execute_nbassign_q_thread, 0);
                        START_PARALLEL_WITH_CYCLE_COUNT(execute_nbassign_q_thread,
                                                        cycles_parallel_begin, cycles_parallel_end,
                                                        cycles_parallel_total);
                    }
#else
#ifndef CPU_MODE
                    execute_nbassign_q_thread();
#else
                    execute_nbassign_q_thread_cpu();
#endif

#endif
                    pint_q_head[NBASSIGN_Q] = 0;
                    active_head = pint_swap_active_and_inactive_queue();
                }
                _mem_subt_chunk_head = 0;
            } while ((active_head > 0) || (static_event_list_table_num > 0));

            // 3.process delay0 queue
            delay0_num = pint_q_head[DELAY0_Q];
            if (delay0_num > 0) {
                pint_q_head[DELAY0_Q] = 0;
                for (int delay0_i = 0; delay0_i < delay0_num; delay0_i++) {
                    pint_enqueue_thread(pint_q_base[DELAY0_Q][delay0_i], ACTIVE_Q);
                }
                continue;
            }

            // 4.process monitor queue
            if (pint_q_head[MONITOR_Q] > 0) {
                unsigned int monitor_num = pint_q_head[MONITOR_Q];
                executed_thread_num += monitor_num;
                parallel_cnt++;

#ifdef PINT_PARALLEL_MODE
                {
                    MCORE_PERF_MEASURE(execute_monitor_q_thread, 0);
                    START_PARALLEL_WITH_CYCLE_COUNT(execute_monitor_q_thread, cycles_parallel_begin,
                                                    cycles_parallel_end, cycles_parallel_total);
                }
#else
                execute_monitor_q_thread();
#endif
                pint_q_head[MONITOR_Q] = 0;
                active_head = pint_swap_active_and_inactive_queue();
            }

            if ((monitor_func_ptr != NULL) && monitor_on) {
                (*monitor_func_ptr)();
            }

            total_thread += executed_thread_num;
            executed_thread_num = 0;

            // 5.process future queue
            simu_time_t next_time = 0;
            _mem_subt_chunk_head = 0;

            int future_q_find_head = 0;
            for (int i = 0; i < pint_future_q_head; i++) {
                pint_future_thread_t fthread = pint_get_future_queue_thread(i);

                if (0 == next_time) {
                    next_time = fthread->T;
                    pint_future_q_find[future_q_find_head++] = i;
                } else if (fthread->T < next_time) {
                    next_time = fthread->T;
                    future_q_find_head = 0;
                    pint_future_q_find[future_q_find_head++] = i;
                } else if (fthread->T == next_time) {
                    pint_future_q_find[future_q_find_head++] = i;
                }
            }

            // dlpm
            simu_time_t lpm_next_time = 0;

            int lpm_future_q_find_head = 0;
            for (int i = 0; i < pint_lpm_future_q_head; i++) {
                int delay_lpm_idx = pint_lpm_future_q_idx_active[i];
                pint_future_thread_t fthread = pint_get_lpm_future_queue_thread(delay_lpm_idx);

                if (0 == lpm_next_time) {
                    lpm_next_time = fthread->T;
                    pint_lpm_future_q_idx_find[lpm_future_q_find_head++] = i;
                } else if (fthread->T < lpm_next_time) {
                    lpm_next_time = fthread->T;
                    lpm_future_q_find_head = 0;
                    pint_lpm_future_q_idx_find[lpm_future_q_find_head++] = i;
                } else if (fthread->T == lpm_next_time) {
                    pint_lpm_future_q_idx_find[lpm_future_q_find_head++] = i;
                }
            }

            bool time_freeze = true;

            if (lpm_future_q_find_head && future_q_find_head) {
                if (lpm_next_time > next_time) {
                    lpm_future_q_find_head = 0;
                } else {
                    future_q_find_head = 0;
                    next_time = lpm_next_time;
                    time_freeze = (lpm_next_time != next_time);
                }
            }

#ifndef PINT_PLI_MODE
            T = next_time;
            if (time_freeze) {
                time_steps++;
                if (random_num) {
                    pint_mcore_cycle_get(cycles_2);
                    random_num = cycles_2[0];
                }

                if (pint_dump_enable) {
                    pint_vcddump_value(&next_time);
                }
            }

            if (next_time) {
                for (int i = future_q_find_head - 1; i >= 0; i--) {
                    int find_idx = pint_future_q_find[i];
                    pint_future_thread_t fthread = pint_get_future_queue_thread(find_idx);
                    if (FUTURE_THREAD_TYPE == fthread->type) {
                        pint_enqueue_thread(fthread->run_func, ACTIVE_Q);
                    } else {
                        pint_handle_future_nb(fthread->nb, fthread->nb_id);
                    }

                    if (find_idx < pint_future_q_head - 1) //??
                    {
                        fthread = pint_get_future_queue_thread(pint_future_q_head - 1);
                        pint_set_future_queue_thread(find_idx, fthread);
                    }
                    pint_future_q_head--;
                }

                // dlpm
                for (int i = lpm_future_q_find_head - 1; i >= 0; i--) {
                    int find_idx = pint_lpm_future_q_idx_find[i];
                    int delay_lpm_idx = pint_lpm_future_q_idx_active[find_idx];
                    pint_future_thread_t fthread = pint_get_lpm_future_queue_thread(delay_lpm_idx);

                    fthread->lpm_state = FUTURE_LPM_STATE_EXEC;
                    pint_enqueue_thread(fthread->run_func, ACTIVE_Q);
                    pint_clear_lpm_future_queue_thread(delay_lpm_idx);

                    if (find_idx < pint_lpm_future_q_head - 1) {
                        delay_lpm_idx = pint_lpm_future_q_idx_active[pint_lpm_future_q_head - 1];
                        pint_lpm_future_q_idx_active[find_idx] = delay_lpm_idx;
                    }
                    pint_lpm_future_q_head--;
                }
            } else {
                pint_stop = 1;
            }
#else
            if (next_time) {
                printf("ERROR: pli currently do not suppor delay in dut yet, stop the "
                       "simulation!!!\n");
                pint_stop = 1;
            }
#endif

#ifndef CPU_MODE
            pint_mem_compact();
#endif

#ifdef PINT_PLI_MODE
            if (0 == pint_init_done) {
                pint_init_done = 123;

                // mcore wait
                asm volatile("uap.wait\tx0, x0, 448");
            } else {
                // send mcore interrupt
                unsigned int m2h = 2;
                asm volatile("csrw\t0x22,%0" ::"r"(m2h));
                // mcore wait
                if (pint_stop)
                    break;
                asm volatile("uap.wait\tx0, x0, 448");
            }

            pint_dcache_discard();
            if (random_num) {
                pint_mcore_cycle_get(cycles_2);
                random_num = cycles_2[0];
            }
            if (pint_dump_enable) {
                pint_vcddump_value(&T);
            }
            pint_output_port_change_num = 0;
            memset((char *)pint_output_port_id_flag, 0,
                   pint_output_port_id_flag_size * sizeof(unsigned short));
            enqueue_change_callback_list();
            pint_input_port_change_num = 0;
#endif
        }
    }
TEST:
    pint_mcore_cycle_get(cycles_2);
#ifdef PROF_ON
    gperf_mcore_parallel_cnt[0] = parallel_cnt;
    PerfEnd();
#endif

#ifdef PINT_PLI_MODE
    pint_init_done = 0;
    pint_need_update_ctrl = 0;
#endif

    unsigned int nsec;
    unsigned int sec;
    if (cycles_1[0] > cycles_2[0]) {
        nsec = cycles_2[0] + (0xffffffff - cycles_1[0]);
        sec = cycles_2[1] - cycles_1[1] - 1;
    } else {
        nsec = cycles_2[0] - cycles_1[0];
        sec = cycles_2[1] - cycles_1[1];
    }

    float sec_f = 0xffffffff / (PINT_CLOCK)*sec + nsec / (PINT_CLOCK);
    total_thread_h = (unsigned)(total_thread >> 32);
    total_thread_l = (unsigned)(total_thread & 0xffffffff);
    char total_t_str[32];
    longlong_decimal_to_string(total_t_str, &T);

#ifndef CPU_MODE
    float avg_thread = (float)0xffffffff / (float)parallel_cnt * (float)total_thread_h +
                       (float)total_thread_l / (float)parallel_cnt;
    pint_printf("------pint verilog simulation end\n");
    printf("======Event counts======\n");
    printf("    Total_T:        %s\n"
           "    T_steps:        %u\n"
           "    Total_thread:   %u * 2^32 + %u\n"
           "    Parallel_cnt:   %u\n"
           "    Avg_thread:     %u\n",
           total_t_str, time_steps, total_thread_h, total_thread_l, parallel_cnt,
           (unsigned)avg_thread);
    printf("Kernel execute:");
    pint_printFloat(sec_f);
    printf("seconds, Cycles[%u %u].\n", sec, nsec);
#else
    pint_printf("------pint verilog simulation end\n");
    printf("======Event counts======\n");
    printf("    Total_T:        %s\n"
           "    T_steps:        %u\n"
           "    Total_thread:   %u * 2^32 + %u\n",
           total_t_str, time_steps, total_thread_h, total_thread_l);
    float sec_f_cpu = sec * (0xffffffff / (CPU_CLOCK)) + nsec / (CPU_CLOCK);
    printf("Kernel execute: %f seconds.\n", sec_f_cpu);
    host_free_sys_args();
#endif

#ifdef COUNT_PARALLEL_RATIO_ENABLE
    printf("Parallel/Total:");
    float sec_parallel_f =
        ((float)*(unsigned *)&cycles_parallel_total +
         (float)0x100000000 * ((float)*(((unsigned *)&cycles_parallel_total) + 1))) /
        (PINT_CLOCK);
    float parallel_rate = sec_parallel_f / sec_f * 100.0;
#ifndef CPU_MODE
    pint_printFloat(parallel_rate);
#else
    printf("%f", parallel_rate);
#endif
    printf("%%\n");
#endif

#ifdef PROF_NCORE_OCCUPANCY
    unsigned long long total_ncore_cycles = 0;
    unsigned long long mcore_parallel_cycles = total_cycles_per_ncore[0];
    for (int i = 1; i < NCORE_NUM; ++i) {
        total_ncore_cycles += total_cycles_per_ncore[i];
    }
    unsigned int total_ncore_cycles_h = (unsigned)(total_ncore_cycles >> 32);
    unsigned int total_ncore_cycles_l = (unsigned)(total_ncore_cycles & 0xffffffff);
    unsigned int total_mcore_cycles_h = (unsigned)(mcore_parallel_cycles >> 32);
    unsigned int total_mcore_cycles_l = (unsigned)(mcore_parallel_cycles & 0xffffffff);

    float sec_ncore_f = ((float)*(unsigned *)&total_ncore_cycles +
                         (float)0x100000000 * ((float)*(((unsigned *)&total_ncore_cycles) + 1))) /
                        (PINT_CLOCK);
    float sec_mcore_parallel_f =
        ((float)*(unsigned *)&mcore_parallel_cycles +
         (float)0x100000000 * ((float)*(((unsigned *)&mcore_parallel_cycles) + 1))) /
        (PINT_CLOCK);
    float ncore_parallel_rate = sec_ncore_f / (sec_mcore_parallel_f * 16) * 100.0;
    pint_printf("ncore_parallel_rate = ");
    pint_printFloat(ncore_parallel_rate);
    pint_printf("%%\n");
#endif

    return 0;
}
