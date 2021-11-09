
#include "../common/perf.h"

// ncore_num rows, of_type_MAX cols, RMO
// unsigned long long gperf_cycle_per_offunc[NCORE_NUM * of_type::of_type_MAX] = {0L};
// unsigned int gperf_calltimes_per_offunc[NCORE_NUM * of_type::of_type_MAX] = {0L};
// unsigned long long gperf_cycle_per_ncore[NCORE_NUM] = {0};

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

__kernel_arg__ int gperf_ncore_num_to_perf;
__kernel_arg__ int gperf_measure_type;
__kernel_arg__ unsigned int *gperf_calltimes_per_ncore;
__kernel_arg__ unsigned int lenof_gperf_calltimes_per_ncore;
__kernel_arg__ unsigned int gperf_dcache_counter_config;
__kernel_arg__ unsigned int gperf_scache_counter_config;
__kernel_arg__ struct perf_event_s *gperf_total_event; // d_total_events
__kernel_arg__ unsigned int lenof_gperf_total_event;
__kernel_arg__ unsigned int *gperf_event_startpos_per_ncore; // d_event_startpos
struct perf_event_s *gperf_event_pointer_per_ncore[NCORE_NUM];
struct perf_event_s *gperf_event_pointer_ori_per_ncore[NCORE_NUM];
unsigned long long total_cycles_per_ncore[NCORE_NUM];

volatile unsigned int gperf_is_do_ncore_prof = 1;
__kernel_arg__ unsigned int *gperf_event_realnum_per_ncore; // d_event_realnum
__kernel_arg__ unsigned int *gperf_mcore_parallel_cnt;
__kernel_arg__ unsigned int gperf_mcore_parallel_begin = 0;
__kernel_arg__ unsigned int gperf_mcore_parallel_end = 0xFFFFFFFF;
__kernel_arg__ unsigned int gperf_total_thread_num;
// for ncore_perf_summary
__kernel_arg__ perf_summary_s *device_func_summary_array;
perf_summary_s *func_summary_array;
__kernel_arg__ perf_thread_lpm_num_s *device_thread_lpm_num;

#if defined(__cplusplus)
}
#endif /* __cplusplus */

void PerfBegin() {
    const int ncore_num_to_perf = gperf_ncore_num_to_perf;
    // pint_assert( ncore_num_to_perf<=16 );
    if (ncore_num_to_perf > 17)
        pint_printf((char *)"\n\n[Assert fail]\n\n");

    if (gperf_measure_type == _EnableEventPerNcore) {
        //// calc startpos pointer
        for (int i = 0; i < ncore_num_to_perf; i++) {
            gperf_event_pointer_per_ncore[i] =
                gperf_total_event + gperf_event_startpos_per_ncore[i];
            gperf_event_pointer_ori_per_ncore[i] = gperf_event_pointer_per_ncore[i];
        }
    }
    gperf_is_do_ncore_prof = 1;
#ifdef PROF_SUMMARY_ON
    func_summary_array = device_func_summary_array;
#endif
}

// void PerfEnd()
// {
//   const int ncore_num_to_perf = gperf_ncore_num_to_perf;
//   //pint_assert( ncore_num_to_perf<=16 );

//   if(gperf_measure_type == _EnableEventPerNcore){
//     //// calc event realnum
//     for(int i=0; i<ncore_num_to_perf; i++)
//     {
//       gperf_event_realnum_per_ncore[i] =
//         gperf_event_pointer_per_ncore[i] - gperf_event_pointer_ori_per_ncore[i];
//     }

//     // for(int i=0; i<ncore_num_to_perf; i++)
//     // {
//     //   gperf_event_pointer_per_ncore[i] =
//     //     gperf_total_event + gperf_event_startpos_per_ncore[i];
//     //   gperf_event_pointer_ori_per_ncore[i] = gperf_event_pointer_per_ncore[i];
//     // }

//     pint_printf((char*)"\ngperf_event_realnum_per_ncore:\n");
//     for(int i=0; i<ncore_num_to_perf; i++)
//     {
//       pint_printf((char*)"%d, ",gperf_event_realnum_per_ncore[i]);
//       if(gperf_event_realnum_per_ncore[i] > 200000) {
//         pint_printf("gperf_event_realnum_per_ncore[i] is bigger than 200000!!!!!!!!!!!\n");
//       }
//     }
//     pint_printf((char*)"\n\n");
//   }
//   gperf_is_do_ncore_prof = 0;
// }

void PerfEnd() {
    const int ncore_num_to_perf = gperf_ncore_num_to_perf;
    // pint_assert( ncore_num_to_perf<=16 );

    if (gperf_measure_type == _EnableEventPerNcore) {
        //// calc event realnum
        for (int i = 0; i < ncore_num_to_perf; i++) {
            gperf_event_realnum_per_ncore[i] =
                gperf_event_pointer_per_ncore[i] - gperf_event_pointer_ori_per_ncore[i];
        }

        pint_printf((char *)"\ngperf_event_realnum_per_ncore:\n");
        for (int i = 0; i < ncore_num_to_perf; i++) {
            pint_printf((char *)"%d, ", gperf_event_realnum_per_ncore[i]);
        }
        pint_printf((char *)"\n\n");
    }
    gperf_is_do_ncore_prof = 0;
}

int pint_get_profile_total_event_num() {
    int total_event_num = 0;
    const int ncore_num_to_perf = gperf_ncore_num_to_perf;
    // pint_assert( ncore_num_to_perf<=17 );

    if (gperf_measure_type == _EnableEventPerNcore) {
        //// calc event realnum
        for (int i = 0; i < ncore_num_to_perf; i++) {
            total_event_num +=
                gperf_event_pointer_per_ncore[i] - gperf_event_pointer_ori_per_ncore[i];
        }

        // pint_printf((char*)"total_event_num = %d, ",total_event_num);
        return total_event_num;
    }
    return 0;
}