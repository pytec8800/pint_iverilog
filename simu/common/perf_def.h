#ifndef __PERF_DEF_H__
#define __PERF_DEF_H__

#define NCORE_NUM (256)
#define IDX_RMO(r, c, ld) ((c) + (r) * (ld))

template <unsigned int bit_index> constexpr unsigned long long gen_pow2n_u64() {
    static_assert(bit_index < 64, "");
    return (1 << bit_index);
}

template <unsigned int bit_index, unsigned long long in_mask> constexpr bool is_bit_set() {
    constexpr unsigned long long inner_prof_bit = gen_pow2n_u64<bit_index>();
    return (inner_prof_bit & (in_mask)) != 0;
}

#define PROFMASK0_MCORE_SUMMARY 31
#define PROFMASK0_MCORE_PARALLEL_RANGE 30
#define PROFMASK0_USE_ALL_NCORE 29
#define PROFMASK0_NCORE_CACHE_COUNTER 28

#pragma pack(push, 1)
struct perf_summary_s {
    unsigned long long total_cycles;
    unsigned long long call_times;
};
#pragma pack(pop)

struct perf_thread_lpm_num_s {
    unsigned lpm_num;
    unsigned monitor_lpm_num;
    unsigned thread_lpm_num;
    unsigned force_lpm_num;
    unsigned event_num;
    unsigned thread_num;
};

// google trace viewer "complete event"
#pragma pack(push, 1)
struct perf_event_s {
    // perf_event_s(){}
    // perf_event_s(const perf_event_s& e){
    //  ts_ = e.ts_;
    //  dur_ = e.dur_;
    //  func_ = e.func_;
    //}
    perf_event_s &operator=(const perf_event_s &e) { // fixed: undefine reference to 'memcpy'
        ts_ = e.ts_;
        dur_ = e.dur_;
        func_ = e.func_;
        cache_counter_ = e.cache_counter_;
        core_idx_ = e.core_idx_;
        return *this;
    }
    unsigned long long ts_; // start time stamp (in cycle)
    long long dur_;         // duration (in cycle)
    unsigned int func_;     // func address (32bit)
    unsigned int
        cache_counter_; // lower 16bit store dcache_count and higher 16bit store scache_count
    int core_idx_;
};
#pragma pack(pop)

#define _HasEnable(x, mask) (((x) & (mask)) != 0) /* no used */

#define _EnableCalltimesPerNcore (0x01)
#define _EnableEventPerNcore (0x02)
#define _EnableNcoreOccupancy (0x03)
#define PROFILE_TIME_SETP_INTERVAL (2000)
#define PROFILE_TIME_SETP_EACH (200)
#define PROFILE_EVENT_NUM_THRESHOLD (500000)
#define TOTAL_PROFILE_EVENT_NUM_THRESHOLD (0xC0000)

#endif
