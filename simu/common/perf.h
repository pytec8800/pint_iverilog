#ifndef __PERF_H__
#define __PERF_H__

#ifdef CPU_MODE
#include <stdio.h>
#include "../inc/cpu_adapt.h"
inline void pint_ncore_cycle_get_64bit(unsigned long long *ts) {
    static_assert(sizeof(*ts) == 8, "");
    pint_mcore_cycle_get((unsigned int *)ts);
}
#else
#include "pintdev.h"
//#define CSR_YZENG_27 0x27
inline void pint_ncore_cycle_get_64bit(unsigned long long *ts) {
    static_assert(sizeof(*ts) == 8, "");
    read_csr_safe(CSR_NCORE_CYCLE_RD, ((unsigned int *)ts)[0]);
    read_csr_safe((0x27), ((unsigned int *)ts)[1]);
}
#endif

#include "perf_def.h"
#include "../common/pint_perf_counter.h"

#define __kernel_arg__ __attribute__((section(".builtin_kernel_args")))
//#define NO_INSTRUMENT __attribute__((no_instrument_function))

extern unsigned pint_ro_start; // used in execute_nbassign_q_thread
#define PROF_BEGIN(func_name, func_idx)                                                            \
    int core_id = pint_core_id();                                                                  \
    constexpr bool vv = is_bit_set<PROFMASK0_USE_ALL_NCORE, g_prof_mask>();                        \
    if (vv || ((0 == (core_id & 15)) && (core_id < (gperf_ncore_num_to_perf << 4)))) {             \
        CACHE_COUNT_START();                                                                       \
        NCORE_PERF_MEASURE((func_name), 0);                                                        \
        NCORE_PERF_SUMMARY((func_idx));                                                            \
        pint_ro_seg_set(pint_ro_start, PINT_STACK_END);

#define PROF_BEGIN_NO_COREID_DECLARE(func_name, func_idx)                                          \
    if (vv || ((0 == (core_id & 15)) && (core_id < (gperf_ncore_num_to_perf << 4)))) {             \
        CACHE_COUNT_START();                                                                       \
        NCORE_PERF_MEASURE((func_name), 0);                                                        \
        NCORE_PERF_SUMMARY((func_idx));                                                            \
        pint_ro_seg_set(pint_ro_start, PINT_STACK_END);

#define PROF_END()                                                                                 \
    pint_ro_seg_set(1, 0);                                                                         \
    }

#ifdef PROF_ON
#ifndef PROF_MASK
#define PROF_MASK (0xFFFFFFFFFFFFFFFF)
#endif
constexpr unsigned long long g_prof_mask = (unsigned long long)(PROF_MASK);
constexpr unsigned int measure_type = ((unsigned long)(PROF_MASK) & (3UL << 27)) >> 27;

template <unsigned int bit_index> constexpr bool is_do_prof() {
    constexpr unsigned long long inner_prof_bit = gen_pow2n_u64<bit_index>();
    return (inner_prof_bit & (g_prof_mask)) != 0;
}
#define NCORE_PERF_MEASURE(func, level)                                                            \
    /*ncore_perf_t<0, (PROF_LEVEL_MIN<=(level))&&((level)<PROF_LEVEL_MAX)> */                      \
    ncore_perf_t<measure_type, is_do_prof<(level)>()> __xxxyzeng_ncore_perf_obj(                   \
        (unsigned int)(func));
// #define NCORE_PERF_MEASURE_WITH_DQU_IDX(func, level, deq_idx)                                      \
//     /*ncore_perf_t<0, (PROF_LEVEL_MIN<=(level))&&((level)<PROF_LEVEL_MAX)> */                      \
//     ncore_perf_t<measure_type, is_do_prof<(level)>()> __xxxyzeng_ncore_perf_obj(                   \
//         (unsigned int)(func), (int)(deq_idx));
#define MCORE_PERF_MEASURE(func, level)                                                            \
    /*ncore_perf_t<0, (PROF_LEVEL_MIN<=(level))&&((level)<PROF_LEVEL_MAX)> */                      \
    ncore_perf_t<measure_type, is_do_prof<(level)>()> __xxxyzeng_ncore_perf_obj(                   \
        (unsigned int)(func), (0));

#define NCORE_PERF_PINT_NET_SUMMARY(index)
#else
#define NCORE_PERF_MEASURE(func, level)
// #define NCORE_PERF_MEASURE_WITH_DQU_IDX(func, level, deq_idx)
#define MCORE_PERF_MEASURE(func, level)
#endif

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

extern int gperf_ncore_num_to_perf;
extern int gperf_measure_type;
extern unsigned int lenof_gperf_calltimes_per_ncore;
extern unsigned int gperf_dcache_counter_config;
extern unsigned int gperf_scache_counter_config;

extern unsigned int *gperf_calltimes_per_ncore; //_EnableCalltimesPerNcore
extern unsigned int lenof_gperf_total_event;
extern perf_event_s *gperf_total_event; //_EnableEventPerNcore

// size_of = NCORE_NUM * sizeof(perf_event_s*)
extern unsigned long long total_cycles_per_ncore[];
extern struct perf_event_s *gperf_event_pointer_per_ncore[];
extern struct perf_event_s *gperf_event_pointer_ori_per_ncore[];
extern unsigned int *gperf_event_startpos_per_ncore;
extern unsigned int *gperf_event_realnum_per_ncore;
extern volatile unsigned int gperf_is_do_ncore_prof;
extern unsigned int *gperf_mcore_parallel_cnt;
extern unsigned int gperf_mcore_parallel_begin;
extern unsigned int gperf_mcore_parallel_end;
extern perf_summary_s *device_func_summary_array;
extern perf_summary_s *func_summary_array;

// for ncore_perf_summary
extern unsigned int gperf_total_thread_num;

// extern unsigned int parallel_cnt;

#define CACHE_COUNT_START()                                                                        \
    if                                                                                             \
        constexpr(measure_type == Enable_DCACHE_COUNT) {                                           \
            pint_dcache_counter_start(gperf_dcache_counter_config);                                \
        }                                                                                          \
    else if                                                                                        \
        constexpr(measure_type == Enable_DCACHE2SCACHE_COUNT) {                                    \
            pint_dcache2shared_counter_start(gperf_scache_counter_config);                         \
        }                                                                                          \
    else if                                                                                        \
        constexpr(measure_type == Enable_DCACHE_AND_SCACHE_COUNT) {                                \
            pint_dcache_counter_start(gperf_dcache_counter_config);                                \
            pint_dcache2shared_counter_start(gperf_scache_counter_config);                         \
        }

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#ifdef PROF_SUMMARY_ON
class ncore_perf_summary {
  public:
    explicit ncore_perf_summary(unsigned int index) : func_index_(index) {
        pint_ncore_cycle_get((unsigned int *)(&(start_cycle_)));
    }

    ~ncore_perf_summary() {
        pint_ncore_cycle_get((unsigned int *)(&(stop_cycle_)));
        if (start_cycle_ > stop_cycle_) {
            cycle_ = stop_cycle_ - start_cycle_ + 0xffffffff;
        } else {
            cycle_ = stop_cycle_ - start_cycle_;
        }
        int core_id = (pint_core_id() >> 4);
        func_summary_array[core_id * gperf_total_thread_num + func_index_].total_cycles += cycle_;
        func_summary_array[core_id * gperf_total_thread_num + func_index_].call_times++;
        // pint_printf("cycle_ = %d.\n", cycle_);
        // pint_printf("call_times = %d.\n", func_summary_array[core_id*gperf_total_thread_num +
        // func_index_].call_times);
        // pint_dcache_discard();
    }

    unsigned int start_cycle_;
    unsigned int stop_cycle_;
    unsigned int cycle_;
    unsigned int func_index_;
};

#define NCORE_PERF_SUMMARY(index) ncore_perf_summary ncore_perf_summary_obj((unsigned int)(index));
#define MCORE_PERF_SUMMARY(index) ncore_perf_summary ncore_perf_summary_obj((unsigned int)(index));
#define NCORE_PERF_PINT_NET_SUMMARY(index)

#else
#define NCORE_PERF_SUMMARY(index)
#define MCORE_PERF_SUMMARY(index)
#define NCORE_PERF_PINT_NET_SUMMARY(index)
#endif

#ifdef PROF_ON
template <int measure_type /*for future use*/, bool do_prof = true> class ncore_perf_t {
  public:
    explicit ncore_perf_t(unsigned int func_addr) noexcept {
        measure_type_ = gperf_measure_type;
        index_ = (pint_core_id() >> 4) + 1;
        if (gperf_is_do_ncore_prof == 0) {

        } else {
            if (measure_type_ == _EnableEventPerNcore) {
                evt_.func_ = func_addr;
                evt_.ts_ = 0L;
                evt_.core_idx_ = index_;
                pint_ncore_cycle_get((unsigned int *)(&(evt_.ts_)));
                start_cache_count_read();
            } else if (measure_type_ == _EnableNcoreOccupancy) {
                evt_.ts_ = 0L;
                pint_ncore_cycle_get((unsigned int *)(&(evt_.ts_)));
            } else {
            }
        }
    }

    // explicit ncore_perf_t(unsigned int func_addr, int deq_idx) noexcept {
    //     measure_type_ = gperf_measure_type;
    //     index_ = (pint_core_id() >> 4) + 1;
    //     if (gperf_is_do_ncore_prof == 0) {

    //     } else {
    //         if (measure_type_ == _EnableEventPerNcore) {
    //             evt_.func_ = func_addr;
    //             evt_.ts_ = 0L;
    //             evt_.core_idx_ = index_;
    //             pint_ncore_cycle_get((unsigned int *)(&(evt_.ts_)));
    //             start_cache_count_read();
    //         } else {
    //         }
    //     }
    // }

    explicit ncore_perf_t(unsigned int func_addr, int core_id) noexcept {
        measure_type_ = gperf_measure_type;
        index_ = core_id;
        if (gperf_is_do_ncore_prof == 0) {

        } else {
            if (measure_type_ == _EnableEventPerNcore) {
                evt_.func_ = func_addr;
                evt_.ts_ = 0L;
                evt_.core_idx_ = index_;
                pint_ncore_cycle_get((unsigned int *)(&(evt_.ts_)));
                start_cache_count_read();
            } else if (measure_type_ == _EnableNcoreOccupancy) {
                evt_.ts_ = 0L;
                pint_ncore_cycle_get((unsigned int *)(&(evt_.ts_)));
            } else {
            }
        }
    }

    ~ncore_perf_t() noexcept {
        if (gperf_is_do_ncore_prof == 0) {

        } else {
            if (measure_type_ == _EnableEventPerNcore) {
                unsigned long long stop_cycle_ = 0L;
                pint_ncore_cycle_get((unsigned int *)(&stop_cycle_));
                if (evt_.ts_ > stop_cycle_) {
                    evt_.dur_ = stop_cycle_ - evt_.ts_ + 0xffffffff;
                } else {
                    evt_.dur_ = stop_cycle_ - evt_.ts_;
                }
                stop_cache_count_read();
                gperf_event_pointer_per_ncore[index_][0] = evt_;
                gperf_event_pointer_per_ncore[index_]++;
                total_cycles_per_ncore[index_] += evt_.dur_;
            } else if (measure_type_ == _EnableCalltimesPerNcore) {
                gperf_calltimes_per_ncore[index_]++;
            } else if (measure_type_ == _EnableNcoreOccupancy) {
                // pint_printf("enter ncore %d. _EnableNcoreOccupancy.\n", index_);
                unsigned long long stop_cycle_ = 0L;
                pint_ncore_cycle_get((unsigned int *)(&stop_cycle_));
                if (evt_.ts_ > stop_cycle_) {
                    evt_.dur_ = stop_cycle_ - evt_.ts_ + 0xffffffff;
                } else {
                    evt_.dur_ = stop_cycle_ - evt_.ts_;
                }
                total_cycles_per_ncore[index_] += evt_.dur_;
            }
        }
        // pint_dcache_discard();
    }

  private:
    inline void start_cache_count_read() {
        if
            constexpr(measure_type == Enable_DCACHE_COUNT) {
                couter_start_ = pint_get_dcache_counter();
            }
        else if
            constexpr(measure_type == Enable_DCACHE2SCACHE_COUNT) {
                couter_start_ = pint_get_dcache2shared_counter();
            }
        else if
            constexpr(measure_type == Enable_DCACHE_AND_SCACHE_COUNT) {
                couter_start_ = pint_get_dcache_counter();
                scache_couter_start_ = pint_get_dcache2shared_counter();
            }
    }

    inline void stop_cache_count_read() {
        if
            constexpr(measure_type == Enable_PARALLEL_COUNT) {
                evt_.cache_counter_ = 0;
                // evt_.cache_counter_ = parallel_cnt;
                // pint_printf("evt_.cache_counter_ : %d.\n", evt_.cache_counter_);
            }
        if
            constexpr(measure_type == Enable_DCACHE_COUNT) {
                unsigned int dcache_count = pint_get_dcache_counter() - couter_start_;
                evt_.cache_counter_ = dcache_count;
            }
        else if
            constexpr(measure_type == Enable_DCACHE2SCACHE_COUNT) {
                unsigned int scache_count = pint_get_dcache2shared_counter() - couter_start_;
                evt_.cache_counter_ = scache_count << 16; // higher 16bit store scache_count
            }
        else if
            constexpr(measure_type == Enable_DCACHE_AND_SCACHE_COUNT) {
                unsigned int dcache_count = pint_get_dcache_counter() - couter_start_;
                unsigned int scache_count = pint_get_dcache2shared_counter() - scache_couter_start_;
                evt_.cache_counter_ =
                    dcache_count +
                    (scache_count
                     << 16); // lower 16bit store dcache_count and higher 16bit store scache_count
            }
    }

  public:
    perf_event_s evt_;
    int measure_type_;
    unsigned int couter_start_;
    unsigned int scache_couter_start_;
    int index_;
};
template <int measure_type> class ncore_perf_t<measure_type, false> {
  public:
    ncore_perf_t(unsigned int func_addr) noexcept {}
    // ncore_perf_t(unsigned int func_addr, int deq_idx) noexcept {}
    ncore_perf_t(unsigned int func_addr, int core_id) noexcept {}
};
#endif

#define LSTR(x) #x

#ifdef PROF_ON
#define MCORE_PERF_MEASURE_PRINT(value1, value2 /*, level*/)                                       \
    timer_t<g_prof_mask> ____xx_timer(value1, LSTR(value1), value2, LSTR(value2));                 \
    asm volatile("uap.wait.wreq");
#else
#define MCORE_PERF_MEASURE_PRINT(value1, value2 /*, level*/)
#endif

template <typename T> class TD {};

// int a[PROF_MASK];
// TD<decltype(a)> b=2;
#ifdef PROF_ON
template <unsigned long long prof_mask> class timer_t {
  public:
    explicit timer_t(unsigned int value_1, const char *s1, unsigned int value_2, const char *s2)
        : val_1_(value_1), val_desc_str_1_(s1), val_2_(value_2), val_desc_str_2_(s2) {
        // int a[prof_mask];
        // TD<decltype(a)> b=2;
        // static_assert(is_bit_set<PROFMASK0_MCORE_PARALLEL_RANGE, prof_mask>(), "");
        if
            constexpr(is_bit_set<PROFMASK0_MCORE_PARALLEL_RANGE, prof_mask>()) {
                if ((gperf_mcore_parallel_begin <= val_2_) && (val_2_ < gperf_mcore_parallel_end)) {
                    gperf_is_do_ncore_prof = 1;
                } else {
                    gperf_is_do_ncore_prof = 0;
                }
            }

        // static_assert(is_bit_set<PROFMASK0_MCORE_SUMMARY, prof_mask>()=false, "");
        if
            constexpr(is_bit_set<PROFMASK0_MCORE_SUMMARY, prof_mask>()) {
                start_ = 0L;
                pint_mcore_cycle_get((unsigned int *)(&(start_)));
            }
    }

    ~timer_t() noexcept {
        // static_assert(prof_mask==0, "");
        if
            constexpr(is_bit_set<PROFMASK0_MCORE_SUMMARY, prof_mask>()) {
                unsigned long long stop_cycle = 0L;
                unsigned int dur = 0L;
                pint_mcore_cycle_get((unsigned int *)(&stop_cycle));
                if (start_ > stop_cycle) {
                    dur = stop_cycle - start_ + 0xffffffff;
                } else {
                    dur = stop_cycle - start_;
                }
                // pint_printf((char*)"\n[timer_t] cycle:%llu, %s:%u, %s:%u\n",
                //  (stop_cycle - start_),
                //  val_desc_str_1_, val_1_,
                //  val_desc_str_2_, val_2_);
                pint_printf(
                    (char *)"\n[mcore-measure] cycle; %u; %s; %u; %s; %u\n", //, profmask=%x\n",
                    (unsigned int)(dur), val_desc_str_1_, val_1_, val_desc_str_2_,
                    val_2_ /*, prof_mask*/);
            }
    }

  public:
    unsigned long long start_;
    unsigned int val_1_;
    const char *val_desc_str_1_;
    unsigned int val_2_;
    const char *val_desc_str_2_;
};
#endif

// template<
//  unsigned long long prof_mask,
//>
// class timer_t<
//  prof_mask,
//  false
//  //(!is_bit_set<PROFMASK0_MCORE_SUMMARY, g_prof_mask>()) &&
//  //(!is_bit_set<PROFMASK0_MCORE_PARALLEL_RANGE, g_prof_mask>())
//>
//{
// public:
//  explicit timer_t(
//    unsigned int value_1,
//    const char* s1,
//    unsigned int value_2,
//    const char* s2)
//  {
//  }
//
//  ~timer_t() noexcept
//  {
//  }
//};

// void NO_INSTRUMENT
// inline __cyg_profile_func_enter(void* this_func, void* call_site){
//
//}
//
// void NO_INSTRUMENT
// inline __cyg_profile_func_exit(void* this_func, void* call_site){
//}

void PerfBegin();
void PerfEnd();
int pint_get_profile_total_event_num();

#endif
