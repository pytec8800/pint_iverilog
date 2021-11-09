/*
 * Copyright (c) 2000-2020 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * This is a sample target module. All this does is write to the
 * output file some information about each object handle when each of
 * the various object functions is called. This can be used to
 * understand the behavior of the core as it uses a target module.
 */

#include "version_base.h"
#include "version_tag.h"
#include "config.h"
#include "priv.h"
#include "t-dll.h"
#include <time.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#if defined(__WIN32__)
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include "ivl_alloc.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

#include "common.h"

static const char *version_string =
    "Icarus Verilog STUB Code Generator " VERSION " (" VERSION_TAG ")\n\n"
    "Copyright (c) 2000-2020 Stephen Williams (steve@icarus.com)\n\n"
    "  This program is free software; you can redistribute it and/or modify\n"
    "  it under the terms of the GNU General Public License as published by\n"
    "  the Free Software Foundation; either version 2 of the License, or\n"
    "  (at your option) any later version.\n"
    "\n"
    "  This program is distributed in the hope that it will be useful,\n"
    "  but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    "  GNU General Public License for more details.\n"
    "\n"
    "  You should have received a copy of the GNU General Public License along\n"
    "  with this program; if not, write to the Free Software Foundation, Inc.,\n"
    "  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.\n";

#define THREAD_TRACE_ENABLE 0
#define ARRAY_INFO_DATA_SEC ""
#define ARRAY_BSS_SEC ""

/*********************************************************************************/
FILE *out;
int stub_errors = 0;
unsigned thread_buff_file_size = 0;
unsigned thread_buff_file_id = 0;
struct process_info proc_info;

vector<ivl_process_t> dsgn_all_proc;
set<ivl_process_t> dsgn_proc_is_thread;
pint_nbassign_info pint_nbassign_save;
map<string, pint_nbassign_info> nbassign_map;
int pint_thread_fd = -1;
// int pint_dump_fd = -1;

struct dump_info pint_simu_info;

int pint_design_time_precision = 0;
int pint_current_scope_time_unit = 0;
unsigned pint_thread_num = 0;
unsigned pint_thread_pc_max = 0;
unsigned pint_delay_num = 0;
unsigned pint_always_wait_one_event = 0;

unsigned pint_task_pc_max = 0;
unsigned pint_in_task = 0;
void *cur_ivl_task_scope = NULL;

int pint_dump_var = 0;
unsigned pint_min_inst = PINT_MIN_INST_NUM;
unsigned pint_perf_on = 0;
unsigned pint_pli_mode = 0;
unsigned pint_strobe_num = 0;
unsigned pint_static_event_list_table_num = 0;

const char *enval;
const char *enval_flag = "SINGLE";
FILE *pls_log_fp = NULL;
bool g_syntax_unsupport_flag = false;
bool g_log_print_flag = false;
unsigned pint_vpimsg_num = 0;
unsigned pint_open_file_num = 0;
unsigned pint_const0_max_word = 0;
unsigned use_const0_buff = 1;

string array_init_buff;
unsigned pint_array_init_num = 0;
string dir_name;
string start_buff;
string inc_start_buff;
string signal_buff;
string inc_buff;
string signal_list;
string callback_list;
string signal_force;
string thread_buff;
string child_thread_buff;
string dump_file_buff;
string total_thread_buff;
string thread_declare_buff;
string event_static_thread_dec_buff;
map<string, string> nb_delay_same_info;
map<ivl_event_t, string> event_static_thread_buff;
map<ivl_signal_t, pint_signal_s_list_info_s> signal_s_list_map;
map<int, string> thread_child_ind;
map<int, int> thread_is_static_thread;
set<string> exists_sys_args;
string monitor_func_buff = "//monitor functions\n";
string strobe_func_buff = "//strobe functions\n";
string global_init_buff;
string always_run_buff;
vector<string> simu_header_buff;
unsigned lpm_buff_file_id = 0;

string event_declare_buff;
string event_impl_buff;
string event_begin_buff;
string nbassign_buff;
string expr_buff;
string pint_vcddump_file = "pint_dump.vcd";
unsigned pint_dump_vars_num = 0;

//  Loop each stmt in design(proc, func, task)
struct pint_loop_stmt_s {
    ivl_scope_t temp_scope;
    ivl_process_t temp_proc;
};
typedef struct pint_loop_stmt_s *pint_loop_stmt_t;

string task_name;
vector<pint_event_info_t> mcc_event_info;
vector<int> mcc_event_ids;

unsigned pint_event_num = 0; // pad to 32-bit boundary
unsigned pint_event_num_actual = 0;
unsigned pint_process_str_idx = 0;

map<ivl_event_t, pint_event_info_t> pint_event;
map<unsigned, pint_event_info_t> pint_idx_to_evt_info;
set<ivl_event_t> pint_process_event;

//##    Force stmt
unsigned force_lpm_num = 0; //  Note: force_lpm_num may greater than pint_force_lpm.size()
map<ivl_statement_t, int> pint_force_idx;
vector<ivl_statement_t> pint_force_lpm;
vector<int> pint_force_type; // force 0,assign 1;
map<ivl_lval_t, pint_force_lval_t> pint_force_lval;
map<pint_sig_info_t, map<unsigned, unsigned>> force_net_is_const_map;
vector<pint_sig_info_t> force_net_is_const;
map<pint_sig_info_t, map<unsigned, unsigned>> force_net_undrived_by_lpm_map;
vector<pint_sig_info_t> force_net_undrived_by_lpm;

map<unsigned, pint_process_info_t> pint_process_info;
unsigned pint_process_info_num = 0;
unsigned pint_process_num = 0;
unsigned pint_totol_process_num = 0;

map<string, int> pint_output_port_name_id;
map<string, int> pint_input_port_name_id;
unsigned pint_output_port_num = 0;
unsigned pint_input_port_num = 0;
unsigned pint_inout_port_num = 0;
unsigned pint_cross_module_out_port_num = 0;
unsigned pint_cross_module_in_port_num = 0;

string force_buff;
string force_decl_buff;
string force_app_buff;
string force_app_decl_buff;

//  Time statistics
#define PINT_TIME_STAT_INIT                                                                        \
    struct timeval st, ed;                                                                         \
    unsigned sts = 0;                                                                              \
    gettimeofday(&st, NULL);
double t_show_sig = 0;
double t_show_evt = 0;
double t_show_lpm = 0;
double t_show_monitor = 0;
double t_show_func = 0;
double t_show_proc = 0;
double t_pre_parse = 0;
double t_show_force = 0;
double t_dump_buff = 0;
double t_dump_file = 0;
double t_others = 0;

/*********************************************************************************/
#include "multi_assign.h"
#include "analysis_sensitive.h"
#include "pint.h"
#include "pint_show_signal.c"
#include "pint_show_lpm.c"

pint_thread_info_t temp_proc = NULL; //  take care of func & task
pint_mult_info_t temp_mult = NULL;
map<unsigned, pint_mult_info_t> par_mult_anls_ret;
queue<unsigned> par_anls_task;
int par_task_num = 0;
unsigned par_anls_sts[PINT_PAR_ANLS_NUM] = {0};
unsigned par_end_task = 0;
pthread_mutex_t par_lock_wait_proc;
pthread_mutex_t par_lock_wait_task;
pthread_mutex_t par_lock_wait_anls;
pthread_cond_t par_cond_wait_proc;
pthread_cond_t par_cond_wait_task;
pthread_cond_t par_cond_wait_anls;
// nxz
dfa_sig_nxz_t proc_sig_nxz;

/*******************< Common Funciton >*******************/
static struct udp_define_cell {
    ivl_udp_t udp;
    unsigned ref;
    struct udp_define_cell *next;
} *udp_define_list = 0;

string string_format(const char *format, ...) {
    char buffer[2048];
    va_list va_l;
    va_start(va_l, format);
    vsnprintf(buffer, 2048, format, va_l);
    va_end(va_l);
    return string(buffer);
}

int pint_write(int fd, const char *buff, unsigned size) {
    unsigned byte = 0;
    unsigned write_byte = 0;
    unsigned cnt = 0;
    while ((write_byte < size) && (cnt < 5)) {
        byte = write(fd, buff + write_byte, size - write_byte);
        write_byte += byte;
        cnt++;
    }
    if (write_byte != size) {
        return -1;
    }
    return 0;
}

void pint_source_file_ret(string &buff, const char *file, unsigned lineno) {
    if (!file)
        return;
    FILE *fp = fopen(file, "r");
    char tmp[1025];
    unsigned int is_continue = 1;
    unsigned int line_pre = 0;
    unsigned int pos = 0;
    int off = 0;
    int ret, i;
    tmp[1024] = 0;
    while (is_continue) {
        ret = fread(tmp, 1, 1024, fp);
        if (ret < 1024)
            is_continue = 0;
        for (i = 0; i < ret; i++) {
            if (tmp[i] == '\n') {
                line_pre++;
                if (line_pre == lineno - 1) {
                    pos = off + i + 1;
                    is_continue = 0;
                    break;
                }
            }
        }
        off += ret;
    }
    fseek(fp, pos, SEEK_SET);
    ret = fread(tmp, 1, 1024, fp);
    for (i = 0; i < ret; i++) {
        if (tmp[i] == '\n') {
            tmp[i] = 0;
            break;
        }
    }
    fclose(fp);
    buff.append(tmp);
}

unsigned pint_get_bits_value(const char *bits) {
    unsigned long long ret = 0;
    unsigned long long tmp = 1;
    while (*bits) {
        switch (*bits++) {
        case '1':
            ret += tmp;
            break;
        case '0':
            break;
        default:
            return VALUE_XZ;
        }
        tmp <<= 1;
    }
    if (ret >> 32) {
        return VALUE_MAX;
    } else {
        return (unsigned)ret;
    }
}

unsigned pint_get_arr_idx_value(ivl_expr_t net) {
    if (!net) {
        return 0;
    }
    if (ivl_expr_type(net) != IVL_EX_NUMBER) {
        return 0xffffffff;
    }
    const char *bits = ivl_expr_bits(net);
    unsigned width = ivl_expr_width(net);
    unsigned long long ret = 0;
    unsigned long long tmp = 1;
    unsigned i;
    for (i = 0; i < width; i++, tmp <<= 1) {
        switch (bits[i]) {
        case '0':
            break;
        case '1':
            ret += tmp;
            break;
        default:
            return 0xffffffff;
        }
    }
    if (ret >> 32) {
        return 0xffffffff;
    } else {
        return (unsigned)ret;
    }
}

unsigned pint_get_ex_number_value(ivl_expr_t net) {
    if (!net)
        return 0;
    assert(ivl_expr_type(net) == IVL_EX_NUMBER);
    const char *bits = ivl_expr_bits(net);
    unsigned width = ivl_expr_width(net);
    unsigned long long ret = 0;
    unsigned long long tmp = 1;
    unsigned i;
    for (i = 0; i < width; i++, tmp <<= 1) {
        switch (bits[i]) {
        case '0':
            break;
        case '1':
            ret += tmp;
            break;
        default:
            return VALUE_XZ;
        }
    }
    if (ret >> 32) {
        assert(0);
    } else {
        return (unsigned)ret;
    }
}

unsigned long long pint_get_ex_number_value_lu(ivl_expr_t net, bool allow_xz) {
    if (!net)
        return 0;
    assert(ivl_expr_type(net) == IVL_EX_NUMBER);
    const char *bits = ivl_expr_bits(net);
    unsigned width = ivl_expr_width(net);
    unsigned long long ret = 0;
    unsigned long long tmp = 1;
    unsigned i;
    for (i = 0; i < width; i++, tmp <<= 1) {
        switch (bits[i]) {
        case '0':
            break;
        case '1':
            ret += tmp;
            break;
        default:
            if (allow_xz) {
                return 0;
            } else {
                PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net),
                                       "digit neighter 0 nor 1");
            }
        }
    }
    return ret;
}

bool pint_signal_is_force_net_const(pint_sig_info_t sig_info, unsigned word) {
    if (!force_net_is_const_map.count(sig_info))
        return 0;
    if (!force_net_is_const_map[sig_info].count(word))
        return 0;
    return 1;
}

double pint_exec_duration(struct timeval &st, struct timeval &ed, unsigned &sts) {
    double ret = 0;
    if (!sts) {
        sts = 1;
        gettimeofday(&ed, NULL);
        ret += ed.tv_sec - st.tv_sec;
        ret += (double)(ed.tv_usec - st.tv_usec) / 1000000;
    } else {
        sts = 0;
        gettimeofday(&st, NULL);
        ret += st.tv_sec - ed.tv_sec;
        ret += (double)(st.tv_usec - ed.tv_usec) / 1000000;
    }
    return ret;
}

/******************************************************************************** Std length -100 */
//  mult_asgn in process  --API
set<ivl_signal_t> mult_arr_not_support;
mult_asgn_type_t pint_proc_get_sig_mult_type(ivl_signal_t sig) {
    if (!temp_mult) {
        return MULT_NONE;
    }
    std::unordered_map<ivl_signal_t, unsigned> &tgt = temp_mult->p_cnt;
    if (!tgt.count(sig)) {
        return MULT_NONE;
    }
    unsigned sig_words = ivl_signal_array_count(sig);
    unsigned asgn_cnt = tgt[sig];
    if (sig_words == 1) {
        switch (asgn_cnt) {
        case 1:
            return MULT_SIG_1;
        case 2:
            return MULT_SIG_2;
        case 3:
            return MULT_SIG_3;
        default:
            return MULT_NONE;
        }
    } else {
        switch (asgn_cnt) {
        case 1:
            return MULT_ARR_1;
        case 2:
        case 3:
            if (sig_words <= PINT_ARR_MAX_SHADOW_LEN) {
                if (mult_arr_not_support.count(sig)) {
                    return MULT_ARR_1;
                } else if (pint_signal_asgn_in_mult_threads(sig) &&
                           pint_signal_is_part_asgn_in_thread(sig, temp_proc->proc)) {
                    mult_arr_not_support.insert(sig);
                    PINT_UNSUPPORTED_WARN(ivl_signal_file(sig), ivl_signal_lineno(sig),
                                          "mult_arr been part asgned in mult threads.");
                    return MULT_ARR_1;
                } else {
                    return MULT_ARR_23;
                }
            } else {
                if (!mult_arr_not_support.count(sig)) {
                    mult_arr_not_support.insert(sig);
                    PINT_UNSUPPORTED_WARN(ivl_signal_file(sig), ivl_signal_lineno(sig),
                                          "arr_words beyond PINT_ARR_MAX_SHADOW_LEN.");
                }
                return MULT_ARR_1;
            }
        default:
            return MULT_NONE;
        }
    }
}

unsigned pint_proc_get_mult_idx(ivl_signal_t sig) {
    if (!temp_proc || !temp_proc->sig_to_tmp_id.count(sig)) {
        return 0; //  means the signal is not multiple assign
    } else {
        return temp_proc->sig_to_tmp_id[sig];
    }
}

bool pint_asgn_is_full(ivl_lval_t lval) {
    ivl_signal_t sig = ivl_lval_sig(lval);
    pint_sig_info_t sig_info = pint_find_signal(sig);
    if (ivl_lval_width(lval) < sig_info->width) {
        return false;
    }
    ivl_expr_t base = ivl_lval_part_off(lval);
    if (!base) {
        return true;
    }
    if (ivl_expr_type(base) == IVL_EX_NUMBER && !pint_get_bits_value(ivl_expr_bits(base))) {
        return true;
    } else {
        return false;
    }
}

bool pint_proc_mult_sig_needs_mask(ivl_signal_t sig) {
    mult_asgn_type_t asgn_type = pint_proc_get_sig_mult_type(sig);
    if (asgn_type == MULT_SIG_2 || asgn_type == MULT_SIG_3) {
        if (pint_signal_asgn_in_mult_threads(sig) &&
            pint_signal_is_part_asgn_in_thread(sig, temp_proc->proc)) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

string pint_proc_get_sig_name(ivl_signal_t sig, const char *arr_idx, bool is_asgn) {
    mult_asgn_type_t asgn_type = pint_proc_get_sig_mult_type(sig);
    pint_sig_info_t sig_info = pint_find_signal(sig);
    unsigned mult_idx = pint_proc_get_mult_idx(sig);
    if (is_asgn) {
        if (sig_info->arr_words == 1) {
            switch (asgn_type) {
            case MULT_NONE:
            case MULT_SIG_1:
                return sig_info->sig_name;
            case MULT_SIG_2:
            case MULT_SIG_3:
                return string_format("TMP%u", mult_idx);
            default:
                assert(0);
            }
        } else {
            switch (asgn_type) {
            case MULT_NONE:
            case MULT_SIG_1:
            case MULT_ARR_1:
                return sig_info->sig_name + string_format("[%s]", arr_idx);
            case MULT_SIG_2:
            case MULT_SIG_3:
                return string_format("TMP%u", mult_idx);
            case MULT_ARR_23:
                return string_format("TMP%u[%s]", mult_idx, arr_idx);
            }
        }
    } else {
        if (sig_info->arr_words == 1) {
            return sig_info->sig_name + "_nb";
        } else {
            return sig_info->sig_name + string_format("_nb[%s]", arr_idx);
        }
    }
    return "";
}

string pint_proc_gen_copy_code(const char *dst, const char *src, const char *mask, unsigned width) {
    if (!mask) {
        if (width <= 4) {
            return string_format("%s = %s;\n", dst, src);
        } else {
            return string_format("pint_copy_int(%s, %s, %u);\n", dst, src, width);
        }
    } else {
        if (width <= 4) {
            return string_format("pint_mask_copy_char(&%s, &%s, (unsigned char*)&%s);\n", dst, src,
                                 mask);
        } else if (width <= 32) {
            return string_format("pint_mask_copy_int(%s, %s, &%s, %u);\n", dst, src, mask, width);
        } else {
            return string_format("pint_mask_copy_int(%s, %s, %s, %u);\n", dst, src, mask, width);
        }
    }
}

void pint_proc_updt_mult_asgn(string &buff, ivl_signal_t sig) {
    mult_asgn_type_t asgn_type = pint_proc_get_sig_mult_type(sig);
    unsigned mult_idx = pint_proc_get_mult_idx(sig);
    unsigned has_mask = pint_proc_mult_sig_needs_mask(sig);
    pint_sig_info_t sig_info = pint_find_signal(sig);
    string shadow = string_format("TMP%u", mult_idx);
    string mask_info = string_format("MASK%u", mult_idx);
    string dst_info;
    const char *sn = sig_info->sig_name.c_str();
    const char *src = shadow.c_str();
    const char *dst = sn;
    const char *mask = has_mask ? mask_info.c_str() : NULL;
    unsigned arr_words = sig_info->arr_words;
    unsigned width = sig_info->width;
    unsigned forced_var = pint_sig_is_force_var(sig_info);
    if (forced_var) {
        if (sig_info->force_info->assign_idx.size()) {
            buff += string_format("\tif(!force_en_%s && !assign_en_%s){\n", sn, sn);
        } else {
            buff += string_format("\tif(!force_en_%s){\n", sn);
        }
    }
    switch (asgn_type) {
    case MULT_SIG_2:
    case MULT_SIG_3: {
        unsigned word = pint_get_arr_idx_from_sig(sig);
        if (arr_words > 1) {
            dst_info = sig_info->sig_name + string_format("[%u]", word);
            dst = dst_info.c_str();
        }
        if (!pint_signal_has_addl_opt_when_updt(sig_info, word)) {
            buff += "\t" + pint_proc_gen_copy_code(dst, src, mask, width);
        } else {
            if (width <= 4) {
                buff += string_format("\tif(%s != %s){\n", dst, src);
                if (pint_signal_has_sen_evt(sig_info, word)) {
                    if (width == 1) {
                        buff += string_format("\t\tchange_flag = edge_1bit(%s, %s);\n", dst, src);
                    } else {
                        buff += string_format("\t\tchange_flag = edge_char(%s, %s);\n", dst, src);
                    }
                    buff += "\t" + pint_proc_gen_copy_code(dst, src, mask, width);
                    buff += pint_gen_sig_e_list_code(sig_info, "change_flag", word) + "\n";
                } else {
                    buff += "\t" + pint_proc_gen_copy_code(dst, src, mask, width);
                }
            } else {
                buff += string_format("\tif(!pint_case_equality_int_ret(%s, %s, %u)){\n", dst, src,
                                      width);
                if (pint_signal_has_sen_evt(sig_info, word)) {
                    buff +=
                        string_format("\t\tchange_flag = edge_int(%s, %s, %u);\n", dst, src, width);
                    buff += "\t\t" + pint_proc_gen_copy_code(dst, src, mask, width);
                    buff += pint_gen_sig_e_list_code(sig_info, "change_flag", word) + "\n";
                } else {
                    buff += "\t\t" + pint_proc_gen_copy_code(dst, src, mask, width);
                }
            }
            if (pint_signal_has_sen_lpm(sig_info, word)) {
                buff += pint_gen_sig_l_list_code(sig_info, word) + "\n";
            }
            if (pint_signal_has_p_list(sig_info)) {
                buff += pint_gen_sig_p_list_code(sig_info) + "\n";
            }
            pint_vcddump_var(sig_info, &buff);
            if (pint_pli_mode && sig_info->is_interface &&
                (IVL_SIP_OUTPUT == sig_info->port_type)) {
                buff += string_format("\t\tpint_update_output_value_change(%d);\n",
                                      sig_info->output_port_id);
            }
            buff += "\t}\n";
        }
        break;
    }
    case MULT_ARR_23: {
        unsigned has_p = pint_signal_has_p_list(sig_info);
        string copy_code;
        string updt_code;
        if (width <= 4) {
            copy_code = string_format("%s[wd] = %s[wd];", dst, src);
        } else {
            copy_code = string_format("pint_copy_int(%s[wd], %s[wd], %u);", dst, src, width);
        }
        if (!pint_signal_has_addl_opt_when_updt(sig_info)) {
            updt_code = copy_code;
        } else {
            unsigned has_e = pint_signal_has_sen_evt(sig_info);
            unsigned has_l = pint_signal_has_sen_lpm(sig_info);
            if (has_e) {
                if (width <= 4) {
                    if (width == 1) {
                        updt_code += string_format("change_flag = edge_1bit(%s[wd], %s[wd]); ", dst, src);
                    } else {
                        updt_code += string_format("change_flag = edge_char(%s[wd], %s[wd]); ", dst, src);
                    }
                } else {
                    updt_code += string_format("change_flag = edge_int(%s[wd], %s[wd], %u); ", dst,
                                               src, width);
                }
                updt_code += "if(change_flag){ ";
                updt_code += copy_code + " ";
                updt_code += pint_gen_sig_e_list_code(sig_info, "change_flag", "wd") + " ";
            } else {
                if (width <= 4) {
                    updt_code += string_format("if(%s[wd] != %s[wd]){ ", dst, src);
                } else {
                    updt_code += string_format(
                        "if(!pint_case_equality_int_ret(%s[wd], %s[wd], %u)){ ", dst, src, width);
                }
                updt_code += copy_code + " ";
            }
            if (has_l) {
                updt_code += pint_gen_sig_l_list_code(sig_info, "wd") + " ";
            }
            if (has_p) {
                updt_code += "if(!pset){ pset=1; ";
                updt_code += pint_gen_sig_p_list_code(sig_info);
                updt_code += " }";
            }
            updt_code += "}";
        }
        buff += "{\n";
        if (!has_p) {
            buff += string_format("\tunsigned i, wd, num = UPDT%u[0];\n", mult_idx);
        } else {
            buff += string_format("\tunsigned i, wd, pset = 0, num = UPDT%u[0];\n", mult_idx);
        }
        buff += "\tif(num <= 3){\n";
        buff += string_format("\t\tfor(i=1; i<=num; i++){ wd = UPDT%u[i]; ", mult_idx);
        buff += updt_code;
        buff += "}\n";
        buff += "\t}else{\n";
        buff +=
            string_format("\t\tfor(wd=0; wd<%u; wd++){ if(FLG%u[wd] == 3) { ", arr_words, mult_idx);
        buff += updt_code;
        buff += "}}\n";
        buff += "\t}\n";
        buff += "}\n";
        break;
    }
    default:
        assert(0);
    }
    if (forced_var) {
        buff += "\t}\n";
    }
}

string pint_proc_gen_exit_code(bool has_ret, int ret_val) {
    if (temp_proc && temp_proc->chk_before_exit) {
        return "\tgoto PC_END;\n";
    } else if (!has_ret) {
        return "\treturn;\n";
    } else {
        return string_format("\treturn %d;\n", ret_val);
    }
}

string pint_proc_gen_exit_code() { return pint_proc_gen_exit_code(0, 0); }

bool pint_parse_mult_up_oper(string &buff, ivl_statement_t parent, unsigned sub_idx) {
    if (!temp_mult || !temp_mult->p_up.count(parent)) {
        return 0;
    }
    map<unsigned, ivl_signal_t> needs_up; //  <mult_idx, signal>
    std::unordered_map<ivl_signal_t, unsigned int> &tgt = temp_mult->p_up[parent];
    std::unordered_map<ivl_signal_t, unsigned int>::iterator i;
    ivl_signal_t sig;
    mult_asgn_type_t asgn_type;
    unsigned mult_idx;
    for (i = tgt.begin(); i != tgt.end(); i++) {
        if (!parent || i->second == sub_idx) { //  parent = NULL, means up at the begining
            sig = i->first;
            asgn_type = pint_proc_get_sig_mult_type(sig);
            if (asgn_type == MULT_SIG_2) { //  only type-MULT_SIG_2 cares up-down oper
                mult_idx = pint_proc_get_mult_idx(sig);
                needs_up[mult_idx] = sig;
            }
        }
    }
    if (!needs_up.size()) {
        return 0;
    }
    // buff += string_format("//  [up-oper]    parent: 0x%x  type: %u  sub_idx: %u\n",
    //     parent, ivl_statement_type(parent), sub_idx);  //  debug
    map<unsigned, ivl_signal_t>::iterator j;
    pint_sig_info_t sig_info;
    const char *src;
    unsigned width;
    string src_info;
    for (j = needs_up.begin(); j != needs_up.end(); j++) {
        mult_idx = j->first;
        sig = j->second;
        sig_info = pint_find_signal(sig);
        width = sig_info->width;
        if (sig_info->arr_words == 1) {
            src = sig_info->sig_name.c_str();
        } else {
            unsigned word = pint_get_arr_idx_from_sig(sig);
            src_info = sig_info->sig_name + string_format("[%u]", word);
            src = src_info.c_str();
        }
        if (width <= 4) {
            buff += string_format("\tTMP%u = %s;\n", mult_idx, src);
        } else {
            buff += string_format("\tpint_copy_int(TMP%u, %s, %u);\n", mult_idx, src, width);
        }
    }
    return 1;
}

bool pint_parse_mult_down_oper(string &buff, ivl_statement_t parent, unsigned sub_idx) {
    //  If parent == NULL, means down at last
    if (!temp_mult || !temp_mult->p_down.count(parent)) {
        return 0;
    }
    map<unsigned, ivl_signal_t> needs_down; //  <mult_idx, signal>
    std::unordered_map<ivl_signal_t, unsigned int> &tgt = temp_mult->p_down[parent];
    std::unordered_map<ivl_signal_t, unsigned int>::iterator i;
    ivl_signal_t sig;
    mult_asgn_type_t asgn_type;
    unsigned mult_idx;
    for (i = tgt.begin(); i != tgt.end(); i++) {
        if (!parent || i->second == sub_idx) { //  parent = NULL, means up at the begining
            sig = i->first;
            asgn_type = pint_proc_get_sig_mult_type(sig);
            if (asgn_type == MULT_SIG_2) { //  only type-MULT_SIG_2 cares up-down oper
                mult_idx = pint_proc_get_mult_idx(sig);
                needs_down[mult_idx] = sig;
            }
        }
    }
    if (!needs_down.size()) {
        return 0;
    }
    // buff += string_format("//  [down-oper]  parent: 0x%x  type: %u  sub_idx: %u\n",
    //     parent, ivl_statement_type(parent), sub_idx);  //  debug
    map<unsigned, ivl_signal_t>::iterator j;
    for (j = needs_down.begin(); j != needs_down.end(); j++) {
        sig = j->second;
        pint_proc_updt_mult_asgn(buff, sig);
    }
    return 1;
}

void pint_print_signal_up_down_info(ivl_signal_t sig, unsigned is_up) {
    up_down_t *tgt;
    up_down_t::iterator i;
    std::unordered_map<ivl_signal_t, unsigned int>::iterator j;
    ivl_statement_t parent;
    unsigned sub_idx;
    const char *file;
    unsigned lineno;
    if (is_up) {
        printf("\tmult up-oper:\n");
        tgt = &(temp_mult->p_up);
    } else {
        printf("\tmult down-oper:\n");
        tgt = &(temp_mult->p_down);
    }
    for (i = tgt->begin(); i != tgt->end(); i++) {
        parent = i->first;
        for (j = i->second.begin(); j != i->second.end(); j++) {
            if (j->first == sig) {
                sub_idx = j->second;
                if (parent) {
                    file = ivl_stmt_file(parent);
                    lineno = ivl_stmt_lineno(parent);
                } else {
                    file = "NULL";
                    lineno = 0;
                }
                printf("\t\t%s[%u]:\tsub_idx: %u\n", file, lineno, sub_idx);
            }
        }
    }
}

void pint_show_sig_mult_anls_info(ivl_signal_t sig) {
    if (!temp_proc || !temp_mult) {
        printf("mult anls failed.\n");
        return;
    }
    pint_sig_info_t sig_info = pint_find_signal(sig);
    printf("Mult-info of %s:\n", sig_info->sig_name.c_str());
    printf("\tmult_asgn_type:   %u\n", pint_proc_get_sig_mult_type(sig));
    pint_print_signal_up_down_info(sig, 1);
    pint_print_signal_up_down_info(sig, 0);
}

bool pint_proc_has_addl_opt_after_sig_asgn(ivl_signal_t sig) {
    mult_asgn_type_t asgn_type = pint_proc_get_sig_mult_type(sig);
    switch (asgn_type) {
    case MULT_SIG_2:
    case MULT_SIG_3:
    case MULT_ARR_23:
        return false;
    default:
        return pint_signal_has_addl_opt_when_updt(pint_find_signal(sig),
                                                  pint_get_arr_idx_from_sig(sig));
    }
}

void pint_proc_mult_opt_before_sig_asgn(string &buff, ivl_lval_t lval, const char *word) {
    ivl_signal_t sig = ivl_lval_sig(lval);
    mult_asgn_type_t asgn_type = pint_proc_get_sig_mult_type(sig);
    if (asgn_type == MULT_ARR_23 && !pint_asgn_is_full(lval)) {
        pint_sig_info_t sig_info = pint_find_signal(sig);
        const char *sn = sig_info->sig_name.c_str();
        unsigned width = sig_info->width;
        unsigned mult_idx = pint_proc_get_mult_idx(sig);
        buff +=
            string_format("\tif(!FLG%u[%s]){\n\t\tFLG%u[%s] = 1; ", mult_idx, word, mult_idx, word);
        if (width <= 4) {
            buff += string_format("TMP%u[%s] = %s[%s];", mult_idx, word, sn, word);
        } else {
            buff += string_format("pint_copy_int(TMP%u[%s], %s[%s], %u);", mult_idx, word, sn, word,
                                  width);
        }
        buff += "\n\t}\n";
    }
}

void pint_proc_mult_opt_when_sig_asgn(string &buff, ivl_signal_t sig, const char *word,
                                      const char *asgn_base, unsigned asgn_width) {
    mult_asgn_type_t asgn_type = pint_proc_get_sig_mult_type(sig);
    switch (asgn_type) {
    case MULT_SIG_2:
    case MULT_SIG_3:
        if (pint_proc_mult_sig_needs_mask(sig)) {
            unsigned mult_idx = pint_proc_get_mult_idx(sig);
            pint_sig_info_t sig_info = pint_find_signal(sig);
            unsigned width = sig_info->width;
            if (width <= 32) {
                buff += string_format("\tpint_set_mask(&MASK%u, %s, %u, %u);\n", mult_idx,
                                      asgn_base, asgn_width, width);
            } else {
                buff += string_format("\tpint_set_mask(MASK%u, %s, %u, %u);\n", mult_idx, asgn_base,
                                      asgn_width, width);
            }
        }
        break;
    case MULT_ARR_23: {
        unsigned mult_idx = pint_proc_get_mult_idx(sig);
        buff += string_format("\t\tFLG%u[%s] = 3;\n", mult_idx, word);
        buff += string_format("\t\tpint_set_mult_arr_updt_word(UPDT%u, %s);\n", mult_idx, word);
        break;
    }
    default:
        break;
    }
}

void pint_proc_mult_opt_after_sig_asgn(string &buff, ivl_signal_t sig, const char *word) {
    mult_asgn_type_t asgn_type = pint_proc_get_sig_mult_type(sig);
    switch (asgn_type) {
    case MULT_SIG_2:
    case MULT_SIG_3:
    case MULT_ARR_23:
        return;
    default: {
        pint_sig_info_t sig_info = pint_find_signal(sig);
        if (pint_signal_has_p_list(sig_info)) {
            buff += pint_gen_sig_p_list_code(sig_info) + "\n";
        }
        if (pint_signal_has_sen_lpm(sig_info)) {
            buff += pint_gen_sig_l_list_code(sig_info, word) + "\n";
        }
        if (pint_signal_has_sen_evt(sig_info)) {
            buff += pint_gen_sig_e_list_code(sig_info, "change_flag", word) + "\n";
        }
        pint_vcddump_var(sig_info, &buff);
        if (pint_pli_mode && sig_info->is_interface && (IVL_SIP_OUTPUT == sig_info->port_type)) {
            buff += string_format("\t\tpint_update_output_value_change(%d);\n",
                                  sig_info->output_port_id);
        }
        break;
    }
    }
}
void pint_proc_mult_opt_before_sig_use(string &buff, ivl_signal_t sig, const char *word) {
    mult_asgn_type_t asgn_type = pint_proc_get_sig_mult_type(sig);
    if (asgn_type == MULT_ARR_23) {
        pint_sig_info_t sig_info = pint_find_signal(sig);
        const char *sn = sig_info->sig_name.c_str();
        unsigned width = sig_info->width;
        unsigned mult_idx = pint_proc_get_mult_idx(sig);
        buff +=
            string_format("\tif(!FLG%u[%s]){\n\t\tFLG%u[%s] = 1; ", mult_idx, word, mult_idx, word);
        if (width <= 4) {
            buff += string_format("TMP%u[%s] = %s[%s];", mult_idx, word, sn, word);
        } else {
            buff += string_format("pint_copy_int(TMP%u[%s], %s[%s], %u);", mult_idx, word, sn, word,
                                  width);
        }
        buff += "\n\t}\n";
    }
}

void pint_proc_mult_opt_before_sig_use(string &buff, ivl_signal_t sig, unsigned word) {
    string wn = string_format("%u", word);
    pint_proc_mult_opt_before_sig_use(buff, sig, wn.c_str());
}

/******************************************************************************** Std length -100 */
//##    Parse all force info in design

void pint_dump_process_signal_info(void) {
    unsigned num = pint_sig_info_needs_dump.size();
    unsigned i;
    for (i = 0; i < num; i++) {
        pint_sig_info_t sig_info = pint_sig_info_needs_dump[i];
        if (sig_info->sens_process_list.size()) {
            printf("%s:", sig_info->sig_name.c_str());
            for (unsigned ii = 0; ii < sig_info->sens_process_list.size(); ii++) {
                printf("%u ", sig_info->sens_process_list[ii]);
            }
            printf("\n");
        }
    }

    map<unsigned, pint_process_info_t>::iterator p_iter;
    pint_process_info_t pint_proc;
    for (p_iter = pint_process_info.begin(); p_iter != pint_process_info.end(); p_iter++) {
        pint_proc = p_iter->second;
        printf("process%u(%lu):", pint_proc->flag_id,
               (unsigned long)pint_proc->sens_sig_list.size());
        for (unsigned i = 0; i < pint_proc->sens_sig_list.size(); i++) {
            printf("%s ", pint_proc->sens_sig_list[i]->sig_name.c_str());
        }
        printf("\n");
    }
}

void pint_add_process_sens_signal(pint_sig_info_t sig_info, unsigned process_id) {
    unsigned push_flag = 1;
    // add signal to process sens-signal-list
    for (const auto &item : pint_process_info[process_id]->sens_sig_list) {
        if (item == sig_info) {
            push_flag = 0;
            break;
        }
    }
    if (push_flag) {
        pint_process_info[process_id]->sens_sig_list.push_back(sig_info);
    }
    // add proess id to singals sens-process-list
    for (const auto &item : sig_info->sens_process_list) {
        if (item == process_id) {
            push_flag = 0;
            break;
        }
    }
    if (push_flag) {
        sig_info->sens_process_list.push_back(process_id);
    }
}

void pint_parse_lval_force_info(ivl_lval_t net, ivl_statement_t stmt, unsigned is_force,
                                unsigned is_assign) {
    ivl_signal_t sig = ivl_lval_sig(net);
    if (!sig)
        return;
    pint_sig_info_t sig_info = pint_find_signal(sig);
    unsigned word = pint_get_ex_number_value(ivl_lval_idx(net));
    unsigned updt_base = pint_get_ex_number_value(ivl_lval_part_off(net));
    unsigned updt_size = ivl_lval_width(net);
    pint_force_lval_t lval_info = new struct pint_force_lval_s;
    lval_info->sig_info = sig_info;
    lval_info->word = word < sig_info->arr_words ? word : VALUE_MAX;
    lval_info->updt_base = updt_base < sig_info->width ? updt_base : VALUE_MAX;
    lval_info->updt_size = updt_size;
    pint_force_lval[net] = lval_info;
    pint_force_info_t force_info = pint_find_signal_force_info(sig_info, word);
    if (!force_info) {
        force_info = pint_gen_new_force_info(sig, word, stmt, is_assign);
        if (force_info->type == FORCE_NET) {
            if (pint_signal_is_const(sig_info, word)) { //  net is a const
                if (!force_net_is_const_map.count(sig_info)) {
                    force_net_is_const.push_back(sig_info);
                }
                force_net_is_const_map[sig_info][word] = force_lpm_num++;
            } else if (pint_find_lpm_from_output(ivl_signal_nex(sig, word)) ==
                       -1) { //  net not driven by lpm
                if (!force_net_undrived_by_lpm_map.count(sig_info)) {
                    force_net_undrived_by_lpm.push_back(sig_info);
                }
                force_net_undrived_by_lpm_map[sig_info][word] = force_lpm_num++;
            }
        }
    } else {
        if (is_force) {
            force_info->force_idx.insert(pint_force_idx[stmt]); //  Placed before judgement, in case
                                                                //  stmt_new and stmt_old are the
                                                                //  same
            if (!force_info->mult_force && force_info->force_idx.size() > 1) {
                force_info->mult_force = 1;
            }
        }
        if (is_assign) {
            force_info->assign_idx.insert(
                pint_force_idx[stmt]); //  Placed before judgement, in case
            if (!force_info->mult_assign && force_info->assign_idx.size() > 1) {
                force_info->mult_assign = 1;
            }
        }
        ivl_scope_t scope = ivl_signal_scope(sig);
        if (scope != force_info->scope) {
            PINT_UNSUPPORTED_ERROR(ivl_stmt_file(stmt), ivl_stmt_lineno(stmt),
                                   "find force stmt of the same signal in different scope.");
        }
    }
    if (!force_info->part_force) {
        if (updt_base || updt_size != sig_info->width) {
            force_info->part_force = 1;
            if (force_info->type == FORCE_VAR) {
                PINT_UNSUPPORTED_ERROR(ivl_stmt_file(stmt), ivl_stmt_lineno(stmt),
                                       "force/release on bit select or part select of a vector "
                                       "variable is not valid.");
                // assert(0);
            }
        }
    }
}

//##    Dump all lpm generated by force stmt
void pint_dump_force_lpm() {
    ivl_statement_t stmt;
    unsigned num = pint_force_lpm.size();
    unsigned i;
    force_buff += "//force lpm info\n";
    for (i = 0; i < num; i++) {
        stmt = pint_force_lpm[i];
        unsigned type = pint_force_type[i];
        if (pint_force_idx[stmt] >= 0)
            pint_show_force_lpm(stmt, pint_force_idx[stmt], type);
    }
}

void pint_show_force_lpm(ivl_statement_t net, unsigned force_idx, unsigned type) {
    map<pint_sig_info_t, map<unsigned, pint_force_lhs_t>> force_lhs_map;
    vector<pint_force_lhs_t> force_lhs_set;
    unsigned force_lhs_num = 0;
    string lhs_define;
    string lhs_asgn;
    string rhs_decl;
    string rhs_oper;
    unsigned rhs_width = ivl_expr_width(ivl_stmt_rval(net));
    unsigned rhs_base = 0;
    unsigned thread_id = force_idx + FORCE_THREAD_OFFSET;
    unsigned nlval = ivl_stmt_lvals(net);
    ivl_lval_t lval;
    pint_force_lval_t lval_info;
    pint_sig_info_t sig_info;
    unsigned word;
    pint_force_lhs_t force_lhs;
    unsigned i;
    // nxz
    ExpressionConverter rhs(ivl_stmt_rval(net), net, 0, NUM_EXPRE_SIGNAL);
    rhs.GetPreparation(&rhs_oper, &rhs_decl);
    pint_add_force_lpm_into_rhs_sen(rhs.Sensitive_Signals(), force_idx + FORCE_LPM_OFFSET);
    pint_gen_lpm_head(force_buff, force_decl_buff, thread_id);
    pint_show_stmt_signature(force_buff, net);
    for (i = 0; i < nlval; i++, rhs_base += ivl_lval_width(lval)) {
        lval = ivl_stmt_lval(net, i);
        lval_info = pint_force_lval[lval];

        if (lval_info->word == VALUE_MAX || lval_info->updt_base == VALUE_MAX) {
            continue;
        }

        sig_info = lval_info->sig_info;
        word = lval_info->word;
        if (force_lhs_map.count(sig_info) && force_lhs_map[sig_info].count(word)) {
            force_lhs = force_lhs_map[sig_info][word];
        } else {
            force_lhs = NULL;
        }
        if (!force_lhs) {
            force_lhs = new struct pint_force_lhs_s;
            force_lhs->lhs_idx = force_lhs_num++;
            pint_parse_new_force_lhs(force_lhs, lval_info, rhs.GetResultName(), rhs_width, rhs_base,
                                     thread_id, type);
            force_lhs_map[sig_info][word] = force_lhs;
            force_lhs_set.push_back(force_lhs);
        } else {
            pint_parse_exists_force_lhs(force_lhs, lval_info, rhs.GetResultName(), rhs_width,
                                        rhs_base); //  It must be part_force
        }
    }
    if (force_lhs_num) {
        force_buff += "\tif(!(";
        for (i = 0; i < force_lhs_num; i++) {
            lhs_define += force_lhs_set[i]->define;
            force_buff += force_lhs_set[i]->cond + " || ";
            if (force_lhs_num > 1)
                lhs_asgn += string_format("\tif(%s){\n", force_lhs_set[i]->cond.c_str());
            lhs_asgn += force_lhs_set[i]->asgn;
            lhs_asgn += force_lhs_set[i]->upate;
            if (force_lhs_num > 1)
                lhs_asgn += "\t}\n";
            delete force_lhs_set[i];
        }
        force_buff.erase(force_buff.end() - 4, force_buff.end());
        force_buff += ")) return;\n";
        force_buff += lhs_define;
        force_buff += rhs_decl;
        force_buff += "\tint change_flag;\n";
        force_buff += rhs_oper;
        force_buff += lhs_asgn;
    }
    force_buff += "}\n";
}

void pint_add_force_lpm_into_rhs_sen(const set<pint_sig_info_t> *rhs_sig, unsigned lpm_idx) {
    set<pint_sig_info_t>::iterator iter;
    pint_sig_info_t sig_info;
    for (iter = rhs_sig->begin(); iter != rhs_sig->end(); iter++) {
        sig_info = *iter;
        pint_sig_info_add_lpm(sig_info, 0xffffffff, lpm_idx);
    }
}

void pint_show_stmt_signature(string &buff, ivl_statement_t net) {
    if (!net)
        return;
    buff += string_format("//def:%s:%d\n", ivl_stmt_file(net), ivl_stmt_lineno(net));
}

void pint_parse_new_force_lhs(pint_force_lhs_t force_lhs, pint_force_lval_t lval_info,
                              const char *rhs, unsigned rhs_width, unsigned rhs_base,
                              unsigned thread_id, unsigned type = 0) {
    unsigned lhs_idx = force_lhs->lhs_idx;
    pint_sig_info_t sig_info = lval_info->sig_info;
    unsigned width = sig_info->width;
    unsigned cnt = (width + 0x1f) >> 5;
    unsigned word = lval_info->word;
    unsigned updt_base = lval_info->updt_base;
    unsigned updt_size = lval_info->updt_size;
    pint_force_info_t force_info = pint_find_signal_force_info(sig_info, word);
    string sig_name = pint_get_force_sig_name(sig_info, word);
    string mask = string_format("fmask_%u_", thread_id) + sig_name; //  Only used by part_force
    string update = string_format("pint_out_%u", lhs_idx);
    const char *sn = sig_name.c_str();
    const char *sm = mask.c_str();
    const char *updt = update.c_str();
    unsigned i;
    //  define
    if (width <= 4) {
        force_lhs->define = string_format("\tunsigned char %s;\n", updt);
    } else {
        force_lhs->define = string_format("\tunsigned int  %s[%u];\n", updt, cnt * 2);
    }
    //  assign
    if (!updt_base && !rhs_base && width == updt_size && rhs_width == updt_size) {
        if (width <= 4) {
            force_lhs->asgn += string_format("\t\t%s = %s;\n", updt, rhs);
        } else {
            force_lhs->asgn += string_format("\t\tpint_copy_int(%s, %s, %u);\n", updt, rhs, width);
        }
    } else {
        if (width <= 4) {
            force_lhs->asgn += string_format("\t\t%s = 0;\n", updt);
            if (rhs_width <= 4) {
                force_lhs->asgn += string_format("\t\tpint_copy_char_char(&%s, %u, &%s, %u, %u);\n",
                                                 rhs, rhs_base, updt, updt_base, updt_size);
            } else {
                force_lhs->asgn +=
                    string_format("\t\tpint_copy_int_char(%s, %u, %u, &%s, %u, %u);\n", rhs,
                                  rhs_width, rhs_base, updt, updt_base, updt_size);
            }
        } else {
            force_lhs->asgn += string_format("\t\tpint_set_int(%s, %u, 0);\n", updt, width);
            if (rhs_width <= 4) {
                force_lhs->asgn +=
                    string_format("\t\tpint_copy_char_int(&%s, %u, %s, %u, %u, %u);\n", rhs,
                                  rhs_base, updt, width, updt_base, updt_size);
            } else {
                force_lhs->asgn +=
                    string_format("\t\tpint_copy_int_int(%s, %u, %u, %s, %u, %u, %u);\n", rhs,
                                  rhs_width, rhs_base, updt, width, updt_base, updt_size);
            }
        }
    }
    //  condition
    if (force_info->part_force) {
        if (width <= 32) {
            force_lhs->cond = mask;
        } else {
            for (i = 0; i < cnt - 1; i++) {
                force_lhs->cond += string_format("%s[%u] || ", sm, i);
            }
            force_lhs->cond += string_format("%s[%u]", sm, cnt - 1);
        }
    } else {
        if (type) {
            if (force_info->force_idx.size() > 0) {
                if (!force_info->mult_assign) {
                    force_lhs->cond = string_format("assign_en_%s && (force_en_%s == 0)", sn, sn);
                } else {
                    force_lhs->cond = string_format(
                        "(assign_en_%s  && (force_en_%s == 0) && assign_thread_%s == pint_lpm_%u)",
                        sn, sn, sn, thread_id);
                }
            } else {
                if (!force_info->mult_assign) {
                    force_lhs->cond = string_format("assign_en_%s", sn);
                } else {
                    force_lhs->cond = string_format(
                        "(assign_en_%s && assign_thread_%s == pint_lpm_%u)", sn, sn, thread_id);
                }
            }
        } else {
            if (!force_info->mult_force) {
                force_lhs->cond = string_format("force_en_%s", sn);
            } else {
                force_lhs->cond = string_format("(force_en_%s && force_thread_%s == pint_lpm_%u)",
                                                sn, sn, thread_id);
            }
        }
    }
    //  update
    pint_gen_force_lpm_update(force_lhs->upate, sig_info, word, update, mask);
}

void pint_parse_exists_force_lhs(pint_force_lhs_t force_lhs, pint_force_lval_t lval_info,
                                 const char *rhs, unsigned rhs_width, unsigned rhs_base) {
    unsigned lhs_idx = force_lhs->lhs_idx;
    pint_sig_info_t sig_info = lval_info->sig_info;
    unsigned width = sig_info->width;
    unsigned word = lval_info->word;
    unsigned updt_base = lval_info->updt_base;
    unsigned updt_size = lval_info->updt_size;
    pint_force_info_t force_info = pint_find_signal_force_info(sig_info, word);
    assert(force_info->part_force);
    if (width <= 4) {
        force_lhs->asgn += string_format("\t\tpint_out_%u = 0;\n", lhs_idx);
        if (rhs_width <= 4) {
            force_lhs->asgn +=
                string_format("\t\tpint_copy_char_char(&%s, %u, &pint_out_%u, %u, %u);\n", rhs,
                              rhs_base, lhs_idx, updt_base, updt_size);
        } else {
            force_lhs->asgn +=
                string_format("\t\tpint_copy_int_char(%s, %u, %u, &pint_out_%u, %u, %u);\n", rhs,
                              rhs_width, rhs_base, lhs_idx, updt_base, updt_size);
        }
    } else {
        force_lhs->asgn += string_format("\t\tpint_set_int(pint_out_%u, %u, 0);\n", lhs_idx, width);
        if (rhs_width <= 4) {
            force_lhs->asgn +=
                string_format("\t\tpint_copy_char_int(&%s, %u, pint_out_%u, %u, %u, %u);\n", rhs,
                              rhs_base, lhs_idx, width, updt_base, updt_size);
        } else {
            force_lhs->asgn +=
                string_format("\t\tpint_copy_int_int(%s, %u, %u, pint_out_%u, %u, %u, %u);\n", rhs,
                              rhs_width, rhs_base, lhs_idx, width, updt_base, updt_size);
        }
    }
}

void pint_gen_lpm_head(string &buff, string &decl_buff, unsigned thread_id) {
    decl_buff += string_format("void pint_lpm_%u(void);\n", thread_id);
    buff += string_format("void pint_lpm_%u(void){\n", thread_id);
    if (pint_perf_on) {
        unsigned lpm_perf_id = thread_id - pint_event_num + PINT_PERF_BASE_ID;
        buff += string_format("\tNCORE_PERF_SUMMARY(%d);\n", lpm_perf_id);
    }
    buff += string_format("\tpint_enqueue_flag_clear(%d);\n", thread_id);
}

void pint_gen_force_lpm_update(string &buff, pint_sig_info_t sig_info, unsigned word, string source,
                               string mask) {
    unsigned width = sig_info->width;
    pint_force_info_t force_info = pint_find_signal_force_info(sig_info, word);
    string str_value = pint_get_sig_value_name(sig_info, word);
    const char *sv = str_value.c_str();
    const char *sn = sig_info->sig_name.c_str();
    const char *sm = mask.c_str();
    const char *src = source.c_str();
    if (force_info->part_force) {
        if (width <= 4) {
            buff += string_format("\tif((%s ^ %s) & %s){\n", sv, src, sm);

            if (sig_info->sens_process_list.size()) {
                buff += string_format("\t\tpint_set_event_flag(p_list_%s);\n", sn);
            }

            if (pint_signal_has_sen_evt(sig_info, word)) {
                if (width == 1) {
                    buff += string_format("\t\tchange_flag = edge_1bit(%s, %s);\n"
                                      "\t\tpint_mask_copy_char(&%s, &%s, &%s);\n",
                                      sv, src, sv, src, sm);
                } else {
                    buff += string_format("\t\tchange_flag = edge_char(%s & %s, %s & %s);\n"
                                      "\t\tpint_mask_copy_char(&%s, &%s, &%s);\n",
                                      sv, sm, src, sm, sv, src, sm);
                }

                buff += pint_gen_sig_e_list_code(sig_info, "change_flag", word) + "\n";
            } else {
                buff += string_format("\t\tpint_mask_copy_char(&%s, &%s, &%s);\n", sv, src, sm);
            }
        } else if (width <= 32) {
            buff += string_format("\tif(!pint_mask_case_equality_int(%s, %s, &%s, %u)){\n", sv, src,
                                  sm, width);

            if (sig_info->sens_process_list.size()) {
                buff += string_format("\t\tpint_set_event_flag(p_list_%s);\n", sn);
            }

            if (pint_signal_has_sen_evt(sig_info, word)) {
                buff += string_format("\t\tif(%s & 0x1)  change_flag = edge_int(%s, %s, %u);\n"
                                      "\t\telse          change_flag = pint_nonedge;\n"
                                      "\t\tpint_mask_copy_int(%s, %s, &%s, %u);\n",
                                      sm, sv, src, width, sv, src, sm, width);
                buff += pint_gen_sig_e_list_code(sig_info, "change_flag", word) + "\n";
            } else {
                buff +=
                    string_format("\t\tpint_mask_copy_int(%s, %s, &%s, %u);\n", sv, src, sm, width);
            }
        } else {
            buff += string_format("\tif(!pint_mask_case_equality_int(%s, %s, %s, %u)){\n", sv, src,
                                  sm, width);
            if (sig_info->sens_process_list.size()) {
                buff += string_format("\t\tpint_set_event_flag(p_list_%s);\n", sn);
            }

            if (pint_signal_has_sen_evt(sig_info, word)) {
                buff += string_format("\t\tif(%s[0] & 0x1)  change_flag = edge_int(%s, %s, %u);\n"
                                      "\t\telse             change_flag = pint_nonedge;\n"
                                      "\t\tpint_mask_copy_int(%s, %s, %s, %u);\n",
                                      sm, sv, src, width, sv, src, sm, width);
                buff += pint_gen_sig_e_list_code(sig_info, "change_flag", word) + "\n";
            } else {
                buff +=
                    string_format("\t\tpint_mask_copy_int(%s, %s, %s, %u);\n", sv, src, sm, width);
            }
        }
    } else {
        if (width <= 4) {
            buff += string_format("\tif(%s != %s){\n", sv, src);
            if (sig_info->sens_process_list.size()) {
                buff += string_format("\t\tpint_set_event_flag(p_list_%s);\n", sn);
            }

            if (pint_signal_has_sen_evt(sig_info, word)) {
                if (width == 1) {
                    buff += string_format("\t\tchange_flag = edge_1bit(%s, %s);\n"
                                      "\t\t%s = %s;\n",
                                      sv, src, sv, src);
                } else {
                    buff += string_format("\t\tchange_flag = edge_char(%s, %s);\n"
                                      "\t\t%s = %s;\n",
                                      sv, src, sv, src);
                }
                buff += pint_gen_sig_e_list_code(sig_info, "change_flag", word) + "\n";
            } else {
                buff += string_format("\t\t%s = %s;\n", sv, src);
            }
        } else {
            buff +=
                string_format("\tif(!pint_case_equality_int_ret(%s, %s, %u)){\n", sv, src, width);

            if (sig_info->sens_process_list.size()) {
                buff += string_format("\t\tpint_set_event_flag(p_list_%s);\n", sn);
            }

            if (pint_signal_has_sen_evt(sig_info, word)) {
                buff += string_format("\t\tchange_flag = edge_int(%s, %s, %u);\n"
                                      "\t\tpint_copy_int(%s, %s, %u);\n",
                                      sv, src, width, sv, src, width);
                buff += pint_gen_sig_e_list_code(sig_info, "change_flag", word) + "\n";
            } else {
                buff += string_format("\t\tpint_copy_int(%s, %s, %u);\n", sv, src, width);
            }
        }
    }
    if (pint_signal_has_sen_lpm(sig_info, word)) {
        buff += pint_gen_sig_l_list_code(sig_info, word) + "\n";
    }
    buff += "\t}\n";
}

//  Dump force shadow lpm
void pint_parse_all_force_shadow_lpm() {
    unsigned num = force_net_undrived_by_lpm.size();
    pint_sig_info_t shadow_info;
    pint_sig_info_t sig_info;
    unsigned word;
    unsigned force_idx;
    unsigned i;
    map<unsigned, unsigned>::iterator iter;
    for (i = 0; i < num; i++) {
        sig_info = force_net_undrived_by_lpm[i];
        for (iter = force_net_undrived_by_lpm_map[sig_info].begin();
             iter != force_net_undrived_by_lpm_map[sig_info].end(); iter++) {
            word = iter->first;
            force_idx = iter->second;
            shadow_info = pint_gen_force_shadow_sig_info(sig_info, word);
            pint_show_force_shadow_lpm(sig_info, word, force_idx);
            //  shadow_info can not be an arr
            pint_sig_info_add_lpm(shadow_info, 0, force_idx + FORCE_LPM_OFFSET);
        }
    }
    force_net_undrived_by_lpm.clear();
}

void pint_show_force_shadow_lpm(pint_sig_info_t sig_info, unsigned word, unsigned force_idx) {
    pint_force_info_t force_info = pint_find_signal_force_info(sig_info, word);
    unsigned thread_id = force_idx + FORCE_THREAD_OFFSET;
    unsigned width = sig_info->width;
    unsigned cnt = (width + 0x1f) >> 5;
    string sig_name = pint_get_force_sig_name(sig_info, word);
    string mask = "umask_" + sig_name;
    string str_cond;
    unsigned i;
    pint_gen_lpm_head(force_app_buff, force_app_decl_buff, thread_id);
    if (pint_signal_has_sen_evt(sig_info, word)) {
        force_app_buff += "\tint  change_flag;\n";
    }
    if (force_info->part_force) {
        if (width <= 32) {
            str_cond = "!" + mask;
        } else {
            str_cond = "!(";
            for (i = 0; i < cnt - 1; i++) {
                str_cond += mask + string_format("[%u] || ", i);
            }
            str_cond += mask + string_format("[%u])", cnt - 1);
        }
    } else {
        str_cond = "force_en_" + sig_name;
    }
    force_app_buff += string_format("\tif(%s)  return;\n", str_cond.c_str());
    pint_gen_force_lpm_update(force_app_buff, sig_info, word, "force_" + sig_name,
                              "umask_" + sig_name);
    force_app_buff += "}\n";
}

void pint_parse_all_force_const_lpm() {
    unsigned num = force_net_is_const.size();
    pint_sig_info_t sig_info;
    unsigned word;
    unsigned force_idx;
    unsigned i;
    map<unsigned, unsigned>::iterator iter;
    for (i = 0; i < num; i++) {
        sig_info = force_net_is_const[i];
        for (iter = force_net_is_const_map[sig_info].begin();
             iter != force_net_is_const_map[sig_info].end(); iter++) {
            word = iter->first;
            force_idx = iter->second;
            pint_sig_info_set_const(sig_info, word, 0);
            pint_show_force_const_lpm(sig_info, word, force_idx);
        }
    }
    force_net_is_const.clear();
}

void pint_show_force_const_lpm(pint_sig_info_t sig_info, unsigned word, unsigned force_idx) {
    pint_force_info_t force_info = pint_find_signal_force_info(sig_info, word);
    unsigned thread_id = force_idx + FORCE_THREAD_OFFSET;
    unsigned width = sig_info->width;
    unsigned cnt = (width + 0x1f) >> 5;
    string sig_name = pint_get_force_sig_name(sig_info, word);
    string mask = "umask_" + sig_name;
    string value;
    string str_cond;
    unsigned i;
    pint_ret_signal_const_value(value, sig_info, word);
    pint_gen_lpm_head(force_app_buff, force_app_decl_buff, thread_id);
    if (width <= 4) {
        force_app_buff += "\tunsigned char value = ";
    } else {
        force_app_buff += string_format("\tunsigned int  value[%u] = ", cnt * 2);
    }
    force_app_buff += value + ";\n";
    if (pint_signal_has_sen_evt(sig_info, word)) {
        force_app_buff += "\tint  change_flag;\n";
    }
    if (force_info->part_force) {
        if (width <= 32) {
            str_cond = "!" + mask;
        } else {
            str_cond = "!(";
            for (i = 0; i < cnt - 1; i++) {
                str_cond += mask + string_format("[%u] || ", i);
            }
            str_cond += mask + string_format("[%u])", cnt - 1);
        }
    } else {
        str_cond = "force_en_" + sig_name;
    }
    force_app_buff += string_format("\tif(%s)  return;\n", str_cond.c_str());
    pint_gen_force_lpm_update(force_app_buff, sig_info, word, "value", "umask_" + sig_name);
    force_app_buff += "}\n";
}

/*******************< Process find sig_init >*******************/
int pint_init_is_sig_init(ivl_process_t net) {
    if (ivl_process_type(net) == IVL_PR_INITIAL) {
        string sfile;
        pint_source_file_ret(sfile, ivl_process_file(net), ivl_process_lineno(net));
        if (sfile.find("module") != string::npos)
            return 1;
    }
    return 0;
}

void pint_assign_show_sig_init(string &buff, ivl_statement_t net) {
    ivl_lval_t lval = ivl_stmt_lval(net, 0);
    ivl_signal_t sig = ivl_lval_sig(lval);
    pint_sig_info_t sig_info = pint_find_signal(sig);
    const char *sn = sig_info->sig_name.c_str();
    ivl_expr_t rval = ivl_stmt_rval(net);
    unsigned width_num = ivl_expr_width(rval);
    use_const0_buff = 0;

    buff += "\t";
    if (ivl_expr_type(rval) == IVL_EX_NUMBER) {
        const char *bits = ivl_expr_bits(rval);
        pint_show_ex_number(buff, buff, NULL, NULL, sn, bits, width_num, EX_NUM_ASSIGN_SIG,
                            ivl_stmt_file(net), ivl_stmt_lineno(net));
    } else if (ivl_expr_type(rval) == IVL_EX_STRING) {
        const char *str = ivl_expr_string(rval);
        int str_num = strlen(str);
        char *bits = (char *)malloc(str_num * 8 + 1);
        char *bits_walk = bits;
        for (int bytes_i = str_num - 1; bytes_i >= 0; bytes_i--) {
            char char_tmp = str[bytes_i];
            for (int bit_i = 0; bit_i < 8; bit_i++) {
                *bits_walk++ = (char_tmp >> bit_i & 1) ? '1' : '0';
            }
        }
        *bits_walk = 0;
        pint_show_ex_number(buff, buff, NULL, NULL, sn, bits, width_num, EX_NUM_ASSIGN_SIG,
                            ivl_stmt_file(net), ivl_stmt_lineno(net));
    } else {
        PINT_UNSUPPORTED_ERROR(ivl_expr_file(rval), ivl_expr_lineno(rval),
                               "initial sig with expr type %d", ivl_expr_type(rval));
    }
    use_const0_buff = 1;
}

void pint_init_show_sig_init(string &buff, ivl_process_t net) {
    ivl_statement_t sub_stmt = ivl_process_stmt(net);
    ivl_statement_type_t sub_type = ivl_statement_type(sub_stmt);
    switch (sub_type) {
    case IVL_ST_ASSIGN:
        pint_assign_show_sig_init(buff, sub_stmt);
        break;
    case IVL_ST_BLOCK: {
        unsigned int nsig = ivl_stmt_block_count(sub_stmt);
        unsigned i;
        for (i = 0; i < nsig; i++) {
            pint_assign_show_sig_init(buff, ivl_stmt_block_stmt(sub_stmt, i));
        }
        break;
    }
    default:
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(sub_stmt), ivl_stmt_lineno(sub_stmt),
                               "process type not supported. Type_id: %u.", sub_type);
        break;
    }
}

// nxz
void pint_proc_sig_nxz_clear() { proc_sig_nxz.clear(); }

bool pint_proc_sig_nxz_get(ivl_statement_t stmt, ivl_signal_t sig) {
    if (proc_sig_nxz.count(stmt)) {
        if (proc_sig_nxz[stmt].count(sig)) {
            return true;
        }
    }
    return false;
}

int analysis_sig_nxz(const ivl_process_t net, dfa_sig_nxz_t *p_sig_nxz) {
    analysis_process(ivl_process_stmt(net), 0, &proc_info, 0);
    dfa_sig_nxz_t &t_sig_nxz = *p_sig_nxz;
    for (int i = 0; i < proc_info.n_stmt; i++) {
        ivl_statement_t key = proc_info.stmt_list[i];
        sig_anls_result_t p_sig_anls_result = proc_info.stmt_in[i];
        for (int j = 0; j < proc_info.n_sig; j++) {
            if (p_sig_anls_result[j].binary == 1) {
                t_sig_nxz[key].insert(proc_info.sig_list[j]);
            }
        }
    }
    analysis_free_process_info(&proc_info);
    return 0;
}

/******************************************************************************** Std length -100 */
//  asgn_count in dsgn      --function
map<ivl_process_t, set<ivl_lval_t>> proc_asgn;       // asgn in process
map<ivl_process_t, set<ivl_scope_t>> proc_task_call; // function/task call in process
map<ivl_scope_t, set<ivl_lval_t>> task_asgn;         // asgn in task
map<ivl_scope_t, set<ivl_scope_t>> task_task_call;   // function/task call in task/function

map<ivl_process_t, set<ivl_signal_t>> proc_part_asgn;
map<pint_sig_info_t, set<ivl_process_t>> sig_asgn_proc;
map<pint_sig_info_t, map<unsigned, set<ivl_process_t>>> arr_asgn_proc;

void pint_parse_asgn_lval(ivl_lval_t lval, pint_loop_stmt_t stmt_env_t) {
    ivl_process_t temp_proc = stmt_env_t->temp_proc;
    if (temp_proc) {
        proc_asgn[temp_proc].insert(lval);
    } else {
        ivl_scope_t temp_scope = stmt_env_t->temp_scope;
        if (temp_scope) {
            task_asgn[temp_scope].insert(lval);
        }
    }
}

void pint_parse_task_call(ivl_scope_t task, pint_loop_stmt_t stmt_env_t) {
    ivl_process_t temp_proc = stmt_env_t->temp_proc;
    if (temp_proc) {
        proc_task_call[temp_proc].insert(task);
    } else {
        ivl_scope_t temp_scope = stmt_env_t->temp_scope;
        if (temp_scope && temp_scope != task) {
            task_task_call[temp_scope].insert(task);
        }
    }
}

void pint_parse_func_call(ivl_scope_t func, pint_loop_stmt_t stmt_env_t) {
    ivl_process_t temp_proc = stmt_env_t->temp_proc;
    if (temp_proc) {
        proc_task_call[temp_proc].insert(func);
    } else {
        ivl_scope_t temp_scope = stmt_env_t->temp_scope;
        if (temp_scope && temp_scope != func) {
            task_task_call[temp_scope].insert(func);
        }
    }
}

set<ivl_scope_t> task_parsed_calls;
void pint_clct_asgn_in_task(ivl_scope_t net) {
    if (task_task_call.count(net)) {
        set<ivl_scope_t>::iterator i;
        ivl_scope_t sub;
        for (i = task_task_call[net].begin(); i != task_task_call[net].end(); i++) {
            sub = *i;
            if (!task_parsed_calls.count(sub)) {
                pint_clct_asgn_in_task(sub);
            }
            if (task_asgn[sub].size()) {
                set<ivl_lval_t>::iterator j;
                for (j = task_asgn[sub].begin(); j != task_asgn[sub].end(); j++) {
                    task_asgn[net].insert(*j);
                }
            }
        }
    }
    task_parsed_calls.insert(net);
}

void pint_clct_asgn_in_thread() {
    set<ivl_process_t>::iterator i;
    set<ivl_scope_t>::iterator j;
    set<ivl_lval_t>::iterator k;
    ivl_process_t proc;
    ivl_scope_t task;
    for (i = dsgn_proc_is_thread.begin(); i != dsgn_proc_is_thread.end(); i++) {
        proc = *i;
        if (proc_task_call.count(proc)) {
            for (j = proc_task_call[proc].begin(); j != proc_task_call[proc].end(); j++) {
                task = *j;
                if (!task_parsed_calls.count(task)) {
                    pint_clct_asgn_in_task(task);
                }
                if (task_asgn[task].size()) {
                    for (k = task_asgn[task].begin(); k != task_asgn[task].end(); k++) {
                        proc_asgn[proc].insert(*k);
                    }
                }
            }
        }
    }
    proc_task_call.clear();
    task_task_call.clear();
    task_parsed_calls.clear();
}

void pint_parse_dsgn_signal_asgn_info() {
    set<ivl_process_t>::iterator i;
    set<ivl_lval_t>::iterator j;
    ivl_process_t proc;
    ivl_lval_t lval;
    ivl_signal_t sig;
    pint_sig_info_t sig_info;
    unsigned word;
    for (i = dsgn_proc_is_thread.begin(); i != dsgn_proc_is_thread.end(); i++) {
        proc = *i;
        if (proc_asgn.count(proc)) {
            for (j = proc_asgn[proc].begin(); j != proc_asgn[proc].end(); j++) {
                lval = *j;
                sig = ivl_lval_sig(lval);
                if (!sig) {
                    continue;
                }
                if (!pint_asgn_is_full(lval)) {
                    proc_part_asgn[proc].insert(sig);
                }
                sig_info = pint_find_signal(sig);
                if (sig_info->arr_words == 1) {
                    sig_asgn_proc[sig_info].insert(proc);
                } else {
                    word = pint_get_arr_idx_from_sig(sig);
                    if (word == 0xffffffff) {
                        word = pint_get_arr_idx_value(ivl_lval_idx(lval));
                    }
                    arr_asgn_proc[sig_info][word].insert(proc);
                }
            }
        }
    }
    proc_asgn.clear();
    task_asgn.clear();
}

//  asgn_count in dsgn      --API
void pint_print_signal_asgn_info(pint_sig_info_t sig_info) {
    printf(">>  asgn_info of '%s':\n", sig_info->sig_name.c_str());
    if (sig_info->arr_words == 1) {
        if (sig_asgn_proc.count(sig_info)) {
            set<ivl_process_t>::iterator i;
            ivl_process_t proc;
            for (i = sig_asgn_proc[sig_info].begin(); i != sig_asgn_proc[sig_info].end(); i++) {
                proc = *i;
                printf("    %s[%u]\n", ivl_process_file(proc), ivl_process_lineno(proc));
            }
        } else {
            printf("    None\n");
        }
    } else {
        if (arr_asgn_proc.count(sig_info)) {
            map<unsigned, set<ivl_process_t>>::iterator i;
            set<ivl_process_t>::iterator j;
            unsigned word;
            ivl_process_t proc;
            for (i = arr_asgn_proc[sig_info].begin(); i != arr_asgn_proc[sig_info].end(); i++) {
                word = i->first;
                printf("    Word-0x%x:\n", word);
                for (j = i->second.begin(); j != i->second.end(); j++) {
                    proc = *j;
                    printf("        %s[%u]\n", ivl_process_file(proc), ivl_process_lineno(proc));
                }
            }
        } else {
            printf("    None\n");
        }
    }
}

bool pint_signal_asgn_in_mult_threads(ivl_signal_t sig) {
    pint_sig_info_t sig_info = pint_find_signal(sig);
    if (sig_info->arr_words == 1) {
        if (sig_asgn_proc[sig_info].size() > 1) {
            return true;
        } else {
            return false;
        }
    } else {
        if (arr_asgn_proc[sig_info].count(0xffffffff)) {
            if (arr_asgn_proc[sig_info][0xffffffff].size() > 1) {
                return true;
            } else {
                ivl_process_t any_thread = *(arr_asgn_proc[sig_info][0xffffffff].begin());
                map<unsigned, set<ivl_process_t>>::iterator i;
                unsigned word;
                for (i = arr_asgn_proc[sig_info].begin(); i != arr_asgn_proc[sig_info].end(); i++) {
                    word = i->first;
                    if (word == 0xffffffff) {
                        continue;
                    }
                    if (i->second.size() > 1 || *(i->second.begin()) != any_thread) {
                        return true;
                    }
                }
            }
        } else {
            map<unsigned, set<ivl_process_t>>::iterator i;
            for (i = arr_asgn_proc[sig_info].begin(); i != arr_asgn_proc[sig_info].end(); i++) {
                if (i->second.size() > 1) {
                    return true;
                }
            }
        }
        return false;
    }
}

bool pint_signal_is_part_asgn_in_thread(ivl_signal_t sig, ivl_process_t proc) {
    if (proc_part_asgn[proc].count(sig)) {
        return true;
    } else {
        return false;
    }
}

/******************************************************************************** Std length -100 */
//  mult_asgn in process  --function
void pint_proc_mult_info_init() {
    if (!temp_mult) {
        return;
    }
    map<string, ivl_signal_t> sig_has_tmp;   //  ordered by sig_name
    map<string, ivl_signal_t> sig_needs_chk; //  ordered by sig_name
    std::unordered_map<ivl_signal_t, unsigned>::iterator i;
    ivl_signal_t sig;
    mult_asgn_type_t asgn_type;
    pint_sig_info_t sig_info;
    for (i = temp_mult->p_cnt.begin(); i != temp_mult->p_cnt.end(); i++) {
        sig = i->first;
        asgn_type = pint_proc_get_sig_mult_type(sig);
        sig_info = pint_find_signal(sig);
        // if (sig_info->sig_name == "") { //  debug
        //     pint_show_sig_mult_anls_info(sig);
        // }
        switch (asgn_type) {
        case MULT_SIG_2:
            sig_has_tmp[sig_info->sig_name] = sig;
            break;
        case MULT_SIG_3:
            sig_has_tmp[sig_info->sig_name] = sig;
            sig_needs_chk[sig_info->sig_name] = sig;
            break;
        case MULT_ARR_23:
            sig_has_tmp[sig_info->sig_name] = sig;
            sig_needs_chk[sig_info->sig_name] = sig;
            break;
        default:
            break;
        }
    }
    std::unordered_map<ivl_signal_t, unsigned int> &chk_down = temp_mult->p_down[NULL];
    std::unordered_map<ivl_signal_t, unsigned int>::iterator j;
    if (chk_down.size()) {
        for (j = chk_down.begin(); j != chk_down.end(); j++) {
            sig = j->first;
            sig_info = pint_find_signal(sig);
            if (pint_proc_get_sig_mult_type(sig) == MULT_SIG_2) {
                sig_needs_chk[sig_info->sig_name] = sig;
            }
        }
    }
    unsigned idx = 1; //  mult_idx start with 1
    map<string, ivl_signal_t>::iterator k;
    map<ivl_signal_t, unsigned> &tgt0 = temp_proc->sig_to_tmp_id;
    vector<ivl_signal_t> &tgt1 = temp_proc->proc_sig_has_tmp;
    vector<ivl_signal_t> &tgt2 = temp_proc->proc_sig_needs_chk;
    for (k = sig_has_tmp.begin(); k != sig_has_tmp.end(); k++) {
        sig = k->second;
        tgt0[sig] = idx++;
        tgt1.push_back(sig);
    }
    if (sig_needs_chk.size()) {
        temp_proc->chk_before_exit = 1;
        for (k = sig_needs_chk.begin(); k != sig_needs_chk.end(); k++) {
            sig = k->second;
            tgt2.push_back(sig);
        }
    }
}

void pint_proc_mult_asgn_show_signal_define(string &buff) {
    vector<ivl_signal_t> &tgt = temp_proc->proc_sig_has_tmp;
    unsigned num = tgt.size();
    ivl_signal_t sig;
    pint_sig_info_t sig_info;
    const char *sn;
    mult_asgn_type_t asgn_type;
    unsigned arr_words;
    unsigned arr_idx;
    unsigned width;
    unsigned cnt2;
    unsigned i;
    for (i = 0; i < num;) {
        sig = tgt[i++]; //  The idx of mult_xx start from 1
        asgn_type = pint_proc_get_sig_mult_type(sig);
        sig_info = pint_find_signal(sig);
        width = sig_info->width;
        sn = sig_info->sig_name.c_str();
        if (width <= 4) {
            switch (asgn_type) {
            case MULT_SIG_2:
                buff += string_format("\tunsigned char TMP%u;\n", i);
                break;
            case MULT_SIG_3:
                if (sig_info->arr_words == 1) {
                    buff += string_format("\tunsigned char TMP%u = %s;\n", i, sn);
                } else { //  case: sig_words = 1, arr_words > 1
                    arr_idx = pint_get_arr_idx_from_sig(sig);
                    buff += string_format("\tunsigned char TMP%u = %s[%u];\n", i, sn, arr_idx);
                }
                break;
            case MULT_ARR_23:
                arr_words = sig_info->arr_words;
                buff += string_format("\tunsigned char TMP%u[%u];\n", i, arr_words);
                buff += string_format("\tunsigned char FLG%u[%u] = {0};\n", i, arr_words);
                buff += string_format("\tunsigned int UPDT%u[4] = {0};\n", i);
                break;
            default:
                break;
            }
        } else {
            cnt2 = ((width + 0x1f) >> 5) * 2;
            switch (asgn_type) {
            case MULT_SIG_2:
                buff += string_format("\tunsigned int  TMP%u[%u];\n", i, cnt2);
                break;
            case MULT_SIG_3:
                buff += string_format("\tunsigned int  TMP%u[%u];\n", i, cnt2);
                if (sig_info->arr_words == 1) {
                    buff += string_format("\tpint_copy_int(TMP%u, %s, %u);\n", i, sn, width);
                } else {
                    arr_idx = pint_get_arr_idx_from_sig(sig);
                    buff += string_format("\tpint_copy_int(TMP%u, %s[%u], %u);\n", i, sn, arr_idx,
                                          width);
                }
                break;
            case MULT_ARR_23:
                arr_words = sig_info->arr_words;
                buff += string_format("\tunsigned int  TMP%u[%u][%u];\n", i, arr_words, cnt2);
                buff += string_format("\tunsigned char FLG%u[%u] = {0};\n", i, arr_words);
                buff += string_format("\tunsigned int UPDT%u[4] = {0};\n", i);
                break;
            default:
                break;
            }
        }
        if (pint_proc_mult_sig_needs_mask(sig)) {
            // printf("Note:  --find mult sig needs mask!!!\n");
            // pint_print_signal_asgn_info(sig_info);
            if (width <= 32) {
                buff += string_format("\tunsigned int  MASK%u = 0;\n", i);
            } else {
                unsigned cnt = (width + 0x1f) >> 5;
                buff += string_format("\tunsigned int  MASK%u[%u] = {0};\n", i, cnt);
            }
        }
    }
    pint_parse_mult_up_oper(buff, NULL, 0);
    tgt.clear();
}

void pint_proc_mult_gen_exit_code(string &buff) {
    if (!temp_proc->chk_before_exit) {
        return;
    }
    vector<ivl_signal_t> &tgt = temp_proc->proc_sig_needs_chk;
    unsigned num = tgt.size();
    unsigned i;
    buff += "PC_END:\n\t;\n";
    for (i = 0; i < num; i++) {
        pint_proc_updt_mult_asgn(buff, tgt[i]);
    }
}

/******************************************************************************** Std length -100 */
//  Multi-thread used on "show_process"

//  function
void pint_alloc_mult_anls_task(unsigned proc_num) {
    unsigned idx = 0;
    unsigned all_finished;
    unsigned i;
    while (idx < proc_num) {
        pthread_mutex_lock(&par_lock_wait_proc);
        while (par_mult_anls_ret.size() >= PINT_PAR_ANLS_NUM) {
            pthread_cond_wait(&par_cond_wait_proc, &par_lock_wait_proc);
        }
        par_task_num++;
        pthread_mutex_unlock(&par_lock_wait_proc);
        pthread_mutex_lock(&par_lock_wait_task);
        par_anls_task.push(idx++);
        pthread_cond_signal(&par_cond_wait_task);
        pthread_mutex_unlock(&par_lock_wait_task);
    }
    par_end_task = 1;
    do {
        pthread_mutex_lock(&par_lock_wait_task);
        pthread_cond_broadcast(&par_cond_wait_task);
        pthread_mutex_unlock(&par_lock_wait_task);
        usleep(1000);
        all_finished = 1;
        for (i = 0; i < PINT_PAR_ANLS_NUM; i++) {
            all_finished &= par_anls_sts[i];
        }
    } while (!all_finished);
}

void *pint_par_mult_anls(void *x) {
    unsigned *sts = (unsigned *)x;
    unsigned idx;
    while (1) {
        pthread_mutex_lock(&par_lock_wait_task);
        while (!par_anls_task.size() && !par_end_task) {
            pthread_cond_wait(&par_cond_wait_task, &par_lock_wait_task);
        }
        if (par_anls_task.size()) {
            idx = par_anls_task.front();
            par_anls_task.pop();
            pthread_mutex_unlock(&par_lock_wait_task);
        } else { //  par_end_task = 1
            *sts = 1;
            pthread_mutex_unlock(&par_lock_wait_task);
            pthread_exit(NULL);
        }
        ivl_process_t net = dsgn_all_proc[idx];
        pint_mult_info_t mult_anls_ret = new struct pint_mult_info_s;
        int mult_anls_sts = analysis_massign_v2(net, &mult_anls_ret->p_down, &mult_anls_ret->p_up,
                                                &mult_anls_ret->p_cnt);
        pthread_mutex_lock(&par_lock_wait_anls);
        if (!mult_anls_sts) { //  mult anls succeed
            par_mult_anls_ret[idx] = mult_anls_ret;
        } else {
            par_mult_anls_ret[idx] = NULL;
            delete mult_anls_ret;
        }
        pthread_cond_signal(&par_cond_wait_anls);
        pthread_mutex_unlock(&par_lock_wait_anls);
    }
}

void *pint_par_show_process(void *x) {
    unsigned proc_num = dsgn_all_proc.size();
    unsigned idx = 0;
    pint_mult_info_t mult_anls_ret;
    (void)x;
    while (idx < proc_num) {
        pthread_mutex_lock(&par_lock_wait_anls);
        while (!par_mult_anls_ret.count(idx)) {
            pthread_cond_wait(&par_cond_wait_anls, &par_lock_wait_anls);
        }
        mult_anls_ret = par_mult_anls_ret[idx];
        par_mult_anls_ret.erase(idx);
        pthread_mutex_unlock(&par_lock_wait_anls);
        pthread_mutex_lock(&par_lock_wait_proc);
        par_task_num--;
        pthread_cond_signal(&par_cond_wait_proc);
        pthread_mutex_unlock(&par_lock_wait_proc);
        show_process(dsgn_all_proc[idx], mult_anls_ret);
        if (mult_anls_ret) {
            delete mult_anls_ret;
        }
        idx++;
    }

    return NULL;
}

//  Execution process
int pint_dsgn_clct_proc(ivl_process_t net, void *x) {
    (void)x;
    dsgn_all_proc.push_back(net);
    return 0;
}

void pint_dump_process() {
    unsigned proc_num = dsgn_all_proc.size();
    ivl_process_t net;
    unsigned i;
    if (PINT_MULT_ASGN_EN) {
        if (!PINT_PAR_MODE_EN) {
            int mult_anls_sts;
            for (i = 0; i < proc_num; i++) {
                struct pint_mult_info_s mult_anls;
                net = dsgn_all_proc[i];
                mult_anls_sts =
                    analysis_massign_v2(net, &mult_anls.p_down, &mult_anls.p_up, &mult_anls.p_cnt);
                if (!mult_anls_sts) { //  mult anls succeed
                    show_process(net, &mult_anls);
                } else {
                    show_process(net, NULL);
                }
            }
        } else {
            // map<unsigned, struct pint_mult_info_s> par_mult_anls_ret;
            pthread_t pid[PINT_PAR_ANLS_NUM + 1];
            int sub_sts = 0;
            pthread_mutex_init(&par_lock_wait_proc, NULL);
            pthread_mutex_init(&par_lock_wait_task, NULL);
            pthread_mutex_init(&par_lock_wait_anls, NULL);
            pthread_cond_init(&par_cond_wait_proc, NULL);
            pthread_cond_init(&par_cond_wait_task, NULL);
            pthread_cond_init(&par_cond_wait_anls, NULL);
            sub_sts |= pthread_create(&pid[PINT_PAR_ANLS_NUM], NULL, pint_par_show_process, NULL);
            for (i = 0; i < PINT_PAR_ANLS_NUM; i++) {
                sub_sts |= pthread_create(&pid[i], NULL, pint_par_mult_anls, &par_anls_sts[i]);
            }
            if (sub_sts) {
                printf("Error!  --create sub thread failed.\n");
                exit(0);
            }
            pint_alloc_mult_anls_task(proc_num);
            for (i = 0; i < PINT_PAR_ANLS_NUM + 1; i++) {
                pthread_join(pid[i], NULL);
            }
        }
    } else {
        for (i = 0; i < proc_num; i++) {
            show_process(dsgn_all_proc[i], NULL);
        }
    }
}

/******************************************************************************** Std length -100 */
//  event  --API
pint_event_info_t pint_find_event(ivl_event_t event) {
    if (pint_event.count(event)) {
        return pint_event[event];
    } else {
        return NULL;
    }
}

pint_event_info_t pint_find_event(unsigned idx) {
    if (pint_idx_to_evt_info.count(idx)) {
        return pint_idx_to_evt_info[idx];
    } else {
        return NULL;
    }
}

bool pint_is_evt_needs_dump(unsigned idx) {
    if (!pint_idx_to_evt_info.count(idx)) {
        return 0;
    }
    pint_event_info_t evt_info = pint_idx_to_evt_info[idx];
    if (evt_info->wait_count) {
        return 1;
    } else {
        return 0;
    }
}

bool pint_find_process_event(ivl_event_t event) {
    if (pint_process_event.count(event)) {
        return 1;
    } else {
        return 0;
    }
}

void pint_add_process_event(ivl_event_t evt) { pint_process_event.insert(evt); }

void pint_vcddump_var(pint_sig_info_t sig_info, string *buff) {
    if (sig_info->arr_words > 1) { // array unsuppport dumpvar
        return;
    }
    if (sig_info->is_dump_var) {
        if (sig_info->width <= 4) {
            *buff +=
                string_format("        pint_vcddump_char(%s, %d, \"%s\");\n",
                              sig_info->sig_name.c_str(), sig_info->width, sig_info->dump_symbol);
        } else {
            *buff +=
                string_format("        pint_vcddump_int(%s, %d, \"%s\");\n",
                              sig_info->sig_name.c_str(), sig_info->width, sig_info->dump_symbol);
        }
    }
}

void pint_vcddump_var_without_check(pint_sig_info_t sig_info, string *buff) {
    if (sig_info->arr_words > 1) { // array unsuppport dumpvar
        return;
    }
    if (sig_info->is_dump_var) {
        if (sig_info->width <= 4) {
            *buff +=
                string_format("        pint_vcddump_char_without_check(%s, %d, \"%s\");\n",
                              sig_info->sig_name.c_str(), sig_info->width, sig_info->dump_symbol);
        } else {
            *buff +=
                string_format("        pint_vcddump_int_without_check(%s, %d, \"%s\");\n",
                              sig_info->sig_name.c_str(), sig_info->width, sig_info->dump_symbol);
        }
    }
}

void pint_vcddump_off(pint_sig_info_t sig_info, string *buff) {
    if (sig_info->arr_words > 1) { // array unsuppport dumpvar
        return;
    }
    if (sig_info->is_dump_var) {
        *buff += string_format("        pint_vcddump_off(%d, \"%s\");\n", sig_info->width,
                               sig_info->dump_symbol);
    }
}

void pint_vcddump_init(string *buff) {
    *buff += "    pint_simu_vpi(PINT_DUMP_VAR, \"#0\");\n"
             "    pint_simu_vpi(PINT_DUMP_VAR, \"$dumpvars\");\n";
    map<ivl_nexus_t, pint_sig_info_t>::iterator iter;
    int idx = 0;
    for (iter = pint_nex_to_sig_info.begin(); iter != pint_nex_to_sig_info.end(); iter++) {
        pint_sig_info_t sig_info = iter->second;
        if (sig_info->is_func_io)
            continue;
        if ((idx % PINT_DUMP_FILE_LINE_NUM) == 0) {
            if (idx > 0) {
                dump_file_buff += "}\n\n";
            }
            *buff += string_format("    pint_dump_init_%d();\n", idx / PINT_DUMP_FILE_LINE_NUM);
            dump_file_buff +=
                string_format("void pint_dump_init_%d(void) {\n", idx / PINT_DUMP_FILE_LINE_NUM);
            thread_declare_buff +=
                string_format("void pint_dump_init_%d(void);\n", idx / PINT_DUMP_FILE_LINE_NUM);
        }
        pint_vcddump_var_without_check(sig_info, &dump_file_buff);
        idx++;
    }
    dump_file_buff += "}\n\n";
    *buff += "    pint_simu_vpi(PINT_DUMP_VAR, \"$end\");\n";
}

void pint_vcddump_off(string *buff) {
    *buff += "    pint_simu_vpi(PINT_DUMP_VAR, \"$dumpoff\");\n";
    map<ivl_nexus_t, pint_sig_info_t>::iterator iter;
    for (iter = pint_nex_to_sig_info.begin(); iter != pint_nex_to_sig_info.end(); iter++) {
        pint_sig_info_t sig_info = iter->second;
        if (sig_info->is_func_io)
            continue;
        pint_vcddump_off(sig_info, buff);
    }
    *buff += "    pint_simu_vpi(PINT_DUMP_VAR, \"$end\");\n";
}

void pint_vcddump_on(string *buff) {
    *buff += "    pint_vcddump_value(&T);\n"
             "    pint_simu_vpi(PINT_DUMP_VAR, \"$dumpon\");\n";
    map<ivl_nexus_t, pint_sig_info_t>::iterator iter;
    for (iter = pint_nex_to_sig_info.begin(); iter != pint_nex_to_sig_info.end(); iter++) {
        pint_sig_info_t sig_info = iter->second;
        if (sig_info->is_func_io)
            continue;
        pint_vcddump_var(sig_info, buff);
    }
    *buff += "    pint_simu_vpi(PINT_DUMP_VAR, \"$end\");\n";
}

void pint_event_add_list(void) {
    map<ivl_event_t, pint_event_info_t>::iterator iter;
    pint_event_info_t event_info;
    ivl_nexus_t nex;
    for (iter = pint_event.begin(); iter != pint_event.end(); iter++) {
        event_info = iter->second;
        if (event_info->pos_num) {
            for (unsigned idx = 0; idx < event_info->pos_num; idx += 1) {
                nex = ivl_event_pos(event_info->evt, idx);
                pint_sig_info_add_event(nex, event_info->event_id, POS);
            }
        }
        if (event_info->any_num) {
            if ((event_info->pos_num > 0) || (event_info->neg_num > 0)) {
                for (unsigned idx = 0; idx < event_info->any_num; idx += 1) {
                    nex = ivl_event_any(event_info->evt, idx);
                    pint_sig_info_add_event(nex, event_info->event_id, ANY);
                }
            } else {
                mcc_event_info.push_back(event_info);
            }
        }
        if (event_info->neg_num) {
            for (unsigned idx = 0; idx < event_info->neg_num; idx += 1) {
                nex = ivl_event_neg(event_info->evt, idx);
                pint_sig_info_add_event(nex, event_info->event_id, NEG);
            }
        }
    }
}

void pint_dump_event(void) {
    PINT_LOG_HOST("EVT: %u\n", pint_event_num_actual);
    event_declare_buff += "\n//event declare\n";
    event_impl_buff += "\n//event info\n";
    event_begin_buff += "void pint_event_init(void) {\n";

    const char *gen_attr = " __attribute__((section(\".builtin_simu_buffer\")));\n";
    map<unsigned, pint_event_info_t>::iterator iter;
    pint_event_info_t event_info;
    for (iter = pint_idx_to_evt_info.begin(); iter != pint_idx_to_evt_info.end(); ++iter) {
        event_info = iter->second;
        if (!event_info->wait_count) {
            event_declare_buff +=
                string_format("void pint_move_event_%u(void);\n", event_info->event_id);
            inc_buff +=
                string_format("extern void pint_move_event_%u(void);\n", event_info->event_id);
            event_impl_buff +=
                string_format("void pint_move_event_%u(void){}\n", event_info->event_id);
            continue;
        }
        unsigned event_id = event_info->event_id;
        unsigned has_cmn = (event_info->wait_count - event_info->static_wait_count) > 0 ? 1 : 0;
        unsigned cmn_num = event_info->wait_count - event_info->static_wait_count + 1;
        unsigned has_array_signal = event_info->has_array_signal;

        inc_buff += string_format("extern pint_wait_event_s pint_event_%u;\n", event_id);
        event_declare_buff +=
            string_format("pint_wait_event_s pint_event_%u %s", event_id, gen_attr);

        inc_buff += string_format("extern void pint_move_event_%u(void);\n", event_id);
        event_declare_buff += string_format("void pint_move_event_%u(void);\n", event_id);        

        if(has_cmn && !has_array_signal){

          event_impl_buff +=
              string_format("void pint_move_event_%u(void){\n"
                            "\tpint_move_event_thread_list(&pint_event_%u);\n}\n",
                            event_id, event_id);
          event_declare_buff +=
              string_format("pint_thread pint_event_%u_list[%u] %s", event_id, cmn_num, gen_attr);
          inc_buff +=
              string_format("extern pint_thread pint_event_%u_list[%u];\n", event_id, cmn_num);
          event_begin_buff += string_format("\tpint_event_%u.thread_list = pint_event_%u_list;\n",
                                            event_id, event_id);
          event_begin_buff += string_format("\tpint_event_%u.thread_num = %u;\n"
                                            "\tpint_event_%u.thread_head = 0;\n",
                                            event_id, cmn_num, event_id, event_id);                                                                              
        }
        //array and event trigger still use static_table rather than s_list
        else if(has_array_signal || (event_info->any_num == 0 && event_info->pos_num == 0 
                  && event_info->neg_num == 0 && event_info->wait_count > 0)) {
            unsigned move_event_type =
                ((unsigned)(event_info->wait_count > event_info->static_wait_count) << 31) |
                event_info->static_wait_count;
            string static_thread_tbl_str = event_info->static_wait_count
                                              ? string_format("event_%u_static_thread_table", event_id)
                                              : "NULL";
            event_impl_buff += string_format(
                "void pint_move_event_%u(void){\n"
                "\tpint_move_event_thread_list_with_static_table(&pint_event_%u,0x%x, %s);\n}\n",
                event_id, event_id, move_event_type, static_thread_tbl_str.c_str());
            // if (has_cmn) {
            //     event_declare_buff +=
            //         string_format("pint_thread pint_event_%u_list[%u] %s", event_id, cmn_num, gen_attr);
            //     inc_buff +=
            //         string_format("extern pint_thread pint_event_%u_list[%u];\n", event_id, cmn_num);
            //     event_begin_buff += string_format("\tpint_event_%u.thread_list = pint_event_%u_list;\n",
            //                                       event_id, event_id);
            // } else {
            event_begin_buff += string_format("\tpint_event_%u.thread_list = NULL;\n", event_id);
            // }
            event_begin_buff += string_format("\tpint_event_%u.thread_num = %u;\n"
                                              "\tpint_event_%u.thread_head = 0;\n",
                                              event_id, cmn_num, event_id, event_id);
        }
    }
    event_begin_buff += "}\n";
    event_declare_buff += "\n";
    event_impl_buff += "\n";
}

void pint_event_process(ivl_event_t net) {
    pint_event_info_t event_info = new struct pint_event_info;
    event_info->evt = net;
    event_info->any_num = ivl_event_nany(net);
    event_info->neg_num = ivl_event_nneg(net);
    event_info->pos_num = ivl_event_npos(net);
    event_info->wait_count = 0;
    event_info->event_id = pint_event_num;
    event_info->static_wait_count =
        single_depended_events.count(net) ? single_depended_events[net] : 0;
    event_info->has_array_signal = 0;

    pint_event[net] = event_info;
    pint_idx_to_evt_info[pint_event_num++] = event_info;
}

void pint_parse_event_sig_info(ivl_event_t net) {
    //  Function: when evt is sensitive to an output of an local lpm, then this output must
    //  changed
    //  to global signal.
    ivl_nexus_t nex;
    unsigned num;
    unsigned i, j;
    for (i = 0; i < 3; i++) {
        if (i == 0)
            num = ivl_event_nany(net);
        else if (i == 1)
            num = ivl_event_nneg(net);
        else
            num = ivl_event_npos(net);
        for (j = 0; j < num; j++) {
            if (i == 0)
                nex = ivl_event_any(net, j);
            else if (i == 1)
                nex = ivl_event_neg(net, j);
            else
                nex = ivl_event_pos(net, j);
            if (pint_find_signal(nex)->is_local) {
                pint_revise_sig_info_to_global(nex);
                pint_lpm_set_no_need_init(nex);
            }
        }
    }
}

void pint_dump_init(void) {
    start_buff = "#include \"pintsimu.h\"\n\n";
    if (strcmp(enval, enval_flag) != 0) {
        start_buff += "#include  \"pint_thread_stub.h\"\n"
                      "#include  \"thread.h\"\n"
                      "#include  \"lpm.h\"\n\n";
    }
    inc_start_buff = start_buff;
    if (pint_dump_var == 0) {
        global_init_buff += "void pint_vars_dump(void){};\n";
    }
    global_init_buff += "void pint_global_init(void)\n"
                        "{\n"
                        "    pint_event_init();\n";
}

/*
 * This function finds the vector width of a signal. It relies on the
 * assumption that all the signal inputs to the nexus have the same
 * width. The ivl_target API should assert that condition.
 */
unsigned width_of_nexus(ivl_nexus_t nex) {
    unsigned idx;

    for (idx = 0; idx < ivl_nexus_ptrs(nex); idx += 1) {
        ivl_nexus_ptr_t ptr = ivl_nexus_ptr(nex, idx);
        ivl_signal_t sig = ivl_nexus_ptr_sig(ptr);

        if (sig != 0) {
            return ivl_signal_width(sig);
        }
    }

    /* ERROR: A nexus should have at least one signal to carry
        properties like width. */
    return 0;
}

ivl_discipline_t discipline_of_nexus(ivl_nexus_t nex) {
    unsigned idx;

    for (idx = 0; idx < ivl_nexus_ptrs(nex); idx += 1) {
        ivl_nexus_ptr_t ptr = ivl_nexus_ptr(nex, idx);
        ivl_signal_t sig = ivl_nexus_ptr_sig(ptr);

        if (sig != 0) {
            return ivl_signal_discipline(sig);
        }
    }

    /* ERROR: A nexus should have at least one signal to carry
        properties like the data type. */
    return 0;
}

ivl_variable_type_t type_of_nexus(ivl_nexus_t net) {
    unsigned idx;

    for (idx = 0; idx < ivl_nexus_ptrs(net); idx += 1) {
        ivl_nexus_ptr_t ptr = ivl_nexus_ptr(net, idx);
        ivl_signal_t sig = ivl_nexus_ptr_sig(ptr);

        if (sig != 0) {
            return ivl_signal_data_type(sig);
        }
    }

    /* ERROR: A nexus should have at least one signal to carry
        properties like the data type. */
    return IVL_VT_NO_TYPE;
}

const char *data_type_string(ivl_variable_type_t vtype) {
    const char *vt = "??";

    switch (vtype) {
    case IVL_VT_NO_TYPE:
        vt = "NO_TYPE";
        break;
    case IVL_VT_VOID:
        vt = "void";
        break;
    case IVL_VT_BOOL:
        vt = "bool";
        break;
    case IVL_VT_REAL:
        vt = "real";
        break;
    case IVL_VT_LOGIC:
        vt = "logic";
        break;
    case IVL_VT_STRING:
        vt = "string";
        break;
    case IVL_VT_DARRAY:
        vt = "darray";
        break;
    case IVL_VT_CLASS:
        vt = "class";
        break;
    case IVL_VT_QUEUE:
        vt = "queue";
        break;
    }

    return vt;
}

void pint_dump_nbassign(void) {
    int nbassign_num = 0;
    int nbassign_array_num = 0;
    unsigned nbflag_id = pint_thread_num + global_lpm_num + monitor_lpm_num + force_lpm_num +
                         pint_event_num + pint_delay_num;

    map<string, pint_nbassign_info>::iterator it;
    for (it = nbassign_map.begin(); it != nbassign_map.end(); ++it) {
        assert(it->second.type == 0 || it->second.type == 1);
        if (it->second.type == 0) {
            nbassign_num++;
        } else {
            nbassign_array_num++;
        }
    }

    map<string, int> nb_array_decl;
    vector<string> nbassign_info_init;
    vector<string> nbassign_array_info_init;
    nbassign_info_init.resize(nbassign_num);
    nbassign_array_info_init.resize(nbassign_array_num);
    for (it = nbassign_map.begin(); it != nbassign_map.end(); ++it) {
        if (it->second.type == 0) {
            string tmp = "{";
            tmp += string_format("NB_SIGNAL,");
            tmp += string_format("%d,", it->second.size);
            tmp += string_format("%s,", it->second.src.c_str());
            tmp += string_format("%s,", it->second.dst.c_str());
            tmp += string_format("%d,", it->second.offset);
            tmp += string_format("%d,", it->second.updt_size);
            tmp += string_format("%d,", nbflag_id);
            tmp += string_format("%s,", it->second.l_list.c_str());
            tmp += string_format("%s,", it->second.e_list.c_str());
            tmp += string_format("%s,", it->second.p_list.c_str());
            if (pint_pli_mode) {
                tmp += string_format("%s,", it->second.vcd_symbol_ptr.c_str());
                tmp += string_format("%d,", it->second.is_output_interface);
                tmp += string_format("%d},\n", it->second.output_port_id);
            } else {
                tmp += string_format("%s},", it->second.vcd_symbol_ptr.c_str());
            }
            nbassign_info_init[it->second.sig_nb_id] = tmp;
            if ((it->second.vcd_symbol_ptr.compare("NULL"))) {
                start_buff +=
                    string_format("unsigned char %s[] = \"%s\";\n",
                                  it->second.vcd_symbol_ptr.c_str(), it->second.vcd_symbol.c_str());
                inc_buff += string_format("extern unsigned char %s[];\n",
                                          it->second.vcd_symbol_ptr.c_str());
            }
            start_buff += it->second.nb_sig;
        } else {
            string tmp = "{";
            tmp += string_format("NB_ARRAY,");
            tmp += string_format("%d,", it->second.size);
            tmp += string_format("%s,", it->second.src.c_str());
            tmp += string_format("%s,", it->second.dst.c_str());
            tmp += string_format("%d,", it->second.array_size);
            tmp += string_format("0,");
            tmp += string_format("%d,", nbflag_id);
            tmp += string_format("%s,", it->second.l_list.c_str());
            tmp += string_format("%s,", it->second.e_list.c_str());
            tmp += string_format("%s,", it->second.p_list.c_str());
            tmp += string_format("{0},");
            tmp += string_format("NULL},\n");
            nbassign_array_info_init[it->second.sig_nb_id] = tmp;
            map<string, int>::iterator iter = nb_array_decl.find(it->second.nb_sig);
            if (iter == nb_array_decl.end()) {
                start_buff += it->second.nb_sig;
                nb_array_decl.insert(pair<string, int>(it->second.nb_sig, 1));
            }
        }
        inc_buff += it->second.nb_sig_declare;

        nbflag_id++;
    }
    nb_array_decl.clear();

    string nbassign_info_init_str;
    string nbassign_array_info_init_str;
    for (unsigned i = 0; i < nbassign_info_init.size(); i++) {
        nbassign_info_init_str += nbassign_info_init[i];
    }

    for (unsigned i = 0; i < nbassign_array_info_init.size(); i++) {
        nbassign_array_info_init_str += nbassign_array_info_init[i];
    }

    string pint_nb_list_def_str =
        string_format("struct pint_nbassign pint_nb_list[%d] = {\n", nbassign_num) +
        nbassign_info_init_str + "\n};\n";
    string pint_nb_array_list_def_str =
        string_format("struct pint_nbassign_array pint_nb_array_list[%d] = {\n",
                      nbassign_array_num) +
        nbassign_array_info_init_str + "\n};\n";

    if (strcmp(enval, enval_flag) != 0) {
        string nbassign_buff_tmp =
            inc_start_buff + "//nbassin info\n" + pint_nb_list_def_str + pint_nb_array_list_def_str;
        string src_file_path = string_format("%s/src_file/nbassign_init.c", dir_name.c_str());
        FILE *pfilec = fopen(src_file_path.c_str(), "w+");
        fprintf(pfilec, "%s", nbassign_buff_tmp.c_str());
        fclose(pfilec);
    } else {
        nbassign_buff += "\n//nbassin info\n";
        nbassign_buff += pint_nb_list_def_str;
        nbassign_buff += pint_nb_array_list_def_str;
    }
}

void show_parameter(string &ret, ivl_parameter_t net) {
    ivl_expr_t expr = ivl_parameter_expr(net);
    const ivl_expr_type_t code = ivl_expr_type(expr);
    unsigned width = ivl_expr_width(expr);
    int idx;
    switch (code) {
    case IVL_EX_NUMBER: {
        const char *bits = ivl_expr_bits(expr);

        for (idx = width; idx > 0; idx -= 1)
            ret += string_format("%c", bits[idx - 1]);

        ret += " ";
        break;
    }
    case IVL_EX_STRING:
        ret += string_format("=\"%s\" ", ivl_expr_string(expr));
        break;
    default:
        ret += "ERROR: in show_parameters ";
        break;
    }
}

static int show_process_add_list(ivl_process_t net, void *x) {
    (void)x;
    ivl_statement_t sub_stmt = ivl_process_stmt(net);
    lval_idx_num = 0;
    ExpressionConverter::ClearExpressTmpVarId();
    std::set<ivl_signal_t> deletable_signals;

    switch (ivl_process_type(net)) {
    case IVL_PR_INITIAL:
        if (pint_init_is_sig_init(net)) {
            return 0;
        }
        break;
    case IVL_PR_ALWAYS:
        break;
    case IVL_PR_ALWAYS_COMB:
        break;
    case IVL_PR_ALWAYS_FF:
        break;
    case IVL_PR_ALWAYS_LATCH:
        break;
    case IVL_PR_FINAL:
        break;
    default:
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(sub_stmt), ivl_stmt_lineno(sub_stmt),
                               "process type not supported. Type_id: %u.", ivl_process_type(net));
        break;
    }

    std::ostringstream t_num, d_num, pc_num, m_num;
    t_num << pint_thread_num;

    pint_process_event.clear();
    if (net->to_lpm_flag) {
        if (PINT_REMVOE_SISITIVE_SIGNAL_EN) {
            analysis_sensitive(net, deletable_signals);
        }
        int t_idx = global_lpm_num + monitor_lpm_num + pint_event_num + global_thread_lpm_num1;
        global_thread_lpm_num1++;
        thread_to_lpm = t_idx - pint_event_num;
        statement_add_list(sub_stmt, 4, deletable_signals);
        thread_to_lpm = -1;
        expr_buff.clear();
    } else {
        statement_add_list(sub_stmt, 4, deletable_signals);
        expr_buff.clear();
    }

    return 0;
}

void pint_init_thread_info(pint_thread_info_t thread, unsigned id, unsigned to_lpm,
                           unsigned delay_num, ivl_scope_t scope, ivl_process_t proc) {
    // memset((char *)thread, 0, sizeof(struct pint_thread_info));
    thread->proc = proc;
    thread->scope = scope;
    thread->id = id;
    thread->pc = 0;
    thread->to_lpm_flag = to_lpm;
    thread->have_processed_delay = 0;
    thread->delay_num = delay_num;
    thread->future_idx = 0;
    thread->fork_idx = 0;
    thread->fork_cnt = 0;
    thread->proc_sub_stmt_num = 0;
    thread->chk_before_exit = 0;
}

static int show_process(ivl_process_t net, pint_mult_info_t mult_info) {
    string file_name;
    int backslash_index;
    size_t write_bytes;
    thread_buff.clear();
    child_thread_buff.clear();
    pint_process_event.clear();
    pint_process_str_idx = 0;
    ivl_statement_t sub_stmt = ivl_process_stmt(net);
    pint_current_scope_time_unit = ivl_scope_time_units(ivl_process_scope(net));
    if (pint_thread_num == 0 && strcmp(enval, enval_flag) != 0) {
        total_thread_buff = inc_start_buff + total_thread_buff;
        lpm_buff = inc_start_buff + lpm_buff;
    }
    lval_idx_num = 0;
    nb_delay_same_info.clear();
    ExpressionConverter::ClearExpressTmpVarId();
    pint_always_wait_one_event = net->only_one_wait_event;

    //     analysis_process(ivl_process_stmt(net), 0, &proc_info, 2);
    // #if 1
    //     printf("process:%s:%d\n", ivl_stmt_file(ivl_process_stmt(net)),
    //     ivl_stmt_lineno(ivl_process_stmt(net))); printf("n_l_sig=%d n_sig=%d\n",
    //     proc_info.n_l_sig, proc_info.n_sig); printf("*************  signals: "); printf("\n");
    //     for (int i = 0; i < proc_info.n_sig; i++)
    //     {
    //         printf("%d %s file:%s line:%d\n", ((*proc_info.stmt_in) + i)->sensitive,
    //         ivl_signal_basename(proc_info.sig_list[i]), ivl_signal_file(proc_info.sig_list[i]),
    //         ivl_signal_lineno(proc_info.sig_list[i]));
    //     }
    //     printf("\n");
    // #endif
    //     analysis_free_process_info(&proc_info);

    switch (ivl_process_type(net)) {
    case IVL_PR_INITIAL:
        if (pint_init_is_sig_init(net)) {
            pint_init_show_sig_init(global_init_buff, net);
            return 0;
        }
        break;
    case IVL_PR_ALWAYS:
        break;
    case IVL_PR_ALWAYS_COMB:
        break;
    case IVL_PR_ALWAYS_FF:
        break;
    case IVL_PR_ALWAYS_LATCH:
        break;
    case IVL_PR_FINAL:
        break;
    default:
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(sub_stmt), ivl_stmt_lineno(sub_stmt),
                               "process type not supported. Type_id: %u.", ivl_process_type(net));
        break;
    }

    // nxz
    pint_proc_sig_nxz_clear();
    if (PINT_SIG_NXZ_EN) {
        analysis_sig_nxz(net, &proc_sig_nxz);
    }

    struct pint_thread_info thread_info_s;
    temp_proc = &thread_info_s;
    temp_mult = mult_info;
    pint_init_thread_info(temp_proc, pint_thread_num, net->to_lpm_flag, pint_delay_num,
                          ivl_process_scope(net), net);
    pint_proc_mult_info_init();
    pint_proc_mult_asgn_show_signal_define(expr_buff);
    std::ostringstream t_num, pc_num, m_num;
    t_num << pint_thread_num;

    if (net->to_lpm_flag) {
        int t_idx = global_lpm_num + monitor_lpm_num + pint_event_num + global_thread_lpm_num;
        m_num << t_idx;
        thread_buff += "\nvoid pint_lpm_" + m_num.str() + "(void){\n";
        // perl analysis need info
        file_name = ivl_process_file(net);
        backslash_index = file_name.find_last_of('/');
        thread_buff +=
            string_format("//def:%s:%d ", file_name.substr(backslash_index + 1, -1).c_str(),
                          ivl_process_lineno(net));
        thread_buff += "\n";

        thread_buff += string_format("//mcc,test\n");
        if (pint_perf_on) {
            unsigned lpm_perf_id =
                global_lpm_num + monitor_lpm_num + global_thread_lpm_num + PINT_PERF_BASE_ID;
            thread_buff += string_format("    NCORE_PERF_SUMMARY(%d);\n", lpm_perf_id);
        }
        global_thread_lpm_num++;
        thread_buff += string_format("\tpint_enqueue_flag_clear(%d);\n", t_idx);

        unsigned curr_pos = thread_buff.length();
        thread_to_lpm = t_idx - pint_event_num;
        StmtGen to_lpm_gen(sub_stmt, temp_proc, NULL, 0, 4);
        thread_buff += to_lpm_gen.GetStmtGenCode();
        expr_buff += to_lpm_gen.GetStmtDeclCode();
        pint_delay_num = thread_info_s.delay_num;
        thread_to_lpm = -1;
        pint_proc_mult_gen_exit_code(thread_buff);
        thread_buff += "\treturn;\n";
        thread_buff += "}\n";
        expr_buff += "\tunsigned change_flag = 0;\n";
        thread_buff.insert(curr_pos, expr_buff);
        expr_buff.clear();
    } else {
        if (pint_always_wait_one_event) {
            thread_is_static_thread[pint_thread_num] = 1;
        } else {
            thread_is_static_thread[pint_thread_num] = 0;
        }
        thread_buff += "\nvoid pint_thread_" + t_num.str() + "(void){\n";
        // perl analysis need info
        file_name = ivl_process_file(net);
        backslash_index = file_name.find_last_of('/');
        thread_buff +=
            string_format("//def:%s:%d ", file_name.substr(backslash_index + 1, -1).c_str(),
                          ivl_process_lineno(net));
        thread_buff += "\n";

        if (pint_perf_on) {
            unsigned thread_perf_id = global_lpm_num + monitor_lpm_num + global_thread_lpm_num1 + force_lpm_num +
                                      pint_thread_num + PINT_PERF_BASE_ID;
            thread_buff += string_format("    NCORE_PERF_SUMMARY(%d);\n", thread_perf_id);
        }

        thread_declare_buff += "void pint_thread_" + t_num.str() + "(void);\n";
#if THREAD_TRACE_ENABLE
        string print_str = string_format(" go in pint_thread_%u\\n", pint_thread_num);
        thread_buff += "    printf(\"Core %d" + print_str + "\", pint_core_id());\n";
#endif
        unsigned curr_pos = thread_buff.length();
        if (ivl_process_type(net) != IVL_PR_INITIAL && (!pint_always_wait_one_event)) {
            thread_buff += "    while(1) {\n";
        }
        StmtGen thread_stmt_gen(sub_stmt, temp_proc, NULL, 0, 4);
        thread_buff += thread_stmt_gen.GetStmtGenCode();
        expr_buff += thread_stmt_gen.GetStmtDeclCode();
        pint_delay_num = thread_info_s.delay_num;
        if ((ivl_process_type(net) != IVL_PR_INITIAL) && (thread_info_s.pc) &&
            !pint_always_wait_one_event) {
            thread_buff += "    pint_thread_pc_x[" + t_num.str() + "] = 0;\n    }\n";
        }

#if THREAD_TRACE_ENABLE
        print_str = string_format(" go out pint_thread_%u\\n", pint_thread_num);
        thread_buff += "    printf(\"Core %d" + print_str + "\", pint_core_id());\n";
#endif
        pint_proc_mult_gen_exit_code(thread_buff);
        thread_buff += "}\n";

        if (thread_info_s.pc) {
            pc_num << thread_info_s.pc;
            pint_thread_pc_max =
                (thread_info_s.pc > pint_thread_pc_max) ? thread_info_s.pc : pint_thread_pc_max;
            if (!pint_always_wait_one_event) {
                thread_buff.insert(
                    curr_pos, ("    PINT_PROCESS_PC_" + pc_num.str() + "(" + t_num.str() + ");\n"));
            }
        }

        expr_buff += "\tunsigned change_flag = 0;\n";
        thread_buff.insert(curr_pos, expr_buff);
        expr_buff.clear();

        // fork_join will add child thread num
        if (thread_info_s.fork_cnt > 0) {
            pint_thread_num += thread_info_s.fork_cnt;
            thread_buff += child_thread_buff;
            child_thread_buff.clear();
        }

        // dump file
        if (dump_file_buff.size() > 0) {
            thread_buff += dump_file_buff;
            dump_file_buff.clear();
        }

        pint_thread_num++;
        pint_always_wait_one_event = 0;
    }

    if (strcmp(enval, enval_flag) != 0) {
        if (net->to_lpm_flag) {
            static int lpm_buff_file_size = 0;
            lpm_buff += thread_buff;
            if (lpm_buff_file_size > 1024 * 1024 * 1) {
                lpm_buff = inc_start_buff + lpm_buff;
                lpm_buff_file_id += 1;
                lpm_buff_file_size = 0;
            }
            string lpm_file_name =
                string_format("%s/src_file/lpm_normal%d.c", dir_name.c_str(), lpm_buff_file_id);
            int lpm_buff_fd = -1;
            if (lpm_buff.size() > 1024 * 1024) {
                if ((lpm_buff_fd = open(lpm_file_name.c_str(), O_RDWR | O_CREAT | O_APPEND | O_SYNC,
                                        00700)) < 0) {
                    printf("Open lpm.c error! show_process");
                    return -1;
                }
                lpm_buff_file_size += lpm_buff.size();
                write_bytes = write(lpm_buff_fd, lpm_buff.c_str(), lpm_buff.size());
                lpm_buff.clear();
                close(lpm_buff_fd);
            }
        } else {
            total_thread_buff += thread_buff;
        }
        if (thread_buff_file_size > 1024 * 1024 * 1) {
            total_thread_buff = inc_start_buff + total_thread_buff;
            thread_buff_file_id += 1;
            thread_buff_file_size = 0;
        }
        string thread_file_name =
            string_format("%s/src_file/thread%d.c", dir_name.c_str(), thread_buff_file_id);
        int thread_buff_fd = -1;
        if (total_thread_buff.size() > 1024 * 1024) {
            if ((thread_buff_fd = open(thread_file_name.c_str(),
                                       O_RDWR | O_CREAT | O_APPEND | O_SYNC, 00700)) < 0) {
                printf("Open pint_thread.c error! show_process");
                return -1;
            }
            thread_buff_file_size += total_thread_buff.size();
            write_bytes =
                write(thread_buff_fd, total_thread_buff.c_str(), total_thread_buff.size());
            total_thread_buff.clear();
            close(thread_buff_fd);
        }
    } else {
        total_thread_buff += thread_buff;
        thread_buff.clear();
    }
    (void)write_bytes;
    temp_proc = NULL;
    temp_mult = NULL;
    return 0;
}

static void show_primitive(ivl_udp_t net, unsigned ref_count) {
    unsigned rdx;

    fprintf(out, "primitive %s (referenced %u times)\n", ivl_udp_name(net), ref_count);

    if (ivl_udp_sequ(net))
        fprintf(out, "    reg out = %c;\n", ivl_udp_init(net));
    else
        fprintf(out, "    wire out;\n");
    fprintf(out, "    table\n");
    for (rdx = 0; rdx < ivl_udp_rows(net); rdx += 1) {

        /* Each row has the format:
       Combinational: iii...io
       Sequential:  oiii...io
       In the sequential case, the o value in the front is
       the current output value. */
        unsigned idx;
        const char *row = ivl_udp_row(net, rdx);

        fprintf(out, "    ");

        if (ivl_udp_sequ(net))
            fprintf(out, " cur=%c :", *row++);

        for (idx = 0; idx < ivl_udp_nin(net); idx += 1)
            fprintf(out, " %c", *row++);

        fprintf(out, " : out=%c\n", *row++);
    }
    fprintf(out, "    endtable\n");

    fprintf(out, "endprimitive\n");
}

// show event first, built the sig depend list
static int show_all_event(ivl_scope_t net, void *x) {
    unsigned evt_num = ivl_scope_events(net);
    ivl_event_t evt;
    unsigned idx;
    (void)x; // not used
    for (idx = 0; idx < evt_num; idx += 1) {
        evt = ivl_scope_event(net, idx);
        pint_event_process(evt);
        pint_parse_event_sig_info(evt);
    }
    // printf("pint_event_num:%d\n",pint_event_num);
    return ivl_scope_children(net, show_all_event, 0);
}

static void pint_show_task(ivl_statement_t net, ivl_scope_t scope) {
    pint_in_task = 1;
    pint_process_event.clear();

    std::ostringstream d_num, pc_num;
    unsigned curr_pos = thread_buff.length();

    struct pint_thread_info thread_info_s;
    pint_init_thread_info(&thread_info_s, pint_thread_num, 0, pint_delay_num, scope, NULL);
    StmtGen task_gen(net, &thread_info_s, NULL, 0, 0);
    thread_buff += task_gen.GetStmtGenCode();
    expr_buff += task_gen.GetStmtDeclCode();
    pint_delay_num = thread_info_s.delay_num;

    if (thread_info_s.pc) {
        pc_num << thread_info_s.pc;
        pint_task_pc_max =
            (thread_info_s.pc > pint_task_pc_max) ? thread_info_s.pc : pint_task_pc_max;
        thread_buff += "    *pint_task_pc_x = 0;\n";
        thread_buff.insert(curr_pos, ("    PINT_TASK_PC_" + pc_num.str() + "();\n"));
    }

    thread_buff.insert(curr_pos, expr_buff);
    expr_buff.clear();

    pint_in_task = 0;
}

static vector<ivl_signal_t> collect_func_sigs_inorder(ivl_scope_t net) {
    int ports_num = ivl_scope_ports(net);
    int sigs_num = ivl_scope_sigs(net);

    vector<ivl_signal_t> sigs_inorder;

    for (int i = 0; i < ports_num; i++) {
        sigs_inorder.push_back(ivl_scope_port(net, i));
    }

    for (int i = 0; i < sigs_num; i++) {
        ivl_signal_t sig = ivl_scope_sig(net, i);
        ivl_signal_port_t type = ivl_signal_port(sig);
        if ((type != IVL_SIP_INPUT) && (type != IVL_SIP_INOUT) && (type != IVL_SIP_OUTPUT)) {
            sigs_inorder.push_back(sig);
        }
    }

    return sigs_inorder;
}

string task_declare_buff;
map<string, string> func_task_name_implementation;
set<string> invoked_fun_names;

static int show_all_func(ivl_scope_t net, void *x) {
    unsigned idx;
    unsigned arg_num = 0;
    string func_name, arg;

    (void)x; /* Parameter is not used. */
    if (ivl_scope_type(net) == IVL_SCT_FUNCTION) {
        string thread_buff_bak;
        string static_cp_back_code;
        vector<ivl_signal_t> sigs_inorder = collect_func_sigs_inorder(net);
        unsigned sig_num = sigs_inorder.size();
        thread_buff_bak = thread_buff;
        thread_buff = "";

        func_name = ivl_scope_name_c(net);
        task_name = func_name;

        string tmp_var_dec;
        for (idx = 0; idx < sig_num; idx += 1) {
            ivl_signal_t sig = sigs_inorder[idx];
            ivl_signal_port_t type = ivl_signal_port(sig);
            unsigned width = ivl_signal_width(sig);
            unsigned array_count = ivl_signal_array_count(sig);
            pint_sig_info_t sig_info = pint_find_signal(sig);
            sig_info->sig_name = (string)ivl_signal_basename(sig);
            sig_info->is_func_io = 1;
            if ((type == IVL_SIP_INPUT) || (type == IVL_SIP_INOUT) || (type == IVL_SIP_OUTPUT)) {
                if (arg_num++ > 0) {
                    arg += ", ";
                }
                if (type == IVL_SIP_INPUT) {
                    arg += "const ";
                }
                const char *port_type_str;
                switch (type) {
                case IVL_SIP_INPUT:
                    port_type_str = "Input";
                    break;
                case IVL_SIP_INOUT:
                    port_type_str = "Inout";
                    break;
                default:
                    port_type_str = "Output";
                    break;
                }

                if (ivl_signal_data_type(sig) == IVL_VT_REAL) {
                    arg +=
                        string_format("/*%s*/float& %s", port_type_str, ivl_signal_basename(sig));
                } else if (width <= 4) {
                    arg += string_format("/*%s*/unsigned char& %s", port_type_str,
                                         ivl_signal_basename(sig));
                } else {
                    arg += string_format("/*%s*/unsigned int* %s", port_type_str,
                                         ivl_signal_basename(sig));
                }
            } else {
                unsigned is_auto = ivl_scope_is_auto(net);
                if (width <= 4) {
                    if (array_count == 1) {
                        if (is_auto) {
                            tmp_var_dec +=
                                string_format("unsigned char %s;\n", ivl_signal_basename(sig));
                        } else {
                            tmp_var_dec += string_format("static unsigned char %s;\n",
                                                         ivl_signal_basename(sig));
                            tmp_var_dec +=
                                string_format("unsigned char %s_tmp = %s;\n",
                                              ivl_signal_basename(sig), ivl_signal_basename(sig));
                            static_cp_back_code +=
                                string_format("%s = %s_tmp;\n", ivl_signal_basename(sig),
                                              ivl_signal_basename(sig));
                        }
                    } else {
                        if (is_auto) {
                            tmp_var_dec += string_format("unsigned char %s[%d];\n",
                                                         ivl_signal_basename(sig), array_count);
                        } else {
                            tmp_var_dec += string_format("static unsigned char %s[%d];\n",
                                                         ivl_signal_basename(sig), array_count);
                            tmp_var_dec += string_format("unsigned char %s_tmp[%d];\n",
                                                         ivl_signal_basename(sig), array_count);
                            for (unsigned itm_idx = 0; itm_idx < array_count; itm_idx++) {
                                tmp_var_dec += string_format("%s_tmp[%u] = %s[%u];\n",
                                                             ivl_signal_basename(sig), itm_idx,
                                                             ivl_signal_basename(sig), itm_idx);
                                static_cp_back_code += string_format(
                                    "%s[%u] = %s_tmp[%u];\n", ivl_signal_basename(sig), itm_idx,
                                    ivl_signal_basename(sig), itm_idx);
                            }
                        }
                    }
                } else {
                    int word_size = (width + 31) / 32 * 2;
                    if (array_count == 1) {
                        if (is_auto) {
                            tmp_var_dec += string_format("unsigned int %s[%d];\n",
                                                         ivl_signal_basename(sig), word_size);
                        } else {
                            tmp_var_dec += string_format("static unsigned int %s[%d];\n",
                                                         ivl_signal_basename(sig), word_size);
                            tmp_var_dec += string_format("unsigned int %s_tmp[%d];\n",
                                                         ivl_signal_basename(sig), word_size);
                            for (int itm_idx = 0; itm_idx < word_size; itm_idx++) {
                                tmp_var_dec += string_format("%s_tmp[%d] = %s[%d];\n",
                                                             ivl_signal_basename(sig), itm_idx,
                                                             ivl_signal_basename(sig), itm_idx);
                                static_cp_back_code += string_format(
                                    "%s[%d] = %s_tmp[%d];\n", ivl_signal_basename(sig), itm_idx,
                                    ivl_signal_basename(sig), itm_idx);
                            }
                        }
                    } else {
                        if (is_auto) {
                            tmp_var_dec +=
                                string_format("unsigned int %s[%d][%d];\n",
                                              ivl_signal_basename(sig), array_count, word_size);
                        } else {
                            tmp_var_dec +=
                                string_format("static unsigned int %s[%d][%d];\n",
                                              ivl_signal_basename(sig), array_count, word_size);
                            tmp_var_dec +=
                                string_format("unsigned int %s_tmp[%d][%d];\n",
                                              ivl_signal_basename(sig), array_count, word_size);
                            for (unsigned itm_idx = 0; itm_idx < array_count * word_size;
                                 itm_idx++) {
                                tmp_var_dec += string_format("%s_tmp[0][%u] = %s[0][%u];\n",
                                                             ivl_signal_basename(sig), itm_idx,
                                                             ivl_signal_basename(sig), itm_idx);
                                static_cp_back_code += string_format(
                                    "%s[0][%u] = %s_tmp[0][%u];\n", ivl_signal_basename(sig),
                                    itm_idx, ivl_signal_basename(sig), itm_idx);
                            }
                        }
                    }
                }
            }
        }
        thread_buff +=
            string_format("void %s(%s)\n{\n", func_name.c_str(), arg.c_str()) + tmp_var_dec;
        thread_buff +=
            string_format("//def:%s:%d\n", ivl_scope_def_file(net), ivl_scope_def_lineno(net));
        task_declare_buff += string_format("void %s(%s);\n", func_name.c_str(), arg.c_str());
        inc_buff += string_format("extern void %s(%s);\n", func_name.c_str(), arg.c_str());

        thread_buff += string_format("    unsigned change_flag;\n");
        pint_show_task(ivl_scope_def(net), net);
        thread_buff += static_cp_back_code;
        thread_buff += "}\n";

        func_task_name_implementation[func_name] = thread_buff;
        thread_buff = thread_buff_bak;
    }

    return ivl_scope_children(net, show_all_func, 0);
}

static int show_all_task(ivl_scope_t net, void *x) {
    unsigned idx;
    unsigned arg_num = 0;
    string func_name, arg, tmp_var_dec;
    nb_delay_same_info.clear();
    (void)x; /* Parameter is not used. */
    if (ivl_scope_type(net) == IVL_SCT_TASK) {
        unsigned sig_num = ivl_scope_sigs(net);
        bool need_pc_flag = task_pc_autosig_info[net]->need_pc;
        int auto_int_off = 1;
        int auto_char_off = 4 + task_pc_autosig_info[net]->auto_sig_int_len;
        string thread_buff_bak = thread_buff;
        thread_buff.clear();
        func_name = ivl_scope_name_c(net);
        task_name = func_name;
        cur_ivl_task_scope = net;

        for (idx = 0; idx < sig_num; idx += 1) {
            ivl_signal_t sig = ivl_scope_sig(net, idx);
            ivl_signal_port_t type = ivl_signal_port(sig);
            unsigned width = ivl_signal_width(sig);
            unsigned array_count = ivl_signal_array_count(sig);
            pint_sig_info_t sig_info = pint_find_signal(sig);
            const char *sig_full_name = sig_info->sig_name.c_str();
            const char *sig_base_name = ivl_signal_basename(sig);
            if ((type == IVL_SIP_INPUT) || (type == IVL_SIP_INOUT)) {
                if (arg_num > 0) {
                    arg += ", ";
                }
                if (type == IVL_SIP_INPUT) {
                    arg += "const ";
                }
                if (ivl_signal_data_type(sig) == IVL_VT_REAL) {
                    arg += string_format("/*Input*/float %s", sig_full_name);
                } else if (width <= 4) {
                    arg += string_format("/*Input*/unsigned char& %s", sig_full_name);
                } else {
                    arg += string_format("/*Input*/unsigned int* %s", sig_full_name);
                }
                arg_num++;
            } else if (type == IVL_SIP_OUTPUT) {

                if (arg_num > 0) {
                    arg += ", ";
                }
                sig_info->is_func_io = 0;
                if (ivl_signal_data_type(sig) == IVL_VT_REAL) {
                    arg += string_format("/*Output*/float* %s", sig_full_name);
                } else if (width <= 4) {
                    arg += string_format("/*Output*/unsigned char& %s", sig_full_name);
                } else {
                    arg += string_format("/*Output*/unsigned int* %s", sig_full_name);
                }
                arg_num++;
            } else {
                sig_info->is_func_io = 1;
                unsigned is_auto = ivl_scope_is_auto(net);
                if (width <= 4) {
                    if (array_count == 1) {
                        if (is_auto && (strstr(sig_base_name, "_ivl_") != sig_base_name)) {
                            if (need_pc_flag) {
                                tmp_var_dec +=
                                    string_format("unsigned char& %s = ((unsigned char*)sp)[%d];\n",
                                                  sig_full_name, auto_char_off++);
                            } else {
                                tmp_var_dec += string_format("unsigned char %s;\n", sig_full_name);
                            }
                        } else {
                            sig_info->is_func_io = 0;
                        }
                    } else {
                        if (is_auto && (strstr(sig_base_name, "_ivl_") != sig_base_name)) {
                            if (need_pc_flag) {
                                tmp_var_dec += string_format(
                                    "unsigned char* %s = ((unsigned char*)sp) + %d;\n",
                                    sig_full_name, auto_char_off);
                                auto_char_off += array_count;
                            } else {
                                tmp_var_dec += string_format("unsigned char %s[%d];\n",
                                                             sig_full_name, array_count);
                            }
                        } else {
                            sig_info->is_func_io = 0;
                        }
                    }
                } else {
                    int word_size = (width + 31) / 32 * 2;
                    if (array_count == 1) {
                        if (is_auto && (strstr(sig_base_name, "_ivl_") != sig_base_name)) {
                            if (need_pc_flag) {
                                tmp_var_dec +=
                                    string_format("unsigned int* %s = ((unsigned int*)sp) + %d;\n",
                                                  sig_full_name, auto_int_off);
                                auto_int_off += word_size;
                            } else {
                                tmp_var_dec += string_format("unsigned int %s[%d];\n",
                                                             sig_full_name, word_size);
                            }
                        } else {
                            sig_info->is_func_io = 0;
                        }
                    } else {
                        if (is_auto && (strstr(sig_base_name, "_ivl_") != sig_base_name)) {
                            if (need_pc_flag) {
                                tmp_var_dec += string_format(
                                    "unsigned int (*%s)[%d] = ((unsigned int(*)[%d])sp) + "
                                    "%d;\n",
                                    sig_full_name, word_size, word_size, auto_int_off);
                                auto_int_off += array_count * word_size;
                            } else {
                                tmp_var_dec += string_format("unsigned int %s[%d][%d];\n",
                                                             sig_full_name, array_count, word_size);
                            }
                        } else {
                            sig_info->is_func_io = 0;
                        }
                    }
                }
            }
        }

        if (need_pc_flag) {
            if (arg != "") {
                arg += ", ";
            }
            arg += string_format("void* sp, pint_thread p_thread, pint_future_thread_t p_delay");
            if (task_pc_autosig_info[net]->max_stack_depth >= MAX_TASK_STACK_SIZE) {
                arg += ", unsigned stack_total, unsigned cur_stack";
                tmp_var_dec =
                    string_format("\tif(cur_stack + %d > stack_total){\n"
                                  "printf(\"Error!!! Task %s stack overflow 1M bytes "
                                  "stack_total=%%u cur_stack=%%u!\\n\", stack_total, cur_stack);"
                                  " return 0;\n}\n",
                                  ivl_task_cur_stack_size(net), ivl_scope_name(net)) +
                    tmp_var_dec;
            }
            tmp_var_dec = "\tint* pint_task_pc_x = (int*)sp;\n" + tmp_var_dec;
        }
        const char *ret_type = need_pc_flag ? "int" : "void";
        thread_buff += string_format("%s %s(%s)\n{\n", ret_type, func_name.c_str(), arg.c_str());
        thread_buff +=
            string_format("//def:%s:%d\n", ivl_scope_def_file(net), ivl_scope_def_lineno(net));
        task_declare_buff +=
            string_format("%s %s(%s);\n", ret_type, func_name.c_str(), arg.c_str());
        inc_buff += string_format("%s %s(%s);\n", ret_type, func_name.c_str(), arg.c_str());

        thread_buff += string_format("    unsigned change_flag;\n");
        thread_buff += tmp_var_dec;
        pint_show_task(ivl_scope_def(net), net);

        if (need_pc_flag) {
            thread_buff += "    return 0;\n}\n";
        } else {
            thread_buff += "    return;\n}\n";
        }
        func_task_name_implementation[func_name] = thread_buff;
        thread_buff = thread_buff_bak;
    }

    return ivl_scope_children(net, show_all_task, 0);
}

static int show_all_sig(ivl_scope_t net, void *x) {
    (void)x; // not used
    int ret;
    string tmp_buff = "";
    int dump_scope_flag = 1;
    if (pint_simu_info.need_dump) {
        switch (ivl_scope_type(net)) {
        case IVL_SCT_MODULE:
            tmp_buff += "$scope module ";
            break;
        case IVL_SCT_FUNCTION:
            tmp_buff += "$scope function ";
            break;
        case IVL_SCT_TASK:
            tmp_buff += "$scope task ";
            break;
        case IVL_SCT_BEGIN:
            tmp_buff += "$scope begin ";
            break;
        case IVL_SCT_FORK:
            tmp_buff += "$scope fork ";
            break;
        default:
            dump_scope_flag = 0;
            break;
        }

        if (dump_scope_flag) {
            tmp_buff += ivl_scope_basename(net);
            tmp_buff += " $end";
            simu_header_buff.push_back(tmp_buff);
        }
    }

    for (unsigned idx = 0; idx < ivl_scope_sigs(net); idx += 1) {
        unsigned is_root_scope = (NULL == net->parent);
        pint_parse_signal(ivl_scope_sig(net, idx), is_root_scope, dump_scope_flag);
    }
    ret = ivl_scope_children(net, show_all_sig, 0);
    if (pint_simu_info.need_dump && dump_scope_flag)
        simu_header_buff.push_back("$upscope $end");
    return ret;
}

static ivl_signal_t get_port_from_nexus(ivl_scope_t scope, ivl_nexus_t nex, unsigned *word) {
    assert(nex);
    unsigned idx, count = ivl_nexus_ptrs(nex);
    ivl_signal_t sig = 0;
    *word = 0;
    for (idx = 0; idx < count; idx += 1) {
        ivl_nexus_ptr_t nex_ptr = ivl_nexus_ptr(nex, idx);
        ivl_signal_t t_sig = ivl_nexus_ptr_sig(nex_ptr);
        if (t_sig) {
            if (ivl_signal_scope(t_sig) != scope)
                continue;
            assert(!sig);
            sig = t_sig;
            *word = ivl_nexus_ptr_pin(nex_ptr);
        }
    }
    return sig;
}

static int show_all_port(ivl_scope_t net, void *x) {
    (void)x; // not used
    vector<string> pint_inout_port_name;
    pint_inout_port_name.clear();
    for (unsigned idx = 0; idx < ivl_scope_ports(net); idx += 1) {
        if ((NULL != net->parent) && (NULL == net->parent->parent) &&
            (0 == strcmp(ivl_scope_basename(net), "DUT"))) {
            unsigned word;
            ivl_signal_t port = get_port_from_nexus(net, ivl_scope_mod_port(net, idx), &word);
            ivl_signal_port_t port_type = ivl_signal_port(port);
            if (IVL_SIP_OUTPUT == port_type) {
                pint_output_port_name_id[ivl_signal_basename(port)] = pint_output_port_num;
                // printf("---output: %s,%d\n", ivl_signal_basename(port), pint_output_port_num);
                pint_output_port_num++;
            } else if (IVL_SIP_INPUT == port_type) {
                pint_input_port_name_id[ivl_signal_basename(port)] = pint_input_port_num;
                // printf("---input: %s,%d\n", ivl_signal_basename(port), pint_input_port_num);
                pint_input_port_num++;
            } else if (IVL_SIP_INOUT == port_type) {
                pint_inout_port_name.push_back(ivl_signal_basename(port));
                pint_inout_port_num++;
            }
        }
    }

    if (pint_inout_port_name.size() > 0) {
        for (unsigned inout_idx = 0; inout_idx < pint_inout_port_num; inout_idx++) {
            pint_output_port_name_id[pint_inout_port_name[inout_idx]] =
                pint_output_port_num + inout_idx;
            pint_input_port_name_id[pint_inout_port_name[inout_idx]] =
                pint_input_port_num + inout_idx;
            // printf("---inout %s,%d,%d\n", pint_inout_port_name[inout_idx].c_str(),
            // pint_output_port_num + inout_idx, pint_input_port_num + inout_idx);
        }
    }

    return ivl_scope_children(net, show_all_port, (void *)0);
}

static int show_all_log(ivl_scope_t net, void *x) {
    (void)x; // not used
    for (unsigned idx = 0; idx < ivl_scope_logs(net); idx += 1) {
        pint_logic_process(ivl_scope_log(net, idx));
    }
    return ivl_scope_children(net, show_all_log, (void *)0);
}

static int show_all_lpm(ivl_scope_t net, void *x) {
    (void)x; // not used
    for (unsigned idx = 0; idx < ivl_scope_lpms(net); idx += 1) {
        pint_lpm_process(ivl_scope_lpm(net, idx));
    }
    return ivl_scope_children(net, show_all_lpm, (void *)0);
}

static int show_all_switch(ivl_scope_t net, void *x) {
    (void)x; // not used
    for (unsigned idx = 0; idx < ivl_scope_switches(net); idx += 1) {
        pint_switch_process(ivl_scope_switch(net, idx));
    }
    return ivl_scope_children(net, show_all_switch, (void *)0);
}

static int show_all_time_unit(ivl_scope_t net, void *x) {
    (void)x; // not used
    int tu_scope = ivl_scope_time_units(net);
    if (tu_scope - pint_design_time_precision > 5) {
        printf("Warning: scope %s's time_unit is 100000 times larger than time_precision(%d)!\n",
               ivl_scope_name_c(net), tu_scope - pint_design_time_precision);
    }
    return ivl_scope_children(net, show_all_time_unit, (void *)0);
}

static int bstoi(const char *ps, int size) {
    int n = 0;
    for (unsigned i = 0; i < (unsigned)size; i++) {
        n += (ps[i] - '0') * pow(2, i);
    }
    return n;
}

static int show_dump_vars(ivl_scope_t *root_scopes, unsigned nroot) {
    unsigned int idx, nparm;
    //    ivl_scope_t root_scope;
    struct dump_scops tmp_dump_scop;
    ivl_expr_type_t type;
    time_t rawtime;
    struct tm *timeinfo;
    int prec = 0;
    unsigned scale = 1;
    unsigned udx = 0;
    static const char *units_names[] = {"s", "ms", "us", "ns", "ps", "fs"};

    if (!dump_stamts.empty()) {
        pint_simu_info.need_dump = 1;
        vector<ivl_statement_t>::iterator it;
        for (it = dump_stamts.begin(); it != dump_stamts.end(); it++) {
            nparm = ivl_stmt_parm_count(*it);
            prec = ivl_scope_time_precision(root_scopes[0]);
            if (nparm == 0) {
                tmp_dump_scop.layer = 0;
                for (unsigned i = 0; i < nroot; i++)
                    tmp_dump_scop.scopes.push_back(root_scopes[i]);
            }
            if (nparm == 1) {
                tmp_dump_scop.layer = 0;
                for (unsigned i = 0; i < nroot; i++)
                    tmp_dump_scop.scopes.push_back(root_scopes[i]);
            }
            for (idx = 0; idx < nparm; idx++) {
                ivl_expr_t expr = ivl_stmt_parm(*it, idx);
                if (expr) {
                    type = ivl_expr_type(expr);
                    if (type == IVL_EX_NUMBER) {
                        tmp_dump_scop.layer = bstoi(ivl_expr_bits(expr), ivl_expr_width(expr));
                    } else if (type == IVL_EX_SCOPE) {
                        tmp_dump_scop.scopes.push_back(ivl_expr_scope(expr));
                    } else if (type == IVL_EX_SIGNAL) {
                        if (ivl_signal_array_count(ivl_expr_signal(expr)) > 1) {
                            PINT_UNSUPPORTED_ERROR(ivl_expr_file(expr), ivl_expr_lineno(expr),
                                                   "unsupported dumpvar type");
                            return -1;
                        }
                        pint_simu_info.dump_signals.push_back(ivl_expr_signal(expr));
                    } else {
                        PINT_UNSUPPORTED_ERROR(ivl_expr_file(expr), ivl_expr_lineno(expr),
                                               "unsupported dumpvar type");
                        return -1;
                    }
                }
            }
            pint_simu_info.dump_scopes.push_back(tmp_dump_scop);
            tmp_dump_scop.scopes.clear();
        }

        assert(prec >= -15);
        while (prec < 0) {
            udx += 1;
            prec += 3;
        }
        while (prec > 0) {
            scale *= 10;
            prec -= 1;
        }

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        char *time_str = asctime(timeinfo);
        int len = strlen(time_str);
        time_str[len - 1] = '\0';

        simu_header_buff.push_back("$date");
        simu_header_buff.push_back(string_format("        %s", time_str));
        simu_header_buff.push_back("$end");
        simu_header_buff.push_back("$version");
        simu_header_buff.push_back("        Icarus Verilog version " VERSION " (" VERSION_TAG ")");
        simu_header_buff.push_back("        PLS version " PLS_VERSION);
        simu_header_buff.push_back("$end");
        simu_header_buff.push_back("$timescale");
        simu_header_buff.push_back(string_format("        %d%s", scale, units_names[udx]));
        simu_header_buff.push_back("$end");
    } else {
        pint_simu_info.need_dump = 0;
    }
    return 0;
}

static int calc_one_task_stack_depth(ivl_task_stack_info_t task_stack_info,
                                     vector<ivl_task_stack_info_t> *traversed_node) {
    bool recursive_flag = false;
    unsigned i;
    for (i = 0; i < traversed_node->size(); i++) {
        if ((*traversed_node)[i] == task_stack_info) {
            recursive_flag = true;
            break;
        }
    }
    if (recursive_flag) {
        printf("Warning: There is a loop invocation in design:");
        for (; i < traversed_node->size(); i++) {
            printf("%s->", ivl_scope_name((*traversed_node)[i]->scop));
        }
        printf("%s!!!\n", ivl_scope_name(task_stack_info->scop));
        task_stack_info->max_stack_depth = MAX_TASK_STACK_SIZE;
        return task_stack_info->max_stack_depth;
    } else {
        traversed_node->push_back(task_stack_info);
    }

    if (task_stack_info->max_stack_depth == -1) {
        int stack_size;
        int inside_task_num = task_stack_info->inside_tasks.size();
        if (inside_task_num == 0) {
            if (task_stack_info->need_pc) {
                stack_size = sizeof(int) + task_stack_info->auto_sig_int_len +
                             task_stack_info->auto_sig_char_len;
            } else {
                stack_size = 0;
            }
        } else {
            int inside_task_stack_depth;
            int max_inside_task_stack_depth = 0;
            ivl_scope_t inside_task;
            for (int i = 0; i < inside_task_num; i++) {
                inside_task = task_stack_info->inside_tasks[i];
                assert(task_pc_autosig_info.count(inside_task) != 0);
                ivl_task_stack_info_t inside_task_info = task_pc_autosig_info[inside_task];

                inside_task_stack_depth =
                    calc_one_task_stack_depth(inside_task_info, traversed_node);
                if (inside_task_stack_depth > max_inside_task_stack_depth) {
                    max_inside_task_stack_depth = inside_task_stack_depth;
                }
                task_stack_info->need_pc |= inside_task_info->need_pc;
                task_stack_info->wait_events.insert(inside_task_info->wait_events.begin(),
                                                    inside_task_info->wait_events.end());
            }
            stack_size = (sizeof(int) + task_stack_info->auto_sig_int_len +
                          task_stack_info->auto_sig_char_len + max_inside_task_stack_depth) *
                         task_stack_info->need_pc;
        }
        task_stack_info->max_stack_depth =
            stack_size > MAX_TASK_STACK_SIZE ? MAX_TASK_STACK_SIZE : stack_size;
    }
    return task_stack_info->max_stack_depth;
}

static void calc_task_stack_depth() {
    map<ivl_scope_t, ivl_task_stack_info_t>::iterator it;

    for (it = task_pc_autosig_info.begin(); it != task_pc_autosig_info.end(); it++) {
        ivl_task_stack_info_t task_stack_info = it->second;
        vector<ivl_task_stack_info_t> traversed_node;
        int depth = calc_one_task_stack_depth(task_stack_info, &traversed_node);
        if (task_stack_info->need_pc) {
            PINT_LOG_HOST("Task %s stack depth 0x%x(%d) cur_depth %d need pc[%s]!\n",
                          ivl_scope_name(it->first), depth, depth,
                          ivl_task_cur_stack_size(it->first),
                          task_stack_info->need_pc ? "true" : "false");
        }
    }
}

map<ivl_nexus_t, int> Multi_drive_record;
map<ivl_nexus_t, int> Multi_drive_record_count;
map<ivl_nexus_t, string> Multi_drive_init;

static void pint_multi_drive_count(ivl_nexus_t nex) {
    pint_sig_info_t sig_info = pint_find_signal(nex);
    unsigned width = sig_info->width;
    string init_buf;
    string nexus_name =
        string_format("%s_%d", sig_info->sig_name.c_str(), pint_get_arr_idx_from_nex(nex));

    if (Multi_drive_record.count(nex) != 0) {
        Multi_drive_record[nex]++;
    } else {
        Multi_drive_init[nex] == "";
        Multi_drive_record[nex] = 1;
    }

    if (pint_parse_nex_con_info(nex).size() && (Multi_drive_record[nex] == 1)) {
        Multi_drive_record[nex]++;
        Multi_drive_record_count[nex] = 1;

        string post_fix;
        int array_idx = pint_get_arr_idx_from_nex(nex);
        if (array_idx > 0) {
            post_fix = string_format("[%d]", array_idx);
        }

        if (width <= 4) {
            init_buf +=
                string_format("*(unsigned char*)(%s_mdrv_tbl+2) = %s%s;\n", nexus_name.c_str(),
                              sig_info->sig_name.c_str(), post_fix.c_str());
        } else {
            init_buf += string_format("pint_copy_int_int_same(%s%s, %s_mdrv_tbl+2, %d);\n",
                                      sig_info->sig_name.c_str(), post_fix.c_str(),
                                      nexus_name.c_str(), width);
        }
    }

    if (width <= 4) {
        init_buf += string_format("((unsigned char*)(%s_mdrv_tbl+2))[%d] = 0xf0;\n",
                                  nexus_name.c_str(), Multi_drive_record[nex] - 1);
    } else {
        int word_num = (width + 31) / 32 * 2;
        init_buf += string_format("pint_set_sig_z(%s_mdrv_tbl+2+%d*%d, %d);", nexus_name.c_str(),
                                  Multi_drive_record[nex] - 1, word_num, width);
    }
    Multi_drive_init[nex] += init_buf;
}

static int show_all_multi_drive(ivl_scope_t net, void *x) {
    (void)x; // not used
    for (unsigned idx = 0; idx < ivl_scope_lpms(net); idx += 1) {
        ivl_nexus_t nex = ivl_lpm_q(ivl_scope_lpm(net, idx)); //  nex of output
        pint_multi_drive_count(nex);
    }

    for (unsigned idx = 0; idx < ivl_scope_logs(net); idx += 1) {
        ivl_nexus_t nex = ivl_logic_pin(ivl_scope_log(net, idx), 0); //  nex of output
        pint_multi_drive_count(nex);
    }

    return ivl_scope_children(net, show_all_multi_drive, (void *)0);
}

static void analyse_multi_drive(ivl_scope_t *root_scopes, unsigned nroot) {
    for (unsigned idx = 0; idx < nroot; idx += 1) {
        show_all_multi_drive(root_scopes[idx], 0);
    }

    for (map<ivl_nexus_t, int>::iterator it = Multi_drive_record.begin();
         it != Multi_drive_record.end();) {
        if (it->second == 1) {
            it = Multi_drive_record.erase(it);
        } else {
            it++;
        }
    }
}

static void show_monitor_exprs() {
    unsigned monitor_num = monitor_stamts.size();
    unsigned width;
    unsigned word_num;
    string express_name;

    for (unsigned i = 0; i < monitor_num; i++) {
        ivl_statement_t monitor_stm = monitor_stamts[i];
        int nparm = ivl_stmt_parm_count(monitor_stm);
        monitor_func_buff += string_format("unsigned monitor%u_update_flag = 0;\n", i);
        pint_simu_info.monitor_stat_id[monitor_stm] = i;
        for (int idx = 0; idx < nparm; idx++) {
            ivl_expr_t expr = ivl_stmt_parm(monitor_stm, idx);
            if ((expr) && (ivl_expr_type(expr) != IVL_EX_STRING) &&
                (ivl_expr_type(expr) != IVL_EX_SFUNC)) {
                width = ivl_expr_width(expr);
                express_name = string_format("monitor%u_expre_%d_result", i, idx);
                pint_simu_info.monitor_exprs_name[expr] = express_name;
                monitor_func_buff +=
                    string_format("//gen monitor %u expression %d using lpm mechanism\n", i, idx);
                if (width <= 4) {
                    monitor_func_buff +=
                        string_format("unsigned char %s[2];\n", express_name.c_str());
                } else {
                    word_num = (width + 31) / 32 * 2;
                    monitor_func_buff +=
                        string_format("unsigned int %s[2][%u];\n", express_name.c_str(), word_num);
                }
                if (strcmp(enval, enval_flag) != 0) {
                    inc_buff += string_format("void pint_lpm_%u(void);\n",
                                              global_lpm_num + pint_event_num + monitor_lpm_num);
                } else {
                    lpm_declare_buff +=
                        string_format("void pint_lpm_%u(void);\n",
                                      global_lpm_num + pint_event_num + monitor_lpm_num);
                }
                monitor_func_buff += string_format(
                    "void pint_lpm_%u(void){\n", global_lpm_num + pint_event_num + monitor_lpm_num);
                if (pint_perf_on) {
                    unsigned lpm_perf_id = global_lpm_num + monitor_lpm_num + PINT_PERF_BASE_ID;
                    monitor_func_buff +=
                        string_format("    NCORE_PERF_SUMMARY(%d);\n", lpm_perf_id);
                }
                monitor_func_buff +=
                    string_format("    pint_enqueue_flag_clear(%d);\n",
                                  global_lpm_num + pint_event_num + monitor_lpm_num);
                // nxz
                ExpressionConverter convert(expr, monitor_stm, 0, NUM_EXPRE_SIGNAL);
                string declare, operation;
                convert.GetPreparation(&operation, &declare);
                monitor_func_buff += (declare + operation);

                if (width <= 4) {
                    monitor_func_buff += string_format(
                        "    if (%s[0] != %s){\n", express_name.c_str(), convert.GetResultName());
                    monitor_func_buff += string_format(
                        "        %s[0] = %s;\n", express_name.c_str(), convert.GetResultName());
                } else {
                    monitor_func_buff += string_format("unsigned char equal_flag;\n");
                    monitor_func_buff +=
                        string_format("pint_case_equality_int(&equal_flag,%s[0],%s,%u);\n",
                                      express_name.c_str(), convert.GetResultName(), width);
                    monitor_func_buff += string_format("    if (!equal_flag){\n");
                    monitor_func_buff +=
                        string_format("        pint_copy_int_int_same(%s,%s[0],%u);\n",
                                      convert.GetResultName(), express_name.c_str(), width);
                }
                monitor_func_buff += string_format("        monitor%u_update_flag = 1;\n", i);
                monitor_func_buff += "    }\n}\n";
                const set<pint_sig_info_t> *sensitive_sigs = convert.Sensitive_Signals();
                set<pint_sig_info_t>::iterator it;
                pint_sig_info_t sig_info;
                for (it = sensitive_sigs->begin(); it != sensitive_sigs->end(); it++) {
                    sig_info = *it;
                    pint_sig_info_add_lpm(sig_info, 0xffffffff, global_lpm_num + monitor_lpm_num);
                }
                monitor_lpm_num++;
            }
        }
    }
}

//// dataflow analysis sample
//============================================================================================
// this is a example for using `analysis_massign()`
// static int massign_process(const ivl_process_t net, void *) {
//    dfa_massign_t up_result;
//    dfa_massign_t down_result;
//    std::set<ivl_signal_t> nb_lval_signals;
//    std::set<ivl_signal_t> lval_signals;
//
//    int re = analysis_massign(net, &down_result, &up_result, &nb_lval_signals, &lval_signals);
//    // assert(0==re);
//    if (re == 0) {
//        __DebugBegin(2);
//        // fprintf(stdout, "\n%s:%d.process:\n",
//        //    ivl_process_file(net),
//        //    ivl_process_lineno(net));
//        fprintf(stdout, "up_result:\n");
//        display_massign_result(stdout, &up_result);
//
//        fprintf(stdout, "down_result:\n");
//        display_massign_result(stdout, &down_result);
//        __DebugEnd;
//    } else {
//        __DebugBegin(2);
//        fprintf(stdout, "\n%s:%d.DFA process not support:\n", ivl_process_file(net),
//                ivl_process_lineno(net));
//        __DebugEnd;
//    }
//
//    return 0;
//}
static int massign_process_2(const ivl_process_t net, void *x) {
    (void)x; // not used
    up_down_t up_result;
    up_down_t down_result;
    // std::set<ivl_signal_t> nb_lval_signals;
    // std::set<ivl_signal_t> lval_signals;
    assign_cnt_t assigncnt;

    int re = analysis_massign_v2(net, &down_result, &up_result, &assigncnt);
    if (re == 0) {
        __DebugBegin(10);
        fprintf(stdout, "\n%s:%d.process:\n", ivl_process_file(net), ivl_process_lineno(net));

        fprintf(stdout, "assigncnt:\n");
        for (const auto &pair : assigncnt) {
            fprintf(stdout, "[%s, %u], ", ivl_signal_basename(pair.first), pair.second);
        }
        fprintf(stdout, "\n");

        fprintf(stdout, "up_result:\n");
        display_massign_result_2(stdout, &up_result);

        fprintf(stdout, "down_result:\n");
        display_massign_result_2(stdout, &down_result);

        fprintf(stdout, "\n-----------------------------------------------------------\n");
        __DebugEnd;
    } else {
        __DebugBegin(10);
        fprintf(stdout, "\n%s:%d.DFA process not support:\n", ivl_process_file(net),
                ivl_process_lineno(net));
        __DebugEnd;
    }

    return 0;
}

//============================================================================================
// get lval assign in multi always
struct multi_assign_lval_s {
    pint_sig_info_t sig_info;
    unsigned last_proc_num;
    unsigned is_multi;
};
typedef struct multi_assign_lval_s *multi_assign_lval_t;

map<pint_sig_info_t, multi_assign_lval_t> pint_multi_assign_lval;
unsigned wait_delay = 0;
unsigned pint_proc_lval_num = 0;

void pint_parse_multi_assign_lval(ivl_statement_t net) {
    if (!net)
        return;
    ivl_statement_type_t type = ivl_statement_type(net);
    unsigned num = 0;
    unsigned i;
    switch (type) {
    //  Type that has sub stmt
    case IVL_ST_BLOCK:          //  4, block_
    case IVL_ST_FORK:           //  16
    case IVL_ST_FORK_JOIN_ANY:  //  28
    case IVL_ST_FORK_JOIN_NONE: //  29
        num = ivl_stmt_block_count(net);
        for (i = 0; i < num; i++) {
            pint_parse_multi_assign_lval(ivl_stmt_block_stmt(net, i));
        }
        break;
    case IVL_ST_CASE:  //  5, case_
    case IVL_ST_CASEX: //  6
    case IVL_ST_CASEZ: //  7
    case IVL_ST_CASER: //  24, /* Case statement with real expressions. */
        num = ivl_stmt_case_count(net);
        for (i = 0; i < num; i++) {
            pint_parse_multi_assign_lval(ivl_stmt_case_stmt(net, i));
        }
        break;

    case IVL_ST_CONDIT: //  9
        pint_parse_multi_assign_lval(ivl_stmt_cond_true(net));
        pint_parse_multi_assign_lval(ivl_stmt_cond_false(net));
        break;
    case IVL_ST_DELAY:  //  11
    case IVL_ST_DELAYX: //  12
        wait_delay = 0;
        pint_parse_multi_assign_lval(ivl_stmt_sub_stmt(net));
        break;

    case IVL_ST_FOREVER:  //  15
    case IVL_ST_REPEAT:   //  18
    case IVL_ST_WHILE:    //  23
    case IVL_ST_DO_WHILE: //  30
        pint_parse_multi_assign_lval(ivl_stmt_sub_stmt(net));
        break;
    case IVL_ST_WAIT: //  22
        wait_delay = 0;
        pint_parse_multi_assign_lval(ivl_stmt_sub_stmt(net));
        break;
    //  Type that force related
    case IVL_ST_FORCE:   //  14
    case IVL_ST_RELEASE: //  17
        break;
    case IVL_ST_CASSIGN:  //  8
    case IVL_ST_DEASSIGN: //  10
    case IVL_ST_ASSIGN:
    case IVL_ST_ASSIGN_NB: //  3, used to collect signal has_nb info
    {
        if (!wait_delay) {
            unsigned nlval = ivl_stmt_lvals(net);
            pint_sig_info_t sig_info;
            unsigned i;
            for (i = 0; i < nlval; i++) {
                sig_info = pint_find_signal(ivl_lval_sig(ivl_stmt_lval(net, i)));
                if (sig_info) {
                    if (!(ivl_scope_type(ivl_signal_scope(sig_info->sig)) == IVL_SCT_BEGIN ||
                          sig_info->is_const || sig_info->is_local)) {
                        if (pint_multi_assign_lval.count(sig_info)) {
                            if (pint_proc_lval_num !=
                                pint_multi_assign_lval[sig_info]->last_proc_num) {
                                pint_multi_assign_lval[sig_info]->is_multi = 1;
                            }
                        } else {
                            multi_assign_lval_t multi_lval = new struct multi_assign_lval_s;
                            multi_lval->sig_info = sig_info;
                            multi_lval->is_multi = 0;
                            multi_lval->last_proc_num = pint_proc_lval_num;
                            pint_multi_assign_lval[sig_info] = multi_lval;
                        }
                    }
                }
            }
        }
        break;
    }
    default:
        break;
    }
}

int pint_process_multi_assign_lval(ivl_process_t net, void *x) {
    // T = 0 , INITAL lval dont care
    if (ivl_process_type(net) == IVL_PR_INITIAL) {
        wait_delay = 1;
    } else {
        wait_delay = 0;
    }
    pint_parse_multi_assign_lval(ivl_process_stmt(net));
    pint_proc_lval_num++;
    (void)x;
    return 0;
}

void pint_parse_expr(ivl_statement_t net) {
    if (!net)
        return;
    ivl_statement_type_t type = ivl_statement_type(net);
    unsigned num = 0;
    unsigned i;
    switch (type) {
    //  Type that has sub stmt
    case IVL_ST_BLOCK:          //  4, block_
    case IVL_ST_FORK:           //  16
    case IVL_ST_FORK_JOIN_ANY:  //  28
    case IVL_ST_FORK_JOIN_NONE: //  29
        num = ivl_stmt_block_count(net);
        for (i = 0; i < num; i++) {
            pint_parse_expr(ivl_stmt_block_stmt(net, i));
        }
        break;

    case IVL_ST_STASK:
    case IVL_ST_UTASK:
        pint_process_info[pint_process_num]->always_run = 1;
        return;

    case IVL_ST_CASE:  //  5, case_
    case IVL_ST_CASEX: //  6
    case IVL_ST_CASEZ: //  7
    case IVL_ST_CASER: //  24, /* Case statement with real expressions. */
        num = ivl_stmt_case_count(net);
        for (i = 0; i < num; i++) {
            pint_parse_expr(ivl_stmt_case_stmt(net, i));
        }
        pint_show_expression(ivl_stmt_cond_expr(net));
        break;

    case IVL_ST_CONDIT: //  9
        pint_show_expression(ivl_stmt_cond_expr(net));
        pint_parse_expr(ivl_stmt_cond_true(net));
        pint_parse_expr(ivl_stmt_cond_false(net));
        break;
    case IVL_ST_DELAY:   //  11
    case IVL_ST_DELAYX:  //  12
    case IVL_ST_FOREVER: //  15
        pint_parse_expr(ivl_stmt_sub_stmt(net));
        break;
    case IVL_ST_REPEAT: //  18
    case IVL_ST_WHILE:  //  23
        pint_show_expression(ivl_stmt_cond_expr(net));
        pint_parse_expr(ivl_stmt_sub_stmt(net));
        break;

    case IVL_ST_DO_WHILE: //  30
    case IVL_ST_WAIT:     //  22
        pint_parse_expr(ivl_stmt_sub_stmt(net));
        break;

    //  Type that force related
    case IVL_ST_FORCE:   //  14
    case IVL_ST_CASSIGN: //  8
    case IVL_ST_ASSIGN:
    case IVL_ST_ASSIGN_NB: //  3, used to collect signal has_nb info
        pint_show_expression(ivl_stmt_rval(net));
        break;

    default:
        break;
    }
}

int pint_process_get_sens_map(ivl_process_t net, void *x) {
    if (ivl_process_type(net) == IVL_PR_INITIAL) {
        if (pint_init_is_sig_init(net)) {
            return 0;
        }
    }
    if (net->only_one_wait_event) {
        pint_process_info_t proc_info_tmp = new struct pint_process_info_s;
        proc_info_tmp->proc = net;
        proc_info_tmp->flag_id = pint_process_num;
        proc_info_tmp->always_run = net->always_run;
        pint_process_info[pint_process_num] = proc_info_tmp;
        pint_process_info_num++;

        // find sfunc expr
        pint_parse_expr(ivl_process_stmt(net));
        if (proc_info_tmp->always_run) {
            always_run_buff += string_format("    pint_event_flag[%d] = 0x55;\n", pint_process_num);
        } else {
            analysis_process(ivl_process_stmt(net), 0, &proc_info, 2);
            pint_sig_info_t sig_info;
            for (int i = 0; i < proc_info.n_l_sig; i++) {
                sig_info = pint_find_signal(proc_info.sig_list[i]);
                // if sig_info in multi-assign map add to sens map
                if (pint_multi_assign_lval.count(sig_info)) {
                    if (pint_multi_assign_lval[sig_info]->is_multi) {
                        pint_add_process_sens_signal(sig_info, pint_process_num);
                    }
                }
            }

            for (int i = 0; i < proc_info.n_sig; i++) {
                sig_info = pint_find_signal(proc_info.sig_list[i]);
                if (((*proc_info.stmt_in) + i)->sensitive == 1) {
                    pint_add_process_sens_signal(sig_info, pint_process_num);
                }
            }
#if 0
        // printf("process:%s:%d\n", ivl_stmt_file(ivl_process_stmt(net)), ivl_stmt_lineno(ivl_process_stmt(net)));
        // printf("n_l_sig=%d n_sig=%d\n", proc_info.n_l_sig, proc_info.n_sig);
        // printf("*************  signals: ");
        // printf("\n");
        for (int i = 0; i < proc_info.n_l_sig; i++)
        {
            printf("%d %s file:%s line:%d\n", ((*proc_info.stmt_in) + i)->sensitive, ivl_signal_basename(proc_info.sig_list[i]), ivl_signal_file(proc_info.sig_list[i]), ivl_signal_lineno(proc_info.sig_list[i]));
        }
        printf("\n");

        // for (int i = proc_info.n_l_sig; i < proc_info.n_sig; i++)
        // {
        //     printf("%d %s file:%s line:%d\n", ((*proc_info.stmt_in) + i)->sensitive, ivl_signal_basename(proc_info.sig_list[i]), ivl_signal_file(proc_info.sig_list[i]), ivl_signal_lineno(proc_info.sig_list[i]));
        // }
        // printf("\n");
#endif
            analysis_free_process_info(&proc_info);
        }
    }

    if (!net->to_lpm_flag) {
        pint_process_num++;
    }
    pint_totol_process_num++;
    (void)x;
    return 0;
}

/******************************************************************************** Std length -100 */
//  Loop each stmt & expr in design(proc, func, task)
void pint_loop_stmt_in_proc(ivl_statement_t net, pint_loop_stmt_t stmt_env_t);
void pint_loop_expr_in_stmt(ivl_expr_t net, pint_loop_stmt_t stmt_env_t);

int pint_loop_func_task_in_dsgn(ivl_scope_t net, void *x) {
    (void)x;
    ivl_scope_type_t type = ivl_scope_type(net);
    if (type == IVL_SCT_TASK || type == IVL_SCT_FUNCTION) {
        ivl_statement_t sub_stmt = ivl_scope_def(net);
        struct pint_loop_stmt_s stmt_env = {.temp_scope = net, .temp_proc = NULL};
        pint_loop_stmt_in_proc(sub_stmt, &stmt_env);
    }
    return 0;
}

int pint_loop_proc_in_dsgn(ivl_process_t net, void *x) {
    (void)x;
    if (!pint_init_is_sig_init(net)) {
        dsgn_proc_is_thread.insert(net);
        ivl_statement_t sub_stmt = ivl_process_stmt(net);
        struct pint_loop_stmt_s stmt_env = {.temp_scope = NULL, .temp_proc = net};
        pint_loop_stmt_in_proc(sub_stmt, &stmt_env);
    }
    return 0;
}

void pint_loop_stmt_in_proc(ivl_statement_t net, pint_loop_stmt_t stmt_env_t) {
    if (!net) {
        return;
    }
    struct pint_loop_stmt_s stmt_env = *stmt_env_t;
    ivl_statement_type_t type = ivl_statement_type(net);
    unsigned is_force = 0;
    unsigned is_assign = 0;
    switch (type) {
    //  Stmt has sub
    case IVL_ST_BLOCK:          //  4, block_
    case IVL_ST_FORK:           //  16
    case IVL_ST_FORK_JOIN_ANY:  //  28
    case IVL_ST_FORK_JOIN_NONE: //  29
    {
        unsigned num = ivl_stmt_block_count(net);
        for (unsigned i = 0; i < num; i++) {
            pint_loop_stmt_in_proc(ivl_stmt_block_stmt(net, i), &stmt_env);
        }
    } break;
    case IVL_ST_CASE:  //  5, case_
    case IVL_ST_CASEX: //  6
    case IVL_ST_CASEZ: //  7
    case IVL_ST_CASER: //  24, /* Case statement with real expressions. */
    {
        unsigned num = ivl_stmt_case_count(net);
        unsigned i;
        for (i = 0; i < num; i++) {
            pint_loop_stmt_in_proc(ivl_stmt_case_stmt(net, i), &stmt_env);
        }
        pint_loop_expr_in_stmt(ivl_stmt_cond_expr(net), &stmt_env);
        for (i = 0; i < num; i++) {
            pint_loop_expr_in_stmt(ivl_stmt_case_expr(net, i), &stmt_env);
        }
    } break;
    case IVL_ST_CONDIT: //  9,  condit_
        pint_loop_stmt_in_proc(ivl_stmt_cond_true(net), &stmt_env);
        pint_loop_stmt_in_proc(ivl_stmt_cond_false(net), &stmt_env);
        pint_loop_expr_in_stmt(ivl_stmt_cond_expr(net), &stmt_env);
        break;
    case IVL_ST_DELAY: //  11, delay_
        pint_loop_stmt_in_proc(ivl_stmt_sub_stmt(net), &stmt_env);
        break;
    case IVL_ST_DELAYX: //  12, delayx_
        pint_loop_stmt_in_proc(ivl_stmt_sub_stmt(net), &stmt_env);
        pint_loop_expr_in_stmt(ivl_stmt_delay_expr(net), &stmt_env);
        break;
    case IVL_ST_FOREVER: //  15, forever_
        pint_loop_stmt_in_proc(ivl_stmt_sub_stmt(net), &stmt_env);
        break;
    case IVL_ST_REPEAT:   //  18, while_
    case IVL_ST_WHILE:    //  23
    case IVL_ST_DO_WHILE: //  30
        pint_loop_stmt_in_proc(ivl_stmt_sub_stmt(net), &stmt_env);
        pint_loop_expr_in_stmt(ivl_stmt_cond_expr(net), &stmt_env);
        break;
    case IVL_ST_WAIT: //  22, wait_
        pint_loop_stmt_in_proc(ivl_stmt_sub_stmt(net), &stmt_env);
        break;
    /************************************************************/
    case IVL_ST_NONE: //  0
    case IVL_ST_NOOP: //  1
        break;
    case IVL_ST_ASSIGN: //  2,  assign_
    {                   //  part_asgn & asgn_in_mult_threads
        unsigned nlval = ivl_stmt_lvals(net);
        ivl_lval_t lval;
        unsigned i;
        for (i = 0; i < nlval; i++) {
            lval = ivl_stmt_lval(net, i);
            pint_loop_expr_in_stmt(ivl_lval_part_off(lval), &stmt_env);
            pint_loop_expr_in_stmt(ivl_lval_idx(lval), &stmt_env);

            pint_parse_asgn_lval(lval, &stmt_env);
        }
        pint_loop_expr_in_stmt(ivl_stmt_rval(net), &stmt_env);
        pint_loop_expr_in_stmt(ivl_stmt_delay_expr(net), &stmt_env);
    } break;
    case IVL_ST_ASSIGN_NB: //  3
    {                      //  sig_info->has_nb
        unsigned nlval = ivl_stmt_lvals(net);
        ivl_lval_t lval;
        pint_sig_info_t sig_info;
        unsigned i;
        for (i = 0; i < nlval; i++) {
            lval = ivl_stmt_lval(net, i);
            pint_loop_expr_in_stmt(ivl_lval_part_off(lval), &stmt_env);
            pint_loop_expr_in_stmt(ivl_lval_idx(lval), &stmt_env);

            sig_info = pint_find_signal(ivl_lval_sig(lval));
            if (sig_info) {
                sig_info->has_nb = 1;
            }
        }
        pint_loop_expr_in_stmt(ivl_stmt_rval(net), &stmt_env);
        pint_loop_expr_in_stmt(ivl_stmt_delay_expr(net), &stmt_env);
    } break;
    case IVL_ST_CASSIGN: //  8, Note: no break here
        pint_force_idx[net] = force_lpm_num++;
        pint_force_lpm.push_back(net);
        pint_force_type.push_back(1);
        is_assign = 1;
        pint_loop_expr_in_stmt(ivl_stmt_rval(net), &stmt_env);
    case IVL_ST_DEASSIGN: //  10
    {
        unsigned nlval = ivl_stmt_lvals(net);
        ivl_lval_t lval;
        unsigned i;
        for (i = 0; i < nlval; i++) {
            lval = ivl_stmt_lval(net, i);
            pint_loop_expr_in_stmt(ivl_lval_part_off(lval), &stmt_env);
            pint_loop_expr_in_stmt(ivl_lval_idx(lval), &stmt_env);

            pint_parse_lval_force_info(lval, net, is_force, is_assign);
        }
    } break;
    case IVL_ST_FORCE: //  14, Note: no break here
        pint_force_idx[net] = force_lpm_num++;
        pint_force_lpm.push_back(net);
        pint_force_type.push_back(0);
        is_force = 1;
        pint_loop_expr_in_stmt(ivl_stmt_rval(net), &stmt_env);
    case IVL_ST_RELEASE: //  17
    {
        unsigned nlval = ivl_stmt_lvals(net);
        ivl_lval_t lval;
        unsigned i;
        for (i = 0; i < nlval; i++) {
            lval = ivl_stmt_lval(net, i);
            pint_loop_expr_in_stmt(ivl_lval_part_off(lval), &stmt_env);
            pint_loop_expr_in_stmt(ivl_lval_idx(lval), &stmt_env);

            pint_parse_lval_force_info(lval, net, is_force, is_assign);
        }
    } break;
    case IVL_ST_DISABLE: //  13, disable_
        break;
    case IVL_ST_STASK: //  19, stask_
    {
        unsigned num = ivl_stmt_parm_count(net);
        unsigned i;
        for (i = 0; i < num; i++) {
            pint_loop_expr_in_stmt(ivl_stmt_parm(net, i), &stmt_env);
        }
    } break;
    case IVL_ST_TRIGGER: //  20, wait_
        break;
    case IVL_ST_UTASK: //  21, utask_
        pint_parse_task_call(ivl_stmt_call(net), &stmt_env);
        break;
    case IVL_ST_ALLOC: //  25, alloc_
        break;
    case IVL_ST_FREE: //  26, free_
        break;
    case IVL_ST_CONTRIB: //  27, contrib_
        pint_loop_expr_in_stmt(ivl_stmt_lexp(net), &stmt_env);
        pint_loop_expr_in_stmt(ivl_stmt_rval(net), &stmt_env);
        break;
    default:
        break;
    }
}

void pint_loop_expr_in_stmt(ivl_expr_t net, pint_loop_stmt_t stmt_env_t) {
    if (!net) {
        return;
    }
    struct pint_loop_stmt_s stmt_env = *stmt_env_t;
    ivl_expr_type_t type = ivl_expr_type(net);
    switch (type) {
    case IVL_EX_NONE: //  0,
        break;
    case IVL_EX_BINARY: //  2, binary_
        pint_loop_expr_in_stmt(ivl_expr_oper1(net), &stmt_env);
        pint_loop_expr_in_stmt(ivl_expr_oper2(net), &stmt_env);
        break;
    case IVL_EX_CONCAT: //  3, concat_
    {
        unsigned num = ivl_expr_parms(net);
        for (unsigned i = 0; i < num; i++) {
            pint_loop_expr_in_stmt(ivl_expr_parm(net, i), &stmt_env);
        }
    } break;
    case IVL_EX_MEMORY: //  4, memory_
        pint_loop_expr_in_stmt(ivl_expr_oper1(net), &stmt_env);
        break;
    case IVL_EX_NUMBER: //  5, number_
        break;
    case IVL_EX_SCOPE: //  6, scope_
        break;
    case IVL_EX_SELECT: //  7, select_
        pint_loop_expr_in_stmt(ivl_expr_oper1(net), &stmt_env);
        pint_loop_expr_in_stmt(ivl_expr_oper2(net), &stmt_env);
        break;
    case IVL_EX_SFUNC: //  8, sfunc_
    {
        unsigned num = ivl_expr_parms(net);
        for (unsigned i = 0; i < num; i++) {
            pint_loop_expr_in_stmt(ivl_expr_parm(net, i), &stmt_env);
        }
    } break;
    case IVL_EX_SIGNAL: //  9, signal_
        pint_loop_expr_in_stmt(ivl_expr_oper1(net), &stmt_env);
        break;
    case IVL_EX_STRING: //  10, string_
        break;
    case IVL_EX_TERNARY: //  11, ternary_
        pint_loop_expr_in_stmt(ivl_expr_oper1(net), &stmt_env);
        pint_loop_expr_in_stmt(ivl_expr_oper2(net), &stmt_env);
        pint_loop_expr_in_stmt(ivl_expr_oper3(net), &stmt_env);
        break;
    case IVL_EX_UFUNC: //  12, ufunc_
    {
        unsigned num = ivl_expr_parms(net);
        for (unsigned i = 0; i < num; i++) {
            pint_loop_expr_in_stmt(ivl_expr_parm(net, i), &stmt_env);
        }
        pint_parse_func_call(ivl_expr_def(net), &stmt_env);
    } break;
    case IVL_EX_ULONG: //  13, ulong_
        break;
    case IVL_EX_UNARY: //  14, unary_
        pint_loop_expr_in_stmt(ivl_expr_oper1(net), &stmt_env);
        break;
    case IVL_EX_REALNUM: //  16, real_
        break;
    case IVL_EX_EVENT: //  17, event_
        break;
    case IVL_EX_ARRAY: //  18, signal_
        break;
    case IVL_EX_BACCESS: //  19, branch_
        break;
    case IVL_EX_DELAY: //  20, delay_
        break;
    case IVL_EX_ENUMTYPE: //  21, enumtype_
        break;
    case IVL_EX_NULL: //  22,
        break;
    case IVL_EX_NEW: //  23, new_
        pint_loop_expr_in_stmt(ivl_expr_oper1(net), &stmt_env);
        pint_loop_expr_in_stmt(ivl_expr_oper2(net), &stmt_env);
        break;
    case IVL_EX_PROPERTY: //  24, property_
        pint_loop_expr_in_stmt(ivl_expr_oper1(net), &stmt_env);
        break;
    case IVL_EX_SHALLOWCOPY: //  25, shallow_
        pint_loop_expr_in_stmt(ivl_expr_oper1(net), &stmt_env);
        pint_loop_expr_in_stmt(ivl_expr_oper2(net), &stmt_env);
        break;
    case IVL_EX_ARRAY_PATTERN: //  26, array_pattern_
    {
        unsigned num = ivl_expr_parms(net);
        for (unsigned i = 0; i < num; i++) {
            pint_loop_expr_in_stmt(ivl_expr_parm(net, i), &stmt_env);
        }
    } break;
    default:
        break;
    }
}

/******************************************************************************** Std length -100 */
int target_design(ivl_design_t des) {
    pint_design_time_precision = ivl_design_time_precision(des);
    ivl_scope_t *root_scopes;
    unsigned nroot = 0;
    unsigned idx;
    char *root_path;
    bool is_path = false;
    size_t write_bytes;
    int systm_ret;
    string chmod_cmd;

    struct timeval time_begin, time_end;
    double codegen_time, b_time, e_time;
    PINT_TIME_STAT_INIT
    gettimeofday(&time_begin, NULL);
    if ((root_path = getcwd(NULL, 0)) == NULL) {
        printf("get root_path error!\n");
        return -1;
    }
    if (!(enval = getenv("PINT_COMPILE_MODE"))) {
        enval = "NORMAL";
    }

    const char *log_flag = getenv("PINT_GENCODE_LOG");
    if (log_flag != NULL) {
        if ((strcmp(log_flag, "ON") == 0) || (strcmp(log_flag, "on") == 0)) {
            g_log_print_flag = true;
        }
    }

    gettimeofday(&time_begin, NULL);
    if (strcmp(enval, enval_flag) != 0) {
        systm_ret = system("rm -rf thread*.c lpm*.c thread.h lpm.h pint_thread_stub.h");
    }
#if 0
    pid_t cur_pid = getpid();
    printf("This is pid %d\n", cur_pid);
    sleep(10);
#endif

    const char *path = ivl_design_flag(des, "-o");
    if (path == 0) {
        return -1;
    }

    out = fopen(path, "w");
    if (out == 0) {
        perror(path);
        return -2;
    }

    dir_name = string_format("%s_pint_simu", path);
    string mkdir_cmd =
        string_format("mkdir -p %s/src_file && mkdir -p %s/out_file && rm -rf %s/src_file/*",
                      dir_name.c_str(), dir_name.c_str(), dir_name.c_str());
    systm_ret = system(mkdir_cmd.c_str());
    for (unsigned int idx = 0; idx <= strlen(path); idx++) {
        if (path[idx] == '/') {
            is_path = true;
        }
    }

    string pint_thread_stub_src_name =
        string_format("%s/src_file/pint_thread_stub.c", dir_name.c_str());
    if ((pint_thread_fd = open(pint_thread_stub_src_name.c_str(),
                               O_RDWR | O_CREAT | O_TRUNC | O_SYNC, 0666)) < 0) {
        printf("create pint_thread.c file failed.\n");
        return -2;
    }

    string pint_log_file_name = string_format("%s/pls.log", dir_name.c_str());
    pls_log_fp = fopen(pint_log_file_name.c_str(), "w+");

    // enable cmn env
    char *pint_cmn_env = NULL;
    pint_cmn_env = getenv("PINT_MODE_CMN");
    if (pint_cmn_env) {
        if (!strcmp(pint_cmn_env, "OFF")) {
            pint_min_inst = 0xffffffff;
        } else {
            pint_min_inst = PINT_MIN_INST_NUM;
        }
    }
    // pint_min_inst = 0xffffffff;

    // env for perf counter id generate
    char *pint_perf_env = NULL;
    pint_perf_env = getenv("PINT_MODE_PERF");
    if (pint_perf_env) {
        if (!strcmp(pint_perf_env, "ON")) {
            pint_perf_on = 1;
        } else {
            pint_perf_on = 0;
        }
    }

    // enable pli mode
    char *pint_pli_mode_env = NULL;
    pint_pli_mode_env = getenv("PINT_PLI_MODE");
    if (pint_pli_mode_env) {
        if (!strcmp(pint_pli_mode_env, "OFF")) {
            pint_pli_mode = 0;
        } else {
            pint_pli_mode = 1;
        }
    }

    for (idx = 0; idx < ivl_design_disciplines(des); idx += 1) {
        // fprintf(out, "discipline %s\n", ivl_discipline_name(dis));
    }

    ivl_design_roots(des, &root_scopes, &nroot);
    calc_task_stack_depth();
    pint_dump_init();
    if (show_dump_vars(root_scopes, nroot) != 0) {
        printf("show dump fail!\n");
        return -2;
    }
    t_others += pint_exec_duration(st, ed, sts);
    for (idx = 0; idx < nroot; idx += 1) {

        ivl_scope_t cur = root_scopes[idx];
        // printf("root scope name: %s idx: %d nroot: %d\n", ivl_scope_name(cur), idx, nroot);
        switch (ivl_scope_type(cur)) {
        case IVL_SCT_TASK:
            // fprintf(out, "task = %s\n", ivl_scope_name(cur));
            break;
        case IVL_SCT_FUNCTION:
            // fprintf(out, "function = %s\n", ivl_scope_name(cur));
            break;
        case IVL_SCT_CLASS:
            // fprintf(out, "class = %s\n", ivl_scope_name(cur));
            break;
        case IVL_SCT_PACKAGE:
            // fprintf(out, "package = %s\n", ivl_scope_name(cur));
            break;
        case IVL_SCT_MODULE:
            // fprintf(out, "root module = %s\n", ivl_scope_name(cur));
            break;
        default:
            // fprintf(out, "ERROR scope %s unknown type\n", ivl_scope_name(cur));
            stub_errors += 1;
            break;
        }
        //##    Warning: The order of those "show_all_xx" must be keeped. Do not change
        gettimeofday(&st, NULL);
        if (pint_pli_mode) {
            show_all_port(root_scopes[idx], 0);
        }
        show_all_sig(root_scopes[idx], 0);
        t_show_sig += pint_exec_duration(st, ed, sts);
        show_all_event(root_scopes[idx], 0);
        t_show_evt += pint_exec_duration(st, ed, sts);
        show_all_log(root_scopes[idx], 0);
        show_all_lpm(root_scopes[idx], 0);
        t_show_lpm += pint_exec_duration(st, ed, sts);
        show_all_switch(root_scopes[idx], 0);
        pint_give_lpm_no_need_init_idx();
        show_all_time_unit(root_scopes[idx], 0);
        pint_event_num_actual = pint_event_num;
        while (pint_event_num & 0x1f)
            pint_event_num++; // pad to 32-bit boundary
        t_others += pint_exec_duration(st, ed, sts);
    }
    gettimeofday(&st, NULL);
    analyse_multi_drive(root_scopes, nroot);

    /*
        massign sample
    */
    // ivl_design_process(des, massign_process_2, (void *)0);   //  debug
    t_others += pint_exec_duration(st, ed, sts);
    ivl_design_scope(des, pint_loop_func_task_in_dsgn, (void *)0);
    ivl_design_process(des, pint_loop_proc_in_dsgn, (void *)0);
    pint_clct_asgn_in_thread();
    pint_parse_dsgn_signal_asgn_info();
    t_pre_parse += pint_exec_duration(st, ed, sts);
    /* add thread change flag process
     * 1\get multi assign lval, assign in multi always
     * 2\set process always run flag
     * 3\get signal's sens-process list and process's sens-signal list
     */
    ivl_design_process(des, pint_process_multi_assign_lval, (void *)0);
    ivl_design_process(des, pint_process_get_sens_map, (void *)0);

    t_show_force += pint_exec_duration(st, ed, sts);
    if (pint_simu_info.need_dump)
        simu_header_buff.push_back("$enddefinitions $end");

    while (udp_define_list) {
        struct udp_define_cell *cur = udp_define_list;
        udp_define_list = cur->next;
        show_primitive(cur->udp, cur->ref);
        free(cur);
    }

    for (idx = 0; idx < ivl_design_consts(des); idx += 1) {
        ivl_net_const_t con = ivl_design_const(des, idx);
        show_constant(con);
    }
    pint_event_add_list();
    pint_lpm_add_list();
    t_others += pint_exec_duration(st, ed, sts);
    show_monitor_exprs();
    t_show_monitor += pint_exec_duration(st, ed, sts);
    thread_buff += "\n//process dump\n";
    total_thread_buff += thread_buff;
    ivl_design_process(des, show_process_add_list, (void *)0);

    for (unsigned ii = 0; ii < mcc_event_info.size(); ii++) {
        pint_event_info_t event_info = mcc_event_info[ii];
        bool find_flag = false;
        for (unsigned jj = 0; jj < mcc_event_ids.size(); jj++) {
            if (event_info->event_id == (unsigned)mcc_event_ids[jj]) {
                find_flag = true;
                break;
            }
        }
        if (!find_flag) {
            for (unsigned idx = 0; idx < event_info->any_num; idx += 1) {
                ivl_nexus_t nex = ivl_event_any(event_info->evt, idx);
                pint_sig_info_add_event(nex, event_info->event_id, ANY);
            }
        }
    }

    // dataflow analysis sample.
    // ivl_design_process(des, massign_process, (void *)0);
    t_others += pint_exec_duration(st, ed, sts);
    pint_parse_all_force_shadow_lpm();
    pint_parse_all_force_const_lpm();
    pint_dump_force_lpm(); //  Note: pint_dump_force_lpm must be excuted before show_process
    t_show_force += pint_exec_duration(st, ed, sts);
    thread_buff = "\n//function task dump\n";
    for (unsigned idx = 0; idx < nroot; idx += 1) {
        show_all_func(root_scopes[idx], 0);
        show_all_task(root_scopes[idx], 0);
    }
    total_thread_buff += thread_buff;
    t_show_func += pint_exec_duration(st, ed, sts);
    ivl_design_process(des, pint_dsgn_clct_proc, (void *)0);
    pint_dump_process();
    t_show_proc += pint_exec_duration(st, ed, sts);
    pint_dump_thread_lpm();
    pint_dump_lpm();
    lpm_declare_buff += force_decl_buff;
    lpm_buff += force_buff;
    lpm_declare_buff += force_app_decl_buff;
    lpm_buff += force_app_buff;
    force_decl_buff.clear();
    force_buff.clear();
    force_app_decl_buff.clear();
    force_app_buff.clear();
    pint_dump_signal(); //  Note: dump signal can not be executed before pint_dump_force_lpm

    assert((pint_task_pc_max < 65536) && (pint_thread_pc_max < 65536));

    string macro_PINT_TASK_PC_common = "do { switch(*pint_task_pc_x) {case 0: break; ";
    for (unsigned i = 1; i <= pint_task_pc_max; i++) {
        macro_PINT_TASK_PC_common += string_format("case %u: goto PC_%u; ", i, i);
        thread_declare_buff += string_format("#define PINT_TASK_PC_%u() ", i) +
                               macro_PINT_TASK_PC_common + " } } while(0)\n";
    }

    // generate thread goto label related info
    if (pint_thread_num) {
        std::ostringstream str_pint_thread_num;
        str_pint_thread_num << pint_thread_num;
        start_buff +=
            ("unsigned short pint_thread_pc_x[" + str_pint_thread_num.str() + "] = {0};\n");
        thread_declare_buff +=
            ("\nextern unsigned short pint_thread_pc_x[" + str_pint_thread_num.str() + "];\n");
    }

    string macro_PINT_THREAD_PC_common = "do { switch(pint_thread_pc_x[id]) {case 0: break; ";
    for (unsigned i = 1; i <= pint_thread_pc_max; i++) {
        macro_PINT_THREAD_PC_common += string_format("case %u: goto PC_%u; ", i, i);
        thread_declare_buff += string_format("#define PINT_PROCESS_PC_%u(id) ", i) +
                               macro_PINT_THREAD_PC_common + " } } while(0)\n";
    }

    pint_dump_event();
    unsigned thread_q_len = pint_thread_num + global_lpm_num + monitor_lpm_num + force_lpm_num +
                            pint_event_num + pint_delay_num;
    unsigned flag_len = (thread_q_len + pint_nbassign_array_num + pint_nbassign_num) / 16 + 16;
    // array init
    if (pint_array_init_num) {
        array_init_buff += string_format("};\n");
        start_buff += string_format("unsigned pint_array_init_num = %d;\n", pint_array_init_num);
    } else {
        array_init_buff +=
            string_format("\nstruct pint_array_info array_info[]" ARRAY_INFO_DATA_SEC " = {};\n");
        start_buff += string_format("unsigned pint_array_init_num = 0;\n");
    }

    // dlpm
    for (unsigned i = 0; i < delay_lpm_num; i++) {
        global_init_buff +=
            string_format("    pint_lpm_future_thread[%d].run_func = pint_lpm_%u;\n", i,
                          delay_to_global_lpm_idx[i]);
        global_init_buff +=
            string_format("    pint_lpm_future_thread[%d].lpm_state = FUTURE_LPM_STATE_WAIT;\n", i);
    }

    global_init_buff +=
        string_format("\n    for (int i = 0; i < %d; i++) pint_enqueue_flag[i] = 0;\n", flag_len);
    if (pint_strobe_num) {
        global_init_buff +=
            string_format("\n    for (int i = 0; i < "
                          "sizeof(pint_monitor_enqueue_flag)/sizeof(pint_monitor_enqueue_flag[0]); "
                          "i++) pint_monitor_enqueue_flag[i] = 0;\n");
    }
    global_init_buff += "\n    static_event_list_table_num = 0;\n";

    start_buff += string_format("unsigned pint_event_num =%d;\n", pint_event_num);
    inc_buff += string_format("extern unsigned pint_event_num;\n");
    start_buff += string_format("unsigned short pint_enqueue_flag[%d]  "
                                "__attribute__((section(\".builtin_simu_buffer\")));\n",
                                flag_len);
    if (pint_strobe_num) {
        start_buff += string_format("unsigned short pint_monitor_enqueue_flag[%d]  "
                                    "__attribute__((section(\".builtin_simu_buffer\")));\n",
                                    (pint_strobe_num + 15) / 16);
    }
    start_buff += string_format("pint_thread pint_active_q[%d];\n", thread_q_len);
    start_buff += string_format("pint_thread pint_inactive_q[%d];\n", thread_q_len);
    start_buff += string_format("pint_thread pint_monitor_q[%d];\n", pint_strobe_num);

    int delay_num = pint_delay_num ? pint_delay_num : 1;
    start_buff += string_format("pint_thread pint_delay0_q[%d];\n", delay_num);
    start_buff += string_format("pint_future_thread_t pint_future_q[%d];\n", delay_num);
    start_buff +=
        string_format("int pint_future_q_find[%d];\n", pint_delay_num ? pint_delay_num : 1);
    // dlpm
    start_buff += string_format("pint_future_thread_t pint_lpm_future_q[%d] = {0};\n",
                                delay_lpm_num ? delay_lpm_num : 1);
    start_buff +=
        string_format("int pint_lpm_future_q_idx_active[%d];\n", delay_lpm_num ? delay_lpm_num : 1);
    start_buff +=
        string_format("int pint_lpm_future_q_idx_find[%d];\n", delay_lpm_num ? delay_lpm_num : 1);

    start_buff += string_format("pint_nbassign_t pint_nbq_base[%d];\n\n",
                                (pint_nbassign_num + pint_nbassign_array_num)
                                    ? (pint_nbassign_num + pint_nbassign_array_num) + 1
                                    : 1);
    start_buff += string_format("pint_thread_t static_event_list_table[%d];\n",
                                pint_static_event_list_table_num);
    start_buff +=
        string_format("int static_event_list_table_size[%d] __attribute__((aligned(64)));\n",
                      pint_static_event_list_table_num);
    start_buff += string_format(
        "int static_event_list_table_num  __attribute__((section(\".builtin_simu_buffer\")));\n");
    start_buff += string_format("unsigned pint_block_size = %d;\n",
                                (thread_q_len + pint_nbassign_array_num + pint_nbassign_num) * 8);
    if (pint_perf_on) {
        start_buff += string_format("unsigned gperf_lpm_num = %d;\n", global_lpm_num);
        start_buff += string_format("unsigned gperf_monitor_lpm_num = %d;\n", monitor_lpm_num);
        start_buff += string_format("unsigned gperf_thread_lpm_num = %d;\n", global_thread_lpm_num);
        start_buff += string_format("unsigned gperf_force_lpm_num = %d;\n", force_lpm_num);
        start_buff += string_format("unsigned gperf_event_num = %d;\n", pint_event_num);
        start_buff += string_format("unsigned gperf_thread_num = %d;\n", pint_thread_num);
    }

    if (pint_open_file_num) {
        start_buff += string_format("unsigned int pint_fd[%d];\n", pint_open_file_num);
        inc_buff += string_format("extern unsigned int pint_fd[%d];\n", pint_open_file_num);
    }
    // int sys_args_num = exists_sys_args.size();
    start_buff += string_format("__kernel_arg__ int pint_sys_args_num = 0;\n");
    start_buff += string_format("__kernel_arg__ int *pint_sys_args_off;\n");
    start_buff += string_format("__kernel_arg__ char *pint_sys_args_buff;\n");

    start_buff += string_format("pint_future_thread_s pint_future_thread[%u];\n",
                                pint_delay_num > 0 ? pint_delay_num : 1);
    if (pint_pli_mode) {
        start_buff += string_format("unsigned int pint_output_port_change_id[%d] "
                                    "__attribute__((section(\".simu.dut.output\")));\n",
                                    pint_output_port_num + pint_inout_port_num +
                                        pint_cross_module_out_port_num);
        start_buff += string_format("unsigned int pint_input_port_change_id[%d] "
                                    "__attribute__((section(\".simu.dut.input\")));\n",
                                    pint_input_port_num + pint_inout_port_num +
                                        pint_cross_module_in_port_num);
        start_buff += string_format(
            "unsigned short pint_output_port_id_flag[%d] "
            "__attribute__((section(\".builtin_simu_buffer\")));\n",
            (pint_output_port_num + pint_inout_port_num + pint_cross_module_out_port_num + 15) /
                16);
        start_buff += string_format(
            "unsigned int pint_output_port_id_flag_size "
            "__attribute__((section(\".builtin_simu_buffer\"))) = %d;\n\n",
            (pint_output_port_num + pint_inout_port_num + pint_cross_module_out_port_num + 15) /
                16);
    }

    // dlpm
    if (delay_lpm_num) {
        ostringstream d_str;
        d_str << delay_lpm_num;
        start_buff += "pint_future_thread_s pint_lpm_future_thread[" + d_str.str() + "];\n";
        inc_buff += "extern pint_future_thread_s pint_lpm_future_thread[" + d_str.str() + "];\n";
    }

    if (pint_thread_num) {
        global_init_buff += "\n    //  Add process to INACTIVE_Q\n";
        for (unsigned i = 0; i < pint_thread_num; i++) {
            if (thread_child_ind[i] == "child_thread") {
                continue;
            }
            if (thread_is_static_thread[i] == 1) {
                continue;
            }
            ostringstream i_str;
            i_str << i;
            global_init_buff +=
                "    pint_enqueue_thread(pint_thread_" + i_str.str() + ", INACTIVE_Q);\n";
        }
    }

    if (pint_process_num) {
        start_buff += string_format("unsigned char pint_event_flag[%d];\n", pint_process_num);
        global_init_buff += string_format(
            "\n    for (int i = 0; i < %d; i++)pint_event_flag[i] = 1;\n", pint_process_num);
        global_init_buff += always_run_buff;
    } else {
        start_buff += string_format("unsigned char pint_event_flag[1];\n");
    }

    if (global_lpm_num + monitor_lpm_num) {
        global_init_buff += "\n    //enqueue lpm to active queue\n";
        for (unsigned i = 0; i < global_lpm_num + monitor_lpm_num; i++) {
            if (pint_is_lpm_needs_init(i)) {
                global_init_buff += string_format("\tpint_enqueue_thread(pint_lpm_%u, ACTIVE_Q);\n",
                                                  i + pint_event_num);
            }
            // ostringstream i_str;
            // i_str << i + pint_event_num;
            // global_init_buff +=
            //     "    pint_enqueue_thread(pint_lpm_" + i_str.str() + ", ACTIVE_Q);\n";
        }
    }

    fprintf(out, "#! /bin/bash\n");
    // fprintf(out, "trap \"trap - SIGTERM && kill -- -$$\" SIGINT SIGTERM\n");
    fprintf(out, "trap \"pstree $$ -p | awk -F\\\"[()]\\\" "
                 "'{for(i=0;i<=NF;i++)if(\\$i~/^[0-9]+$/)print \\$i}' | xargs kill -9\"  SIGINT "
                 "SIGTERM\n");
    if (is_path) {
        // fprintf(out, "trap \"rm %s_pint_simu/*.md5 > /dev/null 2>&1\" SIGINT SIGTERM\n", path);
        fprintf(out, "if which pint_ivl_tool > /dev/null 2>&1;then\n");
        fprintf(out, "  pint_ivl_tool %s_pint_simu $*\n", path);
        chmod_cmd = string_format("chmod +x %s", path);
    } else {
        // fprintf(out, "trap \"rm %s/%s_pint_simu/*.md5 > /dev/null 2>&1\" SIGINT SIGTERM\n",
        // root_path, path);
        fprintf(out, "if which pint_ivl_tool > /dev/null 2>&1;then\n");
        fprintf(out, "  pint_ivl_tool %s/%s_pint_simu $*\n", root_path, path);
        chmod_cmd = string_format("chmod +x %s/%s", root_path, path);
    }
    fprintf(out, "else\n  echo \"please install pint sdk before the simulation!\"\nfi\n");
    fclose(out);
    systm_ret = system(chmod_cmd.c_str());

    if (pint_simu_info.need_dump) {
        start_buff += string_format("unsigned int simu_fd;\n");
        inc_buff += string_format("extern unsigned int simu_fd;\n");
    } else {
        global_init_buff += string_format("    pint_dump_enable = 0;\n");
    }

    pint_dump_nbassign();
    t_dump_buff += pint_exec_duration(st, ed, sts);
    global_init_buff += "}\n";

    start_buff += string_format("const unsigned int pint_const_0[%u] = {0};\n", pint_const0_max_word+2);
    inc_buff += string_format("extern const unsigned int pint_const_0[%u];\n", pint_const0_max_word+2);
    
    write_bytes = write(pint_thread_fd, start_buff.c_str(), start_buff.size());
    write_bytes = write(pint_thread_fd, signal_buff.c_str(), signal_buff.size());
    write_bytes = write(pint_thread_fd, array_init_buff.c_str(), array_init_buff.size());
    write_bytes = write(pint_thread_fd, monitor_func_buff.c_str(), monitor_func_buff.size());
    write_bytes = write(pint_thread_fd, strobe_func_buff.c_str(), strobe_func_buff.size());
    write_bytes = write(pint_thread_fd, event_declare_buff.c_str(), event_declare_buff.size());
    if (strcmp(enval, enval_flag) == 0) {
        write_bytes = write(pint_thread_fd, lpm_declare_buff.c_str(), lpm_declare_buff.size());
        task_declare_buff = "\n//func declare\n" + task_declare_buff;
        write_bytes = write(pint_thread_fd, task_declare_buff.c_str(), task_declare_buff.size());
        write_bytes =
            write(pint_thread_fd, thread_declare_buff.c_str(), thread_declare_buff.size());

        inc_buff += event_static_thread_dec_buff;
        for(auto iter = signal_s_list_map.begin();
              iter != signal_s_list_map.end(); iter++) 
        {
            inc_buff += iter->second.declare_buff;
        }        
        write_bytes = write(pint_thread_fd, inc_buff.c_str(), inc_buff.size());

        thread_declare_buff.clear();
    } else {
        int main_inc_fd = -1;
        string pint_thread_stub_inc_name =
            string_format("%s/src_file/pint_thread_stub.h", dir_name.c_str());
        if ((main_inc_fd = open(pint_thread_stub_inc_name.c_str(),
                                O_RDWR | O_CREAT | O_APPEND | O_SYNC, 00700)) < 0) {
            printf("Open pint_thread_stub.h error!");
            return -1;
        }
        inc_buff += event_static_thread_dec_buff;
        for(auto iter = signal_s_list_map.begin();
              iter != signal_s_list_map.end(); iter++) 
        {
            inc_buff += iter->second.declare_buff;
        }           
        write_bytes = write(main_inc_fd, inc_buff.c_str(), inc_buff.size());
        inc_buff.clear();
        close(main_inc_fd);
        int lpm_inc_fd = -1;
        string lpm_inc_name = string_format("%s/src_file/lpm.h", dir_name.c_str());
        if ((lpm_inc_fd = open(lpm_inc_name.c_str(), O_RDWR | O_CREAT | O_APPEND | O_SYNC, 00700)) <
            0) {
            printf("Open lpm.h error! tgt_design");
            return -1;
        }
        write_bytes = write(lpm_inc_fd, lpm_declare_buff.c_str(), lpm_declare_buff.size());
        lpm_declare_buff.clear();
        close(lpm_inc_fd);
        if (lpm_buff.size()) {
            int lpm_buff_fd = -1;
            string lpm_buff_file_name =
                string_format("%s/src_file/lpm_normal%d.c", dir_name.c_str(), lpm_buff_file_id);
            if ((lpm_buff_fd = open(lpm_buff_file_name.c_str(),
                                    O_RDWR | O_CREAT | O_APPEND | O_SYNC, 00700)) < 0) {
                printf("Open lpm.c error! tgt_design");
                return -1;
            }
            write_bytes = write(lpm_buff_fd, lpm_buff.c_str(), lpm_buff.size());
            lpm_buff.clear();
            close(lpm_buff_fd);
        }
        int thread_inc_fd = -1;
        string thread_inc_name = string_format("%s/src_file/thread.h", dir_name.c_str());
        if ((thread_inc_fd =
                 open(thread_inc_name.c_str(), O_RDWR | O_CREAT | O_APPEND | O_SYNC, 00700)) < 0) {
            printf("Open thread.h error! tgt_design");
            return -1;
        }
        write_bytes = write(thread_inc_fd, thread_declare_buff.c_str(), thread_declare_buff.size());
        thread_declare_buff.clear();
        close(thread_inc_fd);
        if (total_thread_buff.size()) {
            int thread_buff_fd = -1;
            string thread_file_name =
                string_format("%s/src_file/thread%d.c", dir_name.c_str(), thread_buff_file_id);
            if ((thread_buff_fd = open(thread_file_name.c_str(),
                                       O_RDWR | O_CREAT | O_APPEND | O_SYNC, 00700)) < 0) {
                printf("Open thread.c error! tgt_design");
                return -1;
            }
            write_bytes =
                write(thread_buff_fd, total_thread_buff.c_str(), total_thread_buff.size());
            total_thread_buff.clear();
            close(thread_buff_fd);
        }
    }
    // only generate invoked functions
    set<string>::iterator iter;
    thread_buff_file_id++;

    // e_list l_list
    write_bytes = write(pint_thread_fd, signal_list.c_str(), signal_list.size());
    // pli interface input signal change callback
    if (pint_pli_mode) {
        write_bytes = write(pint_thread_fd, callback_list.c_str(), callback_list.size());
    }
    // signal force
    write_bytes = write(pint_thread_fd, signal_force.c_str(), signal_force.size());
    // nb_assign_info
    write_bytes = write(pint_thread_fd, nbassign_buff.c_str(), nbassign_buff.size());
    // evnet impl
    write_bytes = write(pint_thread_fd, event_impl_buff.c_str(), event_impl_buff.size());
    if (strcmp(enval, enval_flag) == 0) {
        string sensitive_singal_static_declare_buff;
        for(auto iter = signal_s_list_map.begin();
              iter != signal_s_list_map.end(); iter++) 
        {
            sensitive_singal_static_declare_buff += iter->second.declare_buff;
        }         
        write_bytes = write(pint_thread_fd, sensitive_singal_static_declare_buff.c_str(),
                            sensitive_singal_static_declare_buff.size());
        write_bytes = write(pint_thread_fd, event_static_thread_dec_buff.c_str(),
                            event_static_thread_dec_buff.size());                            
    }
    write_bytes = write(pint_thread_fd, event_begin_buff.c_str(), event_begin_buff.size());
    if (strcmp(enval, enval_flag) == 0) {
        write_bytes = write(pint_thread_fd, lpm_buff.c_str(), lpm_buff.size());
        write_bytes = write(pint_thread_fd, total_thread_buff.c_str(), total_thread_buff.size());
    }

    string func_write_buff;
    for (iter = invoked_fun_names.begin(); iter != invoked_fun_names.end(); iter++) {
        if (func_task_name_implementation.count(*iter)) {
            if (strcmp(enval, enval_flag) != 0) {
                func_write_buff += func_task_name_implementation[*iter];
                int thread_buff_fd = -1;
                string thread_file_name =
                    string_format("%s/src_file/thread%d.c", dir_name.c_str(), thread_buff_file_id);
                if (func_write_buff.size() > 1024 * 1024 * 1) {
                    if ((thread_buff_fd = open(thread_file_name.c_str(),
                                               O_RDWR | O_CREAT | O_APPEND | O_SYNC, 00700)) < 0) {
                        printf("Open thread.c error! tgt_design");
                        return -1;
                    }
                    func_write_buff = inc_start_buff + func_write_buff;
                    write_bytes =
                        write(thread_buff_fd, func_write_buff.c_str(), func_write_buff.size());
                    thread_buff_file_id++;
                    func_write_buff.clear();
                    close(thread_buff_fd);
                }
            } else {
                write_bytes = write(pint_thread_fd, func_task_name_implementation[*iter].c_str(),
                                    func_task_name_implementation[*iter].size());
            }
        }
    }
    if (strcmp(enval, enval_flag) != 0) {
        int thread_buff_fd = -1;
        string thread_file_name =
            string_format("%s/src_file/thread%d.c", dir_name.c_str(), thread_buff_file_id);
        if ((thread_buff_fd =
                 open(thread_file_name.c_str(), O_RDWR | O_CREAT | O_APPEND | O_SYNC, 00700)) < 0) {
            printf("Open thread.c error! tgt_design");
            return -1;
        }
        func_write_buff = inc_start_buff + func_write_buff;
        write_bytes = write(thread_buff_fd, func_write_buff.c_str(), func_write_buff.size());
        close(thread_buff_fd);
    }

    // generate static wait on event table
    map<ivl_event_t, string>::iterator iter_1;
    for (iter_1 = event_static_thread_buff.begin(); iter_1 != event_static_thread_buff.end();
         iter_1++) {
        iter_1->second += "};\n";
        write_bytes = write(pint_thread_fd, iter_1->second.c_str(), iter_1->second.size());
    }

    //generate s_list
    for(auto iter = signal_s_list_map.begin();
           iter != signal_s_list_map.end(); iter++) 
    {
        iter->second.define_buff += string_format("{(pint_thread)%d, (pint_thread)%d, (pint_thread)%d,",
                                 iter->second.pos_num, iter->second.any_num, iter->second.neg_num);
        iter->second.define_buff += iter->second.pos_buff;
        iter->second.define_buff += iter->second.any_buff;
        iter->second.define_buff += iter->second.neg_buff;
        iter->second.define_buff += string_format("};\n");
        write_bytes = write(pint_thread_fd, iter->second.define_buff.c_str(), iter->second.define_buff.size());
    }

    write_bytes = write(pint_thread_fd, global_init_buff.c_str(), global_init_buff.size());

    if (g_syntax_unsupport_flag) {
        const char *unsupport_note =
            "[Fatal Error]: code generate fail due to yet unsupported syntax!\n";
        printf("%s", unsupport_note);
        write_bytes = write(pint_thread_fd, unsupport_note, strlen(unsupport_note));
    }

    close(pint_thread_fd);
    fclose(pls_log_fp);
    (void)write_bytes;
    (void)systm_ret;
    t_dump_file += pint_exec_duration(st, ed, sts);
    gettimeofday(&time_end, NULL);

    e_time = time_end.tv_sec + (time_end.tv_usec * 1.0) / 1000000.0;
    b_time = time_begin.tv_sec + (time_begin.tv_usec * 1.0) / 1000000.0;

    if (e_time >= b_time) {
        codegen_time = e_time - b_time;
    } else {
        codegen_time = (0xffffffffffffffff - b_time) + e_time;
    }
    printf("C codegen time: %f seconds.\n", codegen_time);
    if (PINT_GEN_CODE_DISP_T_EN) {
        printf("C code gen time details:\n"
               "\tshow signal:         %.2f s\n"
               "\tshow event:          %.2f s\n"
               "\tshow lpm & log:      %.2f s\n"
               "\tshow monitor:        %.2f s\n"
               "\tshow func & task:    %.2f s\n"
               "\tpre parse dsgn:      %.2f s\n"
               "\tshow process:        %.2f s\n"
               "\tforce:               %.2f s\n"
               "\tdump_buff:           %.2f s\n"
               "\tdump_file:           %.2f s\n"
               "\tothers:              %.2f s\n"
               "\tTotla:               %.2f s\n",
               t_show_sig, t_show_evt, t_show_lpm, t_show_monitor, t_show_func, t_pre_parse,
               t_show_proc, t_show_force, t_dump_buff, t_dump_file, t_others,
               t_show_sig + t_show_evt + t_show_lpm + t_show_monitor + t_show_func + t_pre_parse +
                   t_show_proc + t_show_force + t_dump_buff + t_dump_file + t_others);
    }
#ifndef __cplusplus
    free(pint_local_lpm);
    free(pint_global_lpm);
    free(pint_event);
    free(pint_signal);
#endif
    return stub_errors;
}

const char *target_query(const char *key) {
    if (strcmp(key, "version") == 0)
        return version_string;

    return 0;
}
