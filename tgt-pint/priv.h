#ifndef IVL_priv_H
#define IVL_priv_H
/*
 * Copyright (c) 2004-2014 Stephen Williams (steve@icarus.com)
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

#include <ivl_target.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <queue>

using namespace std;

#define PINT_MIN_INST_NUM 2
#define PINT_MAX_CMN_CALL_NUM 64
#define PINT_MAX_CMN_PARA_NUM 16
#define PINT_PERF_BASE_ID 300
#define PINT_MULT_ASGN_EN 1
#define PINT_ARR_MAX_SHADOW_LEN 1024
#define PINT_DUMP_FILE_LINE_NUM 10000
// nxz
#define PINT_SIG_NXZ_EN 1
#define PINT_COPY_VALID_ONLY 1
#define PINT_REMVOE_SISITIVE_SIGNAL_EN 1
#define PINT_SIMPLE_CODE_EN 0
#define PINT_SIMPLE_SIG_NAME_EN 0
#define PINT_GEN_CODE_DISP_T_EN 0
#define PINT_PAR_MODE_EN 1
#define PINT_PAR_ANLS_NUM 8
#define MAX_TASK_STACK_SIZE 0x10000
// nxz
#define B2S(b) b ? "true" : "false" // bool -> string

#define VALUE_MAX 0x7fffffff
#define VALUE_MIN 0x80000001
#define VALUE_XZ 0x80000000

extern FILE *pls_log_fp;
extern bool g_syntax_unsupport_flag;
#define PINT_UNSUPPORTED_ERROR(file, line, str, args...)                                           \
    do {                                                                                           \
        printf("%s:%u: unsupport[fatal]: " str "\n", file, (unsigned)line, ##args);                \
        fprintf(pls_log_fp, "%s:%u: unsupport: " str "\n", file, (unsigned)line, ##args);          \
        g_syntax_unsupport_flag = true;                                                            \
    } while (0)

#define PINT_UNSUPPORTED_WARN(file, line, str, args...)                                            \
    do {                                                                                           \
        printf("%s:%u: unsupport[warn]: " str "\n", file, (unsigned)line, ##args);                 \
        fprintf(pls_log_fp, "%s:%u: note: " str "\n", file, (unsigned)line, ##args);               \
    } while (0)

#define PINT_UNSUPPORTED_NOTE(file, line, str, args...)                                            \
    do {                                                                                           \
        fprintf(pls_log_fp, "%s:%u: unsupport[note]: " str "\n", file, (unsigned)line, ##args);    \
    } while (0)

extern bool g_log_print_flag;
#define PINT_LOG_HOST(str, args...)                                                                \
    do {                                                                                           \
        if (g_log_print_flag) {                                                                    \
            printf(str, ##args);                                                                   \
        }                                                                                          \
    } while (0)

// Added for analysis start
struct link_list {
    struct link_list *next;
    void *item;
};

struct expr_list {
    struct expr_list *next_same;
    struct expr_list *next_diff;
    ivl_expr_t net;
    int id;
    // if it is expresion for a signal in sig_list, this will index in sig_list, otherwise -1
    int sig_id;
    ivl_signal_t sig;
    struct link_list *sig_dpnd;
    int index; // 1 means this expression appeared as an index
};

struct sig_anls_result {
    int binary;
    int anti_binary; // anticipate binary
    int sensitive;   // used by statements, init blk list sensitivity list

    int loop_linear;
};

struct expr_anls_result {
    int avail;
    int used;
    int anti;   // anticipate binary
    int binary; // updated after signal is determined
};

struct expr_anls_cost{
    int n_reg;
    int binary;
    int value;
    int need_alloc; // it will be used later, if this is 0, not need to keep it in either register
                    // or memory
    int width;
    int reg_l; // register allocation
    int reg_h;
    int offset; // local cache allocation
    int m_alloc;
    int avail_regs[6];
};

struct expr_bundle {
    struct expr_list *exprs;
    struct expr_anls_result *stmt_in;
    int avail_anls; // If this is for availability analysis, it will collect only common exprs from
                    // ternary. relevant to analysis_add_expr
    int index;
};

struct wait_list {
    struct wait_list *next;
    char *thread;
};
struct stmt_list {
    struct stmt_list *next_blk;
    struct stmt_list *next_stmt;
    struct stmt_list *pre_stmt;
    int blk_id;
    ivl_statement_t net;
    int stmt_id;
    int loop_depth;   // the level of the deepst statement
    int loop_level;   // the level
    int dly_in_loop;  // only valid for loop (intermost)
    int loop_st_type; // 0-default 1-loop_linear assignment 2-bit in vector
};
typedef enum vefa_fg_edge_type_e {
    VEFA_EDGE_NORMAL = 0,
    VEFA_EDGE_IF_TRUE = 1,
    VEFA_EDGE_IF_FALSE = 2,
    VEFA_EDGE_WHILE_FWD = 3,
    VEFA_EDGE_WHILE_BWD = 4
} vefa_fg_edge_type_t;

struct fg_edge_list { // fg flow graph
    struct fg_edge_list *next;
    int start;
    int end;
    vefa_fg_edge_type_t type;
};

struct expr_companion {
    struct sig_anls_result *bin_sig_list;
    int gen_xz;
    struct expr_anls_result *stmt_in_expr;
    struct expr_anls_result *stmt_out_expr;
    struct expr_anls_result *stmt_mid_expr;
    int to_vector;      // non-zero value indicate the width of the vector.
    int store_in_stack; // expression value,addr are stored in the stack, this will alow to call
                        // functions
    int gen_code;
    int gen_value;
    int reg_l;
    int reg_h;
    int reg_spill;
};

// for analsysis_expression
struct expr_companion2 {
    ivl_signal_t *sig_list;
    int n_sig;
    struct sig_anls_result *stmt_in_sig;
};

typedef struct stmt_list *stmt_list_t;
typedef struct expr_list *expr_list_t;
typedef struct link_list *link_list_t;
typedef struct fg_edge_list *fg_edge_list_t;
typedef struct sig_anls_result *sig_anls_result_t;
typedef struct expr_anls_result *expr_anls_result_t;
typedef struct expr_anls_cost *expr_anls_cost_t;

struct process_info {
    int n_stmt;
    int n_sig;
    int n_l_sig;
    int n_expr;
    int n_edge;
    struct sig_anls_result **stmt_in;
    ivl_signal_t *sig_list;
    stmt_list_t *block_list;
    fg_edge_list_t *edges;
    ivl_statement_t *stmt_list;
    struct expr_list *exprs;
    struct expr_anls_result **stmt_in_expr;
    struct expr_anls_result **stmt_out_expr;
    struct expr_anls_result **stmt_mid_expr;
    int expr_rsv_size;
    int loop_mode; // 0-normal, >0-extend, <0-skip bit in vec assignment(blocking and non-blocking)
    int regs_avail[16];
};

struct vefa_expr_info {
    int is_const;
    int is_binary; // 0-01xz 1-binary
    unsigned int num;
    int is_allocated;
    int called_func;
    int n_reg;

    // field listed below is used by analysis_expression
    struct link_list *sig_dpnd;
    int loop_linear;
    int biv; // bit in vector
};
typedef struct vefa_expr_info *vefa_expr_info_t;

// Added for analysis end

struct pint_nbassign_info {
    unsigned int type; // 0: signal 1:array
    unsigned int size;
    string src;
    string dst;
    unsigned int offset;
    unsigned int updt_size;
    unsigned int array_size;
    string l_list; // size + thread
    string e_list; //
    string p_list;
    string nb_sig;
    string nb_sig_declare;
    string vcd_symbol;
    string vcd_symbol_ptr;
    int sig_nb_id;
    unsigned int is_output_interface;
    int output_port_id;
};

struct dump_scops {
    int layer;
    vector<ivl_scope_t> scopes;
};

struct dump_info {
    unsigned int need_dump;
    vector<struct dump_scops> dump_scopes;
    vector<ivl_signal_t> dump_signals;
    map<ivl_expr_t, string> monitor_exprs_name;
    map<ivl_statement_t, int> monitor_stat_id;
};

enum event_type_t { POS = 0, NEG, ANY };

enum lpm_type_t { LOG = 0, LPM = 1, SWITCH_ = 2 };

enum assign_type_t {
    TYPE_0 = 0,
    TYPE_1 = 1,
    TYPE_2 = 2,
    TYPE_3 = 3,
    TYPE_4 = 4,
    TYPE_5 = 5,
    TYPE_6 = 6,
    TYPE_7 = 7,
    TYPE_8 = 8,
    TYPE_9 = 9,
    TYPE_10 = 10,
    TYPE_11 = 11,
    TYPE_12 = 12,
    TYPE_13 = 13,
    TYPE_14 = 14,
    TYPE_15 = 15,
    TYPE_16 = 16
};
enum ex_number_type_t {
    EX_NUM_DATA = 0,
    EX_NUM_ASSIGN_SIG = 1,
    EX_NUM_DEFINE_SIG = 2,
    EX_NUM_AUTO_MASK = 0x10,
    EX_NUM_ASSIGN_AUTO = 17,
    EX_NUM_DEFINE_AUTO = 18
};

enum mult_asgn_type_t {
    MULT_NONE = 0,
    MULT_SIG_1 = 1,
    MULT_SIG_2 = 2,
    MULT_SIG_3 = 3,
    MULT_ARR_1 = 4,
    MULT_ARR_23 = 5
};

enum sig_bit_val_t { BIT_0 = 0, BIT_1 = 1, BIT_X = 3, BIT_Z = 2 };

enum force_sig_type_t { FORCE_NET = 0, FORCE_VAR = 1 };

typedef enum {
    NUM_EXPRE_DIGIT = 0, // means number expression can be convert to digit if it is immediate num
    NUM_EXPRE_VARIABLE =
        1, // means number expression must be convert to a variable if it is immediate num
    NUM_EXPRE_SIGNAL =
        2, // means number expression must be convert to a signal whether it is immediate num or not
} NUM_EXPRE_TYPE;

struct pint_sig_sen_s { // The sensitive threads of sig_info
    unsigned lpm_num;
    unsigned pos_evt_num;
    unsigned any_evt_num;
    unsigned neg_evt_num;
    unsigned optimized;
    set<unsigned> lpm_list;
    set<unsigned> pos_evt_list;
    set<unsigned> any_evt_list;
    set<unsigned> neg_evt_list;
};
typedef struct pint_sig_sen_s *pint_sig_sen_t;

struct pint_force_lhs_s {
    unsigned lhs_idx;
    string define;
    string cond;
    string asgn;
    string upate;
};
typedef struct pint_force_lhs_s *pint_force_lhs_t;

struct pint_force_info_s {
    unsigned type : 1;       //  when signal is not drived by global lpm, then it's FORCE_VAR
    unsigned mult_force : 1; //  whether has multiple force stmt to force it.
    unsigned mult_assign : 1; //  whether has multiple assign stmt to force it.
    unsigned part_force : 1; //  whether own stmt that only force/release part of net, only valid
                             //  for  FORCE_NET
    ivl_scope_t scope;
    //  force_idx(not include offset) of force stmt, not include release
    set<unsigned> force_idx;
    //  assign_idx(not include offset) of assign stmt, not include deassign
    set<unsigned> assign_idx;
};
typedef struct pint_force_info_s *pint_force_info_t;
/*
sig_info->dynamic_lpm:
    Means those threads(lpm/event) not sen to any elemnt of the arrin the begining.
'sen_word' needs update following the signal 'sel'
sig_info->sensitives[0xffffffff]:
    Means those threads sen to all the elements of the arr.
*/
struct pint_sig_info_s {
    ivl_signal_t sig; // For arr, sig_info.sig = sig_arr
    unsigned width;
    unsigned arr_words; // For signal, arr_words = 1; for arr, arr_words > 1
    union {
        unsigned is_const;
        set<unsigned> *const_idx; //  The arr_word of element that is const
    };
    unsigned has_nb;
    unsigned is_local;
    unsigned is_func_io;    // Init in 'show_all_task_func'
    unsigned is_dump_var;   // For arr, is_dump_var = 0(not support dump arr)
    unsigned is_interface;
    ivl_signal_port_t port_type;
    int output_port_id;
    int input_port_id;
    unsigned local_tmp_idx; // Used to gen tmp var, not a property of signal
    const char *dump_symbol;
    string sig_name;
    string value; // The init value for signal
    union {
        pint_sig_sen_t sensitive;   // for signal
        map<unsigned, pint_sig_sen_t> *sensitives;
    };
    union {
        pint_force_info_t force_info;
        map<unsigned, pint_force_info_t> *force_infos;
    };
    vector<unsigned> sens_process_list;
};
typedef struct pint_sig_info_s *pint_sig_info_t;

struct pint_force_lval_s {
    pint_sig_info_t sig_info;
    unsigned word;
    unsigned updt_base;
    unsigned updt_size;
};
typedef struct pint_force_lval_s *pint_force_lval_t;

struct pint_lpm_log_info {
    unsigned type; // log | lpm
    ivl_nexus_t nex;
    union {
        ivl_net_logic_t log;
        ivl_lpm_t lpm;
        ivl_switch_t switch_;
    };
    unsigned idx;
};
typedef struct pint_lpm_log_info *pint_lpm_log_info_t;

struct pint_event_info {
    ivl_event_t evt;
    unsigned any_num;
    unsigned neg_num;
    unsigned pos_num;
    unsigned event_id;
    unsigned wait_count;
    unsigned static_wait_count;
    unsigned has_array_signal;
};
typedef struct pint_event_info *pint_event_info_t;

struct pint_cmn_lpm_info {
    string fst_scope_name;
    unsigned inst_count;
    unsigned is_generate_type;
    unsigned leaf_count;
    unsigned para_count;
    unsigned diff_done;
    unsigned call_count;
    unsigned cmn_lpm_id;
    map<string, vector<pint_sig_info_t>> leaf_list;
    map<string, map<pint_sig_info_t, string>> para_decl;
};
typedef struct pint_cmn_lpm_info *pint_cmn_lpm_info_t;

struct pint_cur_lpm_info {
    unsigned is_cmn;
    unsigned para_count;
    unsigned need_gen_cmn;
    unsigned cmn_lpm_id;
    map<pint_sig_info_t, string> sig_para;
    string cmn_decl_str;
    string cmn_call_str;
};

struct pint_thread_info {
    ivl_process_t proc;
    ivl_scope_t scope;
    unsigned id;
    unsigned pc;
    unsigned to_lpm_flag;
    unsigned have_processed_delay;
    unsigned delay_num;
    unsigned future_idx;
    unsigned fork_idx;
    unsigned fork_cnt;
    unsigned proc_sub_stmt_num;
    unsigned chk_before_exit;
    map<ivl_signal_t, unsigned> sig_to_tmp_id; //  start with 1
    vector<ivl_signal_t> proc_sig_has_tmp;
    vector<ivl_signal_t> proc_sig_needs_chk; //  needs check before exit
};
typedef struct pint_thread_info *pint_thread_info_t;

struct pint_process_info_s {
    ivl_process_t proc;
    vector<pint_sig_info_t> sens_sig_list;
    unsigned flag_id;
    unsigned always_run;
};

typedef struct pint_process_info_s *pint_process_info_t;

struct pint_signal_s_list_info_s {
    unsigned pos_num;
    unsigned any_num;
    unsigned neg_num;
    string declare_buff;
    string define_buff;
    string pos_buff;
    string any_buff;
    string neg_buff;
    pint_signal_s_list_info_s(): pos_num(0), any_num(0), neg_num(0) {}
};
typedef struct pint_signal_s_list_info_s *pint_signal_s_list_info_t;

extern map<unsigned, pint_process_info_t> pint_process_info;
extern unsigned pint_process_info_num;
extern unsigned pint_process_num;

class ExpressionConverter {
  public:
    /*mustSignal:      indicate that the result can not be a immediate num, immediate num will be
     * present as a signal which has lbits and hbits */
    /*mustGenTmp:  indicate that must gen a tmp variable to represent the result, this is for signal
     * expression with width > 4                          */
    // nxz
    ExpressionConverter(ivl_expr_t net_, ivl_statement_t stmt_, unsigned expect_width_ = 0,
                        NUM_EXPRE_TYPE numExprType_ = NUM_EXPRE_VARIABLE, bool mustGenTmp = false);

    string GetResultNameString() { return is_Expression_Valid ? result_name : ""; }

    const char *GetResultName() { return is_Expression_Valid ? result_name.c_str() : ""; }

    void GetPreparation(string *operation_code_, string *declare_code_) {
        if (is_Expression_Valid) {
            if (declare_code_ != NULL) {
                *declare_code_ += "    " + declare_code;
            } else {
                preparation_code = declare_code + preparation_code;
            }
            *operation_code_ += preparation_code;
        }
    }

    bool IsResultNxz() { return is_Expression_Valid ? is_result_nxz : false; }

    bool IsImmediateNum() { return is_Expression_Valid ? is_immediate_num : false; }

    bool IsExpressionValid() { return is_Expression_Valid; }

    const set<pint_sig_info_t> *Sensitive_Signals() { return &sensitive_signals; }

    static void ClearExpressTmpVarId() { express_tmp_var_id = 0; }
    static int GetImmediateNumValue(ivl_expr_t word);

  private:
    ivl_expr_t net;
    // nxz
    ivl_statement_t stmt;
    bool is_result_nxz;
    unsigned expect_width;
    bool is_Expression_Valid;
    bool is_immediate_num;
    bool is_must_gen_tmp;
    bool is_pad_declare;
    NUM_EXPRE_TYPE numExprType;
    string result_name;
    string declare_code;
    string operation_code;
    string content_file_line;
    string preparation_code;
    static set<pint_sig_info_t> sensitive_signals;
    static unsigned int express_tmp_var_id;
    // This class may initializes itself inside, this member will record the depth to mark if this
    // object is initilized by outside.
    // this mechanism does not support multi-thread mode
    static unsigned int nest_depth;
    static map<string, vector<string>> design_file_content;
    void MatchResultWidth();
    bool pint_binary_ret_bool(char op);
    void AppendClearCode(bool gen_code);
    void AppendOperCode(ExpressionConverter *oper_converter, const char *comment = NULL);
    void FormatPreparationCode();
    void GenPreparationCode();

    void show_array_expression();
    void show_array_pattern_expression();
    void show_branch_access_expression();
    void show_binary_expression();
    void show_binary_expression_real();
    void show_enumtype_expression();
    void show_function_call();
    void show_sysfunc_call();
    void show_memory_expression();
    void show_new_expression();
    void show_null_expression();
    void show_property_expression();
    void show_number_expression();
    void show_delay_expression();
    void show_real_number_expression();
    void show_string_expression();
    void show_select_expression();
    void show_shallowcopy();
    void show_signal_expression();
    void show_ternary_expression();
    void show_unary_expression();
    void show_concat_expression();
};

class StmtGen {
  public:
    explicit StmtGen(ivl_statement_t net_, pint_thread_info_t thread_, ivl_statement_t parent,
                     unsigned sub_idx, unsigned ind_ = 4);

    string GetStmtGenCode();
    string GetStmtDeclCode();

  private:
    ivl_statement_t net;
    unsigned ind;
    bool is_valid;
    string stmt_code;
    string decl_code;

    pint_thread_info_t thread;

    void GenStmtCode();
    void AppendStmtCode(StmtGen &child_stmt);

    void show_assign_stmt();
    void show_nbassign_stmt();
    void show_condition_stmt();
    void show_stask_stmt();
    void show_utask_stmt();
    void show_wait_stmt();
    void show_while_stmt();
    void show_forever_stmt();
    void show_fork_join_stmt();

    void show_case_stmt(unsigned case_type);
    void show_stmt_wait(ivl_statement_t net, unsigned ind);
    void pint_delay_process(ivl_statement_t net);
    void pint_case_process(ivl_statement_t net, unsigned id_type, unsigned ind);
    void pint_repeat_process(ivl_statement_t net);
    void show_stmt_force(ivl_statement_t net);
    void show_stmt_release(ivl_statement_t net);
    void show_stmt_cassign(ivl_statement_t net);
    void show_stmt_deassign(ivl_statement_t net);
    void show_stmt_trigger(ivl_statement_t net, unsigned ind);
    void show_stmt_print(ivl_statement_t net);
    void pint_nb_assign_enqueue_process(unsigned enqueue_flag, string nb_id, string array_idx);
    void pint_op_base_show(ivl_expr_t oper_po, ExpressionConverter ex_partoff,
                           string partoff_result);
    string gen_signal_value_var(bool isImmediateNum, bool sig_nxz, ivl_expr_t expr,
                                const string *sig_name);
    void pint_assign_process(ivl_statement_t net, bool assign_flag, string delay);
    void show_stmt_nb_assign_info(ivl_lval_t lval, unsigned lval_idx);
};

/********************* Global variable ***********************/
extern unsigned pint_process_num;
extern unsigned pint_event_num;
extern map<ivl_statement_t, int> pint_force_idx;
extern map<ivl_lval_t, pint_force_lval_t> pint_force_lval;
extern map<pint_sig_info_t, map<unsigned, unsigned>> force_net_is_const_map;
extern unsigned global_lpm_num;
extern unsigned monitor_lpm_num;
extern unsigned global_thread_lpm_num;
extern unsigned global_thread_lpm_num1;

extern unsigned lval_idx_num;
extern unsigned pint_nbassign_num;
extern unsigned pint_nbassign_array_num;
extern unsigned pint_always_wait_one_event;
extern string event_static_thread_dec_buff;
extern map<string, string> nb_delay_same_info;
extern map<ivl_event_t, string> event_static_thread_buff;
extern map<ivl_signal_t, pint_signal_s_list_info_s> signal_s_list_map;
extern set<string> exists_sys_args;
extern string monitor_func_buff;
extern string strobe_func_buff;
extern struct dump_info pint_simu_info;
extern vector<string> simu_header_buff;
extern string start_buff;
extern string inc_buff;
extern string global_init_buff;
extern string thread_declare_buff;
extern string child_thread_buff;
extern string dump_file_buff;
extern map<int, string> thread_child_ind;
extern unsigned pint_thread_pc_max;
extern unsigned pint_strobe_num;
extern unsigned pint_static_event_list_table_num;
extern string pint_vcddump_file;
extern unsigned pint_dump_vars_num;
extern unsigned pint_in_task;
extern void *cur_ivl_task_scope;
extern set<string> invoked_fun_names;

extern int pint_design_time_precision;
extern int pint_current_scope_time_unit;
extern pint_nbassign_info pint_nbassign_save;
extern map<string, pint_nbassign_info> nbassign_map;
extern map<ivl_signal_t, unsigned> pint_process_lval_assign;
extern map<ivl_signal_t, unsigned> pint_process_lval_nb;

extern vector<ivl_statement_t> dump_stamts;
extern vector<ivl_statement_t> monitor_stamts;
extern map<ivl_event_t, int> single_depended_events;

extern map<ivl_scope_t, ivl_task_stack_info_t> task_pc_autosig_info;
extern map<ivl_nexus_t, int> Multi_drive_record;
extern map<ivl_nexus_t, int> Multi_drive_record_count;
extern map<ivl_nexus_t, string> Multi_drive_init;

extern string task_name;
extern unsigned pint_process_str_idx;

extern int thread_to_lpm;
extern vector<int> mcc_event_ids;
extern unsigned pint_vpimsg_num;
extern unsigned pint_open_file_num;
extern unsigned pint_perf_on;
extern unsigned force_lpm_num;
extern unsigned pint_const0_max_word;
extern unsigned use_const0_buff;

extern unsigned pint_pli_mode;

#define LPM_THREAD_OFFSET (pint_event_num)
#define FORCE_THREAD_OFFSET                                                                        \
    (pint_event_num + global_lpm_num + monitor_lpm_num + global_thread_lpm_num1)
#define FORCE_LPM_OFFSET (global_lpm_num + monitor_lpm_num + global_thread_lpm_num1)

/******************************************************************************** Std length -100 */
//##    API in "pint.c"
extern void statement_add_list(ivl_statement_t net, unsigned ind,
                               std::set<ivl_signal_t> &sensitive_signals);
extern void pint_source_file_ret(string &buff, const char *file, unsigned lineno);
extern unsigned pint_get_bits_value(const char *bits);
extern unsigned pint_get_ex_number_value(ivl_expr_t net); //  Will assert ret is valid.
extern unsigned long long pint_get_ex_number_value_lu(ivl_expr_t net, bool allow_xz);
extern bool pint_signal_is_force_net_const(pint_sig_info_t sig_info, unsigned word);
//  mult_asgn in process
extern mult_asgn_type_t pint_proc_get_sig_mult_type(ivl_signal_t sig);
extern unsigned pint_proc_get_mult_idx(ivl_signal_t sig);
extern bool pint_asgn_is_full(ivl_lval_t lval);
extern bool pint_proc_mult_sig_needs_mask(ivl_signal_t sig);
extern string pint_proc_get_sig_name(ivl_signal_t sig, const char *arr_idx, bool is_asgn);
extern void pint_proc_updt_mult_asgn(string &buff, ivl_signal_t sig);
extern string pint_proc_gen_exit_code(bool has_ret, int ret_val);
extern string pint_proc_gen_exit_code();
extern bool pint_parse_mult_up_oper(string &buff, ivl_statement_t parent, unsigned sub_idx);
extern bool pint_parse_mult_down_oper(string &buff, ivl_statement_t parent, unsigned sub_idx);
extern void pint_show_sig_mult_anls_info(ivl_signal_t sig);
extern bool pint_proc_has_addl_opt_after_sig_asgn(ivl_signal_t sig);
extern void pint_proc_mult_opt_before_sig_asgn(string &buff, ivl_lval_t lval, const char *word);
extern void pint_proc_mult_opt_when_sig_asgn(string &buff, ivl_signal_t sig, const char *word,
                                             const char *asgn_base, unsigned asgn_width);
extern void pint_proc_mult_opt_after_sig_asgn(string &buff, ivl_signal_t sig, const char *word);
extern void pint_proc_mult_opt_before_sig_use(string &buff, ivl_signal_t sig, const char *word);
extern void pint_proc_mult_opt_before_sig_use(string &buff, ivl_signal_t sig, unsigned word);

extern void pint_init_thread_info(pint_thread_info_t thread, unsigned id, unsigned to_lpm,
                                  unsigned delay_num, ivl_scope_t scope, ivl_process_t proc);
extern int pint_find_lpm_from_output(ivl_nexus_t nex); //  If not find, return -1

extern void pint_show_ex_number(string &oper, string &decl, bool *p_is_immediate_num,
                                bool *p_has_xz, const char *name, const char *bits, unsigned width,
                                ex_number_type_t cmd_type, const char *file, unsigned lineno);
extern void pint_show_expression_string(string &buff_pre, string &buff_oper, ivl_expr_t net,
                                        ivl_statement_t stmt, unsigned idx);

pint_event_info_t pint_find_event(ivl_event_t event);
pint_event_info_t pint_find_event(unsigned idx);
bool pint_is_evt_needs_dump(unsigned idx);
bool pint_find_process_event(ivl_event_t event);
void pint_add_process_event(ivl_event_t evt);
extern string string_format(const char *format, ...);
extern void pint_vcddump_var(pint_sig_info_t sig_info, string *buff);
extern void pint_vcddump_var_without_check(pint_sig_info_t sig_info, string *buff);
extern void pint_vcddump_off(pint_sig_info_t sig_info, string *buff);
extern void pint_vcddump_init(string *buff);
extern void pint_vcddump_off(string *buff);
extern void pint_vcddump_on(string *buff);
//  asgn_count in dsgn      --API
extern void pint_print_signal_asgn_info(pint_sig_info_t sig_info);
extern bool pint_signal_asgn_in_mult_threads(ivl_signal_t sig);
extern bool pint_signal_is_part_asgn_in_thread(ivl_signal_t sig, ivl_process_t proc);

/******************************************************************************** Std length -100 */
//  API in "pint_show_signal.c"
extern void pint_show_signal_define(string &buff, string &dec_buff, pint_sig_info_t sig_info,
                                    unsigned is_nb);
extern void pint_print_sig_info_details(pint_sig_info_t sig_info); //  used for debug
extern pint_sig_info_t pint_find_signal(ivl_signal_t sig);
extern pint_sig_info_t pint_find_signal(ivl_nexus_t nex);
extern ivl_nexus_t pint_find_nex(pint_sig_info_t sig_info, unsigned word);
extern pint_sig_sen_t pint_find_signal_sen(pint_sig_info_t sig_info, unsigned word);
extern pint_force_info_t pint_find_signal_force_info(pint_sig_info_t sig_info, unsigned word);
extern unsigned pint_get_arr_idx_from_nex(ivl_nexus_t nex);
//  Note(next): if sig = sig_arr, ret = 0xffffffff(means can not get arr_idx from sig_arr)
extern unsigned pint_get_arr_idx_from_sig(ivl_signal_t sig);
extern string pint_get_sig_value_name(pint_sig_info_t sig_info, unsigned word);
extern string pint_get_force_sig_name(pint_sig_info_t sig_info, unsigned word);
extern string pint_gen_sig_l_list_code(pint_sig_info_t sig_info, unsigned word);
extern string pint_gen_sig_l_list_code(pint_sig_info_t sig_info, const char *word);
extern string pint_gen_sig_l_list_code(pint_sig_info_t sig_info, const char *list, unsigned word);
extern string pint_gen_sig_e_list_code(pint_sig_info_t sig_info, const char *edge, unsigned word);
extern string pint_gen_sig_e_list_code(pint_sig_info_t sig_info, const char *edge,
                                       const char *word);
extern string pint_gen_sig_e_list_code(pint_sig_info_t sig_info, const char *list, const char *edge,
                                       unsigned word);
extern string pint_gen_sig_p_list_code(pint_sig_info_t sig_info);
extern bool pint_signal_has_sen(pint_sig_info_t sig_info, unsigned word);
//  Function: check whether the entire sig_info has sen
extern bool pint_signal_has_sen(pint_sig_info_t sig_info);
extern bool pint_signal_has_sen_evt(pint_sig_info_t sig_info, unsigned word);
//  Function: check whether the entire sig_info has sen evt
extern bool pint_signal_has_sen_evt(pint_sig_info_t sig_info);
extern bool pint_signal_has_static_list(pint_sig_info_t sig_info);
extern bool pint_signal_has_sen_lpm(pint_sig_info_t sig_info, unsigned word);
//  Function: check whether the entire sig_info has sen lpm
extern bool pint_signal_has_sen_lpm(pint_sig_info_t sig_info);
extern bool pint_signal_has_p_list(pint_sig_info_t sig_info);
extern bool pint_signal_has_addl_opt_when_updt(pint_sig_info_t sig_info, unsigned word);
extern bool pint_signal_has_addl_opt_when_updt(pint_sig_info_t sig_info);
extern bool pint_signal_is_const(pint_sig_info_t sig_info, unsigned word);
extern bool pint_signal_is_const(ivl_nexus_t nex);
extern unsigned pint_get_const_value(ivl_nexus_t nex);
extern void pint_ret_signal_const_value(string &buff, pint_sig_info_t sig_info, unsigned word);
//  Note(next): For arr, return true when an element has force_info
extern bool pint_signal_been_forced(pint_sig_info_t sig_info);
extern bool pint_sig_is_force_var(pint_sig_info_t sig_info);
extern void pint_sig_info_add_lpm(pint_sig_info_t sig_info, unsigned word, unsigned lpm_idx);
extern void pint_sig_info_add_lpm(ivl_nexus_t nex, unsigned lpm_idx);
extern void pint_sig_info_add_event(ivl_nexus_t nex, unsigned event_idx, event_type_t type);
extern void pint_sig_info_set_const(pint_sig_info_t sig_info, unsigned word, unsigned is_const);
extern void pint_revise_sig_info_to_global(ivl_nexus_t nex);
extern void pint_revise_sig_info_to_global(ivl_signal_t sig);
extern pint_force_info_t pint_gen_new_force_info(ivl_signal_t sig, unsigned word,
                                                 ivl_statement_t stmt, unsigned type);
extern pint_sig_info_t pint_gen_force_shadow_sig_info(pint_sig_info_t sig_info, unsigned word);
extern void pint_reset_sig_init_value(ivl_nexus_t nex, sig_bit_val_t bit_updt);

extern string pint_gen_signal_set_x_code(const char *sn, unsigned width);

/******************************************************************************** Std length -100 */
//  API in "pint_show_lpm.c"
extern void pint_lpm_set_no_need_init(ivl_nexus_t nex);
extern void pint_give_lpm_no_need_init_idx();
extern bool pint_is_lpm_needs_init(unsigned idx);

/*
 * This is the output file where the generated result should be
 * written.
 */
extern FILE *out;

/*
 * Keep a running count of errors that the stub detects. This will be
 * the error count returned to the ivl_target environment.
 */
extern int stub_errors;

/*
 * This function finds the vector width of a signal. It relies on the
 * assumption that all the signal inputs to the nexus have the same
 * width. The ivl_target API should assert that condition.
 */
extern unsigned width_of_nexus(ivl_nexus_t nex);

extern ivl_variable_type_t type_of_nexus(ivl_nexus_t nex);

extern ivl_discipline_t discipline_of_nexus(ivl_nexus_t nex);

extern unsigned width_of_type(ivl_type_t net);

/*
 * Test that a given expression is a valid delay expression, and
 * print an error message if not.
 */
extern void test_expr_is_delay(ivl_expr_t expr);

extern void show_class(ivl_type_t net);
extern void show_enumerate(ivl_enumtype_t net);
extern void show_constant(ivl_net_const_t net);

/*
 * Show the details of the expression.
 */
extern void show_expression(ivl_expr_t net, unsigned ind);

/*
 * Show the type of the signal, in one line.
 */
extern void show_type_of_signal(ivl_signal_t);

extern void show_switch(ivl_switch_t net);

/*
 * Show the type of the net
*/
extern const char *data_type_string(ivl_variable_type_t vtype);

extern void show_net_type(ivl_type_t net_type);

extern string code_of_expr_value(ivl_expr_t expr, const char *name, bool sig_nxz);
extern string code_of_sig_value(const char *sig_name, unsigned width, bool is_signed, bool sig_nxz);
extern string code_of_bit_select(const char *src_name, unsigned src_width, const char *dst_name,
                                 unsigned dst_width, const char *base, const char *src_nxz_s);

// nxz
bool pint_proc_sig_nxz_get(ivl_statement_t stmt, ivl_signal_t sig);

extern unsigned pint_show_expression(ivl_expr_t net);
extern void pint_add_process_sens_signal(pint_sig_info_t sig_info, unsigned process_id);
extern void analysis_process(ivl_statement_t net, unsigned ind, struct process_info *proc_info,
                             int opcode);
extern void analysis_free_process_info(struct process_info *p);
#endif /* IVL_priv_H */
