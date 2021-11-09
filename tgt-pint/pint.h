/*******************< Funciton in pint_show_signal.c >*******************/
//  Show sig_info
void pint_gen_const_value(string &buff, vector<ivl_net_const_t> net);
pint_sig_info_t pint_gen_new_sig_info(ivl_signal_t net);
void pint_gen_siganl_init_value(string &buff, sig_bit_val_t bit_val, unsigned width);
sig_bit_val_t pint_get_sig_info_type(ivl_signal_t sig, unsigned word);
sig_bit_val_t pint_get_signal_type(ivl_signal_t sig);
vector<ivl_net_const_t> pint_parse_nex_con_info(ivl_nexus_t nex);
ivl_signal_t pint_parse_signal_arr_info(ivl_signal_t net);
void pint_parse_signal_con_val_info(pint_sig_info_t sig_info, ivl_signal_t net);
void pint_parse_arr_nex(set<unsigned> &ret, pint_sig_info_t sig_info);
void pint_parse_signal_dump_info(pint_sig_info_t sig_info, ivl_signal_t net);
void pint_update_sig_info_map(pint_sig_info_t net, ivl_signal_t sig_arr);
void pint_add_nex_into_sig_map(ivl_nexus_t nex, pint_sig_info_t sig_info);
void pint_add_signal_into_sig_map(ivl_signal_t sig, pint_sig_info_t sig_info);
void pint_add_arr_idx_into_sig_map(ivl_signal_t sig, int arr_idx);
void pint_show_sig_name(string &sig_name, ivl_signal_t net, bool is_arr);

//  Dump sig_info
void pint_dump_signal();
void pint_remove_sig_info_no_need_dump();
void pint_show_signal_sen_list_define(string &buff, string &dec_buff, pint_sig_info_t sig_info);
void pint_show_signal_callback_list_define(string &buff, pint_sig_info_t sig_info);
void pint_sen_gen_list_define(string &buff, string &dec_buff, pint_sig_sen_t sen,
                              const char *sig_name, const char *sig_sub);
void pint_show_signal_force_define(string &buff, string &dec_buff, pint_sig_info_t sig_info);
void pint_sig_info_gen_force_var_define(string &buff, string &dec_buff, pint_sig_info_t sig_info,
                                        pint_force_info_t force_info, unsigned word);

/*******************< Funciton in pint_show_lpm.c >*******************/
//  Show LPM
void pint_logic_process(ivl_net_logic_t net);
void pint_lpm_process(ivl_lpm_t net);
void pint_switch_process(ivl_switch_t net);
void pint_record_lpm_cmn_info(pint_lpm_log_info_t lpm_info);

//  Add lpm into sig_info sensitive
void pint_lpm_add_list();
void pint_parse_lpm_rhs(pint_lpm_log_info_t net_info, string scope_name,
                        struct pint_cmn_lpm_info *cmn_info);
pint_lpm_log_info_t pint_find_local_lpm(ivl_nexus_t nex);

//  Dump LPM
void pint_dump_lpm(void);
void pint_dump_thread_lpm(void);
void pint_show_lpm_signature(string &buf, pint_lpm_log_info_t net_info);
void pint_show_lpm(pint_lpm_log_info_t net_info, string &operation, string &declare);
void pint_show_logic_type(string &operation, string &declare, pint_lpm_log_info_t net_info);
void pint_show_lpm_type(string &operation, string &declare, pint_lpm_log_info_t net_info);
void pint_show_switch_type(string &ret, pint_lpm_log_info_t net_info);
void pint_lpm_sensitive_sig(ivl_lpm_t net, vector<ivl_nexus_t> &rhs_nex, ivl_signal_t &rhs_arr);
void pint_gen_lpm_force_cond(string &buff, ivl_nexus_t nex);
void pint_gen_lpm_force_assign(string &buff, ivl_nexus_t nex);

//  Common LPM
pint_cmn_lpm_info_t pint_get_cmn_lpm_info(pint_lpm_log_info_t net_info);
string pint_get_lpm_scope_name(pint_lpm_log_info_t net_info);
string pint_get_sig_decl_str(pint_sig_info_t sig_info, string decl_name, bool is_output);
void pint_gen_lpm_call_cmn(bool gen_cmn_decl, unsigned lpm_id);
void pint_diff_lpm_leaf_list(struct pint_cmn_lpm_info *cmn_info);
void pint_clear_cur_lpm_info();
void pint_set_cur_lpm_cmn_info(pint_lpm_log_info_t net_info, unsigned lpm_id);

//  Common Functions
void pint_show_sig_define(string &ret, pint_sig_info_t sig_info);
void pint_lpm_undrived_sig(string &ret, pint_sig_info_t sig_info);
unsigned pint_nexus_find_arr_word(ivl_signal_t sig, ivl_nexus_t nex);
string pint_lpm_gen_output_list_decl();
void pint_lpm_update_output(string &ret, ivl_nexus_t nex, pint_lpm_log_info_t net_info);
void pint_lpm_update_multi_drv_lock(string &ret, ivl_nexus_t nex);
void pint_lpm_update_multi_drv_unlock(string &ret, ivl_nexus_t nex);
void pint_get_sig_name(string &ret, ivl_nexus_t nex);
void pint_get_sig_name(string &ret, ivl_signal_t sig);
const char *pint_find_bits_from_nexus(ivl_nexus_t net);
bool pint_bits_is_zero(const char *bits);
string pint_lpm_show_source_file(pint_lpm_log_info_t net_info);
string pint_ret_sig_sign_bit(ivl_nexus_t nex);
unsigned pint_log_chk_support(ivl_net_logic_t net);
unsigned pint_lpm_chk_support(ivl_lpm_t net);

/*******************< Funciton in pint.c >*******************/
double pint_exec_duration(struct timeval &st, struct timeval &ed, unsigned &sts);
static int show_process(ivl_process_t net, pint_mult_info_t mult_info);
void show_parameter(string &ret, ivl_parameter_t net);
void pint_parse_event_sig_info(ivl_event_t net);
void pint_parse_process_stmt_info(ivl_statement_t net, int in_branch, int in_fork);
int pint_loop_proc_in_dsgn(ivl_process_t net, void *x);
void pint_parse_all_force_const_lpm();
void pint_show_force_const_lpm(pint_sig_info_t sig_info, unsigned word, unsigned force_idx);
void pint_parse_all_force_shadow_lpm();
void pint_loop_stmt_in_proc(ivl_statement_t net);
void pint_parse_lval_force_info(ivl_lval_t net, ivl_statement_t stmt, unsigned is_force,
                                unsigned is_assign);
void pint_dump_force_lpm();
void pint_show_force_shadow_lpm(pint_sig_info_t sig_info, unsigned word, unsigned force_idx);
void pint_gen_lpm_head(string &buff, string &decl_buff, unsigned thread_id);
void pint_gen_force_lpm_update(string &buff, pint_sig_info_t sig_info, unsigned word, string source,
                               string mask);
void pint_show_force_lpm(ivl_statement_t net, unsigned force_idx, unsigned type);
void pint_add_force_lpm_into_rhs_sen(const set<pint_sig_info_t> *rhs_sig, unsigned lpm_idx);
void pint_show_stmt_signature(string &buff, ivl_statement_t net);
void pint_parse_new_force_lhs(pint_force_lhs_t force_lhs, pint_force_lval_t lval_info,
                              const char *rhs, unsigned rhs_width, unsigned rhs_base,
                              unsigned thread_id, unsigned type);
void pint_parse_exists_force_lhs(pint_force_lhs_t force_lhs, pint_force_lval_t lval_info,
                                 const char *rhs, unsigned rhs_width, unsigned rhs_base);
// nxz
void pint_proc_sig_nxz_clear();
