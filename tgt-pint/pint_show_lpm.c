//  Global variable of LPM
vector<pint_lpm_log_info_t> pint_global_lpm;
map<ivl_nexus_t, unsigned> pint_nex_to_lpm_idx; //  pin of output
map<ivl_nexus_t, pint_lpm_log_info_t> pint_local_lpm;
set<ivl_nexus_t> pint_local_lpm_defined;
map<string, struct pint_cmn_lpm_info> pint_cmn_lpm;
set<ivl_nexus_t> pint_lpm_no_need_init;
set<unsigned> pint_lpm_no_need_init_idx;

unsigned local_lpm_num = 0;
unsigned global_lpm_num = 0;
unsigned monitor_lpm_num = 0;
// dlpm
unsigned delay_lpm_num = 0;
map<int, int> delay_to_global_lpm_idx;

unsigned global_thread_lpm_num = 0;
unsigned global_thread_lpm_num1 = 0;
int thread_to_lpm = -1;
unsigned pint_curr_lpm_idx = 0;
unsigned lpm_buff_file_size = 0;

int lpm_tmp_char = 0; //  0, The global lpm hasn't defined  'tmp_char'; 1, The global lpm defined
                      //  var 'tmp_char' already.
int lpm_tmp_int_cnt = 0;
unsigned lpm_tmp_idx = 0;
unsigned lpm_tmp_value_idx = 0;
string lpm_buff;
string lpm_declare_buff;
string cur_lpm_buff;

struct pint_cur_lpm_info cur_lpm_info;
unsigned lpm_total_cmn_num = 0;
unsigned lpm_total_call_cmn_num = 0;

/*******************< Show LPM >*******************/
void pint_logic_process(ivl_net_logic_t net) {
    if (!pint_log_chk_support(net)) {
        return;
    }
    pint_lpm_log_info_t lpm_info = new struct pint_lpm_log_info;
    ivl_nexus_t nex = ivl_logic_pin(net, 0); //  nex of output
    pint_sig_info_t sig_info = pint_find_signal(nex);

    lpm_info->type = LOG;
    lpm_info->nex = nex;
    lpm_info->log = net;
    if (sig_info->is_local) {
        lpm_info->idx = local_lpm_num++;
        pint_local_lpm[nex] = lpm_info;

        if (ivl_logic_delay(net, 0)) {
            PINT_UNSUPPORTED_WARN(ivl_logic_file(net), ivl_logic_lineno(net),
                                  "assign delay in local lpm");
        }
    } else {
        pint_nex_to_lpm_idx[nex] = global_lpm_num;
        lpm_info->idx = global_lpm_num++;
        pint_global_lpm.push_back(lpm_info);
        pint_record_lpm_cmn_info(lpm_info);
    }
}

void pint_lpm_process(ivl_lpm_t net) {
    if (!pint_lpm_chk_support(net)) {
        return;
    }
    pint_lpm_log_info_t lpm_info = new struct pint_lpm_log_info;
    ivl_nexus_t nex = ivl_lpm_q(net); //  nex of output
    pint_sig_info_t sig_info = pint_find_signal(nex);

    lpm_info->type = LPM;
    lpm_info->nex = nex;
    lpm_info->lpm = net;
    if (sig_info->is_local) {
        lpm_info->idx = local_lpm_num++;
        pint_local_lpm[nex] = lpm_info;

        if (ivl_lpm_delay(net, 0)) {
            PINT_UNSUPPORTED_WARN(ivl_lpm_file(net), ivl_lpm_lineno(net),
                                  "assign delay in local lpm");
        }
    } else {
        pint_nex_to_lpm_idx[nex] = global_lpm_num;
        lpm_info->idx = global_lpm_num++;
        pint_global_lpm.push_back(lpm_info);
        pint_record_lpm_cmn_info(lpm_info);
    }
}

void pint_switch_process(ivl_switch_t net) {
    pint_lpm_log_info_t lpm_info = new struct pint_lpm_log_info;
    ivl_nexus_t nex = ivl_switch_b(net); //  nex of output
    pint_sig_info_t sig_info = pint_find_signal(nex);

    PINT_UNSUPPORTED_ERROR(ivl_switch_file(net), ivl_switch_lineno(net), "switch");

    lpm_info->type = SWITCH_;
    lpm_info->nex = nex;
    lpm_info->switch_ = net;
    if (sig_info->is_local) {
        lpm_info->idx = local_lpm_num++;
        pint_local_lpm[nex] = lpm_info;
    } else {
        pint_nex_to_lpm_idx[nex] = global_lpm_num;
        lpm_info->idx = global_lpm_num++;
        pint_global_lpm.push_back(lpm_info);
        pint_record_lpm_cmn_info(lpm_info);
    }
}

void pint_record_lpm_cmn_info(pint_lpm_log_info_t lpm_info) {
    string file_lineno_basename;
    string scope_name;
    ivl_scope_t m_scope;
    unsigned is_generate_type = 0;
    if (LOG == lpm_info->type) {
        file_lineno_basename =
            string_format("%s_%d_%s", ivl_logic_file(lpm_info->log),
                          ivl_logic_lineno(lpm_info->log), ivl_logic_basename(lpm_info->log));
        m_scope = ivl_logic_scope(lpm_info->log);
        scope_name = ivl_scope_name(m_scope);
    } else if (SWITCH_ == lpm_info->type) {
        file_lineno_basename = string_format("%s_%d_%s", ivl_switch_file(lpm_info->switch_),
                                             ivl_switch_lineno(lpm_info->switch_),
                                             ivl_switch_basename(lpm_info->switch_));
        m_scope = ivl_switch_scope(lpm_info->switch_);
        scope_name = ivl_scope_name(m_scope);
    } else {
        file_lineno_basename =
            string_format("%s_%d_%s", ivl_lpm_file(lpm_info->lpm), ivl_lpm_lineno(lpm_info->lpm),
                          ivl_lpm_basename(lpm_info->lpm));
        m_scope = ivl_lpm_scope(lpm_info->lpm);
        scope_name = ivl_scope_name(m_scope);
    }

    map<string, struct pint_cmn_lpm_info>::iterator iter;
    iter = pint_cmn_lpm.find(file_lineno_basename);
    if (iter != pint_cmn_lpm.end()) {
        if (scope_name != iter->second.fst_scope_name) {
            iter->second.inst_count++;
            vector<pint_sig_info_t> tmp_vec_sig;
            iter->second.leaf_list.insert(
                pair<string, vector<pint_sig_info_t>>(scope_name, tmp_vec_sig));
            map<pint_sig_info_t, string> tmp_para;
            iter->second.para_decl.insert(
                pair<string, map<pint_sig_info_t, string>>(scope_name, tmp_para));
        }
    } else {
        struct pint_cmn_lpm_info tmp_cmn_lpm;
        tmp_cmn_lpm.fst_scope_name = scope_name;
        tmp_cmn_lpm.inst_count = 1;
        tmp_cmn_lpm.leaf_count = 0;
        tmp_cmn_lpm.para_count = 0;
        tmp_cmn_lpm.diff_done = 0;
        tmp_cmn_lpm.call_count = 0;
        tmp_cmn_lpm.cmn_lpm_id = 0;

        while (m_scope != NULL) {
            if (ivl_scope_type(m_scope) == IVL_SCT_GENERATE) {
                is_generate_type = 1;
            }
            m_scope = ivl_scope_parent(m_scope);
        }
        tmp_cmn_lpm.is_generate_type = is_generate_type;

        vector<pint_sig_info_t> tmp_vec_sig;
        tmp_cmn_lpm.leaf_list.insert(
            pair<string, vector<pint_sig_info_t>>(scope_name, tmp_vec_sig));
        map<pint_sig_info_t, string> tmp_para;
        tmp_cmn_lpm.para_decl.insert(
            pair<string, map<pint_sig_info_t, string>>(scope_name, tmp_para));
        pint_cmn_lpm.insert(
            pair<string, struct pint_cmn_lpm_info>(file_lineno_basename, tmp_cmn_lpm));
    }
}

/*******************< Add lpm into sig_info sensitive >*******************/
void pint_lpm_add_list() {
    for (unsigned int i = 0; i < global_lpm_num; i++) {
        pint_curr_lpm_idx = i;
        pint_lpm_log_info_t net_info = pint_global_lpm[i];
        struct pint_cmn_lpm_info *cmn_info = pint_get_cmn_lpm_info(pint_global_lpm[i]);
        string scope_name;
        ivl_scope_t m_scope;
        if (net_info->type == LOG) {
            ivl_net_logic_t net = net_info->log;
            m_scope = ivl_logic_scope(net);
            scope_name = ivl_scope_name(m_scope);
            ivl_nexus_t nex_ri = ivl_logic_pin(net, 0);
            pint_sig_info_t sig_ri = pint_find_signal(nex_ri);
            cmn_info->leaf_list[scope_name].push_back(sig_ri);
        } else if (net_info->type == SWITCH_) {
            ivl_switch_t net = net_info->switch_;
            m_scope = ivl_switch_scope(net);
            scope_name = ivl_scope_name(m_scope);
            ivl_nexus_t nex_ri = ivl_switch_a(net);
            pint_sig_info_t sig_ri = pint_find_signal(nex_ri);
            cmn_info->leaf_list[scope_name].push_back(sig_ri);
        } else {
            ivl_lpm_t net = net_info->lpm;
            ivl_lpm_type_t type = ivl_lpm_type(net);
            m_scope = ivl_lpm_scope(net);
            scope_name = ivl_scope_name(m_scope);
            ivl_nexus_t nex = ivl_lpm_q(net);
            pint_sig_info_t sig = pint_find_signal(nex);
            cmn_info->leaf_list[scope_name].push_back(sig);
            if (IVL_LPM_UFUNC == type) {
                cmn_info->inst_count = 0;
            }
        }
        pint_parse_lpm_rhs(net_info, scope_name, cmn_info);
        if (0 == cmn_info->leaf_count) {
            cmn_info->leaf_count = cmn_info->leaf_list[scope_name].size();
        } else {
            // if leaf count not same, do not gen cmn func
            if (cmn_info->leaf_count != cmn_info->leaf_list[scope_name].size()) {
                cmn_info->inst_count = 0;
            }
        }
    }
}

void pint_parse_lpm_rhs(pint_lpm_log_info_t net_info, string scope_name,
                        struct pint_cmn_lpm_info *cmn_info) {
    if (!net_info)
        return;
    ivl_nexus_t nex;
    pint_sig_info_t sig_info;
    unsigned is_const;
    unsigned i;
    if (net_info->type == LOG) {
        ivl_net_logic_t net = net_info->log;
        unsigned npins = ivl_logic_pins(net);
        for (i = 1; i < npins; i++) {
            nex = ivl_logic_pin(net, i);
            sig_info = pint_find_signal(nex);
            is_const = pint_signal_is_const(nex);
            if (sig_info->is_local && !is_const) {
                pint_parse_lpm_rhs(pint_find_local_lpm(nex), scope_name, cmn_info);
            } else {
                pint_sig_info_add_lpm(nex, pint_curr_lpm_idx);
                cmn_info->leaf_list[scope_name].push_back(sig_info);
            }
        }
    } else if (net_info->type == SWITCH_) {
        ivl_switch_t net = net_info->switch_;
        nex = ivl_switch_a(net);
        sig_info = pint_find_signal(nex);
        is_const = pint_signal_is_const(nex);
        if (sig_info->is_local && !is_const) {
            pint_parse_lpm_rhs(pint_find_local_lpm(nex), scope_name, cmn_info);
        } else {
            pint_sig_info_add_lpm(nex, pint_curr_lpm_idx);
            cmn_info->leaf_list[scope_name].push_back(sig_info);
        }
    } else {
        ivl_lpm_t net = net_info->lpm;
        ivl_lpm_type_t type = ivl_lpm_type(net);
        if (IVL_LPM_UFUNC == type) {
            cmn_info->inst_count = 0;
        }
        vector<ivl_nexus_t> rhs_nex;
        ivl_signal_t rhs_arr = NULL; //  Only used by IVL_LPM_ARRAY
        pint_lpm_sensitive_sig(net, rhs_nex, rhs_arr);
        unsigned nrhs = rhs_nex.size();
        for (i = 0; i < nrhs; i++) {
            nex = rhs_nex[i];
            sig_info = pint_find_signal(nex);
            is_const = pint_signal_is_const(nex);
            if (sig_info->is_local && !is_const) {
                pint_parse_lpm_rhs(pint_find_local_lpm(nex), scope_name, cmn_info);
            } else {
                pint_sig_info_add_lpm(nex, pint_curr_lpm_idx);
                cmn_info->leaf_list[scope_name].push_back(sig_info);
            }
        }
        if (rhs_arr) {
            sig_info = pint_find_signal(rhs_arr);
            if (!(sig_info->is_local)) { //  arr can not be the output of a lpm
                pint_sig_info_add_lpm(sig_info, 0xffffffff, pint_curr_lpm_idx);
                cmn_info->leaf_list[scope_name].push_back(sig_info);
            }
        }
    }
}

pint_lpm_log_info_t pint_find_local_lpm(ivl_nexus_t nex) {
    if (pint_local_lpm.count(nex)) {
        return pint_local_lpm[nex];
    } else {
        return NULL;
    }
}

// dlpm
ivl_expr_t pint_lpm_delay_expr(pint_lpm_log_info_t net_info, unsigned idx) {
    ivl_expr_t delay_expr;
    if (net_info->type == LOG) {
        ivl_net_logic_t net = net_info->log;
        delay_expr = ivl_logic_delay(net, idx);
    } else {
        ivl_lpm_t net = net_info->lpm;
        delay_expr = ivl_lpm_delay(net, idx);
    }
    return delay_expr;
}

ivl_expr_type_t pint_show_lpm_delay(string &delay_value_str, ivl_expr_t delay_expr) {
    ivl_expr_type_t type_delay = ivl_expr_type(delay_expr);
    switch (type_delay) {
    case IVL_EX_NUMBER: {
        delay_value_str = string_format("%lu", pint_get_ex_number_value_lu(delay_expr, true));
        break;
    }
    case IVL_EX_SIGNAL: {
        ivl_signal_t sig_delay = ivl_expr_signal(delay_expr);
        pint_sig_info_t sig_info_delay = pint_find_signal(sig_delay);
        if (sig_info_delay->arr_words == 1) {
            if (sig_info_delay->is_local && !sig_info_delay->is_const) {
                pint_lpm_log_info_t lpm_delay = pint_find_local_lpm(ivl_signal_nex(sig_delay, 0));
                if (lpm_delay) {
                    string oper_tmp;
                    string dec_tmp;
                    pint_show_lpm(lpm_delay, oper_tmp, dec_tmp);
                    cur_lpm_buff += dec_tmp + oper_tmp;
                    string delay_sig_name;
                    pint_get_sig_name(delay_sig_name, sig_delay);
                    delay_value_str = string_format("%s_value", delay_sig_name.c_str());
                    string delay_code = "        unsigned int " + delay_value_str + " = ";
                    delay_code +=
                        code_of_expr_value(delay_expr, delay_sig_name.c_str(), false) + ";\n";
                    cur_lpm_buff += delay_code;
                } else {
                    delay_value_str = "0";
                }
            } else {
                string delay_sig_name = sig_info_delay->sig_name;
                delay_value_str = string_format("%s_value", delay_sig_name.c_str());
                string delay_code = "        unsigned int " + delay_value_str + " = ";
                delay_code += code_of_expr_value(delay_expr, delay_sig_name.c_str(), false) + ";\n";
                cur_lpm_buff += delay_code;
            }
        } else {
            assert(0);
        }
        break;
    }
    default: {
        assert(0);
        break;
    }
    }
    return type_delay;
}

/*******************< Dump LPM >*******************/
void pint_dump_lpm(void) {
    map<int, string> cmn_dump_map;
    int cmn_lpm_num = 0;
    size_t write_bytes;
    if (strcmp(enval, enval_flag) != 0) {
        if (lpm_buff.size() == 0) {
            lpm_buff = inc_start_buff + lpm_buff;
        }
    } else {
        lpm_buff += "//lpm info\n";
    }
    lpm_declare_buff += "//lpm declare\n";
    for (unsigned i = 0; i < global_lpm_num; i++) {
        lpm_tmp_char = 0;
        lpm_tmp_int_cnt = 0;
        lpm_tmp_value_idx = 0;
        cur_lpm_buff.clear();

        unsigned idx = i + pint_event_num;
        lpm_declare_buff += string_format("void pint_lpm_%u(void);\n", idx);
        // dlpm
        ivl_expr_t delay_expr = pint_lpm_delay_expr(pint_global_lpm[i], 0);
        ivl_expr_t delay_expr1 = pint_lpm_delay_expr(pint_global_lpm[i], 1);
        ivl_expr_t delay_expr2 = pint_lpm_delay_expr(pint_global_lpm[i], 2);
        if (!delay_expr) {
            pint_set_cur_lpm_cmn_info(pint_global_lpm[i], idx);
        } else {
            pint_clear_cur_lpm_info();

            if ((delay_expr1 != delay_expr) || (delay_expr2 != delay_expr)) {
                if (pint_global_lpm[i]->type == LOG) {
                    PINT_UNSUPPORTED_ERROR(ivl_logic_file(pint_global_lpm[i]->log),
                                           ivl_logic_lineno(pint_global_lpm[i]->log),
                                           "two or three delays in assign delay");
                } else if (pint_global_lpm[i]->type == LPM) {
                    PINT_UNSUPPORTED_ERROR(ivl_lpm_file(pint_global_lpm[i]->lpm),
                                           ivl_lpm_lineno(pint_global_lpm[i]->lpm),
                                           "two or three delays in assign delay");
                } else {
                    assert(0);
                }
            }
        }
        // printf("------cur lpm info: is_cmn:%d para_count:%d map_size:%d need_gen_cmn:%d\n",
        //        cur_lpm_info.is_cmn, cur_lpm_info.para_count, cur_lpm_info.sig_para.size(),
        //        cur_lpm_info.need_gen_cmn);
        unsigned insert_pos = 0;
        if (cur_lpm_info.need_gen_cmn) {
            pint_show_lpm_signature(cur_lpm_buff, pint_global_lpm[i]);
            insert_pos = cur_lpm_buff.length();
        } else {
            cur_lpm_buff += string_format("void pint_lpm_%u(void){\n", idx);
            pint_show_lpm_signature(cur_lpm_buff, pint_global_lpm[i]);
            if (pint_perf_on) {
                unsigned lpm_perf_id = i + PINT_PERF_BASE_ID;
                cur_lpm_buff += string_format("    NCORE_PERF_SUMMARY(%d);\n", lpm_perf_id);
            }
            cur_lpm_buff += string_format("\tpint_enqueue_flag_clear(%d);\n", idx);
        }

        ivl_nexus_t nex = pint_global_lpm[i]->nex;
        // dlpm
        if (delay_expr) {
            string delay_stru_str = string_format("pint_lpm_future_thread[%d]", delay_lpm_num);
            cur_lpm_buff += string_format("    if (%s.lpm_state == FUTURE_LPM_STATE_WAIT) {\n",
                                          delay_stru_str.c_str());

            string delay_value_str;
            ivl_expr_type_t type_delay = pint_show_lpm_delay(delay_value_str, delay_expr);

            cur_lpm_buff += string_format("        //delay %s\n", delay_value_str.c_str());
            if (type_delay == IVL_EX_NUMBER) {
                if (delay_value_str != "0") {
                    cur_lpm_buff += string_format("        %s.T = T + %s;\n",
                                                  delay_stru_str.c_str(), delay_value_str.c_str());
                    cur_lpm_buff += string_format("        pint_lpm_enqueue_thread_future(&%s, "
                                                  "FUTURE_Q, %d);\n",
                                                  delay_stru_str.c_str(), delay_lpm_num);
                } else {
                    cur_lpm_buff += string_format("        %s.lpm_state = FUTURE_LPM_STATE_EXEC;\n",
                                                  delay_stru_str.c_str());
                    cur_lpm_buff +=
                        string_format("        pint_enqueue_thread(%s.run_func, INACTIVE_Q);\n",
                                      delay_stru_str.c_str());
                }
            } else {
                cur_lpm_buff += string_format("        if (%s > 0) {\n", delay_value_str.c_str());
                cur_lpm_buff += string_format("            %s.T = T + %s;\n",
                                              delay_stru_str.c_str(), delay_value_str.c_str());
                cur_lpm_buff += string_format("            pint_lpm_enqueue_thread_future(&%s, "
                                              "FUTURE_Q, %d);\n",
                                              delay_stru_str.c_str(), delay_lpm_num);
                cur_lpm_buff += "        } else {\n";
                cur_lpm_buff += string_format("            %s.lpm_state = FUTURE_LPM_STATE_EXEC;\n",
                                              delay_stru_str.c_str());
                cur_lpm_buff +=
                    string_format("            pint_enqueue_thread(%s.run_func, INACTIVE_Q);\n",
                                  delay_stru_str.c_str());
                cur_lpm_buff += "        }\n";
            }
            cur_lpm_buff += "        return;\n";
            cur_lpm_buff += "    }\n";
            cur_lpm_buff += string_format("    %s.lpm_state = FUTURE_LPM_STATE_WAIT;\n",
                                          delay_stru_str.c_str());

            delay_to_global_lpm_idx[delay_lpm_num] = idx;
            delay_lpm_num++;

            pint_reset_sig_init_value(nex, BIT_X);
        }

        unsigned width = pint_find_signal(nex)->width;
        pint_local_lpm_defined.clear();
        lpm_tmp_idx = 0;
        pint_gen_lpm_force_cond(cur_lpm_buff, nex);
        if (width <= 4)
            cur_lpm_buff += "\tunsigned char pint_out;\n";
        else {
            unsigned int cnt = (width + 0x1f) >> 5;
            cur_lpm_buff += string_format("\tunsigned int  pint_out[%u];\n", cnt * 2);
        }

        string declare;
        string operation;
        pint_show_lpm(pint_global_lpm[i], operation, declare);
        cur_lpm_buff += declare + operation;
        pint_gen_lpm_force_assign(cur_lpm_buff, nex);

        bool lpm_part_pv_flag = pint_global_lpm[i] && (pint_global_lpm[i]->type == LPM) &&
                                (ivl_lpm_type(pint_global_lpm[i]->lpm) == IVL_LPM_PART_PV);
        if (!lpm_part_pv_flag) {
            pint_lpm_update_multi_drv_lock(cur_lpm_buff, nex);
        }
        pint_lpm_update_output(cur_lpm_buff, nex, pint_global_lpm[i]);
        pint_lpm_update_multi_drv_unlock(cur_lpm_buff, nex);
        cur_lpm_buff += "}\n";
        if (cur_lpm_info.is_cmn) {
            struct pint_cmn_lpm_info *cmn_info = pint_get_cmn_lpm_info(pint_global_lpm[i]);
            if (cur_lpm_info.need_gen_cmn) {
                cmn_lpm_num++;
                cur_lpm_buff.insert(insert_pos, cur_lpm_info.cmn_decl_str.c_str());
                if (strcmp(enval, enval_flag) != 0) {
                    cmn_dump_map[cmn_info->cmn_lpm_id] = cur_lpm_buff;
                } else {
                    lpm_buff += cur_lpm_buff;
                }
            }
            if (strcmp(enval, enval_flag) != 0) {
                map<int, string>::iterator it;
                it = cmn_dump_map.find(cmn_info->cmn_lpm_id);
                if (it != cmn_dump_map.end()) {
                    it->second += cur_lpm_info.cmn_call_str;
                }
            } else {
                lpm_buff += cur_lpm_info.cmn_call_str.c_str();
            }
        } else {
            lpm_buff += cur_lpm_buff;
        }
        if (strcmp(enval, enval_flag) != 0) {
            string lpm_file_name = string_format("%s/src_file/lpm_normal%d.c", dir_name.c_str(),
                                                 lpm_buff_file_id + cmn_lpm_num);
            int lpm_buff_fd = -1;
            if (lpm_buff.size() > 1024 * 1024) {
                if ((lpm_buff_fd = open(lpm_file_name.c_str(), O_RDWR | O_CREAT | O_APPEND | O_SYNC,
                                        00700)) < 0) {
                    printf("Open lpm.c error! pint_dump_lpm\n");
                    return;
                }
                lpm_buff_file_size += lpm_buff.size();
                write_bytes = write(lpm_buff_fd, lpm_buff.c_str(), lpm_buff.size());

                lpm_buff.clear();
                close(lpm_buff_fd);
                if (lpm_buff_file_size > 1024 * 1024 * 1) {
                    lpm_buff_file_id++;
                    lpm_buff_file_size = 0;
                    lpm_buff = inc_start_buff + lpm_buff;
                }
            }
        }
    }
    if (strcmp(enval, enval_flag) != 0) {
        lpm_buff_file_id = 0;
        string cmn_lpm_buff = inc_start_buff;
        int lpm_buff_fd = -1;
        static int cmn_lpm_buff_file_size = 0;
        map<int, string>::iterator it;
        for (it = cmn_dump_map.begin(); it != cmn_dump_map.end(); ++it) {
            cmn_lpm_buff += it->second;
            string lpm_file_name =
                string_format("%s/src_file/lpm_cmn%d.c", dir_name.c_str(), lpm_buff_file_id);
            if (cmn_lpm_buff.size() > 1024 * 1024) {
                if ((lpm_buff_fd = open(lpm_file_name.c_str(), O_RDWR | O_CREAT | O_APPEND | O_SYNC,
                                        00700)) < 0) {
                    printf("Open lpm.c error! pint_dump_lpm\n");
                    return;
                }
                cmn_lpm_buff_file_size += cmn_lpm_buff.size();
                write_bytes = write(lpm_buff_fd, cmn_lpm_buff.c_str(), cmn_lpm_buff.size());
                cmn_lpm_buff.clear();
                close(lpm_buff_fd);
                if (cmn_lpm_buff_file_size > 1024 * 1024 * 1) {
                    lpm_buff_file_id++;
                    cmn_lpm_buff_file_size = 0;
                    cmn_lpm_buff = inc_start_buff + cmn_lpm_buff;
                }
            }
        }
        lpm_buff_file_id++;
        string lpm_file_name =
            string_format("%s/src_file/lpm_cmn%d.c", dir_name.c_str(), lpm_buff_file_id);
        if ((lpm_buff_fd =
                 open(lpm_file_name.c_str(), O_RDWR | O_CREAT | O_APPEND | O_SYNC, 00700)) < 0) {
            printf("Open lpm.c error! pint_dump_lpm\n");
            return;
        }
        cmn_lpm_buff = inc_start_buff + cmn_lpm_buff;
        write_bytes = write(lpm_buff_fd, cmn_lpm_buff.c_str(), cmn_lpm_buff.size());
        close(lpm_buff_fd);
    }
    (void)write_bytes;
    PINT_LOG_HOST("lpm cmn: %u, lpm cmn call: %u\n", lpm_total_cmn_num, lpm_total_call_cmn_num);
}

void pint_dump_thread_lpm(void) {
    PINT_LOG_HOST("LPM: %u\n", global_lpm_num);
    PINT_LOG_HOST("monitor_lpm_num: %u\n", monitor_lpm_num);
    PINT_LOG_HOST("thread_lpm_num: %u\n", global_thread_lpm_num);
    PINT_LOG_HOST("THREAD: %u\n", pint_thread_num);
    PINT_LOG_HOST("pint_event_num: %u\n", pint_event_num);
    for (unsigned int i = 0; i < global_thread_lpm_num; i++) {
        lpm_tmp_char = 0;
        lpm_tmp_int_cnt = 0;
        unsigned idx = i + pint_event_num + global_lpm_num + monitor_lpm_num;
        lpm_declare_buff += string_format("void pint_lpm_%u(void);\n", idx);
    }
}

void pint_show_lpm_signature(string &buf, pint_lpm_log_info_t net_info) {
    string file_name;
    int backslash_index;
    const char *p_file_name;
    unsigned lineno;
    if (!net_info) {
        return;
    }
    switch (net_info->type) {
    case LOG: {
        ivl_net_logic_t net = net_info->log;
        p_file_name = ivl_logic_file(net);
        lineno = ivl_logic_lineno(net);
        break;
    }
    case SWITCH_: {
        ivl_switch_t net = net_info->switch_;
        p_file_name = ivl_switch_file(net);
        lineno = ivl_switch_lineno(net);
        break;
    }
    case LPM: {
        ivl_lpm_t net = net_info->lpm;
        p_file_name = ivl_lpm_file(net);
        lineno = ivl_lpm_lineno(net);
        break;
    }
    default:
        return;
    }
    file_name = p_file_name ? p_file_name : "";
    backslash_index = file_name.find_last_of('/');
    buf +=
        string_format("//def:%s:%d \n", file_name.substr(backslash_index + 1, -1).c_str(), lineno);
}

void pint_gen_lpm_force_cond(string &buff, ivl_nexus_t nex) {
    pint_sig_info_t sig_info = pint_find_signal(nex);
    unsigned width = sig_info->width;
    unsigned word = pint_get_arr_idx_from_nex(nex);
    pint_force_info_t force_info = pint_find_signal_force_info(sig_info, word);
    if (!force_info)
        return;
    string sig_name = pint_get_force_sig_name(sig_info, word);
    const char *sn = sig_name.c_str();
    if (force_info->part_force) {
        if (width <= 32) {
            buff += string_format("\tif(!umask_%s) return;\n", sn);
        } else {
            unsigned cnt = (width + 0x1f) >> 5;
            unsigned i;
            buff += "\tif(!(";
            for (i = 0; i < cnt; i++) {
                buff += string_format("umask_%s[%u] || ", sn, i);
            }
            buff.erase(buff.end() - 4, buff.end());
            buff += ")) return;\n";
        }
    } else {
        buff += string_format("\tif(force_en_%s) return;\n", sn);
    }
}

void pint_gen_lpm_force_assign(string &buff, ivl_nexus_t nex) {
    pint_sig_info_t sig_info = pint_find_signal(nex);
    unsigned width = sig_info->width;
    unsigned cnt = (width + 0x1f) >> 5;
    unsigned word = pint_get_arr_idx_from_nex(nex);
    pint_force_info_t force_info = pint_find_signal_force_info(sig_info, word);
    if (!force_info)
        return;
    if (!force_info->part_force)
        return; //  Only part_force needs part_update
    string sig_name = pint_get_force_sig_name(sig_info, word);
    string str_value = pint_get_sig_value_name(sig_info, word);
    const char *sn = sig_name.c_str();
    const char *sv = str_value.c_str();
    if (width <= 4) {
        buff +=
            string_format("\tpint_out = (pint_out & umask_%s) | (%s & (~umask_%s));\n", sn, sv, sn);
    } else if (width <= 32) {
        buff += string_format("\tpint_out[0] = (pint_out[0] & umask_%s) | (%s[0] & (~umask_%s));\n",
                              sn, sv, sn);
        buff += string_format("\tpint_out[1] = (pint_out[1] & umask_%s) | (%s[1] & (~umask_%s));\n",
                              sn, sv, sn);
    } else {
        buff += string_format(
            "\tfor(int i =0; i < %u; i++){\n"
            "\t\tpint_out[i] = (pint_out[i] & umask_%s[i]) | (%s[i] & (~umask_%s[i]));\n"
            "\t\tpint_out[i +%u] = (pint_out[i +%u] & umask_%s) | (%s[i +%u] & (~umask_%s[i "
            "+%u]));\n"
            "\t}\n",
            cnt, sn, sv, sn, cnt, cnt, sn, sv, cnt, sn, cnt);
    }
}

void pint_show_lpm(pint_lpm_log_info_t net_info, string &operation, string &declare) {
    if (!net_info)
        return;
    ivl_nexus_t lhs_nex = net_info->nex;
    pint_sig_info_t lhs_info = pint_find_signal(lhs_nex);
    ivl_nexus_t nex_ri;
    pint_sig_info_t sig_ri;
    pint_lpm_log_info_t lpm_ri;
    unsigned is_const;
    unsigned ri;
    //  Debug	printf("LHS:\t%s\t%s\n", lhs_info->sig_name.c_str(), net_info->type ==
    //  LOG?"LOG":"LPM");
    if (net_info->type == LOG) {
        ivl_net_logic_t net = net_info->log;
        unsigned npins = ivl_logic_pins(net);
        for (ri = 1; ri < npins; ri++) {
            nex_ri = ivl_logic_pin(net, ri);
            sig_ri = pint_find_signal(nex_ri);
            is_const = pint_signal_is_const(nex_ri);
            //  Debug	printf("ID:\t%d\tRi: %d\t%s\n", id_lpm, ri -1,
            //  sig_ri->is_local?"Local":"Global");
            if (sig_ri->is_local && !is_const) {
                lpm_ri = pint_find_local_lpm(nex_ri);
                if (lpm_ri) {
                    if ((ivl_logic_type(net_info->log) != IVL_LO_AND) &&
                        (ivl_logic_type(net_info->log) != IVL_LO_OR)) {
                        if (!pint_local_lpm_defined.count(nex_ri)) {
                            pint_local_lpm_defined.insert(nex_ri);
                            pint_show_lpm(lpm_ri, operation, declare);
                        }
                    }
                } else {
                    pint_lpm_undrived_sig(operation, sig_ri);
                }
            }
        }
        if (lhs_info->is_local)
            pint_show_sig_define(declare, lhs_info);
        pint_show_logic_type(operation, declare, net_info);
    } else if (net_info->type == SWITCH_) {
        ivl_switch_t net = net_info->switch_;
        nex_ri = ivl_switch_a(net);
        sig_ri = pint_find_signal(nex_ri);
        is_const = pint_signal_is_const(nex_ri);
        //  Debug	printf("ID:\t%d\tRi: %d\t%s\n", id_lpm, ri -1,
        //  sig_ri->is_local?"Local":"Global");
        if (sig_ri->is_local && !is_const) {
            lpm_ri = pint_find_local_lpm(nex_ri);
            if (lpm_ri) {
                if (!pint_local_lpm_defined.count(nex_ri)) {
                    pint_local_lpm_defined.insert(nex_ri);
                    pint_show_lpm(lpm_ri, operation, declare);
                }
            } else {
                pint_lpm_undrived_sig(operation, sig_ri);
            }
        }

        if (lhs_info->is_local)
            pint_show_sig_define(declare, lhs_info);
        pint_show_switch_type(operation, net_info);
    } else {
        ivl_lpm_t net = net_info->lpm;
        vector<ivl_nexus_t> rhs_nex;
        ivl_signal_t rhs_arr = NULL; //  Only used by IVL_LPM_ARRAY
        pint_lpm_sensitive_sig(net, rhs_nex, rhs_arr);
        unsigned nrhs = rhs_nex.size();

        for (ri = 0; ri < nrhs; ri++) {
            nex_ri = rhs_nex[ri];
            sig_ri = pint_find_signal(nex_ri);
            is_const = pint_signal_is_const(nex_ri);
            //  Debug	printf("ID:\t%d\tRi: %d\t%s\n", id_lpm, ri,
            //  sig_ri->is_local?"Local":"Global");
            if (sig_ri->is_local && !is_const) {
                lpm_ri = pint_find_local_lpm(nex_ri);
                if (lpm_ri) {
                    // do not generate IVL_LPM_MUX here to avoid execute unnecessary code
                    if ((ivl_lpm_type(net_info->lpm) != IVL_LPM_MUX) &&
                        (ivl_lpm_type(net_info->lpm) != IVL_LPM_MULT)) {
                        if (!pint_local_lpm_defined.count(nex_ri)) {
                            pint_local_lpm_defined.insert(nex_ri);
                        }
                        pint_show_lpm(lpm_ri, operation, declare);
                    }
                } else {
                    pint_lpm_undrived_sig(declare, sig_ri);
                }
            }
        }
        if (lhs_info->is_local) {
            pint_show_sig_define(declare, lhs_info);
        }
        pint_show_lpm_type(operation, declare, net_info);
    }
}

/*
 * All logic gates have inputs and outputs that match exactly in
 * width. For example, and AND gate with 4 bit inputs generates a 4
 * bit pint_out, and all the inputs are 4 bits.
 */
void pint_show_logic_type(string &operation, string &declare, pint_lpm_log_info_t net_info) {
    pint_sig_info_t sig_info = pint_find_signal(net_info->nex);
    int is_local = sig_info->is_local;
    ivl_net_logic_t net = net_info->log;
    ivl_logic_t type = ivl_logic_type(net);
    unsigned width = ivl_logic_width(net);
    unsigned npins = ivl_logic_pins(net);
    string pin_name[npins];
    const char *sn[npins]; // sig name
    if (is_local)
        pint_get_sig_name(pin_name[0], net_info->nex);
    else
        pin_name[0] = "pint_out";
    for (unsigned ri = 1; ri < npins; ri++) {
        pint_get_sig_name(pin_name[ri], ivl_logic_pin(net, ri));
    }
    for (unsigned ri = 0; ri < npins; ri++) {
        sn[ri] = pin_name[ri].c_str();
    }

    switch (type) {
    case IVL_LO_NONE: //  0
        break;
    case IVL_LO_AND: { //  1   a & b
        ivl_nexus_t nex_ri1 = ivl_logic_pin(net, 1);
        pint_lpm_log_info_t ander1_info = pint_find_local_lpm(nex_ri1);
        ivl_nexus_t nex_ri2 = ivl_logic_pin(net, 2);
        pint_lpm_log_info_t ander2_info = pint_find_local_lpm(nex_ri2);

        string operation1, operation2, declare1, declare2;
        pint_show_lpm(ander1_info, operation1, declare1);
        pint_show_lpm(ander2_info, operation2, declare2);

        if (operation1.length() < operation2.length()) {
            pint_get_sig_name(pin_name[1], nex_ri1);
            pint_get_sig_name(pin_name[2], nex_ri2);
            operation += operation1;
            declare += declare1;
            sn[1] = pin_name[1].c_str();
            sn[2] = pin_name[2].c_str();
            operation +=
                string_format("if (%s != 0){\n", code_of_sig_value(sn[1], width, 0, false).c_str());
            operation += operation2;
            declare += declare2;
        } else {
            pint_get_sig_name(pin_name[1], nex_ri2);
            pint_get_sig_name(pin_name[2], nex_ri1);
            operation += operation2;
            declare += declare2;
            sn[1] = pin_name[1].c_str();
            sn[2] = pin_name[2].c_str();
            operation +=
                string_format("if (%s != 0){\n", code_of_sig_value(sn[1], width, 0, false).c_str());
            operation += operation1;
            declare += declare1;
        }

        if (width <= 4) {
            operation += string_format("\tpint_bitw_and_char(&%s, &%s, &%s, %u);\n", sn[0], sn[1],
                                       sn[2], width);
            operation += string_format("}else{pint_set_sig_zero(&%s, %d);}", sn[0], width);
        } else {
            operation +=
                string_format("\tpint_bitw_and_int(%s, %s, %s, %u);\n", sn[0], sn[1], sn[2], width);
            operation += string_format("}else{pint_set_sig_zero(%s, %d);}", sn[0], width);
        }
    } break;
    case IVL_LO_OR: { // 12   a|b
        ivl_nexus_t nex_ri1 = ivl_logic_pin(net, 1);
        pint_lpm_log_info_t ander1_info = pint_find_local_lpm(nex_ri1);
        ivl_nexus_t nex_ri2 = ivl_logic_pin(net, 2);
        pint_lpm_log_info_t ander2_info = pint_find_local_lpm(nex_ri2);

        pint_show_lpm(ander2_info, operation, declare);
        pint_get_sig_name(pin_name[2], nex_ri2);
        sn[2] = pin_name[2].c_str();

        if (width == 1) {
            operation += string_format("if (%s == 1){ %s = 1; } else {\n", sn[2], sn[0]);
        }

        pint_show_lpm(ander1_info, operation, declare);
        pint_get_sig_name(pin_name[1], nex_ri1);
        sn[1] = pin_name[1].c_str();
        if (width <= 4)
            operation += string_format("\tpint_bitw_or_char(&%s, &%s, &%s, %u);\n", sn[0], sn[1],
                                       sn[2], width);
        else
            operation +=
                string_format("\tpint_bitw_or_int(%s, %s, %s, %u);\n", sn[0], sn[1], sn[2], width);
        if (width == 1) {
            operation += "}";
        }
    } break;
    case IVL_LO_BUF: //  2
        break;
    case IVL_LO_BUFIF0: //  3
        break;
    case IVL_LO_BUFIF1: //  4
        break;
    case IVL_LO_BUFT: // 24
    case IVL_LO_BUFZ: //  5   a = b
        if (width <= 4)
            operation += string_format("\t%s = %s;\n", sn[0], sn[1]);
        else
            operation += string_format("\tpint_copy_int(%s, %s, %u);\n", sn[0], sn[1], width);
        break;
    case IVL_LO_CMOS: // 22
        break;
    case IVL_LO_EQUIV: // 25
        break;
    case IVL_LO_IMPL: // 26
        break;
    case IVL_LO_NAND: //  6   a ~& b
        if (width <= 4)
            operation += string_format("\tpint_bitw_nand_char(&%s, &%s, &%s, %u);\n", sn[0], sn[1],
                                       sn[2], width);
        else
            operation += string_format("\tpint_bitw_nand_int(%s, %s, %s, %u);\n", sn[0], sn[1],
                                       sn[2], width);
        break;
    case IVL_LO_NMOS: //  7
        break;
    case IVL_LO_NOR: //  8   a ~| b
        if (width <= 4)
            operation += string_format("\tpint_bitw_nor_char(&%s, &%s, &%s, %u);\n", sn[0], sn[1],
                                       sn[2], width);
        else
            operation +=
                string_format("\tpint_bitw_nor_int(%s, %s, %s, %u);\n", sn[0], sn[1], sn[2], width);
        break;
    case IVL_LO_NOT: //  9   ~a
        if (width <= 4)
            operation +=
                string_format("\tpint_bitw_invert_char(&%s, &%s, %u);\n", sn[0], sn[1], width);
        else
            operation +=
                string_format("\tpint_bitw_invert_int(%s, %s, %u);\n", sn[0], sn[1], width);
        break;
    case IVL_LO_NOTIF0: // 10
        break;
    case IVL_LO_NOTIF1: // 11
        break;
    case IVL_LO_PMOS: // 17
        break;
    case IVL_LO_PULLDOWN: // 13
        break;
    case IVL_LO_PULLUP: // 14
        break;
    case IVL_LO_RCMOS: // 23
        break;
    case IVL_LO_RNMOS: // 15
        break;
    case IVL_LO_RPMOS: // 16
        break;
    case IVL_LO_XNOR: // 18   a ~^ b
        if (width <= 4)
            operation += string_format("\tpint_bitw_equivalence_char(&%s, &%s, &%s, %u);\n", sn[0],
                                       sn[1], sn[2], width);
        else
            operation += string_format("\tpint_bitw_equivalence_int(%s, %s, %s, %u);\n", sn[0],
                                       sn[1], sn[2], width);
        break;
    case IVL_LO_XOR: // 19   a ^ b
        if (width <= 4)
            operation += string_format("\tpint_bitw_exclusive_or_char(&%s, &%s, &%s, %u);\n", sn[0],
                                       sn[1], sn[2], width);
        else
            operation += string_format("\tpint_bitw_exclusive_or_int(%s, %s, %s, %u);\n", sn[0],
                                       sn[1], sn[2], width);
        break;
    case IVL_LO_UDP: // 21
        break;
    }
}

void pint_show_switch_type(string &ret, pint_lpm_log_info_t net_info) {
    pint_sig_info_t sig_info = pint_find_signal(net_info->nex);
    int is_local = sig_info->is_local;
    ivl_switch_t net = net_info->switch_;
    ivl_switch_type_t type = ivl_switch_type(net);
    unsigned part = ivl_switch_part(net);
    unsigned offset = ivl_switch_offset(net);
    ivl_nexus_t rhi = ivl_switch_a(net);
    pint_sig_info_t rhi_sig_info = pint_find_signal(rhi);
    string base_str = string_format("%d", offset);

    string pin_name[2];
    const char *sn[2]; // sig name
    if (is_local) {
        pint_get_sig_name(pin_name[0], net_info->nex);
    } else {
        pin_name[0] = "pint_out";
    }

    pint_get_sig_name(pin_name[1], rhi);

    for (unsigned ri = 0; ri < 2; ri++) {
        sn[ri] = pin_name[ri].c_str();
    }

    switch (type) {
    case IVL_SW_TRAN_VP:
        ret +=
            code_of_bit_select(sn[1], rhi_sig_info->width, sn[0], part, base_str.c_str(), "false");
        break;
    case IVL_SW_TRAN:
    case IVL_SW_TRANIF0:
    case IVL_SW_TRANIF1:
    case IVL_SW_RTRAN:
    case IVL_SW_RTRANIF0:
    case IVL_SW_RTRANIF1:
    default:
        PINT_UNSUPPORTED_ERROR(ivl_switch_file(net), ivl_switch_lineno(net), "SWITCH type: %u.",
                               type);
        break;
    }
}

/*
    width:  The width of output
    pint_lpm_size:  get the num of RHS, but not support every type
*/
void pint_show_lpm_type(string &operation, string &declare, pint_lpm_log_info_t net_info) {
    pint_sig_info_t sig_info = pint_find_signal(net_info->nex);
    int is_local = sig_info->is_local;
    ivl_lpm_t net = net_info->lpm;
    ivl_lpm_type_t type = ivl_lpm_type(net);
    unsigned width = ivl_lpm_width(net); // Note: here the width of lpm, not the width of output
    int is_signed = ivl_lpm_signed(net);
    unsigned npins = pint_lpm_size(net) + 1; // For special type: npins =1, only own LHS
    ivl_nexus_t nex[npins];
    pint_sig_info_t pin[npins]; // sig info
    string pin_name[npins];
    const char *sn[npins]; // sig name
    nex[0] = net_info->nex;
    pin[0] = sig_info; // LHS
    if (is_local)
        pint_get_sig_name(pin_name[0], net_info->nex);
    else
        pin_name[0] = "pint_out";

    for (unsigned ri = 1; ri < npins; ri++) {
        nex[ri] = ivl_lpm_data(net, ri - 1);
        pin[ri] = pint_find_signal(nex[ri]);
        pint_get_sig_name(pin_name[ri], nex[ri]);
    }
    for (unsigned ri = 0; ri < npins; ri++) {
        sn[ri] = pin_name[ri].c_str();
    }

    switch (type) {
    //	Normal:	    support ivl_lpm_size()
    case IVL_LPM_ABS: // 32
        break;
    case IVL_LPM_ADD: //  0   "a + b"
    {
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_ADD\n");
        const char *sign = is_signed ? "_s" : "_u";
        if (width <= 4)
            operation += string_format("\tpint_add_char%s(&%s, &%s, &%s, %u);\n", sign, sn[0],
                                       sn[1], sn[2], width);
        else
            operation += string_format("\tpint_add_int%s(%s, %s, %s, %u);\n", sign, sn[0], sn[1],
                                       sn[2], width);
        break;
    }
    case IVL_LPM_CAST_INT: // 34
        break;
    case IVL_LPM_CAST_INT2: // 35
        break;
    case IVL_LPM_CAST_REAL: // 33
        break;
    case IVL_LPM_CONCAT:  // 16   "out = {d1, d0}"
    case IVL_LPM_CONCATZ: // 36
                          //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_CONCAT\n");
        if (width <= 4) {
            unsigned off = 0;
            for (unsigned i = 1; i < npins; i++) {
                operation +=
                    string_format("\t%s %s= %s << %u;\n", sn[0], i == 1 ? " " : "|", sn[i], off);
                off += pin[i]->width;
            }
        } else {
            unsigned cnt = (width + 0x1f) >> 5;
            unsigned base = 0;
            unsigned cnt_b, off, width_i, i;
            unsigned same_input = 1;
            unsigned skip_init = 0;
            for (unsigned ri = 2; ri < npins; ri++) {
                if (ivl_lpm_data(net, ri - 1) != ivl_lpm_data(net, ri - 2)) {
                    same_input = 0;
                    break;
                }
            }
            if (same_input) {
                width_i = pin[1]->width;
                skip_init = width_i % 32 == 0 || width_i == 1 || width_i == 2 || width_i == 4;
            }
            if (skip_init) { // special case optimization
                unsigned width_left = width;
                unsigned j = 0;
                switch (width_i) {
                case 1:
                    operation += string_format("\tif(%s & 0x1){\n", sn[1]);
                    while (width_left >= 32) {
                        operation += string_format("\t\t%s[%u] = 0xffffffff;\n", sn[0], j++);
                        width_left -= 32;
                    }
                    if (width_left)
                        operation += string_format("\t\t%s[%u] = 0x%x;\n", sn[0], j++,
                                                   (1 << width_left) - 1);
                    operation += "\t}else{\n";
                    for (j = 0; j < cnt; j++) {
                        operation += string_format("\t\t%s[%u] = 0;\n", sn[0], j);
                    }
                    operation += "\t}\n";
                    width_left = width;
                    j = cnt;
                    operation += string_format("\tif(%s & 0x10){\n", sn[1]);
                    while (width_left >= 32) {
                        operation += string_format("\t\t%s[%u] = 0xffffffff;\n", sn[0], j++);
                        width_left -= 32;
                    }
                    if (width_left)
                        operation += string_format("\t\t%s[%u] = 0x%x;\n", sn[0], j++,
                                                   (1 << width_left) - 1);
                    operation += "\t}else{\n";
                    for (j = cnt; j < cnt * 2; j++) {
                        operation += string_format("\t\t%s[%u] = 0;\n", sn[0], j);
                    }
                    operation += "\t}\n";
                    break;
                case 2:
                case 4:
                    unsigned int rep_const[2];
                    rep_const[1] = width_i == 2 ? 0x55555555 : 0x11111111;
                    rep_const[0] = ((1 << (width % 32)) - 1) & rep_const[1];
                    operation += string_format("\tif(%s & 0xf){\n", sn[1]);
                    width_left = width;
                    while (width_left >= 32) {
                        operation +=
                            string_format("\t\t%s[%u] = 0x%xu * ((unsigned int)%s & 0xf);\n", sn[0],
                                          j++, rep_const[1], sn[1]);
                        width_left -= 32;
                    }
                    if (width_left)
                        operation +=
                            string_format("\t\t%s[%u] = 0x%xu * ((unsigned int)%s & 0xf);\n", sn[0],
                                          j++, rep_const[0], sn[1]);
                    operation += "\t}else{\n";
                    for (j = 0; j < cnt; j++) {
                        operation += string_format("\t\t%s[%u] = 0;\n", sn[0], j);
                    }
                    operation += "\t}\n";
                    width_left = width;
                    j = cnt;
                    operation += string_format("\tif(%s & 0xf0){\n", sn[1]);
                    width_left = width;
                    while (width_left >= 32) {
                        operation += string_format("\t\t%s[%u] = 0x%xu * ((unsigned int)%s>>4);\n",
                                                   sn[0], j++, rep_const[1], sn[1]);
                        width_left -= 32;
                    }
                    if (width_left)
                        operation += string_format("\t\t%s[%u] = 0x%xu * ((unsigned int)%s>>4);\n",
                                                   sn[0], j++, rep_const[0], sn[1]);
                    operation += "\t}else{\n";
                    for (j = cnt; j < cnt * 2; j++) {
                        operation += string_format("\t\t%s[%u] = 0;\n", sn[0], j);
                    }
                    operation += "\t}\n";
                    break;
                default: {
                    int cnt_i = width_i >> 5;
                    for (unsigned i0 = 0; i0 < npins - 1; i0++) {
                        for (int i1 = 0; i1 < cnt_i; i1++) {
                            operation += string_format("\t%s[%u]  = %s[%u];\n", sn[0],
                                                       i0 * cnt_i + i1, sn[1], i1);
                            operation += string_format("\t%s[%u]  = %s[%u];\n", sn[0],
                                                       (npins - 1) * cnt_i + i0 * cnt_i + i1, sn[1],
                                                       cnt_i + i1);
                        }
                    }
                    break;
                }
                }
                break;
            }
            for (i = 0; i < 2 * cnt; i++) {
                operation += string_format("\t%s[%u]  = 0;\n", sn[0], i);
            }
            for (i = 1; i < npins; i++) {
                width_i = pin[i]->width;
                if (pint_signal_is_const(nex[i])) {
                    const char *bits = pint_find_bits_from_nexus(nex[i]);
                    if (pint_bits_is_zero(bits)) {
                        base += width_i;
                        continue;
                    }
                }
                if (width_i <= 4) {
                    cnt_b = base >> 5;
                    off = base & 0x1f;
                    operation += string_format("\t%s[%u] |= ((unsigned int)%s & 0xf) << %u;\n",
                                               sn[0], cnt_b, sn[i], off);
                    operation += string_format("\t%s[%u] |= ((unsigned int)%s >>  4) << %u;\n",
                                               sn[0], cnt_b + cnt, sn[i], off);
                    base += width_i;
                    if (off + width_i > 32) {
                        cnt_b = base >> 5;
                        off = 32 - off;
                        operation += string_format("\t%s[%u] |= ((unsigned int)%s & 0xf) >> %u;\n",
                                                   sn[0], cnt_b, sn[i], off);
                        operation += string_format("\t%s[%u] |= ((unsigned int)%s >>  4) >> %u;\n",
                                                   sn[0], cnt_b + cnt, sn[i], off);
                    }
                } else {
                    unsigned int cnt_i = (width_i + 0x1f) >> 5;
                    unsigned int len_i = 32;
                    unsigned int j;
                    for (j = 0; j < cnt_i; j++) {
                        if ((j == cnt_i - 1) && (width_i & 0x1f))
                            len_i = width_i & 0x1f;
                        cnt_b = base >> 5;
                        off = base & 0x1f;
                        operation += string_format("\t%s[%u] |= %s[%d] << %u;\n", sn[0], cnt_b,
                                                   sn[i], j, off);
                        operation += string_format("\t%s[%u] |= %s[%d] << %u;\n", sn[0],
                                                   cnt_b + cnt, sn[i], j + cnt_i, off);
                        base += len_i;
                        if (off + len_i > 32) {
                            cnt_b = base >> 5;
                            off = 32 - off;
                            operation += string_format("\t%s[%u] |= (unsigned int)%s[%d] >> %u;\n",
                                                       sn[0], cnt_b, sn[i], j, off);
                            operation += string_format("\t%s[%u] |= (unsigned int)%s[%d] >> %u;\n",
                                                       sn[0], cnt_b + cnt, sn[i], j + cnt_i, off);
                        }
                    }
                }
            }
        }
        break;
    case IVL_LPM_CMP_EEQ: // 18   "a === b"; width_in = width; width_out = 1
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_CMP_EEQ\n");
        if (width <= 4)
            operation += string_format("\tpint_case_equality_char(&%s, &%s, &%s, %u);\n", sn[0],
                                       sn[1], sn[2], width);
        else
            operation += string_format("\tpint_case_equality_int(&%s, %s, %s, %u);\n", sn[0], sn[1],
                                       sn[2], width);
        break;
    case IVL_LPM_CMP_EQX: // 37   "a == b" in casex
        break;
    case IVL_LPM_CMP_EQZ: // 38   "a == b" in casez
        break;
    case IVL_LPM_CMP_EQ: // 10   "a == b"; width_in = width; width_out = 1
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_CMP_EQ\n");
        if (width <= 4)
            operation += string_format("\tpint_logical_equality_char(&%s, &%s, &%s, %u);\n", sn[0],
                                       sn[1], sn[2], width);
        else
            operation += string_format("\tpint_logical_equality_int(&%s, %s, %s, %u);\n", sn[0],
                                       sn[1], sn[2], width);
        break;
    case IVL_LPM_CMP_GE: //  1   "a >= b"; width_in = width; width_out = 1
    {
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_CMP_GE\n");
        const char *sign = is_signed ? "_s" : "_u";
        if (width <= 4)
            operation += string_format("\tpint_greater_than_oreq_char%s(&%s, &%s, &%s, %u);\n",
                                       sign, sn[0], sn[1], sn[2], width);
        else
            operation += string_format("\tpint_greater_than_oreq_int%s(&%s, %s, %s, %u);\n", sign,
                                       sn[0], sn[1], sn[2], width);
        break;
    }
    case IVL_LPM_CMP_GT: //  2   "a > b"; width_in = width; width_out = 1
    {
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_CMP_GT\n");
        const char *sign = is_signed ? "_s" : "_u";
        if (width <= 4)
            operation += string_format("\tpint_greater_than_char%s(&%s, &%s, &%s, %u);\n", sign,
                                       sn[0], sn[1], sn[2], width);
        else
            operation += string_format("\tpint_greater_than_int%s(&%s, %s, %s, %u);\n", sign, sn[0],
                                       sn[1], sn[2], width);
        break;
    }
    case IVL_LPM_CMP_NE: // 11   "a != b"; width_in = width; width_out = 1
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_CMP_NE\n");
        if (width <= 4)
            operation += string_format("\tpint_logical_inquality_char(&%s, &%s, &%s, %u);\n", sn[0],
                                       sn[1], sn[2], width);
        else
            operation += string_format("\tpint_logical_inquality_int(&%s, %s, %s, %u);\n", sn[0],
                                       sn[1], sn[2], width);
        break;
    case IVL_LPM_CMP_NEE: // 19   "a !== b"; width_in = width; width_out = 1
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_CMP_NEE\n");
        if (width <= 4)
            operation += string_format("\tpint_case_inquality_char(&%s, &%s, &%s, %u);\n", sn[0],
                                       sn[1], sn[2], width);
        else
            operation += string_format("\tpint_case_inquality_int(&%s, %s, %s, %u);\n", sn[0],
                                       sn[1], sn[2], width);
        break;
    case IVL_LPM_DIVIDE: // 12   "a / b", Feature: width = width_out = width_a = width_b
    {
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_DIVIDE\n");
        const char *sign = is_signed ? "_s" : "_u";
        if (width <= 4)
            operation += string_format("\tpint_divided_char%s(&%s, &%s, &%s, %u);\n", sign, sn[0],
                                       sn[1], sn[2], width);
        else
            operation += string_format("\tpint_divided_int%s(%s, %s, %s, %u);\n", sign, sn[0],
                                       sn[1], sn[2], width);
        break;
    }
    case IVL_LPM_FF: //  3
        break;
    case IVL_LPM_MOD: // 13   "a % b", Feature: width = width_out = width_a = width_b
    {
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_MOD\n");
        const char *sign = is_signed ? "_s" : "_u";
        if (width <= 4)
            operation += string_format("\tpint_modulob_char%s(&%s, &%s, &%s, %u);\n", sign, sn[0],
                                       sn[1], sn[2], width);
        else
            operation += string_format("\tpint_modulob_int%s(%s, %s, %s, %u);\n", sign, sn[0],
                                       sn[1], sn[2], width);
        break;
    }
    case IVL_LPM_MULT: //  4   "a * b", Feature: width = width_out = width_a = width_b
    {
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_MULT\n");
        const char *sign = is_signed ? "_s" : "_u";
        ivl_nexus_t nex_ri1 = ivl_lpm_data(net, 0);
        pint_lpm_log_info_t ander1_info = pint_find_local_lpm(nex_ri1);
        ivl_nexus_t nex_ri2 = ivl_lpm_data(net, 1);
        pint_lpm_log_info_t ander2_info = pint_find_local_lpm(nex_ri2);

        string operation1, operation2, declare1, declare2;
        pint_show_lpm(ander1_info, operation1, declare1);
        pint_show_lpm(ander2_info, operation2, declare2);

        if (operation1.length() < operation2.length()) {
            pint_get_sig_name(pin_name[1], nex_ri1);
            pint_get_sig_name(pin_name[2], nex_ri2);
            operation += operation1;
            declare += declare1;
            sn[1] = pin_name[1].c_str();
            sn[2] = pin_name[2].c_str();
            operation +=
                string_format("if (%s != 0){\n", code_of_sig_value(sn[1], width, 0, false).c_str());
            operation += operation2;
            declare += declare2;
        } else {
            pint_get_sig_name(pin_name[1], nex_ri2);
            pint_get_sig_name(pin_name[2], nex_ri1);
            operation += operation2;
            declare += declare2;
            sn[1] = pin_name[1].c_str();
            sn[2] = pin_name[2].c_str();
            operation +=
                string_format("if (%s != 0){\n", code_of_sig_value(sn[1], width, 0, false).c_str());
            operation += operation1;
            declare += declare1;
        }

        if (width <= 4) {
            operation += string_format("\tpint_mul_char%s(&%s, &%s, &%s, %u);\n", sign, sn[0],
                                       sn[1], sn[2], width);
            operation += string_format("}else{pint_set_sig_zero(&%s, %d);}", sn[0], width);
        } else {
            operation += string_format("\tpint_mul_int%s(%s, %s, %s, %u);\n", sign, sn[0], sn[1],
                                       sn[2], width);
            operation += string_format("}else{pint_set_sig_zero(%s, %d);}", sn[0], width);
        }
        break;
    }
    case IVL_LPM_MUX: //  5   "out = sel ? d1 : d0"
    {
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_MUX\n");
        ivl_nexus_t sel_nex = ivl_lpm_select(net);
        pint_sig_info_t sel_info = pint_find_signal(sel_nex);
        unsigned int sel_width = sel_info->width;
        unsigned sel_signed = ivl_signal_signed(sel_info->sig);

        // generate sel code
        if (sel_info->is_local && !pint_signal_is_const(sel_nex)) {
            pint_lpm_log_info_t lpm_sel = pint_find_local_lpm(sel_nex);
            if (lpm_sel) {
                pint_show_lpm(lpm_sel, operation, declare);
            }
        }
        string sel_name;
        string sel_value;
        pint_get_sig_name(sel_name, sel_nex);
        const char *sel = sel_name.c_str();
        const char *sv = NULL;
        if (sel_width > 4) {
            sel_value = string_format("value_tmp%d", lpm_tmp_value_idx++);
            sv = sel_value.c_str();
            operation += string_format(
                "int %s = %s;\n", sv, code_of_sig_value(sel, sel_width, sel_signed, false).c_str());
        }

        if (pin[2]->is_local && !pint_signal_is_const(nex[2])) {
            pint_lpm_log_info_t lpm_true = pint_find_local_lpm(nex[2]);
            if (lpm_true) {
                if (sel_width <= 4) {
                    operation += string_format("if(%s){\n", sel);
                } else {
                    operation += string_format("if (%s){\n", sv);
                }
                pint_show_lpm(lpm_true, operation, declare);
                operation += "}\n";
            }
        }
        pint_get_sig_name(pin_name[2], nex[2]);
        sn[2] = pin_name[2].c_str();

        if (pin[1]->is_local && !pint_signal_is_const(nex[1])) {
            pint_lpm_log_info_t lpm_false = pint_find_local_lpm(nex[1]);
            if (lpm_false) {
                if (sel_width <= 4) {
                    operation += string_format("if(%s & 0xf0 || !%s){\n", sel, sel);
                } else {
                    operation += string_format("if ((%s == VALUE_XZ) || (%s == 0)){\n", sv, sv);
                }
                pint_show_lpm(lpm_false, operation, declare);
                operation += "}\n";
            }
        }
        pint_get_sig_name(pin_name[1], nex[1]);
        sn[1] = pin_name[1].c_str();

        if (sel_width <= 4) {
            operation += string_format("\tif(%s & 0xf0) ", sel);
            if (width <= 4) {
                operation += string_format("pint_mux_char_char(&%s, &%s, &%s, &%s, %u, %u);\n",
                                           sn[0], sn[1], sn[2], sel, width, sel_width);
            } else {
                operation += string_format("pint_mux_int_char(%s, %s, %s, &%s, %u, %u);\n", sn[0],
                                           sn[1], sn[2], sel, width, sel_width);
            }
            operation += string_format("\telse if(%s) ", sel);
        } else {
            operation += string_format("\tif(%s == VALUE_XZ)", sv);
            if (width <= 4) {
                operation += string_format("pint_mux_char_int(&%s, &%s, &%s, %s, %u, %u);\n", sn[0],
                                           sn[1], sn[2], sel, width, sel_width);
            } else {
                operation += string_format("pint_mux_int_int(%s, %s, %s, %s, %u, %u);\n", sn[0],
                                           sn[1], sn[2], sel, width, sel_width);
            }
            operation += string_format("\telse if(%s) ", sv);
        }
        if (width <= 4) {
            operation += string_format("%s = %s;\n", sn[0], sn[2]);
            operation += "\telse ";
            operation += string_format("%s = %s;\n", sn[0], sn[1]);
        } else {
            operation += string_format("pint_copy_int(%s, %s, %u);\n", sn[0], sn[2], width);
            operation += "\telse ";
            operation += string_format("pint_copy_int(%s, %s, %u);\n", sn[0], sn[1], width);
        }
        break;
    }
    case IVL_LPM_PART_VP: // 15   "out = in[base+size-1 : base] or out = in[sel +: size]"; pin[1],
                          // data; pin[2], sel
        {
            //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_PART_VP\n");
            if (npins == 2) {
                unsigned width_d = pin[1]->width;
                unsigned base = ivl_lpm_base(net);
                string base_str = string_format("%u", base);
                operation +=
                    code_of_bit_select(sn[1], width_d, sn[0], width, base_str.c_str(), "false");
            } else {
                unsigned width_d = pin[1]->width;
                unsigned sel_width = pin[2]->width;
                int sel_sign = ivl_signal_signed(pin[2]->sig);
                string base_value = string_format("value_tmp%d", lpm_tmp_value_idx++);

                operation +=
                    string_format("int %s = %s;\n", base_value.c_str(),
                                  code_of_sig_value(sn[2], sel_width, sel_sign, false).c_str());
                operation += string_format("if(%s != VALUE_XZ){\n", base_value.c_str());
                operation +=
                    code_of_bit_select(sn[1], width_d, sn[0], width, base_value.c_str(), "false");
                operation += "}else{";
                operation += string_format("pint_set_sig_x(%s%s, %d);\n", (width <= 4) ? "&" : "",
                                           sn[0], width);
                operation += "}";
            }
            break;
        }
    case IVL_LPM_PART_PV: // 17   "out[base+size-1 : base] = in"; width = width_in
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_PART_PV\n");
        if (npins == 2) { // Don't have sig 'sel'
            unsigned base = ivl_lpm_base(net);
            unsigned width_out = pin[0]->width;
            bool mul_drv = (Multi_drive_record.count(nex[0]) != 0);
            string out_info;
            if (!is_local) {
                pint_get_sig_name(out_info, net_info->nex);
                if (width_out <= 4)
                    operation += string_format("\tpint_out = %s;\n", out_info.c_str());
                else
                    operation += string_format("\tpint_copy_int(pint_out, %s, %u);\n",
                                               out_info.c_str(), width_out);

                // set bits directly on signal using set_sub_array to void conflicting
                sn[0] = out_info.c_str();
            }

            string mul_drv_tmp_name;
            if (mul_drv) {
                string tbl_name = string_format("%s_%d_mdrv_tbl", sig_info->sig_name.c_str(),
                                                pint_get_arr_idx_from_nex(nex[0]));

                mul_drv_tmp_name = string_format("tmp_%d", lpm_tmp_value_idx++);
                if (width <= 4) {
                    operation +=
                        string_format("unsigned char %s = %s;\n", mul_drv_tmp_name.c_str(), sn[1]);
                } else {
                    int word_num = (width + 31) / 32 * 2;
                    operation +=
                        string_format("unsigned %s[%d];\n", mul_drv_tmp_name.c_str(), word_num);
                    operation += string_format("\tpint_copy_int(%s, %s, %u);\n",
                                               mul_drv_tmp_name.c_str(), sn[1], width);
                }

                sn[1] = mul_drv_tmp_name.c_str();
                operation += string_format("pint_merge_multi_drive(%s%s, %s, %d, %d, %d, %d);\n",
                                           (width <= 4) ? "&" : "", sn[1], tbl_name.c_str(),
                                           Multi_drive_record_count[nex[0]]++, base, width,
                                           ivl_signal_type(sig_info->sig));
            }

            if (width_out <= 4) {
                operation += string_format("\tpint_set_subarray_char(&%s, &%s, %u, %u, %u);\n",
                                           sn[0], sn[1], base, width, width_out);
            } else if (width <= 4) {
                operation += string_format("\tpint_set_subarray_int_char(%s, &%s, %u, %u, %u);\n",
                                           sn[0], sn[1], base, width, width_out);
            } else {
                operation += string_format("\tpint_set_subarray_int(%s, %s, %u, %u, %u);\n", sn[0],
                                           sn[1], base, width, width_out);
            }
        }
        break;
    case IVL_LPM_POW: // 31   "a ** b"
    {
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_POW\n");
        {
            unsigned oper1_width = pin[1]->width;
            unsigned oper2_width = pin[2]->width;
            string out_name = string_format("%s%s", (width <= 4) ? "&" : "", sn[0]);
            string in1_name = string_format("%s%s", (oper1_width <= 4) ? "&" : "", sn[1]);
            string in2_name = string_format("%s%s", (oper2_width <= 4) ? "&" : "", sn[2]);
            operation +=
                string_format("\tpint_power_u_common(%s, %u, %s, %u, %s, %u);\n", out_name.c_str(),
                              width, in1_name.c_str(), oper1_width, in2_name.c_str(), oper2_width);
        }

        break;
    }
    case IVL_LPM_RE_AND: // 20   "&a"; width_in = width; width_out = 1
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_RE_AND\n");
        if (width <= 4)
            operation +=
                string_format("\tpint_reduction_and_char(&%s, &%s, %u);\n", sn[0], sn[1], width);
        else
            operation +=
                string_format("\tpint_reduction_and_int(&%s, %s, %u);\n", sn[0], sn[1], width);
        break;
    case IVL_LPM_RE_NAND: // 21   "~&a"; width_in = width; width_out = 1
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_RE_NAND\n");
        if (width <= 4)
            operation +=
                string_format("\tpint_reduction_nand_char(&%s, &%s, %u);\n", sn[0], sn[1], width);
        else
            operation +=
                string_format("\tpint_reduction_nand_int(&%s, %s, %u);\n", sn[0], sn[1], width);
        break;
    case IVL_LPM_RE_NOR: // 22   "~|a"; width_in = width; width_out = 1
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_RE_NOR\n");
        if (width <= 4)
            operation +=
                string_format("\tpint_reduction_nor_char(&%s, &%s, %u);\n", sn[0], sn[1], width);
        else
            operation +=
                string_format("\tpint_reduction_nor_int(&%s, %s, %u);\n", sn[0], sn[1], width);
        break;
    case IVL_LPM_RE_OR: // 23   "|a"; width_in = width; width_out = 1
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_RE_OR\n");
        if (width <= 4)
            operation +=
                string_format("\tpint_reduction_or_char(&%s, &%s, %u);\n", sn[0], sn[1], width);
        else
            operation +=
                string_format("\tpint_reduction_or_int(&%s, %s, %u);\n", sn[0], sn[1], width);
        break;
    case IVL_LPM_RE_XNOR: // 24   "~^a"; width_in = width; width_out = 1
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_RE_XNOR\n");
        if (width <= 4)
            operation +=
                string_format("\tpint_reduction_xnor_char(&%s, &%s, %u);\n", sn[0], sn[1], width);
        else
            operation +=
                string_format("\tpint_reduction_xnor_int(&%s, %s, %u);\n", sn[0], sn[1], width);
        break;
    case IVL_LPM_RE_XOR: // 25   "^a"; width_in = width; width_out = 1
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_RE_XOR\n");
        if (width <= 4)
            operation +=
                string_format("\tpint_reduction_xor_char(&%s, &%s, %u);\n", sn[0], sn[1], width);
        else
            operation +=
                string_format("\tpint_reduction_xor_int(&%s, %s, %u);\n", sn[0], sn[1], width);
        break;
    case IVL_LPM_REPEAT: // 26
        break;
    case IVL_LPM_SFUNC: // 29
        break;
    case IVL_LPM_SHIFTL: //  6   "a << b"(unsigned) or "a <<< b"(signed)
    {
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_SHIFTL\n");
        int sign_shitf = ivl_signal_signed(pin[2]->sig);
        unsigned width_shift = pin[2]->width;
        if (width <= 4) {
            operation += string_format(
                "\tpint_lshift_char(&%s, &%s, %s, %u);\n", sn[0], sn[1],
                code_of_sig_value(sn[2], width_shift, sign_shitf, false).c_str(), width);
        } else {
            operation += string_format(
                "\tpint_lshift_int(%s, %s, %s, %u);\n", sn[0], sn[1],
                code_of_sig_value(sn[2], width_shift, sign_shitf, false).c_str(), width);
        }
        break;
    }
    case IVL_LPM_SHIFTR: //  7   "a >> b" or "a >>> b"
    {
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_SHIFTR\n");
        const char *sign = is_signed ? "_s" : "_u";
        int sign_shitf = ivl_signal_signed(pin[2]->sig);
        unsigned width_shift = pin[2]->width;
        if (width <= 4) {
            operation += string_format(
                "\tpint_rshift_char%s(&%s, &%s, %s, %u);\n", sign, sn[0], sn[1],
                code_of_sig_value(sn[2], width_shift, sign_shitf, false).c_str(), width);
        } else {
            operation += string_format(
                "\tpint_rshift_int%s(%s, %s, %s, %u);\n", sign, sn[0], sn[1],
                code_of_sig_value(sn[2], width_shift, sign_shitf, false).c_str(), width);
        }
        break;
    }
    case IVL_LPM_SIGN_EXT: // 27   "Pad a signal when instantiate a module"
    {
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_SIGN_EXT\n");
        const char *sign = is_signed ? "_s" : "_u";
        unsigned int width_in = pin[1]->width;

        if (width <= 4) {
            if (!is_signed) {
                operation += string_format("\t%s = %s;\n", sn[0], sn[1]);
            } else {
                operation += string_format("\tpint_pad_char_to_char_s(&%s, &%s, %u, %u);\n", sn[0],
                                           sn[1], width_in, width);
            }
        } else if (width_in <= 4) {
            operation += string_format("\tpint_pad_char_to_int%s(%s, &%s, %u, %u);\n", sign, sn[0],
                                       sn[1], width_in, width);
        } else {
            operation += string_format("\tpint_pad_int_to_int%s(%s, %s, %u, %u);\n", sign, sn[0],
                                       sn[1], width_in, width);
        }
        break;
    }
    case IVL_LPM_SUB: //  8   "a - b"
    {
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_SUB\n");
        const char *sign = is_signed ? "_s" : "_u";
        if (width <= 4)
            operation += string_format("\tpint_sub_char%s(&%s, &%s, &%s, %u);\n", sign, sn[0],
                                       sn[1], sn[2], width);
        else
            operation += string_format("\tpint_sub_int%s(%s, %s, %s, %u);\n", sign, sn[0], sn[1],
                                       sn[2], width);
        break;
    }
    case IVL_LPM_UFUNC: // 14   function
    {
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_UFUNC\n");
        string func_name = ivl_scope_name_c(ivl_lpm_define(net));
        string args;
        unsigned i;
        args += string_format("%s, ", sn[0]);
        for (i = 1; i < npins; i++) {
            args += string_format("%s, ", sn[i]);
        }
        args.erase(args.end() - 2, args.end());
        operation += string_format("\t%s(%s);\n", func_name.c_str(), args.c_str());
        invoked_fun_names.insert(func_name);
        break;
    }

    //	Special:    not support ivl_lpm_size()
    case IVL_LPM_ARRAY: // 30
    {
        //  LPM TYPE    ret += string_format("//  Type: IVL_LPM_ARRAY\n");
        ivl_nexus_t idx_nex = ivl_lpm_select(net);
        pint_sig_info_t idx_info = pint_find_signal(idx_nex);
        ivl_signal_t arr_sig = ivl_lpm_array(net);
        const char *rn[2]; // sig_name for RHS; 0, arr_info; 1, idx_info
        const char *ti;    // tmp variable of idx
        unsigned width_idx = idx_info->width;
        unsigned signed_idx = ivl_signal_signed(idx_info->sig);
        int array_count = ivl_signal_array_count(arr_sig);
        string idx_name;
        string arr_name;
        string tmp_idx;
        pint_get_sig_name(idx_name, idx_nex);
        pint_get_sig_name(arr_name, arr_sig);
        tmp_idx = "idx_" + pint_find_signal(arr_sig)->sig_name;
        rn[0] = arr_name.c_str();
        rn[1] = idx_name.c_str();
        ti = tmp_idx.c_str();
        if (array_count == 1) {
            if (width_idx <= 4) {
                operation +=
                    string_format("\tif(!pint_get_value_char_u(&%s, %u)){\n", rn[1], width_idx);
            } else {
                operation +=
                    string_format("\tif(!pint_get_value_int_u(%s, %u)){\n", rn[1], width_idx);
            }
            if (width <= 4) {
                operation += string_format("\t\t%s = %s;\n", sn[0], rn[0]);
            } else {
                operation += string_format("\t\tpint_copy_int(%s, %s, %u);\n", sn[0], rn[0], width);
            }
            operation += "\t}else{\n";
            operation += pint_gen_signal_set_x_code(sn[0], width);
            operation += "\t}\n";
        } else {
            if (width_idx <= 4) {
                operation += string_format("\tif(pint_is_xz_char(&%s, %u)){\n", rn[1], width_idx);
            } else {
                operation += string_format("\tif(pint_is_xz_int(%s, %u)){\n", rn[1], width_idx);
            }
            operation += pint_gen_signal_set_x_code(sn[0], width);
            operation += "\t}else{\n";
            operation +=
                string_format("unsigned %s = (unsigned)%s;\n", ti,
                              code_of_sig_value(rn[1], width_idx, signed_idx, false).c_str());

            operation += string_format("if (%s < %d){\n", ti, array_count);
            if (width <= 4)
                operation += string_format("\t\t%s = %s[%s];\n", sn[0], rn[0], ti);
            else
                operation +=
                    string_format("\t\tpint_copy_int(%s, %s[%s], %u);\n", sn[0], rn[0], ti, width);
            operation += "}else{";
            if (width <= 4)
                operation += string_format("\t\tpint_set_sig_x(&%s, %d);\n", sn[0], width);
            else
                operation += string_format("\t\tpint_set_sig_x(%s, %d);\n", sn[0], width);
            operation += "}";
            operation += "\t}\n";
        }
        break;
    }
    case IVL_LPM_SUBSTITUTE: // 39
        break;
    case IVL_LPM_LATCH: // 40
        break;
    case IVL_LPM_CMP_WEQ: // 41
        break;
    case IVL_LPM_CMP_WNE: // 42
        break;
    }
}

/*
    Function:   Get all the sen signal of lpm
*/
void pint_lpm_sensitive_sig(ivl_lpm_t net, vector<ivl_nexus_t> &rhs_nex, ivl_signal_t &rhs_arr) {
    rhs_arr = NULL;
    switch (ivl_lpm_type(net)) {
    //	Normal:	    can get all sensitive signal from ivl_lpm_size()
    case IVL_LPM_ABS:       // 32
    case IVL_LPM_ADD:       //  0
    case IVL_LPM_CAST_INT:  // 34
    case IVL_LPM_CAST_INT2: // 35
    case IVL_LPM_CAST_REAL: // 33
    case IVL_LPM_CONCAT:    // 16
    case IVL_LPM_CONCATZ:   // 36
    case IVL_LPM_CMP_EEQ:   // 18
    case IVL_LPM_CMP_EQX:   // 37
    case IVL_LPM_CMP_EQZ:   // 38
    case IVL_LPM_CMP_EQ:    // 10
    case IVL_LPM_CMP_GE:    //  1
    case IVL_LPM_CMP_GT:    //  2
    case IVL_LPM_CMP_NE:    // 11
    case IVL_LPM_CMP_NEE:   // 19
    case IVL_LPM_DIVIDE:    // 12
    case IVL_LPM_FF:        //  3
    case IVL_LPM_MOD:       // 13
    case IVL_LPM_MULT:      //  4
    case IVL_LPM_PART_VP:   // 15
    case IVL_LPM_PART_PV:   // 17
    case IVL_LPM_POW:       // 31
    case IVL_LPM_RE_AND:    // 20
    case IVL_LPM_RE_NAND:   // 21
    case IVL_LPM_RE_NOR:    // 22
    case IVL_LPM_RE_OR:     // 23
    case IVL_LPM_RE_XNOR:   // 24
    case IVL_LPM_RE_XOR:    // 25
    case IVL_LPM_SFUNC:     // 29
    case IVL_LPM_SHIFTL:    //  6
    case IVL_LPM_SHIFTR:    //  7
    case IVL_LPM_SIGN_EXT:  // 27
    case IVL_LPM_SUB:       //  8
    case IVL_LPM_UFUNC:     // 14
        unsigned nrhs, ri;
        nrhs = pint_lpm_size(net);
        for (ri = 0; ri < nrhs; ri++) {
            rhs_nex.push_back(ivl_lpm_data(net, ri));
        }
        break;

    //	Special:    can not get all sensitive signal from ivl_lpm_size()
    case IVL_LPM_MUX: //  5
    {
        unsigned nrhs = pint_lpm_size(net); //  Warning: Do not forget the sel signal
        for (unsigned ri = 0; ri < nrhs; ri++) {
            rhs_nex.push_back(ivl_lpm_data(net, ri));
        }
        rhs_nex.push_back(ivl_lpm_select(net));
        break;
    }
    case IVL_LPM_REPEAT: // 26
        break;
    case IVL_LPM_ARRAY:                         // 30
        rhs_arr = ivl_lpm_array(net);           //	arr_info
        rhs_nex.push_back(ivl_lpm_select(net)); // 	idx_info
        break;
    case IVL_LPM_SUBSTITUTE: // 39
        break;
    case IVL_LPM_LATCH: // 40
        break;
    case IVL_LPM_CMP_WEQ: // 41
        break;
    case IVL_LPM_CMP_WNE: // 42
        break;
    }
}

/*******************< Common LPM >*******************/
struct pint_cmn_lpm_info *pint_get_cmn_lpm_info(pint_lpm_log_info_t net_info) {
    map<string, struct pint_cmn_lpm_info>::iterator iter;
    string file_lineno_basename;
    string scope_name;
    if (net_info->type == LOG) {
        ivl_net_logic_t net = net_info->log;
        file_lineno_basename = string_format("%s_%d_%s", ivl_logic_file(net), ivl_logic_lineno(net),
                                             ivl_logic_basename(net));
        scope_name = ivl_scope_name(ivl_logic_scope(net));
        iter = pint_cmn_lpm.find(file_lineno_basename);
        assert(iter != pint_cmn_lpm.end());
        return &(iter->second);
    } else if (net_info->type == SWITCH_) {
        ivl_switch_t net = net_info->switch_;
        file_lineno_basename = string_format("%s_%d_%s", ivl_switch_file(net),
                                             ivl_switch_lineno(net), ivl_switch_basename(net));
        scope_name = ivl_scope_name(ivl_switch_scope(net));
        iter = pint_cmn_lpm.find(file_lineno_basename);
        assert(iter != pint_cmn_lpm.end());
        return &(iter->second);
    } else {
        ivl_lpm_t net = net_info->lpm;
        file_lineno_basename = string_format("%s_%d_%s", ivl_lpm_file(net), ivl_lpm_lineno(net),
                                             ivl_lpm_basename(net));
        scope_name = ivl_scope_name(ivl_lpm_scope(net));
        iter = pint_cmn_lpm.find(file_lineno_basename);
        assert(iter != pint_cmn_lpm.end());
        return &(iter->second);
    }
}

string pint_get_lpm_scope_name(pint_lpm_log_info_t net_info) {
    string scope_name;
    if (net_info->type == LOG) {
        ivl_net_logic_t net = net_info->log;
        scope_name = ivl_scope_name(ivl_logic_scope(net));
    } else if (net_info->type == SWITCH_) {
        ivl_switch_t net = net_info->switch_;
        scope_name = ivl_scope_name(ivl_switch_scope(net));
    } else {
        ivl_lpm_t net = net_info->lpm;
        scope_name = ivl_scope_name(ivl_lpm_scope(net));
    }

    return scope_name;
}

string pint_get_sig_decl_str(pint_sig_info_t sig_info, string decl_name, bool is_output) {
    string decl_str = "unsigned ";
    string output_pre = is_output ? "&" : "";
    unsigned array_num = sig_info->arr_words;
    unsigned w_words = (sig_info->width + 31) / 32;
    if (sig_info->width <= 4) {
        if (array_num > 1) {
            decl_str += string_format("char %s[%d], ", decl_name.c_str(), array_num);
        } else {
            decl_str += string_format("char %s%s, ", output_pre.c_str(), decl_name.c_str());
        }
    } else {
        decl_str += string_format("int %s", decl_name.c_str());
        if (array_num > 1) {
            decl_str += string_format("[%d][%d], ", array_num, w_words * 2);
        } else {
            decl_str += string_format("[%d], ", w_words * 2);
        }
    }
    return decl_str;
}

void pint_gen_lpm_call_cmn(bool gen_cmn_decl, unsigned lpm_id) {
    if (gen_cmn_decl) {
        cur_lpm_info.cmn_decl_str = string_format("void pint_lpm_cmn_%d(", cur_lpm_info.cmn_lpm_id);
    }
    cur_lpm_info.cmn_call_str = string_format("void pint_lpm_%u(void){\n", lpm_id);
    if (pint_perf_on) {
        unsigned lpm_perf_id = lpm_id - pint_event_num + PINT_PERF_BASE_ID;
        cur_lpm_info.cmn_call_str += string_format("\tNCORE_PERF_SUMMARY(%d);\n", lpm_perf_id);
    }
    cur_lpm_info.cmn_call_str += string_format("\tpint_enqueue_flag_clear(%d);\n", lpm_id);
    cur_lpm_info.cmn_call_str += string_format("\tpint_lpm_cmn_%d(", cur_lpm_info.cmn_lpm_id);

    vector<string> call_para_str;
    vector<string> decl_para_str;
    for (unsigned i = 0; i < cur_lpm_info.para_count; i++) {
        call_para_str.push_back("");
        decl_para_str.push_back("");
    }

    map<pint_sig_info_t, string>::iterator iter;
    for (iter = cur_lpm_info.sig_para.begin(); iter != cur_lpm_info.sig_para.end(); iter++) {
        pint_sig_info_t sig_info = iter->first;
        string decl_idx = iter->second;
        int para_idx = atoi(decl_idx.c_str());
        call_para_str[para_idx] += string_format("%s, ", sig_info->sig_name.c_str());
        if (gen_cmn_decl) {
            bool is_output = (0 == para_idx);
            string decl_name = string_format("__p%d", para_idx);
            iter->second = decl_name;
            string decl_str = pint_get_sig_decl_str(sig_info, decl_name, is_output);
            decl_para_str[para_idx] += decl_str;
        }
    }

    for (unsigned i = 0; i < cur_lpm_info.para_count; i++) {
        cur_lpm_info.cmn_call_str += call_para_str[i];
        if (gen_cmn_decl) {
            cur_lpm_info.cmn_decl_str += decl_para_str[i];
        }
    }

    call_para_str.clear();
    decl_para_str.clear();
}

void pint_diff_lpm_leaf_list(struct pint_cmn_lpm_info *cmn_info) {
    // compare the different scope lpm to generate para decl name
    for (unsigned idx = 0; idx < cmn_info->leaf_count; idx++) {
        map<string, vector<pint_sig_info_t>>::iterator iter;
        pint_sig_info_t tmp_sig = NULL;
        unsigned para_flag = 0;
        for (iter = cmn_info->leaf_list.begin(); iter != cmn_info->leaf_list.end(); iter++) {
            if (cmn_info->para_decl[iter->first].count(iter->second[idx])) {
                // sig has been declared as para
                break;
            }
            // If the output of lpm has been forced, then there should be an judgement before
            // executing. so excluded
            if (pint_signal_been_forced(iter->second[idx]) || iter->second[idx]->arr_words > 1) {
                cmn_info->para_count = PINT_MAX_CMN_PARA_NUM + 1;
                return;
            }
            if (NULL == tmp_sig) {
                tmp_sig = iter->second[idx];
            } else {
                if (iter->second[idx] != tmp_sig) {
                    // make sure output sig sen lpm/evt should be same
                    if (0 == idx) {
                        if ((pint_signal_has_sen_lpm(tmp_sig) !=
                             pint_signal_has_sen_lpm(iter->second[idx])) ||
                            (pint_signal_has_sen_evt(tmp_sig) !=
                             pint_signal_has_sen_evt(iter->second[idx])) ||
                            ((!tmp_sig->sens_process_list.size()) !=
                             (!iter->second[idx]->sens_process_list.size())) ||
                            (tmp_sig->is_dump_var != iter->second[idx]->is_dump_var) ||
                            (pint_pli_mode &&
                             tmp_sig->is_interface != iter->second[idx]->is_interface) ||
                            (pint_pli_mode && tmp_sig->is_interface &&
                             tmp_sig->port_type != iter->second[idx]->port_type)) {
                            cmn_info->para_count = PINT_MAX_CMN_PARA_NUM + 1;
                            return;
                        }
                    }
                    // make sure para sig width should be same
                    if (iter->second[idx]->width != tmp_sig->width) {
                        cmn_info->para_count = PINT_MAX_CMN_PARA_NUM + 1;
                        return;
                    }
                    // make sure para signed flag should be same
                    if (ivl_signal_signed(iter->second[idx]->sig) !=
                        ivl_signal_signed(tmp_sig->sig)) {
                        cmn_info->para_count = PINT_MAX_CMN_PARA_NUM + 1;
                        return;
                    }
                    // make sure para arr words should be same
                    if (iter->second[idx]->arr_words != tmp_sig->arr_words) {
                        cmn_info->para_count = PINT_MAX_CMN_PARA_NUM + 1;
                        return;
                    }
                    if (1 == tmp_sig->arr_words && iter->second[idx]->is_const &&
                        tmp_sig->is_const) {
                        // const para, compare the const value string
                        if (iter->second[idx]->value != tmp_sig->value) {
                            para_flag = 1;
                        }
                    } else {
                        para_flag = 1;
                    }
                }
            }
        }

        if (para_flag) {
            for (iter = cmn_info->leaf_list.begin(); iter != cmn_info->leaf_list.end(); iter++) {
                string decl_idx = string_format("%d", cmn_info->para_count);
                cmn_info->para_decl[iter->first].insert(
                    pair<pint_sig_info_t, string>(iter->second[idx], decl_idx));
            }
            cmn_info->para_count++;
        }
    }
}

void pint_clear_cur_lpm_info() {
    cur_lpm_info.is_cmn = 0;
    cur_lpm_info.para_count = 0;
    cur_lpm_info.cmn_lpm_id = 0;
    cur_lpm_info.need_gen_cmn = 0;
    cur_lpm_info.sig_para.clear();
    cur_lpm_info.cmn_decl_str.clear();
    cur_lpm_info.cmn_call_str.clear();
}

void pint_set_cur_lpm_cmn_info(pint_lpm_log_info_t net_info, unsigned lpm_id) {
    struct pint_cmn_lpm_info *cmn_info = pint_get_cmn_lpm_info(net_info);
    if ((cmn_info->inst_count < pint_min_inst) || (cmn_info->is_generate_type)) {
        pint_clear_cur_lpm_info();
        return;
    }

    if (((net_info->type == LOG) && Multi_drive_record.count(ivl_logic_pin(net_info->log, 0))) ||
        ((net_info->type == LPM) && Multi_drive_record.count(ivl_lpm_q(net_info->lpm)))) {
        pint_clear_cur_lpm_info();
        return;
    }

    string scope_name = pint_get_lpm_scope_name(net_info);
    if (0 == cmn_info->diff_done) {
        pint_diff_lpm_leaf_list(cmn_info);
        cmn_info->diff_done = 1;
    }

    if (cmn_info->para_count > PINT_MAX_CMN_PARA_NUM || cmn_info->para_count == 0) {
        pint_clear_cur_lpm_info();
        return;
    }

    if (0 == cmn_info->call_count % PINT_MAX_CMN_CALL_NUM) {
        cur_lpm_info.need_gen_cmn = 1;
        lpm_total_cmn_num++;
        cmn_info->cmn_lpm_id = lpm_id;
    } else {
        cur_lpm_info.need_gen_cmn = 0;
    }
    cmn_info->call_count++;
    lpm_total_call_cmn_num++;

    cur_lpm_info.is_cmn = 1;
    cur_lpm_info.para_count = cmn_info->para_count;
    cur_lpm_info.cmn_lpm_id = cmn_info->cmn_lpm_id;
    cur_lpm_info.sig_para = cmn_info->para_decl[scope_name];

    cur_lpm_info.cmn_decl_str.clear();
    cur_lpm_info.cmn_call_str.clear();
    pint_gen_lpm_call_cmn(cur_lpm_info.need_gen_cmn, lpm_id);
}

/**************************************** <Common Funcitons>
 * ****************************************/
void pint_show_sig_define(string &ret, pint_sig_info_t sig_info) {
    unsigned width = sig_info->width;
    unsigned cnt = (width + 0x1f) >> 5;

    lpm_tmp_idx++;
    if (width <= 4)
        ret += string_format("\tunsigned char tmp_%d;\n", lpm_tmp_idx);
    else
        ret += string_format("\tunsigned int  tmp_%d[%u];\n", lpm_tmp_idx, cnt * 2);
    sig_info->local_tmp_idx = lpm_tmp_idx;
}

void pint_lpm_undrived_sig(string &ret, pint_sig_info_t sig_info) {
    unsigned width = sig_info->width;
    unsigned cnt = (width + 0x1f) >> 5;

    lpm_tmp_idx++;
    sig_info->local_tmp_idx = lpm_tmp_idx;
    if (width <= 4) {
        unsigned value = ((1 << width) - 1) << 4;
        ret += string_format("\tunsigned char tmp_%d = 0x%x;\n", lpm_tmp_idx, value);
    } else {
        unsigned value;
        unsigned mod_width = ((width - 1) & 0x1f) + 1; // [1, 32]
        unsigned i;
        ret += string_format("\tunsigned int  tmp_%d[%u];\n", lpm_tmp_idx, cnt * 2);
        for (i = 0; i < cnt; i++) {
            ret += string_format("\ttmp_%d[%u] = 0x0;\n", lpm_tmp_idx, i);
        }
        for (i = 0; i < cnt - 1; i++) {
            ret += string_format("\ttmp_%d[%u] = 0xffffffff;\n", lpm_tmp_idx, i + cnt);
        }
        if (mod_width == 32) {
            value = 0xffffffff;
        } else {
            value = (1 << mod_width) - 1;
        }
        ret += string_format("\ttmp_%d[%d] = 0x%08x;\n", lpm_tmp_idx, 2 * cnt - 1, value);
    }
}

unsigned pint_nexus_find_arr_word(ivl_signal_t sig, ivl_nexus_t nex) {
    ivl_nexus_t nex_i;
    unsigned words = ivl_signal_array_count(sig);
    unsigned i;
    for (i = 0; i < words; i++) {
        nex_i = ivl_signal_nex(sig, i);
        if (nex_i == nex) {
            break;
        }
    }
    if (i == words) {
        printf("Error!  --pint_nexus_find_arr_word, can not find nexus from signal.\n");
    }
    return i;
}

string pint_lpm_gen_output_dump_decl() {
    string decl_name = string_format("__p%d", cur_lpm_info.para_count);
    cur_lpm_info.para_count++;
    string decl_str = string_format("const char *%s, ", decl_name.c_str());
    cur_lpm_info.cmn_decl_str += decl_str;
    return decl_name;
}

string pint_lpm_gen_output_port_id_decl() {
    string decl_name = string_format("__p%d", cur_lpm_info.para_count);
    cur_lpm_info.para_count++;
    string decl_str = string_format("unsigned int %s, ", decl_name.c_str());
    cur_lpm_info.cmn_decl_str += decl_str;
    return decl_name;
}

string pint_lpm_gen_p_list_decl() {
    string decl_name = string_format("__p%d", cur_lpm_info.para_count);
    cur_lpm_info.para_count++;
    string decl_str = string_format("unsigned %s[], ", decl_name.c_str());
    cur_lpm_info.cmn_decl_str += decl_str;
    return decl_name;
}

string pint_lpm_gen_output_list_decl() {
    string decl_name = string_format("__p%d", cur_lpm_info.para_count);
    cur_lpm_info.para_count++;
    string decl_str = string_format("pint_thread %s[], ", decl_name.c_str());
    cur_lpm_info.cmn_decl_str += decl_str;
    return decl_name;
}

void pint_lpm_update_multi_drv_lock(string &ret, ivl_nexus_t nex) {
    if (Multi_drive_record.count(nex) != 0) {

        if (Multi_drive_record_count.count(nex) == 0) {
            Multi_drive_record_count.insert(pair<ivl_nexus_t, int>(nex, 0));
        }

        pint_sig_info_t sig_info_tmp = pint_find_signal(nex);
        unsigned width = sig_info_tmp->width;
        string tbl_name = string_format("%s_%d_mdrv_tbl", sig_info_tmp->sig_name.c_str(),
                                        pint_get_arr_idx_from_nex(nex));

        // because multi drive is only for global lpm, so just use pint_out is ok
        ret += string_format("pint_merge_multi_drive(%spint_out, %s, %d, 0, %d, %d);\n",
                             (width <= 4) ? "&" : "", tbl_name.c_str(),
                             Multi_drive_record_count[nex]++, width,
                             ivl_signal_type(sig_info_tmp->sig));
    }
}

void pint_lpm_update_multi_drv_unlock(string &ret, ivl_nexus_t nex) {
    if (Multi_drive_record.count(nex) != 0) {
        pint_sig_info_t sig_info_tmp = pint_find_signal(nex);
        string tbl_name = string_format("%s_%d_mdrv_tbl", sig_info_tmp->sig_name.c_str(),
                                        pint_get_arr_idx_from_nex(nex));

        ret += string_format("pint_finish_multi_drive(%s, %d);\n", tbl_name.c_str(),
                             Multi_drive_record[nex]);
    }
}

void pint_lpm_update_output(string &ret, ivl_nexus_t nex, pint_lpm_log_info_t net_info) {
    pint_sig_info_t sig_info = pint_find_signal(nex);
    unsigned word = pint_get_arr_idx_from_nex(nex);
    unsigned width = sig_info->width;
    unsigned update_evt = pint_signal_has_sen_evt(sig_info, word);
    bool lpm_part_pv_flag =
        net_info && (net_info->type == LPM) && (ivl_lpm_type(net_info->lpm) == IVL_LPM_PART_PV);
    string fmt_sig;
    string list;
    pint_get_sig_name(fmt_sig, nex);
    const char *fmt = fmt_sig.c_str();
    const char *sn = sig_info->sig_name.c_str();
    if (width <= 4) {
        ret += string_format("\tif(%s != pint_out){\n", fmt);

        if (!lpm_part_pv_flag) {
            if (update_evt) {
                // ret += string_format("\t\tunsigned char out_tmp = %s;\n", fmt);
                if (width == 1) {
                    ret += string_format("\t\tunsigned eg = edge_1bit(%s, pint_out);\n", fmt);
                } else {
                    ret += string_format("\t\tunsigned eg = edge_char(%s, pint_out);\n", fmt);
                }
            }
            ret += string_format("\t\t%s = pint_out;\n", fmt);
        }

        if (sig_info->sens_process_list.size()) {
            if (cur_lpm_info.is_cmn) {
                cur_lpm_info.cmn_call_str += string_format("p_list_%s, ", sn);
            }
            if (!cur_lpm_info.need_gen_cmn) {
                ret += string_format("\t\tpint_set_event_flag(p_list_%s);\n", sn);
            } else {
                string decl_name = pint_lpm_gen_p_list_decl();
                ret += string_format("\t\tpint_set_event_flag(%s);\n", decl_name.c_str());
            }
        }

        if (update_evt) {
            list = "e_list_" + sig_info->sig_name;

            // string edge;
            // if (width == 1) {
            //     edge = lpm_part_pv_flag ? string_format("edge_1bit(pint_out, %s)", fmt)
            //                                : "edge_1bit(out_tmp, pint_out)";
            // } else {
            //     edge = lpm_part_pv_flag ? string_format("edge_char(pint_out, %s)", fmt)
            //                                : "edge_char(out_tmp, pint_out)";
            // }
            if (lpm_part_pv_flag) {
                if (width == 1) {
                    ret += string_format("unsigned eg = edge_1bit(pint_out, %s)", fmt);
                } else {
                    ret += string_format("unsigned eg = edge_char(pint_out, %s)", fmt);
                }
            }
            if (cur_lpm_info.is_cmn) {
                cur_lpm_info.cmn_call_str += list + ", ";
            }
            if (cur_lpm_info.need_gen_cmn) {
                list = pint_lpm_gen_output_list_decl();
            }
            ret += pint_gen_sig_e_list_code(sig_info, list.c_str(), "eg", word) + "\n";
        }
        if (pint_signal_has_sen_lpm(sig_info, word)) {
            list = "l_list_" + sig_info->sig_name;
            if (cur_lpm_info.is_cmn) {
                cur_lpm_info.cmn_call_str += list + ", ";
            }
            if (cur_lpm_info.need_gen_cmn) {
                list = pint_lpm_gen_output_list_decl();
            }
            ret += pint_gen_sig_l_list_code(sig_info, list.c_str(), word) + "\n";
        }

        if (sig_info->is_dump_var) {
            if (cur_lpm_info.is_cmn) {
                cur_lpm_info.cmn_call_str += string_format("\"%s\", ", sig_info->dump_symbol);
            }
            if (!cur_lpm_info.need_gen_cmn) {
                ret += string_format("        pint_vcddump_char(%s, %d, \"%s\");\n", fmt,
                                     sig_info->width, sig_info->dump_symbol);
            } else {
                string decl_name = pint_lpm_gen_output_dump_decl();
                ret += string_format("        pint_vcddump_char(%s, %d, %s);\n", fmt,
                                     sig_info->width, decl_name.c_str());
            }
        }

        if (pint_pli_mode && sig_info->is_interface && (IVL_SIP_OUTPUT == sig_info->port_type)) {
            if (cur_lpm_info.is_cmn) {
                cur_lpm_info.cmn_call_str += string_format("%d, ", sig_info->output_port_id);
            }
            if (!cur_lpm_info.need_gen_cmn) {
                ret += string_format("        pint_update_output_value_change(%d);\n",
                                     sig_info->output_port_id);
            } else {
                string decl_name = pint_lpm_gen_output_port_id_decl();
                ret += string_format("        pint_update_output_value_change(%s);\n",
                                     decl_name.c_str());
            }
        }
        ret += string_format("\t}\n");

    } else {
        ret += string_format("\tif(!pint_case_equality_int_ret(%s, pint_out, %u)){\n", fmt, width);

        if (!lpm_part_pv_flag) {
            if (update_evt) {
                // unsigned w = ((width + 31) >> 4);
                // ret += string_format(
                //     "\t\tunsigned int out_tmp[%d];\n\t\tpint_copy_int(out_tmp, %s, %u);\n", w, fmt,
                //     width);
                ret += string_format("unsigned eg = edge_int(%s, pint_out, %u);\n", fmt, width);
            }
            ret += string_format("\t\tpint_copy_int(%s, pint_out, %u);\n", fmt, width);
        }

        if (sig_info->sens_process_list.size()) {
            if (cur_lpm_info.is_cmn) {
                cur_lpm_info.cmn_call_str += string_format("p_list_%s, ", sn);
            }
            if (!cur_lpm_info.need_gen_cmn) {
                ret += string_format("\t\tpint_set_event_flag(p_list_%s);\n", sn);
            } else {
                string decl_name = pint_lpm_gen_p_list_decl();
                ret += string_format("\t\tpint_set_event_flag(%s);\n", decl_name.c_str());
            }
        }

        if (update_evt) {
            list = "e_list_" + sig_info->sig_name;
            string edge;
            if (lpm_part_pv_flag) {
                // edge = string_format("edge_int(pint_out, %s, %u)", fmt, width);
                ret += string_format("unsigned eg = edge_int(pint_out, %s, %u);\n", fmt, width);
            }
            // } else {
            //     edge = string_format("edge_int(out_tmp, pint_out, %u)", width);
            // }
            if (cur_lpm_info.is_cmn) {
                cur_lpm_info.cmn_call_str += list + ", ";
            }
            if (cur_lpm_info.need_gen_cmn) {
                list = pint_lpm_gen_output_list_decl();
            }
            ret += pint_gen_sig_e_list_code(sig_info, list.c_str(), "eg", word) + "\n";
        }
        if (pint_signal_has_sen_lpm(sig_info, word)) {
            list = "l_list_" + sig_info->sig_name;
            if (cur_lpm_info.is_cmn) {
                cur_lpm_info.cmn_call_str += list + ", ";
            }
            if (cur_lpm_info.need_gen_cmn) {
                list = pint_lpm_gen_output_list_decl();
            }
            ret += pint_gen_sig_l_list_code(sig_info, list.c_str(), word) + "\n";
        }

        if (sig_info->is_dump_var) {
            if (cur_lpm_info.is_cmn) {
                cur_lpm_info.cmn_call_str += string_format("\"%s\", ", sig_info->dump_symbol);
            }
            if (!cur_lpm_info.need_gen_cmn) {
                ret += string_format("        pint_vcddump_int(%s, %d, \"%s\");\n", fmt,
                                     sig_info->width, sig_info->dump_symbol);
            } else {
                string decl_name = pint_lpm_gen_output_dump_decl();
                ret += string_format("        pint_vcddump_int(%s, %d, %s);\n", fmt,
                                     sig_info->width, decl_name.c_str());
            }
        }

        if (pint_pli_mode && sig_info->is_interface && (IVL_SIP_OUTPUT == sig_info->port_type)) {
            if (cur_lpm_info.is_cmn) {
                cur_lpm_info.cmn_call_str += string_format("%d, ", sig_info->output_port_id);
            }
            if (!cur_lpm_info.need_gen_cmn) {
                ret += string_format("        pint_update_output_value_change(%d);\n",
                                     sig_info->output_port_id);
            } else {
                string decl_name = pint_lpm_gen_output_port_id_decl();
                ret += string_format("        pint_update_output_value_change(%s);\n",
                                     decl_name.c_str());
            }
        }
        ret += string_format("\t}\n");
    }

    if (cur_lpm_info.is_cmn) {
        cur_lpm_info.cmn_call_str =
            cur_lpm_info.cmn_call_str.substr(0, cur_lpm_info.cmn_call_str.length() - 2) + ");\n}\n";
        if (cur_lpm_info.need_gen_cmn) {
            cur_lpm_info.cmn_decl_str =
                cur_lpm_info.cmn_decl_str.substr(0, cur_lpm_info.cmn_decl_str.length() - 2) + ")";
            if (strcmp(enval, enval_flag) != 0) {
                lpm_declare_buff += cur_lpm_info.cmn_decl_str + ";\n";
            }
            cur_lpm_info.cmn_decl_str += " {\n";
        }
    }
}

void pint_get_sig_name(string &ret, pint_sig_info_t sig_info, int arr_idx) {
    ret = sig_info->sig_name;        //  for global signal
    if (cur_lpm_info.need_gen_cmn) { //  for cmn para
        map<pint_sig_info_t, string>::iterator it = cur_lpm_info.sig_para.find(sig_info);
        if (it != cur_lpm_info.sig_para.end()) {
            ret = it->second;
        }
    }
    if (sig_info->local_tmp_idx > 0) { //  for local lpm output
        ret = string_format("tmp_%d", sig_info->local_tmp_idx);
    }
    if (sig_info->arr_words > 1 &&
        arr_idx >= 0) { // if arr_idx = -1, sig is sig_arr, means you want to use the entire arr
        ret += string_format("[%u]", arr_idx);
    }
}

void pint_get_sig_name(string &ret, ivl_nexus_t nex) {
    pint_sig_info_t sig_info = pint_find_signal(nex);
    pint_get_sig_name(ret, sig_info, pint_get_arr_idx_from_nex(nex));
}

void pint_get_sig_name(string &ret, ivl_signal_t sig) {
    pint_sig_info_t sig_info = pint_find_signal(sig);
    pint_get_sig_name(ret, sig_info, -1);
}

unsigned pint_width_to_init(unsigned width) {
    unsigned ret = 0;
    for (unsigned i = 0; i < width; i++)
        ret += 1 << i;
    return ret;
}

const char *pint_find_bits_from_nexus(ivl_nexus_t net) {
    const char *ret = NULL;
    unsigned int nptr = ivl_nexus_ptrs(net);
    ivl_net_const_t con;
    unsigned i;
    for (i = 0; i < nptr; i++) {
        con = ivl_nexus_ptr_con(ivl_nexus_ptr(net, i));
        if (con) {
            ret = ivl_const_bits(con);
            break;
        }
    }
    return ret;
}
bool pint_bits_is_zero(const char *bits) {
    while (*bits) {
        if (*(bits++) != '0') {
            return 0;
        }
    }
    return 1;
}

string pint_lpm_show_source_file(pint_lpm_log_info_t net_info) {
    const char *file;
    // unsigned int line;
    int line;
    string buff;

    if (net_info->type == LOG) {
        ivl_net_logic_t log = net_info->log;
        file = ivl_logic_file(log);
        line = ivl_logic_lineno(log);
    } else if (net_info->type == SWITCH_) {
        ivl_switch_t log = net_info->switch_;
        file = ivl_switch_file(log);
        line = ivl_switch_lineno(log);
    } else {
        ivl_lpm_t lpm = net_info->lpm;
        file = ivl_lpm_file(lpm);
        line = ivl_lpm_lineno(lpm);
    }
    pint_source_file_ret(buff, file, line);
    return buff;
}

string pint_ret_sig_sign_bit(ivl_nexus_t nex) {
    pint_sig_info_t sig_info = pint_find_signal(nex);
    unsigned width = sig_info->width;
    string sig_name;
    pint_get_sig_name(sig_name, nex);
    if (width <= 4) {
        return string_format("(%s & 0x%x)", sig_name.c_str(), 1 << (width - 1));
    } else {
        unsigned idx = ((width + 0x1f) >> 5) - 1;
        unsigned mod = (width - 1) & 0x1f;
        return string_format("(%s[%u] & 0x%x)", sig_name.c_str(), idx, 1 << mod);
    }
}

int pint_find_lpm_from_output(ivl_nexus_t nex) {
    if (pint_nex_to_lpm_idx.count(nex)) {
        return pint_nex_to_lpm_idx[nex];
    } else {
        return -1;
    }
}

unsigned pint_log_chk_support(ivl_net_logic_t net) {
    ivl_logic_t type = ivl_logic_type(net);
    unsigned is_support = 1;
    switch (type) {
    //  Type not support at all
    case IVL_LO_NONE:     //  0
    case IVL_LO_BUF:      //  2
    case IVL_LO_BUFIF0:   //  3
    case IVL_LO_BUFIF1:   //  4
    case IVL_LO_NMOS:     //  7
    case IVL_LO_NOTIF0:   // 10
    case IVL_LO_NOTIF1:   // 11
    case IVL_LO_PULLDOWN: // 13
    case IVL_LO_PULLUP:   // 14
    case IVL_LO_RNMOS:    // 15
    case IVL_LO_RPMOS:    // 16
    case IVL_LO_PMOS:     // 17
    case IVL_LO_UDP:      // 21
    case IVL_LO_CMOS:     // 22
    case IVL_LO_RCMOS:    // 23
    case IVL_LO_EQUIV:    // 25
    case IVL_LO_IMPL:     // 26
        is_support = 0;
        break;
    //  Type part support
    case IVL_LO_NAND: //  6   a ~& b
        if (ivl_logic_pins(net) > 3) {
            is_support = 0;
        }
        break;
    default:
        break;
    }

    if (!is_support) {
        const char *file = ivl_logic_file(net);
        unsigned lineno = ivl_logic_lineno(net);
        string content;
        pint_source_file_ret(content, file, lineno);
        PINT_UNSUPPORTED_ERROR(file, lineno, "Logic type-id %u: %s", type, content.c_str());
    }
    return is_support;
}

unsigned pint_lpm_chk_support(ivl_lpm_t net) {
    ivl_lpm_type_t type = ivl_lpm_type(net);
    unsigned is_support = 1;
    switch (type) {
    //  Type not support at all
    case IVL_LPM_FF:         //  3
    case IVL_LPM_REPEAT:     // 26
    case IVL_LPM_SFUNC:      // 29
    case IVL_LPM_ABS:        // 32
    case IVL_LPM_CAST_REAL:  // 33
    case IVL_LPM_CAST_INT:   // 34
    case IVL_LPM_CAST_INT2:  // 35
    case IVL_LPM_CMP_EQX:    // 37
    case IVL_LPM_CMP_EQZ:    // 38
    case IVL_LPM_SUBSTITUTE: // 39
    case IVL_LPM_LATCH:      // 40
    case IVL_LPM_CMP_WEQ:    // 41
    case IVL_LPM_CMP_WNE:    // 42
        is_support = 0;
        break;
    //  Type part support
    case IVL_LPM_MUX: //  5   "out = sel ? d1 : d0"
        if (pint_lpm_size(net) != 2) {
            is_support = 0;
        }
        break;
    case IVL_LPM_PART_PV:              // 17   "out[base+size-1 : base] = in"; width = width_in
        if (pint_lpm_size(net) != 1) { //  has pin 'sel'
            is_support = 0;
        }
        break;
    case IVL_LPM_SHIFTL: //  6   "a << b"(unsigned) or "a <<< b"(signed)
    {
        ivl_nexus_t nex_shift = ivl_lpm_data(net, 1);
        pint_sig_info_t sig_info = pint_find_signal(nex_shift);
        if (ivl_signal_signed(sig_info->sig)) {
            is_support = 0;
        }
        break;
    }
    default:
        break;
    }

    if (!is_support) {
        const char *file = ivl_lpm_file(net);
        unsigned lineno = ivl_lpm_lineno(net);
        string content;
        pint_source_file_ret(content, file, lineno);
        PINT_UNSUPPORTED_ERROR(file, lineno, "Lpm type-id %u: %s", type, content.c_str());
    }
    return is_support;
}

void pint_lpm_set_no_need_init(ivl_nexus_t nex) {
    if (nex) {
        pint_lpm_no_need_init.insert(nex);
    }
}

void pint_give_lpm_no_need_init_idx() {
    set<ivl_nexus_t>::iterator i;
    ivl_nexus_t nex;
    for (i = pint_lpm_no_need_init.begin(); i != pint_lpm_no_need_init.end(); i++) {
        nex = *i;
        if (!pint_nex_to_lpm_idx.count(nex)) {
            assert(0);
        }
        pint_lpm_no_need_init_idx.insert(pint_nex_to_lpm_idx[nex]);
    }
}

bool pint_is_lpm_needs_init(unsigned idx) {
    if (pint_lpm_no_need_init_idx.count(idx)) {
        return 0;
    } else {
        return 1;
    }
}
