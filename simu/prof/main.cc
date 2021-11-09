#include <stdio.h>
#include <cstdio>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <ctime>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <string>
#include <cstring>
#include <algorithm>

#include "pint_runtime_api.h"

#include "perf_def.h"
#include "../common/pint_perf_counter.h"
#include "../common/sys_args.h"

#define DUMP_CSV_WITH_PARALLEL_COUNT
#define MAX_WRITE_EVENT_CNT 10485760

#define CheckPintApi(expr)                                                                         \
    do {                                                                                           \
        pintError_t errcode = expr;                                                                \
        if (errcode != pintSuccess) {                                                              \
            fprintf(stderr, "\n[pintapi error] %s"                                                 \
                            "\nlocation: %s:%d"                                                    \
                            "\nerror code: %d"                                                     \
                            "\n",                                                                  \
                    #expr, __FILE__, __LINE__, (int)errcode);                                      \
            exit(-1);                                                                              \
        }                                                                                          \
    } while (false)

struct perf_event_new_s {
    unsigned long long ts_;      // start time stamp (in cycle)
    long long dur_;              // duration (in cycle)
    unsigned int func_;          // func address (32bit)
    unsigned int cache_counter_; //
    int deq_idx_;
    int tid_;
};

struct summary_info {
    unsigned long long total_dur_time;
    unsigned long long max_dur_time;
    unsigned long long min_dur_time;
    unsigned int call_times;
    unsigned int total_dcache_count;
    unsigned int max_dcache_count;
    unsigned int min_dcache_count;
    unsigned int total_scache_count;
    unsigned int max_scache_count;
    unsigned int min_scache_count;
};

// for sys_args_pulgin
int sys_args_num = 0;
char *sys_args_buf = NULL;
int *sys_args_off = NULL;

enum PROFILE_CMD { TOTAL_SUMMARY_CMD = 0, TIMELINE_CMD, PROF_ALL_CMD, PROF_NCORE_OCCUPANCY_CMD };

void create_pint_bin(const std::string &marco_define, const std::string &src_root,
                     const std::string &thread_file_path, const std::string &temp_dir) {
    std::string srcfile1 = src_root + "src/main.c";
    std::string srcfile2 = src_root + "src/pint_simu.c";
    std::string srcfile3 = src_root + "src/pint_net.c";
    std::string srcfile4 = src_root + "src/pint_vpi.c";
    std::string srcfile5 = src_root + "src/perf.c";
    std::string ccoption = " -ccoption=\"-I" + src_root + "/inc/" + " -I" + src_root +
                           marco_define + " -std=c++1z  -O2  \"";

    std::string cmd = std::string("mkdir -p ") + temp_dir + std::string(" && ") +
                      std::string("pintcc -pint_api -d -o ") + std::string(temp_dir + "pint.out ") +
                      // std::string("-ccoption=\"-I../inc/ -I../common/ -DPROF_ON -O2 \"") +
                      ccoption + srcfile1 + " " + srcfile2 + " " + srcfile3 + " " + srcfile4 + " " +
                      srcfile5 + " " + thread_file_path + " " +
                      std::string("&& pint_generate_pin ") + temp_dir + std::string(" pint ");
    int rc = system(cmd.c_str());
    // fprintf(stderr, "\nsystem command [%s]\n", cmd.c_str());
    if (rc != 0) {
        fprintf(stderr, "\nsystem command [%s] run failed.\n", cmd.c_str());
        std::exit(-2);
    }
}

void argsMemcpyToDevice(int **sys_args_off, char **sys_args_buf, unsigned int **d_sys_args_off,
                        unsigned int **d_sys_args_buf) {
    int sizeof_sys_args_off = SYS_ARGS_OFF_SIZE * sizeof(int);
    int sizeof_sys_args_buf = SYS_ARGS_BUF_SIZE;
    CheckPintApi(pintMalloc((void **)d_sys_args_off, sizeof_sys_args_off));
    CheckPintApi(pintMemcpy(*d_sys_args_off, 0, *sys_args_off, 0, sizeof_sys_args_off,
                            pintMemcpyHostToDevice));

    CheckPintApi(pintMalloc((void **)d_sys_args_buf, sizeof_sys_args_buf));
    CheckPintApi(pintMemcpy(*d_sys_args_buf, 0, *sys_args_buf, 0, sizeof_sys_args_buf,
                            pintMemcpyHostToDevice));
    return;
}

void calc_calltimes_per_ncore(std::vector<unsigned int> &h_calltimes, pintProgram_t &pint_program,
                              int ncore_num_to_perf, unsigned int parallel_begin,
                              unsigned int parallel_end, unsigned int dcache_counter_config,
                              unsigned int scache_counter_config) {
    int sizeof_h_calltimes = h_calltimes.size() * sizeof(unsigned int);
    int measure_type = _EnableCalltimesPerNcore;
    unsigned int *d_calltimes = nullptr;
    unsigned int *d_sys_args_off = (unsigned int *)0x8000;
    unsigned int *d_sys_args_buf = (unsigned int *)0x8000;

    CheckPintApi(pintMalloc((void **)&d_calltimes, sizeof_h_calltimes));
    CheckPintApi(pintMemcpy(d_calltimes, 0, h_calltimes.data(), 0, sizeof_h_calltimes,
                            pintMemcpyHostToDevice));

    if (sys_args_num > 0) {
        argsMemcpyToDevice(&sys_args_off, &sys_args_buf, &d_sys_args_off, &d_sys_args_buf);
    }

    int tt = h_calltimes.size();
    int num = 1;
    printf("sys_args_num in pint_simu_prof calc_calltimes_per_ncore: %d.\n", sys_args_num);
    printf("sys_args_off[0]: %d.\n", sys_args_off[0]);
    printf("sys_args_buf: %s.\n", sys_args_buf);

    pintArgs_t kernel_args[] = {
        {"gperf_calltimes_per_ncore", d_calltimes},
        {"lenof_gperf_calltimes_per_ncore", &tt},
        {"gperf_ncore_num_to_perf", &ncore_num_to_perf},
        {"gperf_measure_type", &measure_type},
        {"gperf_mcore_parallel_begin", &parallel_begin},
        {"gperf_mcore_parallel_end", &parallel_end},
        {"gperf_dcache_counter_config", &dcache_counter_config},
        {"gperf_scache_counter_config", &scache_counter_config},
    };

    pintArgs_t kernel_args_sys[] = {
        {"pint_sys_args_num", &sys_args_num},
        {"pint_sys_args_off", d_sys_args_off},
        {"pint_sys_args_buff", d_sys_args_buf},
    };

    std::vector<pintArgs_t> kernel_args_vec(sizeof(kernel_args) / sizeof(pintArgs_t));
    memcpy(kernel_args_vec.data(), kernel_args, sizeof(kernel_args));
    std::vector<pintArgs_t> kernel_args_sys_vec(sizeof(kernel_args_sys) / sizeof(pintArgs_t));
    memcpy(kernel_args_sys_vec.data(), kernel_args_sys, sizeof(kernel_args_sys));

    if (sys_args_num > 0) {
        kernel_args_vec.insert(kernel_args_vec.end(), kernel_args_sys_vec.begin(),
                               kernel_args_sys_vec.end());
    }

    CheckPintApi(
        pintLaunchProgram(pint_program, kernel_args_vec.data(), kernel_args_vec.size(), NULL, 0));
    CheckPintApi(pintMemcpy(h_calltimes.data(), 0, d_calltimes, 0, sizeof_h_calltimes,
                            pintMemcpyDeviceToHost));

    printf("\nh_calltimes:\n");
    for (auto var : h_calltimes) {
        printf("%d, ", var);
    }
    printf("\n\n");

    CheckPintApi(pintFree(d_calltimes));
    if (sys_args_num > 0) {
        CheckPintApi(pintFree(d_sys_args_off));
        CheckPintApi(pintFree(d_sys_args_buf));
    }
}

////3. make summary file.
void make_summary_map(std::vector<perf_event_new_s> &h_total_events_new,
                      std::unordered_map<std::string, summary_info> &funcname_time_map,
                      std::unordered_map<unsigned int, std::string> &funcaddr_name_map) {
    // std::unordered_map<std::string, summary_info> funcname_time_map;
    for (auto evt : h_total_events_new) {
        const std::string &funcname = funcaddr_name_map[evt.func_];
        auto search = funcname_time_map.find(funcname);
        if (search != funcname_time_map.end()) { // Found
            funcname_time_map[funcname].total_dur_time += evt.dur_;
            funcname_time_map[funcname].max_dur_time =
                funcname_time_map[funcname].max_dur_time >= evt.dur_
                    ? funcname_time_map[funcname].max_dur_time
                    : evt.dur_;
            funcname_time_map[funcname].min_dur_time =
                funcname_time_map[funcname].min_dur_time <= evt.dur_
                    ? funcname_time_map[funcname].min_dur_time
                    : evt.dur_;
            funcname_time_map[funcname].call_times += 1;
            funcname_time_map[funcname].total_dcache_count += evt.cache_counter_ & 0x0000ffff;
            funcname_time_map[funcname].max_dcache_count =
                funcname_time_map[funcname].max_dcache_count >= evt.cache_counter_ & 0x0000ffff
                    ? funcname_time_map[funcname].max_dcache_count
                    : evt.cache_counter_ & 0x0000ffff;
            funcname_time_map[funcname].min_dcache_count =
                funcname_time_map[funcname].min_dcache_count <= evt.cache_counter_ & 0x0000ffff
                    ? funcname_time_map[funcname].min_dcache_count
                    : evt.cache_counter_ & 0x0000ffff;
            funcname_time_map[funcname].total_scache_count += evt.cache_counter_ >> 16;
            funcname_time_map[funcname].max_scache_count =
                funcname_time_map[funcname].max_scache_count >= evt.cache_counter_ >> 16
                    ? funcname_time_map[funcname].max_scache_count
                    : evt.cache_counter_ >> 16;
            funcname_time_map[funcname].min_scache_count =
                funcname_time_map[funcname].min_scache_count <= evt.cache_counter_ >> 16
                    ? funcname_time_map[funcname].min_scache_count
                    : evt.cache_counter_ >> 16;
        } else { // Not found
            funcname_time_map[funcname].total_dur_time = evt.dur_;
            funcname_time_map[funcname].max_dur_time = evt.dur_;
            funcname_time_map[funcname].min_dur_time = evt.dur_;
            funcname_time_map[funcname].call_times = 1;
            funcname_time_map[funcname].total_dcache_count = evt.cache_counter_ & 0x0000ffff;
            funcname_time_map[funcname].max_dcache_count = evt.cache_counter_ & 0x0000ffff;
            funcname_time_map[funcname].min_dcache_count = evt.cache_counter_ & 0x0000ffff;
            funcname_time_map[funcname].total_scache_count = evt.cache_counter_ >> 16;
            funcname_time_map[funcname].max_scache_count = evt.cache_counter_ >> 16;
            funcname_time_map[funcname].min_scache_count = evt.cache_counter_ >> 16;
        }
    }
}

void collect_all_events(pintProgram_t &pint_program,
                        std::vector<unsigned int> &h_event_startpos, /*[out]*/
                        std::vector<perf_event_s> &h_total_events,   /*[out]*/
                        std::vector<unsigned int> &h_event_realnum,  /*[out]*/
                        int ncore_num_to_perf, unsigned int parallel_begin,
                        unsigned int parallel_end, unsigned int dcache_counter_config,
                        unsigned int scache_counter_config) {
    int measure_type = _EnableEventPerNcore;
    unsigned int *d_total_events = nullptr;
    ncore_num_to_perf = 17;
    unsigned int events_max_per_core = 200000;
    unsigned int total_events = ncore_num_to_perf * events_max_per_core;
    unsigned int sizeof_total_events = total_events * sizeof(perf_event_s); // 136MB;
    printf("====================collect_all_events============================.\n");
    printf("sizeof_total_events = %d.\n", sizeof_total_events);

    unsigned int *d_event_startpos = nullptr;
    unsigned int sizeof_event_startpos = ncore_num_to_perf * sizeof(d_event_startpos[0]);

    unsigned int *d_event_realnum = nullptr;
    unsigned int sizeof_event_realnum = ncore_num_to_perf * sizeof(d_event_realnum[0]);

    unsigned int *d_sys_args_off = nullptr;
    unsigned int *d_sys_args_buf = nullptr;
    // h_event_startpos[0]=0;
    // prefix-sum
    h_event_startpos.resize(ncore_num_to_perf, 0);
    h_total_events.resize(total_events, perf_event_s{0, 0, 0, 0, 0});
    h_event_realnum.resize(ncore_num_to_perf, 0);

    assert(h_event_startpos[0] == 0);
    for (size_t i = 1; i < ncore_num_to_perf; i++) {
        h_event_startpos[i] = h_event_startpos[i - 1] + events_max_per_core;
    }

    CheckPintApi(pintMalloc((void **)&d_total_events, sizeof_total_events));
    CheckPintApi(pintMemcpy(d_total_events, 0, h_total_events.data(), 0, sizeof_total_events,
                            pintMemcpyHostToDevice));
    CheckPintApi(pintMalloc((void **)&d_event_startpos, sizeof_event_startpos));
    CheckPintApi(pintMemcpy(d_event_startpos, 0, h_event_startpos.data(), 0, sizeof_event_startpos,
                            pintMemcpyHostToDevice));
    CheckPintApi(pintMalloc((void **)&d_event_realnum, sizeof_event_realnum));

    if (sys_args_num > 0) {
        argsMemcpyToDevice(&sys_args_off, &sys_args_buf, &d_sys_args_off, &d_sys_args_buf);
    }

    pintArgs_t kernel_args[] = {
        {"gperf_measure_type", &measure_type},
        {"gperf_total_event", d_total_events},
        {"gperf_event_startpos_per_ncore", d_event_startpos},
        {"gperf_event_realnum_per_ncore", d_event_realnum},
        {"gperf_ncore_num_to_perf", &ncore_num_to_perf},
        // {"lenof_gperf_total_event", &total_events},
        {"gperf_dcache_counter_config", &dcache_counter_config},
        {"gperf_scache_counter_config", &scache_counter_config},
        {"gperf_mcore_parallel_begin", &parallel_begin},
        {"gperf_mcore_parallel_end", &parallel_end},
    };

    pintArgs_t kernel_args_sys[] = {
        {"pint_sys_args_num", &sys_args_num},
        {"pint_sys_args_off", d_sys_args_off},
        {"pint_sys_args_buff", d_sys_args_buf},
    };

    std::vector<pintArgs_t> kernel_args_vec(sizeof(kernel_args) / sizeof(pintArgs_t));
    memcpy(kernel_args_vec.data(), kernel_args, sizeof(kernel_args));
    std::vector<pintArgs_t> kernel_args_sys_vec(sizeof(kernel_args_sys) / sizeof(pintArgs_t));
    memcpy(kernel_args_sys_vec.data(), kernel_args_sys, sizeof(kernel_args_sys));

    if (sys_args_num > 0) {
        kernel_args_vec.insert(kernel_args_vec.end(), kernel_args_sys_vec.begin(),
                               kernel_args_sys_vec.end());
    }
    CheckPintApi(
        pintLaunchProgram(pint_program, kernel_args_vec.data(), kernel_args_vec.size(), NULL, 0));
    CheckPintApi(pintMemcpy(h_total_events.data(), 0, d_total_events, 0, sizeof_total_events,
                            pintMemcpyDeviceToHost));
    CheckPintApi(pintMemcpy(h_event_realnum.data(), 0, d_event_realnum, 0, sizeof_event_realnum,
                            pintMemcpyDeviceToHost));
    printf("====================collect_all_events end============================.\n");
    printf("prof all event ended.\n");

    // CheckPintApi(
    //   pintMemcpy(
    //     h_total_events.data(), 0, d_total_events, 0, sizeof_total_events,
    //     pintMemcpyDeviceToHost)
    // );
    // CheckPintApi(
    //   pintMemcpy(
    //     h_event_realnum.data(), 0, d_event_realnum, 0, sizeof_event_realnum,
    //     pintMemcpyDeviceToHost)
    // );

    // printf("\nh_total_events:\n");

    // printf("\n\n");

    CheckPintApi(pintFree(d_total_events));
    CheckPintApi(pintFree(d_event_startpos));
    CheckPintApi(pintFree(d_event_realnum));
    if (sys_args_num > 0) {
        CheckPintApi(pintFree(d_sys_args_off));
        CheckPintApi(pintFree(d_sys_args_buf));
    }
}

int parse_profile_raw_data(const std::string &prof_bin, const std::string &temp_dir) {

    std::ifstream in_file(prof_bin, std::ios::in | std::ios::binary);
    int prof_bin_lenth = 0;
    if (in_file.is_open()) {
        in_file.seekg(0, std::ios::end);
        prof_bin_lenth = in_file.tellg();
        in_file.seekg(0, std::ios::beg);
    } else {
        std::exit(0);
    }

    char *buff = new char[MAX_WRITE_EVENT_CNT * sizeof(perf_event_s)];
    // in_file.read(buff, prof_bin_lenth);

    int event_size = prof_bin_lenth / sizeof(perf_event_s);
    int read_event_size = 0;
    perf_event_s *event = (perf_event_s *)buff;
    perf_event_s *event_tmp = event;

    std::string tmpfile_func_addr = temp_dir + "tmp_func_addr";
    std::string tmpfile_func_map = temp_dir + "tmp_func_map";
    std::string tmpfile_pint_elf = temp_dir + "pint.out";

    //// 1. Gets all function addresses and maps them to real function names.

    std::clock_t start = std::clock();

    std::set<unsigned int> func_addr_set;
    for (int readed_size = 0; readed_size < event_size; readed_size += MAX_WRITE_EVENT_CNT) {
        read_event_size = std::min(MAX_WRITE_EVENT_CNT, event_size - readed_size);
        in_file.read(buff, read_event_size * sizeof(perf_event_s));
        event_tmp = event;
        for (int i = 0; i < read_event_size; i++) {
            func_addr_set.insert(event_tmp->func_);
            event_tmp++;
        }
    }
    in_file.seekg(0, std::ios::beg);

    double dur_sec = (std::clock() - start + 0.0) / CLOCKS_PER_SEC;
    printf("elapsed of get function name: %.4lf sec\n", dur_sec);

    {
        std::fstream f(tmpfile_func_addr, std::fstream::out | std::fstream::trunc);
        if (!f.is_open()) {
            fprintf(stderr, "\nfile %s open failed.\n", tmpfile_func_addr.c_str());
            std::exit(-1);
        }
        for (auto addr : func_addr_set)
            f << "0x" << std::hex << addr << std::endl;
    }

    std::string cmd = std::string("riscv32-unknown-elf-addr2line @") + tmpfile_func_addr +
                      std::string(" -e ") + tmpfile_pint_elf +
                      std::string(" -f -C -p -a | awk -F\\ at\\  '{print $1}'") +
                      std::string(" > ") + tmpfile_func_map;
    int rc = system(cmd.c_str());
    // fprintf(stderr, "\nsystem command [%s]\n", cmd.c_str());
    if (rc != 0) {
        fprintf(stderr, "\nsystem command [%s] run failed.\n", cmd.c_str());
        std::exit(-2);
    }

    std::unordered_map<unsigned int, std::string> funcaddr_name_map;

    {
        std::fstream f(tmpfile_func_map, std::fstream::in);
        if (!f.is_open()) {
            fprintf(stderr, "\nfile %s open failed.\n", tmpfile_func_map.c_str());
            std::exit(-1);
        }
        std::string line_str;
        while (std::getline(f, line_str)) {
            // get [addr : name]
            int sep_pos = line_str.find_first_of(":");
            std::string addr_str = line_str.substr(0, sep_pos);
            std::string name_str = line_str.substr(sep_pos + 1, line_str.size());
            // trim(addr_str);
            // trim(name_str);
            unsigned int addr = std::stoul(addr_str, 0, 16);

            funcaddr_name_map.insert({addr, name_str});
        }
    }

    //// 2. rehandle and dump events
    start = std::clock();
    {
        std::string json_filepath = "./simu_timeline.json";
        std::fstream f(json_filepath, std::fstream::out | std::fstream::trunc);
        if (!f.is_open()) {
            fprintf(stderr, "\nfile %s open failed.\n", json_filepath.c_str());
            std::exit(-1);
        }
        f << "{\"displayTimeUnit\":\"ns\",\"traceEvents\":[" << std::endl;

        // MAX_WRITE_EVENT_CNT
        std::string cat = "category";
        constexpr int tmp_size = 2048;
        char tmp_str[tmp_size];
        for (int readed_size = 0; readed_size < event_size; readed_size += MAX_WRITE_EVENT_CNT) {
            read_event_size = std::min(MAX_WRITE_EVENT_CNT, event_size - readed_size);
            in_file.read(buff, read_event_size * sizeof(perf_event_s));
            event_tmp = event;
            for (size_t i = 0; i < read_event_size; i++) {
                int sz = 0;
                // if(measure_type != 0)
                {
                    sz = snprintf(
                        tmp_str, tmp_size,
                        "{\"pid\" : 0,\"ph\" : \"X\","
                        "\"tid\" : %d,"
                        "\"name\" : \"%s\","
                        "\"ts\" : %lf,"
                        "\"dur\" : %lf,"
                        "\"args\": { \"dcache_count\": %d, \"scache_count\": %d} }",
                        event_tmp->core_idx_, funcaddr_name_map[event_tmp->func_].c_str(),
                        ((double)event_tmp->ts_) / 1000.0, ((double)event_tmp->dur_) / 1000.0,
                        event_tmp->cache_counter_ & 0x0000ffff, event_tmp->cache_counter_ >> 16);
                }

                assert(sz <= (tmp_size - 1));
                assert(sz >= 0);
                event_tmp++;

                f << tmp_str;
                if (i < (event_size - 1)) {
                    f << ",";
                }
                f << std::endl;
            }
        }
        f << "]}" << std::endl;
    }
    dur_sec = (std::clock() - start + 0.0) / CLOCKS_PER_SEC;
    printf("elapsed of write json: %.4lf sec\n", dur_sec);
}

void collect_function_call_events(unsigned int total_events, pintProgram_t &pint_program,
                                  std::vector<unsigned int> &h_event_startpos, /*[out]*/
                                  std::vector<perf_event_s> &h_total_events,   /*[out]*/
                                  std::vector<unsigned int> &h_event_realnum,  /*[out]*/
                                  std::vector<unsigned int> &h_event_num, int ncore_num_to_perf,
                                  unsigned int parallel_begin, unsigned int parallel_end,
                                  unsigned int dcache_counter_config,
                                  unsigned int scache_counter_config, float &device_memory_cost) {
    int measure_type = _EnableEventPerNcore;
    unsigned int *d_total_events = nullptr;
    unsigned int sizeof_total_events = total_events * sizeof(perf_event_s);
    printf("sizeof_total_events = %d.\n", sizeof_total_events);

    unsigned int *d_event_startpos = nullptr;
    unsigned int sizeof_event_startpos = ncore_num_to_perf * sizeof(d_event_startpos[0]);

    unsigned int *d_event_realnum = nullptr;
    unsigned int sizeof_event_realnum = ncore_num_to_perf * sizeof(d_event_realnum[0]);

    unsigned int *d_sys_args_off = nullptr;
    unsigned int *d_sys_args_buf = nullptr;
    // h_event_startpos[0]=0;
    // prefix-sum
    assert(h_event_startpos[0] == 0);
    for (size_t i = 1; i < ncore_num_to_perf; i++) {
        h_event_startpos[i] = h_event_startpos[i - 1] + h_event_num[i - 1];
    }

    CheckPintApi(pintMalloc((void **)&d_total_events, sizeof_total_events));
    CheckPintApi(pintMemcpy(d_total_events, 0, h_total_events.data(), 0, sizeof_total_events,
                            pintMemcpyHostToDevice));
    CheckPintApi(pintMalloc((void **)&d_event_startpos, sizeof_event_startpos));
    CheckPintApi(pintMemcpy(d_event_startpos, 0, h_event_startpos.data(), 0, sizeof_event_startpos,
                            pintMemcpyHostToDevice));
    CheckPintApi(pintMalloc((void **)&d_event_realnum, sizeof_event_realnum));

    if (sys_args_num > 0) {
        argsMemcpyToDevice(&sys_args_off, &sys_args_buf, &d_sys_args_off, &d_sys_args_buf);
    }

    device_memory_cost =
        (sizeof_event_realnum + sizeof_event_startpos + sizeof_total_events) / (1024.0 * 1024.0);
    fprintf(stderr, "\ntotal device memory size= %f MB\n", device_memory_cost);

    pintArgs_t kernel_args[] = {
        {"gperf_measure_type", &measure_type},
        {"gperf_total_event", d_total_events},
        {"gperf_event_startpos_per_ncore", d_event_startpos},
        {"gperf_event_realnum_per_ncore", d_event_realnum},
        {"gperf_ncore_num_to_perf", &ncore_num_to_perf},
        {"lenof_gperf_total_event", &total_events},
        {"gperf_dcache_counter_config", &dcache_counter_config},
        {"gperf_scache_counter_config", &scache_counter_config},
        {"gperf_mcore_parallel_begin", &parallel_begin},
        {"gperf_mcore_parallel_end", &parallel_end},
    };

    pintArgs_t kernel_args_sys[] = {
        {"pint_sys_args_num", &sys_args_num},
        {"pint_sys_args_off", d_sys_args_off},
        {"pint_sys_args_buff", d_sys_args_buf},
    };

    std::vector<pintArgs_t> kernel_args_vec(sizeof(kernel_args) / sizeof(pintArgs_t));
    memcpy(kernel_args_vec.data(), kernel_args, sizeof(kernel_args));
    std::vector<pintArgs_t> kernel_args_sys_vec(sizeof(kernel_args_sys) / sizeof(pintArgs_t));
    memcpy(kernel_args_sys_vec.data(), kernel_args_sys, sizeof(kernel_args_sys));

    if (sys_args_num > 0) {
        kernel_args_vec.insert(kernel_args_vec.end(), kernel_args_sys_vec.begin(),
                               kernel_args_sys_vec.end());
    }
    CheckPintApi(
        pintLaunchProgram(pint_program, kernel_args_vec.data(), kernel_args_vec.size(), NULL, 0));
    CheckPintApi(pintMemcpy(h_total_events.data(), 0, d_total_events, 0, sizeof_total_events,
                            pintMemcpyDeviceToHost));
    CheckPintApi(pintMemcpy(h_event_realnum.data(), 0, d_event_realnum, 0, sizeof_event_realnum,
                            pintMemcpyDeviceToHost));

    // printf("\nh_total_events:\n");

    // printf("\n\n");

    CheckPintApi(pintFree(d_total_events));
    CheckPintApi(pintFree(d_event_startpos));
    CheckPintApi(pintFree(d_event_realnum));
    if (sys_args_num > 0) {
        CheckPintApi(pintFree(d_sys_args_off));
        CheckPintApi(pintFree(d_sys_args_buf));
    }
}

int prof_all(std::string &prof_bin, int ncore_num_to_perf, /*[in]*/
             std::vector<unsigned int> &h_event_startpos,  /*[out]*/
             std::vector<perf_event_s> &h_total_events,    /*[out]*/
             std::vector<unsigned int> &h_event_realnum,   /*[out]*/
             const std::string &src_root, const std::string &temp_dir,
             const std::string &thread_file_path, unsigned long long prof_mask,
             int is_build_from_binary, unsigned int parallel_begin, unsigned int parallel_end,
             unsigned int dcache_counter_config, unsigned int scache_counter_config) {
    h_event_startpos.clear();
    h_total_events.clear();
    h_event_realnum.clear();

    if (!is_build_from_binary) {
        printf("prof_mask = %x.\n", (unsigned int)prof_mask);
        std::string marco_define = "/common/ -DPROF_ON -DPROF_MASK=" + std::to_string(prof_mask);

        create_pint_bin(marco_define, src_root, thread_file_path, temp_dir);
    }

    const std::string bin_path = temp_dir + "pint.pin";
    const char *file_path[] = {bin_path.c_str()};

    pintProgram_t pint_program;
    CheckPintApi(pintCreateProgram(&pint_program, file_path, sizeof(file_path) / sizeof(char *), "",
                                   pintBinaryFile));

    collect_all_events(pint_program, h_event_startpos, h_total_events, h_event_realnum,
                       ncore_num_to_perf, parallel_begin, parallel_end, dcache_counter_config,
                       scache_counter_config);

    CheckPintApi(pintFreeProgram(pint_program));

    parse_profile_raw_data(prof_bin, temp_dir);
}

int prof_ncore_occupancy(std::string &prof_bin, int ncore_num_to_perf, /*[in]*/
                         std::vector<unsigned int> &h_event_startpos,  /*[out]*/
                         std::vector<perf_event_s> &h_total_events,    /*[out]*/
                         std::vector<unsigned int> &h_event_realnum,   /*[out]*/
                         const std::string &src_root, const std::string &temp_dir,
                         const std::string &thread_file_path, unsigned long long prof_mask,
                         int is_build_from_binary, unsigned int parallel_begin,
                         unsigned int parallel_end, unsigned int dcache_counter_config,
                         unsigned int scache_counter_config) {
    h_event_startpos.clear();
    h_total_events.clear();
    h_event_realnum.clear();

    if (!is_build_from_binary) {
        printf("prof_mask = %x.\n", (unsigned int)prof_mask);
        std::string marco_define =
            "/common/ -DPROF_NCORE_OCCUPANCY -DCOUNT_PARALLEL_RATIO_ENABLE -DPROF_ON -DPROF_MASK=" +
            std::to_string(prof_mask);

        create_pint_bin(marco_define, src_root, thread_file_path, temp_dir);
    }

    const std::string bin_path = temp_dir + "pint.pin";
    const char *file_path[] = {bin_path.c_str()};

    pintProgram_t pint_program;
    CheckPintApi(pintCreateProgram(&pint_program, file_path, sizeof(file_path) / sizeof(char *), "",
                                   pintBinaryFile));

    int measure_type = _EnableNcoreOccupancy;
    // int measure_type = _EnableEventPerNcore;

    unsigned int *d_total_events = nullptr;
    ncore_num_to_perf = 17;
    unsigned int events_max_per_core = 200000;
    unsigned int total_events = ncore_num_to_perf * events_max_per_core;
    unsigned int sizeof_total_events = total_events * sizeof(perf_event_s); // 136MB;

    unsigned int *d_event_startpos = nullptr;
    unsigned int sizeof_event_startpos = ncore_num_to_perf * sizeof(d_event_startpos[0]);

    unsigned int *d_event_realnum = nullptr;
    unsigned int sizeof_event_realnum = ncore_num_to_perf * sizeof(d_event_realnum[0]);

    unsigned int *d_sys_args_off = nullptr;
    unsigned int *d_sys_args_buf = nullptr;
    // h_event_startpos[0]=0;
    // prefix-sum
    h_event_startpos.resize(ncore_num_to_perf, 0);
    h_total_events.resize(total_events, perf_event_s{0, 0, 0, 0, 0});
    h_event_realnum.resize(ncore_num_to_perf, 0);

    assert(h_event_startpos[0] == 0);
    for (size_t i = 1; i < ncore_num_to_perf; i++) {
        h_event_startpos[i] = h_event_startpos[i - 1] + events_max_per_core;
    }

    CheckPintApi(pintMalloc((void **)&d_total_events, sizeof_total_events));
    CheckPintApi(pintMemcpy(d_total_events, 0, h_total_events.data(), 0, sizeof_total_events,
                            pintMemcpyHostToDevice));
    CheckPintApi(pintMalloc((void **)&d_event_startpos, sizeof_event_startpos));
    CheckPintApi(pintMemcpy(d_event_startpos, 0, h_event_startpos.data(), 0, sizeof_event_startpos,
                            pintMemcpyHostToDevice));
    CheckPintApi(pintMalloc((void **)&d_event_realnum, sizeof_event_realnum));

    if (sys_args_num > 0) {
        argsMemcpyToDevice(&sys_args_off, &sys_args_buf, &d_sys_args_off, &d_sys_args_buf);
    }

    pintArgs_t kernel_args[] = {
        {"gperf_measure_type", &measure_type},
        {"gperf_total_event", d_total_events},
        {"gperf_event_startpos_per_ncore", d_event_startpos},
        {"gperf_event_realnum_per_ncore", d_event_realnum},
        {"gperf_ncore_num_to_perf", &ncore_num_to_perf},
        {"gperf_dcache_counter_config", &dcache_counter_config},
        {"gperf_scache_counter_config", &scache_counter_config},
        {"gperf_mcore_parallel_begin", &parallel_begin},
        {"gperf_mcore_parallel_end", &parallel_end},
    };

    pintArgs_t kernel_args_sys[] = {
        {"pint_sys_args_num", &sys_args_num},
        {"pint_sys_args_off", d_sys_args_off},
        {"pint_sys_args_buff", d_sys_args_buf},
    };

    std::vector<pintArgs_t> kernel_args_vec(sizeof(kernel_args) / sizeof(pintArgs_t));
    memcpy(kernel_args_vec.data(), kernel_args, sizeof(kernel_args));
    std::vector<pintArgs_t> kernel_args_sys_vec(sizeof(kernel_args_sys) / sizeof(pintArgs_t));
    memcpy(kernel_args_sys_vec.data(), kernel_args_sys, sizeof(kernel_args_sys));

    if (sys_args_num > 0) {
        kernel_args_vec.insert(kernel_args_vec.end(), kernel_args_sys_vec.begin(),
                               kernel_args_sys_vec.end());
    }
    CheckPintApi(
        pintLaunchProgram(pint_program, kernel_args_vec.data(), kernel_args_vec.size(), NULL, 0));
    CheckPintApi(pintMemcpy(h_total_events.data(), 0, d_total_events, 0, sizeof_total_events,
                            pintMemcpyDeviceToHost));
    CheckPintApi(pintMemcpy(h_event_realnum.data(), 0, d_event_realnum, 0, sizeof_event_realnum,
                            pintMemcpyDeviceToHost));

    CheckPintApi(pintFree(d_total_events));
    CheckPintApi(pintFree(d_event_startpos));
    CheckPintApi(pintFree(d_event_realnum));
    if (sys_args_num > 0) {
        CheckPintApi(pintFree(d_sys_args_off));
        CheckPintApi(pintFree(d_sys_args_buf));
    }
    printf("====================prof_ncore_occupancy end============================.\n");
    CheckPintApi(pintFreeProgram(pint_program));
    return 0;
}

int get_prof_event_data(int ncore_num_to_perf,                       /*[in]*/
                        std::vector<unsigned int> &h_event_startpos, /*[out]*/
                        std::vector<perf_event_s> &h_total_events,   /*[out]*/
                        std::vector<unsigned int> &h_event_realnum,  /*[out]*/
                        const std::string &src_root, const std::string &temp_dir,
                        const std::string &thread_file_path, unsigned long long prof_mask,
                        int is_build_from_binary, unsigned int parallel_begin,
                        unsigned int parallel_end, unsigned int dcache_counter_config,
                        unsigned int scache_counter_config, float &device_memory_cost) {
    h_event_startpos.clear();
    h_total_events.clear();
    h_event_realnum.clear();

    if (!is_build_from_binary) {
        printf("prof_mask = %x.\n", (unsigned int)prof_mask);
        std::string marco_define = "/common/ -DPROF_ON -DPROF_MASK=" + std::to_string(prof_mask);

        create_pint_bin(marco_define, src_root, thread_file_path, temp_dir);
    }

    std::string bin_path = temp_dir + "pint.pin";
    const char *file_path[] = {bin_path.c_str()};

    pintProgram_t pint_program;
    CheckPintApi(pintCreateProgram(&pint_program, file_path, sizeof(file_path) / sizeof(char *), "",
                                   pintBinaryFile));

    auto h_calltimes = std::vector<unsigned int>(ncore_num_to_perf, 0);

    calc_calltimes_per_ncore(h_calltimes, pint_program, ncore_num_to_perf, parallel_begin,
                             parallel_end, dcache_counter_config, scache_counter_config);

    std::vector<unsigned int> h_event_num = h_calltimes;
    for (auto &var : h_event_num)
        var = 2 * var + 50; // 2n+50

    unsigned int total_events = 0;
    for (auto var : h_event_num)
        total_events += var;

    fprintf(stderr, "\n\ntotal_events = %u\n\n", total_events);

    h_event_startpos.resize(ncore_num_to_perf, 0);
    h_total_events.resize(total_events, perf_event_s{0, 0, 0, 0, 0});
    h_event_realnum.resize(ncore_num_to_perf, 0);

    //// calc calltimes per ncore

    ////
    unsigned long long mask_2n = 1 << PROFMASK0_MCORE_SUMMARY;
    if (0 != (mask_2n & prof_mask)) {
        fprintf(stdout, "\n\nrun one time.\n\n");
        std::exit(0);
    }

    //// collect function call events
    collect_function_call_events(total_events, pint_program, h_event_startpos, h_total_events,
                                 h_event_realnum, h_event_num, ncore_num_to_perf, parallel_begin,
                                 parallel_end, dcache_counter_config, scache_counter_config,
                                 device_memory_cost);

    CheckPintApi(pintFreeProgram(pint_program));
}

void dump_csv_file(std::unordered_map<std::string, summary_info> &funcname_time_map) {
    std::string csv_filepath = "./simu_summary.csv";
    std::fstream f(csv_filepath, std::fstream::out | std::fstream::trunc);
    if (!f.is_open()) {
        fprintf(stderr, "\nfile %s open failed.\n", csv_filepath.c_str());
        std::exit(-1);
    }

    f << "func-name; total-time(cycle); calltimes; avg-time(cycle); max_time(cycle); min_time(cycle);\
  total_dcache_count; avg_dcache_count; max_dcache_count; min_dcache_count;\
  total_scache_count; avg_scache_count; max_scache_count; min_scache_count"
      << std::endl;

    for (const auto &kv : funcname_time_map) {
        auto funcname = kv.first;
        auto total_dur_time = kv.second.total_dur_time;
        auto calltimes = kv.second.call_times;
        auto total_dcache_count = kv.second.total_dcache_count;
        auto total_scache_count = kv.second.total_scache_count;
        auto avg_time = (total_dur_time + 0.0) / calltimes;
        auto max_dur_time = kv.second.max_dur_time;
        auto min_dur_time = kv.second.min_dur_time;
        auto avg_dcache_count = (total_dcache_count + 0.0) / calltimes;
        auto max_dcache_count = kv.second.max_dcache_count;
        auto min_dcache_count = kv.second.min_dcache_count;
        auto avg_scache_count = (total_scache_count + 0.0) / calltimes;
        auto max_scache_count = kv.second.max_scache_count;
        auto min_scache_count = kv.second.min_scache_count;
        f << funcname << ";" << total_dur_time << ";" << calltimes << ";" << avg_time << ";"
          << max_dur_time << ";" << min_dur_time << ";" << total_dcache_count << ";"
          << avg_dcache_count << ";" << max_dcache_count << ";" << min_dcache_count << ";"
          << total_scache_count << ";" << avg_scache_count << ";" << max_scache_count << ";"
          << min_scache_count << ";" << std::endl;
    }
}

int dump_timeline_json(const std::string &json_filepath,                       /*[in]*/
                       const std::string &csv_filepath, int ncore_num_to_perf, /*[in]*/
                       const std::vector<unsigned int> &h_event_startpos,      /*[in]*/
                       const std::vector<perf_event_s> &h_total_events,        /*[in]*/
                       const std::vector<unsigned int> &h_event_realnum,       /*[in]*/
                       const std::string &temp_dir, const unsigned int measure_type) {
    std::string tmpfile_func_addr = temp_dir + "tmp_func_addr";
    std::string tmpfile_func_map = temp_dir + "tmp_func_map";
    std::string tmpfile_pint_elf = temp_dir + "pint.out";

    //// 0. rehandle h_total_events
    std::vector<perf_event_new_s> h_total_events_new;
    for (size_t i = 0; i < ncore_num_to_perf; i++) {
        int core_id = i * 16;
        unsigned int startpos = h_event_startpos[i];
        unsigned int realnum = h_event_realnum[i];

        auto p_event = h_total_events.begin() + startpos;
        auto p_event_end = p_event + realnum;

        assert(p_event <= p_event_end);

        // const char* str_format = "{\"cat\" : \"pint_thread\",\"pid\" : 0,\"tid\" : %d,\"name\" :
        // \"%u\",\"ts\" : %l,\"dur\" : %l, \"ph\" : \"X\",}";
        for (; p_event != p_event_end; p_event++) {
            perf_event_new_s evt_new;
            evt_new.ts_ = p_event->ts_;
            evt_new.dur_ = p_event->dur_;
            evt_new.func_ = p_event->func_;
            evt_new.cache_counter_ = p_event->cache_counter_;
            evt_new.tid_ = p_event->core_idx_;
            // evt_new.deq_idx_ = p_event->deq_idx_;

            h_total_events_new.push_back(evt_new);
        }
    }
    assert(h_total_events_new.size() < h_total_events.size());

    if (h_total_events_new.size() == 0) {
        fprintf(stderr, "\nwarning: no profile event generated.\n");
        // no event measured! return.
        return 0;
    }

    //// 1. Gets all function addresses and maps them to real function names.
    std::set<unsigned int> func_addr_set;
    for (auto evt : h_total_events_new) {
        func_addr_set.insert(evt.func_);
    }

    {
        std::fstream f(tmpfile_func_addr, std::fstream::out | std::fstream::trunc);
        if (!f.is_open()) {
            fprintf(stderr, "\nfile %s open failed.\n", tmpfile_func_addr.c_str());
            std::exit(-1);
        }
        for (auto addr : func_addr_set)
            f << "0x" << std::hex << addr << std::endl;
    }

    std::string cmd = std::string("riscv32-unknown-elf-addr2line @") + tmpfile_func_addr +
                      std::string(" -e ") + tmpfile_pint_elf +
                      std::string(" -f -C -p -a | awk -F\\ at\\  '{print $1}'") +
                      std::string(" > ") + tmpfile_func_map;
    int rc = system(cmd.c_str());
    // fprintf(stderr, "\nsystem command [%s]\n", cmd.c_str());
    if (rc != 0) {
        fprintf(stderr, "\nsystem command [%s] run failed.\n", cmd.c_str());
        std::exit(-2);
    }

    std::unordered_map<unsigned int, std::string> funcaddr_name_map;

    {
        std::fstream f(tmpfile_func_map, std::fstream::in);
        if (!f.is_open()) {
            fprintf(stderr, "\nfile %s open failed.\n", tmpfile_func_map.c_str());
            std::exit(-1);
        }
        std::string line_str;
        while (std::getline(f, line_str)) {
            // get [addr : name]
            int sep_pos = line_str.find_first_of(":");
            std::string addr_str = line_str.substr(0, sep_pos);
            std::string name_str = line_str.substr(sep_pos + 1, line_str.size());
            // trim(addr_str);
            // trim(name_str);
            unsigned int addr = std::stoul(addr_str, 0, 16);

            funcaddr_name_map.insert({addr, name_str});
        }
    }

    //// 2. rehandle and dump events
    {
        std::fstream f(json_filepath, std::fstream::out | std::fstream::trunc);
        if (!f.is_open()) {
            fprintf(stderr, "\nfile %s open failed.\n", json_filepath.c_str());
            std::exit(-1);
        }
        f << "{\"displayTimeUnit\":\"ns\",\"traceEvents\":[" << std::endl;

        // std::string csv_filepath = "./simu.csv";
        // std::fstream f_csv(csv_filepath, std::fstream::out | std::fstream::trunc);
        // if(!f.is_open()) {
        //  fprintf(stderr, "\nfile %s open failed.\n", csv_filepath.c_str());
        //  std::exit(-1);
        //}
        // f_csv <<"core_id,name,ts(ms),dur(ms)"<<std::endl;

        std::string cat = "category";
        constexpr int tmp_size = 2048;
        char tmp_str[tmp_size];
        for (size_t i = 0; i < h_total_events_new.size(); i++) {
            const perf_event_new_s &evt_new = h_total_events_new[i];
            int sz = 0;
            // if(measure_type != 0)
            {
                sz = snprintf(
                    tmp_str, tmp_size,
                    "{\"pid\" : 0,\"ph\" : \"X\","
                    "\"tid\" : %d,"
                    "\"name\" : \"%s\","
                    "\"ts\" : %lf,"
                    "\"dur\" : %lf,"
                    "\"args\": { \"dcache_count\": %d, \"scache_count\": %d, \"deq_idx\": %d} }",
                    evt_new.tid_, funcaddr_name_map[evt_new.func_].c_str(),
                    ((double)evt_new.ts_) / 1000.0, ((double)evt_new.dur_) / 1000.0,
                    evt_new.cache_counter_ & 0x0000ffff, evt_new.cache_counter_ >> 16,
                    evt_new.deq_idx_);
            }
            // else{
            //   sz = snprintf(tmp_str, tmp_size,
            //     "{\"pid\" : 0,\"ph\" : \"X\","
            //     "\"tid\" : %d,"
            //     "\"name\" : \"%s\","
            //     "\"ts\" : %lf,"
            //     "\"dur\" : %lf,"
            //     "\"args\": { \"parallel_cnt\": %d, \"deq_idx\": %d} }",
            //       evt_new.tid_,
            //       funcaddr_name_map[evt_new.func_].c_str(),
            //       ((double)evt_new.ts_)/1000.0,
            //       ((double)evt_new.dur_)/1000.0,
            //       evt_new.cache_counter_,
            //       evt_new.deq_idx_);
            // }
            assert(sz <= (tmp_size - 1));
            assert(sz >= 0);

            f << tmp_str;
            if (i < (h_total_events_new.size() - 1)) {
                f << ",";
            }
            f << std::endl;
        }

        f << "]}" << std::endl;
    }
    std::unordered_map<std::string, summary_info> funcname_time_map;
    make_summary_map(h_total_events_new, funcname_time_map, funcaddr_name_map);
    dump_csv_file(funcname_time_map);
}

int get_summary_data(int ncore_num_to_perf,                             /*[in]*/
                     const std::vector<unsigned int> &h_event_startpos, /*[in]*/
                     const std::vector<perf_event_s> &h_total_events,   /*[in]*/
                     const std::vector<unsigned int> &h_event_realnum,  /*[in]*/
                     const std::string &temp_dir,
                     std::unordered_map<unsigned int, std::string> &funcaddr_name_map,
                     std::unordered_map<std::string, summary_info> &funcname_time_map,
                     int is_build_from_binary) {
    std::string tmpfile_func_addr = temp_dir + "tmp_func_addr";
    std::string tmpfile_func_map = temp_dir + "tmp_func_map";
    std::string tmpfile_pint_elf = temp_dir + "pint.out";

    //// 0. rehandle h_total_events
    std::vector<perf_event_new_s> h_total_events_new;
    for (size_t i = 0; i < ncore_num_to_perf; i++) {
        int core_id = i * 16;
        unsigned int startpos = h_event_startpos[i];
        unsigned int realnum = h_event_realnum[i];

        auto p_event = h_total_events.begin() + startpos;
        auto p_event_end = p_event + realnum;

        assert(p_event <= p_event_end);

        // const char* str_format = "{\"cat\" : \"pint_thread\",\"pid\" : 0,\"tid\" : %d,\"name\" :
        // \"%u\",\"ts\" : %l,\"dur\" : %l, \"ph\" : \"X\",}";
        for (; p_event != p_event_end; p_event++) {
            perf_event_new_s evt_new;
            evt_new.ts_ = p_event->ts_;
            evt_new.dur_ = p_event->dur_;
            evt_new.func_ = p_event->func_;
            evt_new.cache_counter_ = p_event->cache_counter_;
            evt_new.tid_ = core_id;

            h_total_events_new.push_back(evt_new);
        }
    }
    assert(h_total_events_new.size() < h_total_events.size());

    if (h_total_events_new.size() == 0) {
        fprintf(stderr, "\nwarning: no profile event generated.\n");
        // no event measured! return.
        return 0;
    }

    //// 1. Gets all function addresses and maps them to real function names.
    if (!is_build_from_binary) {
        std::set<unsigned int> func_addr_set;
        for (auto evt : h_total_events_new) {
            func_addr_set.insert(evt.func_);
        }

        {
            std::fstream f(tmpfile_func_addr, std::fstream::out | std::fstream::trunc);
            if (!f.is_open()) {
                fprintf(stderr, "\nfile %s open failed.\n", tmpfile_func_addr.c_str());
                std::exit(-1);
            }
            for (auto addr : func_addr_set)
                f << "0x" << std::hex << addr << std::endl;
        }

        std::string cmd = std::string("riscv32-unknown-elf-addr2line @") + tmpfile_func_addr +
                          std::string(" -e ") + tmpfile_pint_elf +
                          std::string(" -f -C -p -a | awk -F\\ at\\  '{print $1}'") +
                          std::string(" > ") + tmpfile_func_map;
        int rc = system(cmd.c_str());
        // fprintf(stderr, "\nsystem command [%s]\n", cmd.c_str());
        if (rc != 0) {
            fprintf(stderr, "\nsystem command [%s] run failed.\n", cmd.c_str());
            std::exit(-2);
        }

        {
            std::fstream f(tmpfile_func_map, std::fstream::in);
            if (!f.is_open()) {
                fprintf(stderr, "\nfile %s open failed.\n", tmpfile_func_map.c_str());
                std::exit(-1);
            }
            std::string line_str;
            while (std::getline(f, line_str)) {
                // get [addr : name]
                int sep_pos = line_str.find_first_of(":");
                std::string addr_str = line_str.substr(0, sep_pos);
                std::string name_str = line_str.substr(sep_pos + 1, line_str.size());
                // trim(addr_str);
                // trim(name_str);
                unsigned int addr = std::stoul(addr_str, 0, 16);

                funcaddr_name_map.insert({addr, name_str});
            }
        }
    }

    ////3. make summary file.
    make_summary_map(h_total_events_new, funcname_time_map, funcaddr_name_map);
}

void get_summary_array_from_device(std::vector<perf_summary_s> &summary_array,
                                   std::string &temp_dir, int ncore_num_to_perf,
                                   unsigned int total_thread_num) {
    std::string bin_path = temp_dir + "pint.pin";
    const char *file_path[] = {bin_path.c_str()};

    pintProgram_t pint_program;
    CheckPintApi(pintCreateProgram(&pint_program, file_path, sizeof(file_path) / sizeof(char *), "",
                                   pintBinaryFile));

    int sizeof_summary_array = sizeof(perf_summary_s) * summary_array.size();
    std::cout << "sizeof_summary_array: " << sizeof_summary_array << std::endl;
    std::cout << "summary_array.size(): " << summary_array.size() << std::endl;

    unsigned int *d_summary_array = nullptr;
    unsigned int *d_sys_args_off = nullptr;
    unsigned int *d_sys_args_buf = nullptr;

    CheckPintApi(pintMalloc((void **)&d_summary_array, sizeof_summary_array));
    CheckPintApi(pintMemcpy(d_summary_array, 0, summary_array.data(), 0, sizeof_summary_array,
                            pintMemcpyHostToDevice));

    if (sys_args_num > 0) {
        argsMemcpyToDevice(&sys_args_off, &sys_args_buf, &d_sys_args_off, &d_sys_args_buf);
    }
    int measure_type = _EnableNcoreOccupancy;

    pintArgs_t kernel_args[] = {
        {"gperf_measure_type", &measure_type},
        {"gperf_ncore_num_to_perf", &ncore_num_to_perf},
        {"gperf_total_thread_num", &total_thread_num},
        {"device_func_summary_array", d_summary_array},
    };

    pintArgs_t kernel_args_sys[] = {
        {"pint_sys_args_num", &sys_args_num},
        {"pint_sys_args_off", d_sys_args_off},
        {"pint_sys_args_buff", d_sys_args_buf},
    };

    std::vector<pintArgs_t> kernel_args_vec(sizeof(kernel_args) / sizeof(pintArgs_t));
    memcpy(kernel_args_vec.data(), kernel_args, sizeof(kernel_args));
    std::vector<pintArgs_t> kernel_args_sys_vec(sizeof(kernel_args_sys) / sizeof(pintArgs_t));
    memcpy(kernel_args_sys_vec.data(), kernel_args_sys, sizeof(kernel_args_sys));

    if (sys_args_num > 0) {
        kernel_args_vec.insert(kernel_args_vec.end(), kernel_args_sys_vec.begin(),
                               kernel_args_sys_vec.end());
    }

    CheckPintApi(
        pintLaunchProgram(pint_program, kernel_args_vec.data(), kernel_args_vec.size(), NULL, 0));

    CheckPintApi(pintMemcpy(summary_array.data(), 0, d_summary_array, 0, sizeof_summary_array,
                            pintMemcpyDeviceToHost));

    CheckPintApi(pintFree(d_summary_array));
    if (sys_args_num > 0) {
        CheckPintApi(pintFree(d_sys_args_off));
        CheckPintApi(pintFree(d_sys_args_buf));
    }
}

void dump_summary_csv_file(std::vector<perf_summary_s> &summary_array,
                           perf_thread_lpm_num_s &pint_thread_lpm_num,
                           unsigned int total_thread_num, const std::string &src_root) {
    int lpm_offset = pint_thread_lpm_num.event_num - 300;
    int lpm_total_num = pint_thread_lpm_num.lpm_num + pint_thread_lpm_num.thread_lpm_num +
                        pint_thread_lpm_num.monitor_lpm_num + pint_thread_lpm_num.force_lpm_num;
    int thread_begin_index = lpm_total_num + 300;

    // std::cout << "total_thread_num: " << total_thread_num << std::endl;
    std::vector<perf_summary_s> summary_result(total_thread_num, perf_summary_s{0, 0});

    for (size_t i = 0; i < total_thread_num; ++i) {
        unsigned long long total_cycles = 0;
        unsigned long long call_times = 0;
        for (size_t j = 0; j < 16; ++j) {
            total_cycles += summary_array[j * total_thread_num + i].total_cycles;
            call_times += summary_array[j * total_thread_num + i].call_times;
        }
        summary_result[i].total_cycles = total_cycles;
        summary_result[i].call_times = call_times;
        // std::cout << "summary_result[i].total_cycles: " << summary_result[i].total_cycles <<
        // std::endl;
        // std::cout << "summary_result[i].call_times: " << summary_result[i].call_times <<
        // std::endl;
    }

    std::string funcname2index_file_path = src_root + "prof/summary_funcname2index";
    // std::cout << "funcname2index_file_path: " << funcname2index_file_path << std::endl;
    std::unordered_map<unsigned int, std::string> funcaddr_name_map;

    {
        std::fstream f(funcname2index_file_path, std::fstream::in);
        if (!f.is_open()) {
            fprintf(stderr, "\nfile %s open failed.\n", funcname2index_file_path.c_str());
            std::exit(-1);
        }
        std::string line_str;
        while (std::getline(f, line_str)) {
            // get [addr : name]
            int sep_pos = line_str.find_first_of(":");
            std::string name_str = line_str.substr(0, sep_pos);
            std::string index_s = line_str.substr(sep_pos + 1, line_str.size());
            unsigned int index = std::stoul(index_s, 0, 0);
            funcaddr_name_map.insert({index, name_str});
        }
    }

    std::string csv_filepath = "./simu_summary_total.csv";
    std::fstream f(csv_filepath, std::fstream::out | std::fstream::trunc);
    if (!f.is_open()) {
        fprintf(stderr, "\nfile %s open failed.\n", csv_filepath.c_str());
        std::exit(-1);
    }

    unsigned long long thread_lpm_total_cycles = 0;
    unsigned long long small_func_total_cycles = 0;
    for (size_t i = 300; i < summary_result.size(); i++) {
        thread_lpm_total_cycles += summary_result[i].total_cycles;
    }
    for (size_t i = 0; i <= 185; i++) {
        small_func_total_cycles += summary_result[i].total_cycles;
    }

    f << "func-name; total-time(cycle); calltimes; avg-time(cycle); %cycles; prof_index;"
      << std::endl;

    for (size_t i = 0; i < summary_result.size(); i++) {
        if (i < 300) {
            if (summary_result[i].total_cycles > 0 && i < 186) {
                f << funcaddr_name_map[i] << ";" << summary_result[i].total_cycles << ";"
                  << summary_result[i].call_times << ";"
                  << (double)summary_result[i].total_cycles / summary_result[i].call_times << ";"
                  << (double)summary_result[i].total_cycles / small_func_total_cycles * 100 << ";"
                  << i << ";" << std::endl;
            } else if (summary_result[i].total_cycles > 0 && i >= 186) {
                f << funcaddr_name_map[i] << ";" << summary_result[i].total_cycles << ";"
                  << summary_result[i].call_times << ";"
                  << (double)summary_result[i].total_cycles / summary_result[i].call_times << ";"
                  << "-"
                  << ";" << i << ";" << std::endl;
            }
        } else if (i >= 300 && i < 300 + lpm_total_num) {
            if (summary_result[i].total_cycles > 0) {
                f << "  pint_lpm_" + std::to_string(i + lpm_offset) << ";"
                  << summary_result[i].total_cycles << ";" << summary_result[i].call_times << ";"
                  << (double)summary_result[i].total_cycles / summary_result[i].call_times << ";"
                  << (double)summary_result[i].total_cycles / thread_lpm_total_cycles * 100 << ";"
                  << i << ";" << std::endl;
            }
        } else {
            if (summary_result[i].total_cycles > 0) {
                f << "  pint_thread_" + std::to_string(i - thread_begin_index) << ";"
                  << summary_result[i].total_cycles << ";" << summary_result[i].call_times << ";"
                  << (double)summary_result[i].total_cycles / summary_result[i].call_times << ";"
                  << (double)summary_result[i].total_cycles / thread_lpm_total_cycles * 100 << ";"
                  << i << ";" << std::endl;
            }
        }
    }
    f.close();
}

void get_pint_thread_lpm_num(perf_thread_lpm_num_s &pint_thread_lpm_num,
                             std::string &pint_thread_stub_file_path) {
    std::fstream f(pint_thread_stub_file_path, std::fstream::in);
    if (!f.is_open()) {
        fprintf(stderr, "\nfile %s open failed.\n", pint_thread_stub_file_path.c_str());
        std::exit(-1);
    }
    std::string file_context;
    std::string line_str;
    std::unordered_map<std::string, unsigned int> thread_num_map;

    while (std::getline(f, line_str)) {
        file_context.append(line_str);
    }
    std::vector<std::string> pint_thread_num_names_vec = {
        "gperf_lpm_num",       "gperf_monitor_lpm_num", "gperf_thread_lpm_num",
        "gperf_force_lpm_num", "gperf_event_num",       "gperf_thread_num"};

    for (auto key_word : pint_thread_num_names_vec) {
        std::size_t key_pose = file_context.find(key_word);
        if (key_pose != std::string::npos) {
            std::string name_str = key_word;
            std::size_t start_pos = file_context.find("=", key_pose);
            std::size_t end_pos = file_context.find(";", key_pose);
            if (start_pos != std::string::npos && end_pos != std::string::npos) {
                std::size_t len = end_pos - start_pos - 1;
                std::string num_str = file_context.substr(start_pos + 1, len);
                unsigned int num = 0;
                try {
                    num = std::stoul(num_str, 0, 0);
                } catch (std::invalid_argument &) {
                    std::cout << "stoul Invalid_argument" << std::endl;
                } catch (std::out_of_range &) {
                    std::cout << "stoul Out_of_range" << std::endl;
                } catch (...) {
                    std::cout << "stoul something else exception." << std::endl;
                }
                thread_num_map.insert({key_word, num});
            }
        }
    }

    for (auto key_word : pint_thread_num_names_vec) {
        if (thread_num_map.find(key_word) == thread_num_map.end()) {
            fprintf(stderr, "\npint_thread_num not found in %s\n",
                    pint_thread_stub_file_path.c_str());
            fprintf(stderr, "Please type the following cmd: export PINT_MODE_PERF=ON\nThen use "
                            "iverilog to generate the code again.\n");
            std::exit(-1);
        }
    }
    pint_thread_lpm_num.lpm_num = thread_num_map["gperf_lpm_num"];
    pint_thread_lpm_num.monitor_lpm_num = thread_num_map["gperf_monitor_lpm_num"];
    pint_thread_lpm_num.thread_lpm_num = thread_num_map["gperf_thread_lpm_num"];
    pint_thread_lpm_num.force_lpm_num = thread_num_map["gperf_force_lpm_num"];
    pint_thread_lpm_num.event_num = thread_num_map["gperf_event_num"];
    pint_thread_lpm_num.thread_num = thread_num_map["gperf_thread_num"];

    std::cout << "lpm_num: " << pint_thread_lpm_num.lpm_num << std::endl;
    std::cout << "thread_lpm_num: " << pint_thread_lpm_num.thread_lpm_num << std::endl;
    std::cout << "event_num: " << pint_thread_lpm_num.event_num << std::endl;
    std::cout << "thread_num: " << pint_thread_lpm_num.thread_num << std::endl;
    std::cout << "monitor_lpm_num: " << pint_thread_lpm_num.monitor_lpm_num << std::endl;
    std::cout << "force_lpm_num: " << pint_thread_lpm_num.force_lpm_num << std::endl;
}

/*
usage:
arg1: ncore num to perf(default=16). should <=16 && >=1
arg2: src-simu root(default=../)
arg3: thread.c file path(default="./pint_thread_stub.c")
arg4: profile mask. 32bit unsigned int
  prof_mask bit description:
    mask[12:0]:
        execute_active/NB:      ->  bit0
        thread/lpm/moveevent:   ->  bit1
        of_function: -> bit2
        pint_move_event_thread_list: -> bit3
        pint_[enqueue|dequeue]_thread: -> bit4
        pint_wait_on_event: -> bit5
        pint_enqueue_flag_set/clear: ->bit6
        pint_add_list_to_active/inactive: ->bit7
        pint_enqueue_nb_thread/dequeue: ->bit8
        pint_process_event_list: ->bit9
        pint_nb_assign/array: ->bit10
        pint_nb_array_process: ->bit11
        edge/edge_int/edge_int_value: ->bit12
    mask[31:27]
      bit27: enable ncore dcache counter-measure
      bit28: enable ncore dcache2sharedcache counter-measure
      bit29: use all 256 ncore to run
      bit30: enable mcore parallel range. if set, ncore-measure is controled by
        range-begin/range-end
      bit31: enable mcore measure(print cycle/parallelcnt/threadnum of each parallel mode)
arg5: 0 -> build from source(default), 1 -> build from binary
arg6: parallel_begin
arg7: parallel_end
arg8: dcache counter config. 32bit unsigned int
arg9: scache counter config. 32bit unsigned int
arg10: enable large profile data mode when profile data > 500MB.
*/
int main(int argc, char *argv[]) {

    int ncore_num_to_perf = 16;
    std::string src_root = "../"; // iverilog/pintapi/
    std::string thread_file_path = "./pint_thread_stub.c";
    std::string prof_mode;
    char *pint_simu_path = nullptr;
    unsigned long long prof_mask = 0x0;
    int is_build_from_binary = 0;
    unsigned long long counter_config = 0;
    unsigned int parallel_begin = 0;
    unsigned int parallel_end = 0xFFFFFFFF;
    unsigned int dcache_counter_config = PINT_DCACHE_COUNTER_CFG;
    unsigned int scache_counter_config = PINT_DCACHE2SHARED_COUNTER_CFG;
    bool is_large_profile_data = false;
    float device_memory_cost = 0.;

    std::string temp_dir = "./__Prof_Temp__/";
    rmdir(temp_dir.c_str());

    // parse for the verilog runtime args like +TESTCASE
    int sys_args_buf_len = 0;
    sys_args_buf = (char *)malloc(SYS_ARGS_BUF_SIZE);
    sys_args_off = (int *)malloc(SYS_ARGS_OFF_SIZE * sizeof(int));
    int valid_sys_arg_idx = 0;
    for (int i = 1; i < argc; i++) {
        char *arg_str = argv[i];
        if (arg_str[0] == '+') {
            int cur_str_len = strlen(arg_str + 1) + 1;
            if (sys_args_num >= SYS_ARGS_OFF_SIZE) {
                printf("Error! only support SYS_ARGS_OFF_SIZE sys args, %s will be lost!\n",
                       arg_str + 1);
                continue;
            }

            if (sys_args_buf_len + cur_str_len > SYS_ARGS_BUF_SIZE) {
                printf("Error! only support SYS_ARGS_BUF_SIZE bytes sys args, %s will be lost!\n",
                       arg_str + 1);
                continue;
            }

            sys_args_num++;
            strcpy(sys_args_buf + sys_args_buf_len, arg_str + 1);
            sys_args_off[valid_sys_arg_idx++] = sys_args_buf_len;
            sys_args_buf_len += cur_str_len;
        }
    }

    if (sys_args_num) {
        argc = argc - sys_args_num;
    }

    switch (argc) {
    case 2:
        thread_file_path = argv[1];
        prof_mode = "TOTAL_SUMMARY";
        prof_mask = 0x00000001;
        break;
    case 3:
        thread_file_path = argv[1];
        prof_mode = argv[2]; // PROF_ALL OR PROF_NCORE_OCCUPANCY
        prof_mask = 0x00000001;
        break;
    case 4:
        thread_file_path = argv[1];
        parallel_begin = std::stoi(argv[2]);
        parallel_end = std::stoi(argv[3]);
        prof_mode = "TIMELINE";
        prof_mask = 0x40000003;
        break;
    case 10:
        ncore_num_to_perf = std::stoi(argv[1]);
        src_root = argv[2];
        thread_file_path = argv[3];
        prof_mask = std::stoull(argv[4], 0, 16);
        is_build_from_binary = std::stoi(argv[5]);
        parallel_begin = std::stoi(argv[6]);
        parallel_end = std::stoi(argv[7]);
        dcache_counter_config = std::stoul(argv[8], 0, 16);
        scache_counter_config = std::stoul(argv[9], 0, 16);
        prof_mode = "TIMELINE";
        break;
    default:
        fprintf(stderr, "\narguments error\n");
        std::exit(-2);
    }

    unsigned int measure_type = ((unsigned long)(prof_mask) & (3UL << 27)) >> 27;

    // load pint_simu_path from env
    if (argc == 2 || argc == 3 || argc == 4) {
        pint_simu_path = getenv("PINT_SIMU_PATH");
        if (!pint_simu_path) {
            fprintf(stderr, "\nPINT_SIMU_PATH is not defined. Make uaptool first.\n");
            std::exit(-2);
        }
        src_root = pint_simu_path;
        src_root += '/';
    }
    ncore_num_to_perf++; // add mcore

    //// print args
    fprintf(stdout, "1. ncore_num_to_perf=%d\n", ncore_num_to_perf);
    fprintf(stdout, "2. src_root=%s\n", src_root.c_str());
    fprintf(stdout, "3. stub_cfile_path=%s\n", thread_file_path.c_str());
    fprintf(stdout, "4. prof_mask=0x%llx\n", prof_mask);
    fprintf(stdout, "5. is_build_from_binary=%d\n", is_build_from_binary);
    fprintf(stdout, "6. parallel_begin=%u\n", parallel_begin);
    fprintf(stdout, "7. parallel_end=%u\n", parallel_end);
    fprintf(stdout, "8. counter_config=0x%x\n", dcache_counter_config);
    fprintf(stdout, "9. counter_config=0x%x\n", scache_counter_config);
    fprintf(stdout, "10. prof_mode=%s\n", prof_mode.c_str());

    std::unordered_map<std::string, PROFILE_CMD> profile_cmd_map = {
        {"TOTAL_SUMMARY", TOTAL_SUMMARY_CMD},
        {"TIMELINE", TIMELINE_CMD},
        {"PROF_ALL_CMD", PROF_ALL_CMD},
        {"PROF_NCORE_OCCUPANCY", PROF_NCORE_OCCUPANCY_CMD},
    };

    switch (profile_cmd_map[prof_mode]) {
    case TOTAL_SUMMARY_CMD: {
        char path[255];
        if (!getcwd(path, 255)) {
            std::cout << "Get path fail!" << std::endl;
        }
        std::string path_str = path;
        perf_thread_lpm_num_s pint_thread_lpm_num;
        std::string pint_thread_stub_cpp = path_str + "/../src_file/pint_thread_stub.c";
        std::cout << "pint_thread_stub_cpp: " << pint_thread_stub_cpp << std::endl;
        get_pint_thread_lpm_num(pint_thread_lpm_num, pint_thread_stub_cpp);

        unsigned int total_thread_num =
            pint_thread_lpm_num.lpm_num + pint_thread_lpm_num.monitor_lpm_num +
            pint_thread_lpm_num.force_lpm_num + pint_thread_lpm_num.thread_lpm_num +
            pint_thread_lpm_num.thread_num + 300;

        std::vector<perf_summary_s> summary_array(total_thread_num * 16, perf_summary_s{0, 0});
        std::cout << "\nenter NCORE_PERF_SUMMARY" << std::endl;
        std::string marco_define = "/common/ -DPROF_NCORE_OCCUPANCY -DCOUNT_PARALLEL_RATIO_ENABLE "
                                   "-DPROF_ON -DPROF_SUMMARY_ON -DPROF_MASK=" +
                                   std::to_string(prof_mask);
        // std::cout << "marco_define: " << marco_define << std::endl;
        std::string csv_filepath = "./simu_summary_total.csv";

        create_pint_bin(marco_define, src_root, thread_file_path, temp_dir);
        get_summary_array_from_device(summary_array, temp_dir, ncore_num_to_perf, total_thread_num);
        dump_summary_csv_file(summary_array, pint_thread_lpm_num, total_thread_num, src_root);

        std::string simu_summary_total_file_path = path_str + "/simu_summary_total.csv";
        std::cout << "\nsummary infomation in: " << simu_summary_total_file_path << std::endl;
        free(sys_args_buf);
        free(sys_args_off);
        break;
        // return 0;
    }
    case PROF_ALL_CMD: {
        std::vector<unsigned int> h_event_startpos;
        std::vector<perf_event_s> h_total_events;
        std::vector<unsigned int> h_event_realnum;
        std::string prof_bin = "./simu_timeline.dat";
        // delete the prof_bin if it exists.
        if (access(prof_bin.c_str(), 0) == 0) {
            int ret = remove(prof_bin.c_str());
            printf("delete %s.\n", prof_bin.c_str());
            assert(ret == 0);
        }

        prof_all(prof_bin, ncore_num_to_perf, h_event_startpos, h_total_events, h_event_realnum,
                 src_root, temp_dir, thread_file_path, prof_mask, is_build_from_binary,
                 parallel_begin, parallel_end, dcache_counter_config, scache_counter_config);
        // return 0;
        break;
    }
    case PROF_NCORE_OCCUPANCY_CMD: {
        printf("================profile enter PROF_ALL ================\n");
        std::vector<unsigned int> h_event_startpos;
        std::vector<perf_event_s> h_total_events;
        std::vector<unsigned int> h_event_realnum;
        std::string prof_bin = "./simu_timeline.dat";
        // delete the prof_bin if it exists.
        if (access(prof_bin.c_str(), 0) == 0) {
            int ret = remove(prof_bin.c_str());
            printf("delete %s.\n", prof_bin.c_str());
            assert(ret == 0);
        }

        prof_ncore_occupancy(prof_bin, ncore_num_to_perf, h_event_startpos, h_total_events,
                             h_event_realnum, src_root, temp_dir, thread_file_path, prof_mask,
                             is_build_from_binary, parallel_begin, parallel_end,
                             dcache_counter_config, scache_counter_config);

        // parse_profile_raw_data(prof_bin, temp_dir);
        break;
        // return 0;
    }
    case TIMELINE_CMD: {
        std::string json_filepath = "./simu_timeline.json";
        std::string csv_filepath = "./simu_summary.csv";

        std::vector<unsigned int> h_event_startpos;
        std::vector<perf_event_s> h_total_events;
        std::vector<unsigned int> h_event_realnum;

        get_prof_event_data(ncore_num_to_perf, h_event_startpos, h_total_events, h_event_realnum,
                            src_root, temp_dir, thread_file_path, prof_mask, is_build_from_binary,
                            parallel_begin, parallel_end, dcache_counter_config,
                            scache_counter_config, device_memory_cost);

        dump_timeline_json(json_filepath, csv_filepath, ncore_num_to_perf, h_event_startpos,
                           h_total_events, h_event_realnum, temp_dir, measure_type);
        char path[255];
        if (!getcwd(path, 255)) {
            std::cout << "Get path fail!" << std::endl;
        }

        std::string path_str = path;
        std::string summary_path = path_str + "/simu_summary.csv";
        std::string timeline_path = path_str + "/simu_timeline.json";

        std::cout << "pint_simu_profile succeeded. Summary infomation in " << summary_path
                  << ". Timeline infomation in " << timeline_path << std::endl;
        free(sys_args_buf);
        free(sys_args_off);
        break;
        // return 0;
    }

    default:
        break;
    }

    return 0;
}
