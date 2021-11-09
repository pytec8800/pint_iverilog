#ifndef __PINT_SIMU_H__
#define __PINT_SIMU_H__

#ifdef CPU_MODE
#include <stdio.h>
#include "cpu_adapt.h"
typedef unsigned long addr_t;
#else
#include "pintdev.h"
#endif

#define PINT_MEM_ALLOC_END 0xe0000000 // 3.5G
#define PINT_STACK_END 0xf0100000     // 0-3.5G code, 256+1M stack, 238M global, 17M(DMA desc, IO).
#define PINT_CACHE_LINE_SIZE 64
#define PINT_MEM_TAB_MIN_SIZE 0x1000000
#define PINT_MEM_SINGLE_ALLOC_MAX_SIZE 0x10000000
#define PINT_MEM_MAGIC_IND 0xbdfacedb
#define PINT_NCORE_LIST_SIZE 256
#define STATIC_SWITCH_MODE_SIZE 4096
#define PINT_BINARY_LEN_ADDR 0xe0
#define PINT_BINARY_RESOURCE_ADDR 0xe4
#define PINT_STACK_BASE_ADDR 0xf4
#define PINT_MEM_MODE_ADDR 0xf8
#define PINT_TEXT_BASE_ADDR 0x120
#define PINT_TEXT_SIZE_ADDR 0x124

#define PINT_CLOCK (950000000.0f) // 950M
#define CPU_CLOCK (1000000000.0f) // ns

#define MIN_STATIC_THREAD_NUM_FOR_PARALLEL 1
#define PINT_EVENT_CHANGE_FLAG

#define _MAX(a, b) ((a) > (b) ? (a) : (b))

#define S_SEC __attribute__((section("_signal_")))
#define C_ALIGN __attribute__((aligned(1)))

#ifndef CPU_MODE
#ifndef PROF_ON
#define PINT_CORE_NUM 256
#else
#define PINT_CORE_NUM 16
#endif
#else
#define PINT_CORE_NUM 1
#endif

#ifndef CPU_MODE
#define pint_wait_write_done() asm volatile("uap.wait.wreq")
#define pint_atomic_swap(lock, swap_data)                                                          \
    asm volatile("uap.xw\t%1, %0" : "+m"((lock)[0]), "+r"(swap_data))
#else
#define pint_wait_write_done()
#define pint_dcache_discard()
#endif

enum pint_edge_t { pint_negedge = 1, pint_posedge = 2, pint_otherchange = 3, pint_nonedge = 0 };

enum pint_bit4_t { BIT4_0 = 0, BIT4_1 = 1, BIT4_X = 3, BIT4_Z = 2 };

enum pint_nb_type { NB_SIGNAL = 0, NB_ARRAY = 1 };

typedef unsigned long long simu_time_t;
extern simu_time_t T; // global simulation time, need to use 64bit
extern simu_time_t sub_T;
extern unsigned int pint_dump_enable;

//-------------data structure define-------------
typedef void (*thread_func_t)(void);
typedef thread_func_t pint_thread;
typedef pint_thread *pint_thread_t;

typedef char pint_signal;
typedef pint_signal *pint_signal_t;

struct pint_wait_event {
    int thread_num;
    int thread_head;
    pint_thread *thread_list;
};
typedef struct pint_wait_event pint_wait_event_s;
typedef struct pint_wait_event *pint_wait_event_t;

struct pint_lpm {
    int thread_num;
    pint_thread *thread_list;
};
typedef struct pint_lpm pint_lpm_s;
typedef struct pint_lpm *pint_lpm_t;

struct pint_array_info {
    void *sig;
    pint_bit4_t bit_val;
    unsigned width;
    unsigned size;
};

struct pint_nbassign {
    unsigned int type; // 0: signal 1:array
    unsigned int size;
    unsigned char *src;
    unsigned char *dst;
    unsigned int offset;
    unsigned int updt_size;
    unsigned int nbflag_id;
    pint_thread_t l_list; // size + thread
    pint_thread_t e_list; //
    unsigned int *p_list;
    unsigned char *vcd_dump_symbol;
#ifdef PINT_PLI_MODE
    unsigned int is_output_interface;
    unsigned int output_port_id;
#endif
};

struct pint_future_nbassign {
    unsigned int width;
    void *src;
    void *dst;
    short base;
    short enqueue_flag;
    unsigned int assign_width;
    int array_idx; //-1 mean not array, >= 0 means element idx in array
};

struct pint_nbassign_array {
    unsigned int type;
    unsigned int size;
    unsigned char *src;
    unsigned char *dst;
    unsigned int array_size; //
    unsigned int updt_cnt;   // if updt_cnt > 16, updt_cnt =0xffffffff, update all word, a = a_nb;
    unsigned int nbflag_id;
    pint_thread_t l_list;
    pint_thread_t e_list;
    unsigned int *p_list;
    unsigned int updt_idx[16];
    unsigned char *vcd_dump_symbol;
};

typedef struct pint_nbassign *pint_nbassign_t;
typedef struct pint_nbassign_array *pint_nbassign_array_t;
typedef struct pint_future_nbassign *pint_future_nbassign_t;

enum future_q_ele_type {
    FUTURE_THREAD_TYPE = 0,
    FUTURE_NBASSIGN_TYPE = 1,
    // dlpm
    FUTURE_LPM_TYPE = 2,
};

enum future_q_lmp_state {
    FUTURE_LPM_STATE_WAIT = 0,
    FUTURE_LPM_STATE_EXEC = 1,
    FUTURE_LPM_STATE_MAX = 2,
};

struct pint_future_thread {
    simu_time_t T;
    unsigned int type;
    union {
        pint_thread run_func;
        pint_future_nbassign_t nb;
    };
    union {
        unsigned int nb_id;
        unsigned int lpm_state;
    };
};
typedef struct pint_future_thread pint_future_thread_s;
typedef struct pint_future_thread *pint_future_thread_t;

struct pint_mem_alloc_tab_s {
    unsigned int type;
    unsigned int n_byte;
    unsigned int magic;
    unsigned int *p;
};
typedef struct pint_mem_alloc_tab_s *pint_mem_alloc_tab_t;

enum pint_queue_type {
    ACTIVE_Q = 0,
    INACTIVE_Q = 1,
    NBASSIGN_Q = 2,
    MONITOR_Q = 3,
    FUTURE_Q = 4,
    DELAY0_Q = 5,
    PINT_Q_NUM = 6,
};

extern unsigned short pint_enqueue_flag[];
extern unsigned short pint_monitor_enqueue_flag[];
extern struct pint_nbassign pint_nb_list[];
extern struct pint_nbassign_array pint_nb_array_list[];
extern int pint_q_head[PINT_Q_NUM];
extern unsigned pint_event_num;
extern pint_nbassign_t pint_nbq_base[];
extern pint_thread pint_active_q[];
extern pint_thread pint_inactive_q[];
extern pint_thread pint_monitor_q[];
extern pint_thread pint_delay0_q[];
extern pint_future_thread_t pint_future_q[];
extern pint_future_thread_s pint_future_thread[];
extern int pint_future_q_find[];

// dlpm
extern pint_future_thread_t pint_lpm_future_q[];
extern int pint_lpm_future_q_idx_active[];
extern int pint_lpm_future_q_idx_find[];

extern pint_thread monitor_func_ptr;
extern unsigned monitor_on;
extern pint_thread_t pint_q_base[];
extern struct pint_array_info array_info[];
extern unsigned pint_array_init_num;
extern unsigned pint_block_size;
extern unsigned int pint_mem_compact_count;
extern int _mem_subt_chunk_head;
extern unsigned char pint_event_flag[];

// static table
extern pint_thread_t static_event_list_table[];
extern int static_event_list_table_size[];
extern int static_event_list_table_num;

#ifdef PINT_PLI_MODE
// pli output
extern unsigned int pint_output_port_change_id[];
extern unsigned short pint_output_port_id_flag[];
extern unsigned int pint_output_port_id_flag_size;

//pli input
extern unsigned int pint_input_port_change_id[];
extern int pint_input_port_change_num;
#endif

//-------------function api-------------
void pint_global_init(void);
void pint_stop_simu();
void pint_advance_simu_time(simu_time_t next_time);

//---queue management
int pint_enqueue_thread(pint_thread thread, enum pint_queue_type q_type);

inline pint_thread pint_dequeue_thread(enum pint_queue_type q_type) {
    int deq_idx = pint_atomic_add(&pint_q_head[q_type], -1);
    if (deq_idx > 0) {
        pint_thread thread = pint_q_base[q_type][deq_idx - 1];
        return thread;
    } else {
        return NULL;
    }
}

#ifdef PROF_ON
inline pint_thread pint_dequeue_thread_prof(enum pint_queue_type q_type, int *deq_idx_t) {
    int deq_idx = pint_atomic_add(&pint_q_head[q_type], -1);
    if (deq_idx > 0) {
        pint_thread thread = pint_q_base[q_type][deq_idx - 1];
        *deq_idx_t = deq_idx - 1;
        return thread;
    } else {
        return NULL;
    }
}
#endif

// dlpm
int pint_lpm_enqueue_thread_future(pint_future_thread_t fthread, enum pint_queue_type q_type,
                                   int delay_lpm_idx);
int pint_enqueue_thread_future(pint_future_thread_t thread);
void pint_enqueue_nb_future(void *src, void *nb_dst, unsigned sig_width, int base,
                            unsigned copy_width, int array_idx, unsigned delay_idx, unsigned delay,
                            unsigned nb_id);
void pint_handle_future_nb(pint_future_nbassign_t nb, unsigned nb_id);
void pint_enqueue_nb_thread_by_idx(pint_nbassign_t nb);
void pint_enqueue_nb_thread_array_by_idx(unsigned id, unsigned array_idx);
pint_nbassign_t pint_dequeue_nb_thread(void);
int pint_wait_on_event(pint_wait_event_t wait_event, pint_thread thread);
int pint_move_event_thread_list(pint_wait_event_t wait_event);
int pint_move_event_thread_list_with_static_table(pint_wait_event_t wait_event, unsigned type,
                                                  pint_thread_t static_table);
// void pint_add_list_to_active(pint_thread_t list, unsigned thread_num);
void pint_add_list_to_inactive(pint_thread_t list, unsigned thread_num);
void pint_process_event_list(pint_thread_t e_list, unsigned e_type);
void pint_add_arr_list_to_inactive(pint_thread_t list, unsigned thread_num, unsigned word);
void pint_process_arr_event_list(pint_thread_t e_list, unsigned e_type, unsigned word);
void pint_process_list(struct pint_nbassign *nb, unsigned e_type);
void pint_nb_assign(pint_nbassign_t nb);
void execute_nbassign_q_thread(void);
void pint_set_event_flag(unsigned int *list);
#ifndef CPU_MODE
inline unsigned pint_enqueue_flag_set(unsigned int func_id) // func_id bit  1
{
    unsigned int rs2;
    unsigned short val;
    unsigned half_offset = func_id % 16;
    unsigned int offset = func_id >> 4;
    val = 1 << half_offset;
    rs2 = 0x10001 << half_offset;
    asm volatile("uap.xhum\t%1, %0" : "+m"(pint_enqueue_flag[offset]), "+r"(rs2));
    return rs2 & val;
}

inline void pint_enqueue_flag_clear(unsigned int func_id) // bit 0
{
    unsigned int rs2;
    unsigned half_offset = func_id % 16;
    unsigned int offset = func_id >> 4;
    rs2 = 0x10000 << half_offset;
    asm volatile("uap.xhum\t%1, %0" : "+m"(pint_enqueue_flag[offset]), "+r"(rs2));
}
#else
unsigned pint_enqueue_flag_set(unsigned int func_id);
void pint_enqueue_flag_clear(unsigned int func_id);

#endif

unsigned pint_enqueue_flag_set_any(unsigned short *buf, unsigned int func_id);
void pint_enqueue_flag_clear_any(unsigned short *buf, unsigned int func_id);
int edge(char from, char to);
int edge_char(char from, char to);
int edge_1bit(char from, char to);

int edge_int(unsigned *from, unsigned *to, unsigned size);
void pint_array_init(void);
void pint_array_init_parallel(void);
int pint_get_value_plusargs(const char *str, char format, void *dst, unsigned width);
int pint_get_test_plusargs(const char *str);

typedef enum pint_mem_alloc_type {
    SUB_T_CLEAR = 0, // after each sub_T, mem will be automatically freed
    T_CLEAR = 1,     // after each T, mem will be automatically freed
    CROSS_T = 2,     // the mem life scope will cross T with ref addr, so it need call free manually
} alloc_type;
unsigned int *pint_mem_alloc(unsigned int n_byte, alloc_type type);
void pint_mem_free(unsigned int *p);
void pint_mem_set_ref(unsigned int *p, unsigned int *ref);
void pint_mem_init();
void pint_mem_compact();

#ifdef PINT_PLI_MODE
void pint_update_output_value_change(unsigned int port_id);
extern void enqueue_change_callback_list(void);
#endif

#ifndef CPU_MODE
extern "C" void *memset(void *s, int c, size_t n);
extern "C" void *memcpy(void *dest, const void *src, size_t len);
#endif

#endif // __PINT_SIMU_H__
