/*
 * Copyright (c) 2004-2013 Stephen Williams (steve@icarus.com)
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

#include "config.h"
#include "priv.h"
#include <assert.h>
#include <string.h>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// dump nb assign data vector
unsigned pint_nbassign_num = 0;
unsigned pint_nbassign_array_num = 0;
unsigned pint_repeat_idx = 0;
unsigned lval_idx_num = 0;
unsigned pint_while_num = 0;
string key_val;
string key_val_array[128];
int display_type = -1;

/******************* <Common Function>*******************/
void pint_mask_var_int(string &buff, const char *sn, unsigned base, unsigned len) {
    unsigned idx_st = base >> 5;
    unsigned idx_end = (base + len - 1) >> 5;
    unsigned mask_st = 0xffffffff << (base & 0x1f);
    unsigned mask_end = 0xffffffff >> (31 - ((base + len - 1) & 0x1f));
    unsigned i;
    if (idx_st == idx_end) {
        buff += string_format("\t%s[%u] |= 0x%x;\n", sn, idx_st, mask_st & mask_end);
    } else {
        buff += string_format("\t%s[%u] |= 0x%x;\n", sn, idx_st, mask_st);
        for (i = idx_st + 1; i < idx_end; i++) {
            buff += string_format("\t%s[%u]  = 0xffffffff;\n", sn, i);
        }
        buff += string_format("\t%s[%u] |= 0x%x;\n", sn, idx_end, mask_end);
    }
}

void pint_unmask_var_int(string &buff, const char *sn, unsigned base, unsigned len) {
    unsigned idx_st = base >> 5;
    unsigned idx_end = (base + len - 1) >> 5;
    unsigned mask_st = 0xffffffff << (base & 0x1f);
    unsigned mask_end = 0xffffffff >> (31 - ((base + len - 1) & 0x1f));
    unsigned i;
    if (idx_st == idx_end) {
        buff += string_format("\t%s[%u] &= 0x%x;\n", sn, idx_st, ~(mask_st & mask_end));
    } else {
        buff += string_format("\t%s[%u] &= 0x%x;\n", sn, idx_st, ~mask_st);
        for (i = idx_st + 1; i < idx_end; i++) {
            buff += string_format("\t%s[%u]  = 0x0;\n", sn, i);
        }
        buff += string_format("\t%s[%u] &= 0x%x;\n", sn, idx_end, ~mask_end);
    }
}

/*******************< Show force stmt >*******************/
void pint_show_force_lval(string &buff, pint_force_lval_t lval_info, unsigned idx) {
    if ((lval_info->word == 0xffffffff) || (lval_info->updt_base == 0xffffffff)) {
        return;
    }
    pint_sig_info_t sig_info = lval_info->sig_info;
    unsigned word = lval_info->word;
    string sig_name = pint_get_force_sig_name(sig_info, word);
    const char *sn = sig_name.c_str();
    pint_force_info_t force_info = pint_find_signal_force_info(sig_info, word);
    if (force_info->part_force) {
        unsigned updt_base = lval_info->updt_base;
        unsigned updt_size = lval_info->updt_size;
        unsigned width = sig_info->width;
        set<unsigned>::iterator iter;
        unsigned force_idx;
        // buff += string_format("//  updt_base: %u\n", updt_base);
        // buff += string_format("//  updt_size: %u\n", updt_size);
        if (width <= 4) {
            unsigned char mask, _mask;
            mask = ((1 << updt_size) - 1) << updt_base;
            mask |= mask << 4;
            _mask = ~mask;
            buff += string_format("\tumask_%s &= 0x%x;\n", sn, _mask);
            for (iter = force_info->force_idx.begin(); iter != force_info->force_idx.end();
                 iter++) {
                force_idx = *iter;
                if (force_idx == idx) {
                    buff += string_format("\tfmask_%u_%s |= 0x%x;\n",
                                          force_idx + FORCE_THREAD_OFFSET, sn, mask);
                } else {
                    buff += string_format("\tfmask_%u_%s &= 0x%x;\n",
                                          force_idx + FORCE_THREAD_OFFSET, sn, _mask);
                }
            }
        } else if (width <= 32) { //  updt_base, [0, 31]; updt_size, [1, 32]
            unsigned mask, _mask;
            if (updt_size == 32) {
                mask = 0xffffffff;
            } else {
                mask = (1 << updt_size) - 1;
            }
            mask <<= updt_base;
            _mask = ~mask;
            buff += string_format("\tumask_%s &= 0x%x;\n", sn, _mask);
            for (iter = force_info->force_idx.begin(); iter != force_info->force_idx.end();
                 iter++) {
                force_idx = *iter;
                if (force_idx == idx) {
                    buff += string_format("\tfmask_%u_%s |= 0x%x;\n",
                                          force_idx + FORCE_THREAD_OFFSET, sn, mask);
                } else {
                    buff += string_format("\tfmask_%u_%s &= 0x%x;\n",
                                          force_idx + FORCE_THREAD_OFFSET, sn, _mask);
                }
            }
        } else {
            string str_mask = string_format("umask_%s", sn);
            pint_unmask_var_int(buff, str_mask.c_str(), updt_base, updt_size);
            for (iter = force_info->force_idx.begin(); iter != force_info->force_idx.end();
                 iter++) {
                force_idx = *iter;
                str_mask = string_format("fmask_%u_%s", sn, force_idx + FORCE_THREAD_OFFSET, sn);
                if (force_idx == idx) {
                    pint_mask_var_int(buff, str_mask.c_str(), updt_base, updt_size);
                } else {
                    pint_unmask_var_int(buff, str_mask.c_str(), updt_base, updt_size);
                }
            }
        }
    } else {
        buff += string_format("\tforce_en_%s = 1;\n", sn);
        if (force_info->mult_force) {
            buff +=
                string_format("\tforce_thread_%s = pint_lpm_%u;\n", sn, idx + FORCE_THREAD_OFFSET);
        }
    }
    buff += string_format("\tpint_lpm_%u();\n", idx + FORCE_THREAD_OFFSET);
}

int pint_show_assign_lval(string &buff, pint_force_lval_t lval_info, unsigned idx,
                          ivl_statement_t net) {
    if ((lval_info->word == 0xffffffff) || (lval_info->updt_base == 0xffffffff)) {
        return 0;
    }
    pint_sig_info_t sig_info = lval_info->sig_info;
    unsigned word = lval_info->word;
    string sig_name = pint_get_force_sig_name(sig_info, word);
    const char *sn = sig_name.c_str();
    pint_force_info_t force_info = pint_find_signal_force_info(sig_info, word);
    if (force_info->part_force) {
        // printf("cassign not support part_force\n");
        const char *file = ivl_stmt_file(net);
        int lineno = ivl_stmt_lineno(net);
        PINT_UNSUPPORTED_ERROR(file, lineno, "cassign not support part_force.");
        return -1;
    } else {
        buff += string_format("\tassign_en_%s = 1;\n", sn);
        if (force_info->mult_assign) {
            buff +=
                string_format("\tassign_thread_%s = pint_lpm_%u;\n", sn, idx + FORCE_THREAD_OFFSET);
        }
    }
    buff += string_format("\tpint_lpm_%u();\n", idx + FORCE_THREAD_OFFSET);
    return 0;
}

void StmtGen::show_stmt_force(ivl_statement_t net) {
    unsigned nlhs = ivl_stmt_lvals(net);
    unsigned i;
    for (i = 0; i < nlhs; i++) {
        pint_show_force_lval(stmt_code, pint_force_lval[ivl_stmt_lval(net, i)],
                             pint_force_idx[net]);
    }
}

void StmtGen::show_stmt_cassign(ivl_statement_t net) {
    unsigned nlhs = ivl_stmt_lvals(net);
    unsigned i;
    for (i = 0; i < nlhs; i++) {
        pint_show_assign_lval(stmt_code, pint_force_lval[ivl_stmt_lval(net, i)],
                              pint_force_idx[net], net);
    }
}

void pint_show_release_lval(string &buff, pint_force_lval_t lval_info) {
    if ((lval_info->word == 0xffffffff) || (lval_info->updt_base == 0xffffffff)) {
        return;
    }
    pint_sig_info_t sig_info = lval_info->sig_info;
    unsigned width = sig_info->width;
    unsigned cnt = (width + 0x1f) >> 5;
    unsigned word = lval_info->word;
    string sig_name = pint_get_force_sig_name(sig_info, word);
    string str_value = pint_get_sig_value_name(sig_info, word);
    const char *sn = sig_name.c_str();
    const char *sv = str_value.c_str();
    pint_force_info_t force_info = pint_find_signal_force_info(sig_info, word);
    if (force_info->part_force) {
        unsigned updt_base = lval_info->updt_base;
        unsigned updt_size = lval_info->updt_size;
        set<unsigned>::iterator iter;
        unsigned thread_id;
        if (width <= 4) {
            unsigned char mask, _mask;
            mask = ((1 << updt_size) - 1) << updt_base;
            mask |= mask << 4;
            _mask = ~mask;
            buff += string_format("\tumask_%s |= 0x%x;\n", sn, mask);
            for (iter = force_info->force_idx.begin(); iter != force_info->force_idx.end();
                 iter++) {
                thread_id = *iter + FORCE_THREAD_OFFSET;
                buff += string_format("\tfmask_%u_%s &= 0x%x;\n", thread_id, sn, _mask);
            }
        } else if (width <= 32) {
            unsigned mask, _mask;
            if (updt_size == 32) {
                mask = 0xffffffff;
            } else {
                mask = (1 << updt_size) - 1;
            }
            mask <<= updt_base;
            _mask = ~mask;
            buff += string_format("\tumask_%s |= 0x%x;\n", sn, mask);
            for (iter = force_info->force_idx.begin(); iter != force_info->force_idx.end();
                 iter++) {
                thread_id = *iter + FORCE_THREAD_OFFSET;
                buff += string_format("\tfmask_%u_%s &= 0x%x;\n", thread_id, sn, _mask);
            }
        } else {
            string str_mask = string_format("umask_%s", sn);
            pint_mask_var_int(buff, str_mask.c_str(), updt_base, updt_size);
            for (iter = force_info->force_idx.begin(); iter != force_info->force_idx.end();
                 iter++) {
                thread_id = *iter + FORCE_THREAD_OFFSET;
                str_mask = string_format("fmask_%u_%s", sn, thread_id, sn);
                pint_unmask_var_int(buff, str_mask.c_str(), updt_base, updt_size);
            }
        }
    } else {
        buff += string_format("\tforce_en_%s = 0;\n", sn);
    }
    if (force_info->type == FORCE_NET) {
        int dirven_lpm = pint_find_lpm_from_output(ivl_signal_nex(sig_info->sig, word));
        if (pint_signal_is_force_net_const(sig_info, word)) {
            unsigned idx = force_net_is_const_map[sig_info][word] + FORCE_THREAD_OFFSET;
            buff += string_format("\tpint_lpm_%u();\n", idx);
        } else if (dirven_lpm != -1) {
            unsigned idx = dirven_lpm + LPM_THREAD_OFFSET;
            buff += string_format("\tpint_lpm_%u();\n", idx);
        } else {
            if (force_info->part_force) {
                if (width <= 4) {
                    buff += string_format("\t%s = (force_%s & umask_%s) | (%s & (~umask_%s));\n",
                                          sv, sn, sn, sv, sn);
                } else if (width <= 32) {
                    buff += string_format(
                        "\t%s[0] = (force_%s[0] & umask_%s) | (%s[0] & (~umask_%s));\n", sv, sn, sn,
                        sv, sn);
                    buff += string_format(
                        "\t%s[1] = (force_%s[1] & umask_%s) | (%s[1] & (~umask_%s));\n", sv, sn, sn,
                        sv, sn);
                } else {
                    buff += string_format(
                        "\tfor(int i =0; i < %u; i++){\n"
                        "\t\t%s[i] = (force_%s[i] & umask_%s[i]) | (%s[i] & (~umask_%s[i]));\n"
                        "\t\t%s[i +%u] = (force_%s[i +%u] & umask_%s[i]) | (%s[i +%u] & "
                        "(~umask_%s[i]));\n"
                        "\t}\n",
                        cnt, sv, sn, sn, sv, sn, sv, cnt, sn, cnt, sn, sv, cnt, sn);
                }
            } else {
                bool event_care_flag = pint_signal_has_sen_evt(sig_info, word);
                if (width <= 4) {
                    buff += string_format("if (%s != force_%s){\n", sv, sn);
                    if (event_care_flag) {
                        buff += string_format(
                            "\tint change_flag = pint_copy_char_char_same_edge(%s, force_%s);\n",
                            sv, sn);
                    } else {
                        buff += string_format("%s = force_%s;\n", sv, sn);
                    }
                } else {
                    buff += string_format("if (!pint_case_equality_int_ret(%s, force_%s, %u)){\n",
                                          sv, sn, width);
                    if (event_care_flag) {
                        buff += string_format(
                            "\tint change_flag = pint_copy_int_int_same_edge(force_%s, %s, %u);\n",
                            sn, sv, width);
                    } else {
                        buff += string_format("\tpint_copy_int_int_same(force_%s, %s, %u);\n", sn,
                                              sv, width);
                    }
                }
                if (event_care_flag) {
                    buff += "\t\t" + pint_gen_sig_e_list_code(sig_info, "change_flag", word) + "\n";
                }
                if (pint_signal_has_sen_lpm(sig_info, word)) {
                    buff += "\t\t" + pint_gen_sig_l_list_code(sig_info, word) + "\n";
                }
                if (sig_info->sens_process_list.size()) {
                    buff += string_format("\t\tpint_set_event_flag(p_list_%s);\n", sn);
                }
                buff += "}";
            }
        }
    } else {
        if (pint_signal_has_p_list(sig_info)) {
            buff += pint_gen_sig_p_list_code(sig_info) + "\n";
        }
    }
}

void pint_show_deassign_lval(string &buff, pint_force_lval_t lval_info, ivl_statement_t net) {
    if ((lval_info->word == 0xffffffff) || (lval_info->updt_base == 0xffffffff)) {
        return;
    }
    pint_sig_info_t sig_info = lval_info->sig_info;
    unsigned word = lval_info->word;
    string sig_name = pint_get_force_sig_name(sig_info, word);
    const char *sn = sig_name.c_str();
    pint_force_info_t force_info = pint_find_signal_force_info(sig_info, word);
    if (force_info->part_force) {
        const char *file = ivl_stmt_file(net);
        int lineno = ivl_stmt_lineno(net);
        PINT_UNSUPPORTED_ERROR(file, lineno, "deassign not support part_force.");
    } else {
        buff += string_format("\tassign_en_%s = 0;\n", sn);
        if (pint_signal_has_p_list(sig_info)) {
            buff += pint_gen_sig_p_list_code(sig_info) + "\n";
        }
    }
}

static void pint_collect_signal_s_list_info(pint_event_info_t event_info, pint_thread_info_t thread,
                                            ivl_event_t evt) 
{
    ivl_nexus_t nex = NULL;
    pint_sig_info_t sig_info = NULL;
    ivl_signal_t sig = NULL;

    if (event_info->pos_num) {
        for (unsigned idx = 0; idx < event_info->pos_num; idx += 1) {
            nex = ivl_event_pos(event_info->evt, idx);
            sig_info = pint_find_signal(nex);
            if(sig_info) {
              sig = sig_info->sig;
            }
            if(sig && sig_info->arr_words == 1) {
                if (signal_s_list_map.count(sig) > 0) {
                    signal_s_list_map[sig].pos_num++;
                    signal_s_list_map[sig].pos_buff +=
                        string_format("pint_thread_%d, (pint_thread)%u, ", thread->id, thread->id);
                }
                else {
                    pint_static_event_list_table_num++;
                    struct pint_signal_s_list_info_s signal_s_list_info;
                    signal_s_list_info.pos_num++;
                    signal_s_list_info.pos_buff = string_format("pint_thread_%d, (pint_thread)%u, ", 
                                                                thread->id, thread->id);
                    signal_s_list_info.declare_buff =  string_format("extern pint_thread s_list_%s[];\n",
                                                                    sig_info->sig_name.c_str());
                    signal_s_list_info.define_buff = string_format("pint_thread s_list_%s[] = ",
                                                                    sig_info->sig_name.c_str());
                    signal_s_list_map[sig] = signal_s_list_info;
                }
            }
            if(sig_info->arr_words > 1) {
                event_info->has_array_signal = 1;
            }
        }
    }
    if (event_info->any_num) {
        for (unsigned idx = 0; idx < event_info->any_num; idx += 1) {
            nex = ivl_event_any(event_info->evt, idx);
            sig_info = pint_find_signal(nex);
            if(sig_info) {
              sig = sig_info->sig;
            }
            if(sig && sig_info->arr_words == 1) {
                if (signal_s_list_map.count(sig) > 0) {
                    signal_s_list_map[sig].any_num++;
                    signal_s_list_map[sig].any_buff +=
                        string_format("pint_thread_%d, (pint_thread)%u, ", thread->id, thread->id);
                }
                else {
                    pint_static_event_list_table_num++;
                    struct pint_signal_s_list_info_s signal_s_list_info;
                    signal_s_list_info.any_num++;
                    signal_s_list_info.any_buff = string_format("pint_thread_%d, (pint_thread)%u, ", 
                                                                thread->id, thread->id);
                    signal_s_list_info.declare_buff =  string_format("extern pint_thread s_list_%s[];\n",
                                                                    sig_info->sig_name.c_str());
                    signal_s_list_info.define_buff = string_format("pint_thread s_list_%s[] = ",
                                                                    sig_info->sig_name.c_str());
                    signal_s_list_map[sig] = signal_s_list_info;
                }
            }
            if(sig_info->arr_words > 1) {
                event_info->has_array_signal = 1;
            }                            
        }
    }
    if (event_info->neg_num) {
        for (unsigned idx = 0; idx < event_info->neg_num; idx += 1) {
            nex = ivl_event_neg(event_info->evt, idx);
            sig_info = pint_find_signal(nex);
            if(sig_info) {
              sig = sig_info->sig;
            }
            if(sig && sig_info->arr_words == 1) {
                if (signal_s_list_map.count(sig) > 0) {
                    signal_s_list_map[sig].neg_num++;
                    signal_s_list_map[sig].neg_buff +=
                        string_format("pint_thread_%d, (pint_thread)%u, ", thread->id, thread->id); 
                }
                else {
                    pint_static_event_list_table_num++;
                    struct pint_signal_s_list_info_s signal_s_list_info;
                    signal_s_list_info.neg_num++;
                    signal_s_list_info.neg_buff = string_format("pint_thread_%d, (pint_thread)%u, ", 
                                                                thread->id, thread->id);
                    signal_s_list_info.declare_buff =  string_format("extern pint_thread s_list_%s[];\n",
                                                                    sig_info->sig_name.c_str());
                    signal_s_list_info.define_buff = string_format("pint_thread s_list_%s[] = \n",
                                                                    sig_info->sig_name.c_str());
                    signal_s_list_map[sig] = signal_s_list_info;
                }
            }
            if(sig_info->arr_words > 1) {
                event_info->has_array_signal = 1;
            }                                                        
        }
    }
    if(event_info->has_array_signal || (event_info->any_num == 0 && 
          event_info->pos_num == 0  && event_info->neg_num == 0
          && event_info->wait_count > 0) ) {
        if (event_static_thread_buff.count(evt) > 0) {
            event_static_thread_buff[evt] += string_format(
                "pint_thread_%d, (pint_thread)%u, ", thread->id, thread->id);
        } else {
            pint_static_event_list_table_num++;
            event_static_thread_dec_buff +=
                string_format("extern pint_thread event_%d_static_thread_table[];\n",
                              event_info->event_id);
            event_static_thread_buff[evt] =
                string_format("pint_thread event_%d_static_thread_table[] = {"
                              "pint_thread_%d, (pint_thread)%u, ",
                              event_info->event_id, thread->id, thread->id);
        }                      
    }
}

void StmtGen::show_stmt_release(ivl_statement_t net) {
    unsigned nlhs = ivl_stmt_lvals(net);
    unsigned i;
    for (i = 0; i < nlhs; i++) {
        pint_show_release_lval(stmt_code, pint_force_lval[ivl_stmt_lval(net, i)]);
    }
}

void StmtGen::show_stmt_deassign(ivl_statement_t net) {
    unsigned nlhs = ivl_stmt_lvals(net);
    unsigned i;
    for (i = 0; i < nlhs; i++) {
        pint_show_deassign_lval(stmt_code, pint_force_lval[ivl_stmt_lval(net, i)], net);
    }
}
/*******************< End >*******************/

/*
 * A trigger statement is the "-> name;" syntax in Verilog, where a
 * trigger signal is sent to a named event. The trigger statement is
 * actually a very simple object.
 */
void StmtGen::show_stmt_trigger(ivl_statement_t net, unsigned ind) {
    unsigned cnt = ivl_stmt_nevent(net);
    unsigned idx;

    (void)ind;

    for (idx = 0; idx < cnt; idx += 1) {
        ivl_event_t event = ivl_stmt_events(net, idx);
        // fprintf(out, " %s", ivl_event_basename(event));

        pint_event_info_t event_info = pint_find_event(event);
        unsigned event_id = event_info->event_id;
        stmt_code += string_format("    pint_move_event_%u();\n", event_id);
    }

    /* The compiler should make exactly one target event, so if we
       find more or less, then print some error text. */
    if (cnt != 1) {
        // fprintf(out, " /* ERROR: Expect one target event, got %u */", cnt);
    }

    // fprintf(out, ";\n");
}

/*
 * The wait statement contains simply an array of events to wait on,
 * and a sub-statement to execute when an event triggers.
 */
void StmtGen::show_stmt_wait(ivl_statement_t net, unsigned ind) {
    unsigned idx;
    if (thread->to_lpm_flag) {
        return;
    }
    /* Emit a SystemVerilog wait fork. */
    if ((ivl_stmt_nevent(net) == 1) && (ivl_stmt_events(net, 0) == 0)) {
        assert(ivl_statement_type(ivl_stmt_sub_stmt(net)) == IVL_ST_NOOP);
        return;
    }

    if (ivl_stmt_nevent(net) >= 2) {
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net),
                               "more than one stmt event");
    }

    for (idx = 0; idx < ivl_stmt_nevent(net); idx += 1) {
        ivl_event_t evt = ivl_stmt_events(net, idx);

        if (evt == 0) {
            // fprintf(out, "%s/*ERROR*/", comma);
        } else {
            pint_event_info_t event_info = pint_find_event(evt);
            if (NULL != event_info) {
                if ((!pint_in_task)) {
                    if (!pint_find_process_event(evt)) {
                        event_info->wait_count++;
                        pint_add_process_event(evt);
                    }
                }
            }
            thread->pc++;

            if (pint_in_task) {
                stmt_code += string_format("    pint_wait_on_event(&pint_event_%d, p_thread);\n",
                                           event_info->event_id);
                stmt_code += string_format("    *pint_task_pc_x = %d;\n", thread->pc);
            } else {
                if (!pint_always_wait_one_event) {
                    stmt_code +=
                        string_format("    pint_wait_on_event(&pint_event_%d, pint_thread_%d);\n",
                                      event_info->event_id, thread->id);
                } else {
                    pint_collect_signal_s_list_info(event_info, thread, evt);
                }
                if (!pint_always_wait_one_event) {
                    stmt_code +=
                        string_format("    pint_thread_pc_x[%d] = %d;\n", thread->id, thread->pc);
                }
            }
            if (!pint_always_wait_one_event) {
                //  if in task, then can not be mult_asgn
                stmt_code += pint_proc_gen_exit_code(pint_in_task, -1);
            }
            if (!pint_always_wait_one_event) {
                stmt_code += string_format("PC_%d:\n    ;\n", thread->pc);
            }
            if (IVL_ST_NOOP == ivl_statement_type(ivl_stmt_sub_stmt(net))) {
                stmt_code += string_format("    ;\n");
            }
        }
    }

    StmtGen wait_gen(ivl_stmt_sub_stmt(net), thread, net, 0, ind + 4);
    AppendStmtCode(wait_gen);
}

void parse_string_format(string &str, int &data_num, ivl_statement_t net) {
    const char *p = str.c_str();
    data_num = 0;

    while (p[0] != 0) {
        if ((p[0] == '%') && (p[1] != '%') && (p[1] != 0)) {
            data_num++;
            do {
                p++;
            } while ((p[0] >= '0') && (p[0] < '9'));
            if ((p[0] == 't') || (p[0] == 'T')) {
                PINT_UNSUPPORTED_NOTE(ivl_stmt_file(net), ivl_stmt_lineno(net),
                                      "%%%c is not supported perfectly, we are fixing it now",
                                      p[0]);
            }
            p++;
        } else if ((p[0] == '%') && (p[1] == '%')) {
            p += 2;
        } else {
            p++;
        }
    }
}

void StmtGen::show_stmt_print(ivl_statement_t net) {
    bool is_write = !strcmp(ivl_stmt_name(net), "$write");
    bool is_monitor = !strcmp(ivl_stmt_name(net), "$monitor");
    bool is_strobe = !strcmp(ivl_stmt_name(net), "$strobe");
    bool is_fdisplay = !strcmp(ivl_stmt_name(net), "$fdisplay");
    bool is_fwrite = !strcmp(ivl_stmt_name(net), "$fwrite");
    bool is_printtimescale = !strcmp(ivl_stmt_name(net), "$printtimescale");
    // printf("enter show_stmt_print")

    int idx;

    vector<string> paras_tmp_name;
    vector<bool> is_para_immediate;
    vector<bool> is_valid;
    string monitor_expr_res_dec;
    string monitor_expr_res_ope;
    string print_format_string;
    int need_format_data_num = 0;
    int para_num = ivl_stmt_parm_count(net);
    paras_tmp_name.resize(para_num);
    is_para_immediate.resize(para_num);
    is_valid.resize(para_num);
    bool first_flag = true;
    static const char *units_names[] = {"s", "ms", "us", "ns", "ps", "fs"};

    if (is_printtimescale) {
        string dut_name;
        int prec = 0;
        int time_units = 0;
        unsigned prec_scale = 1;
        unsigned units_name_prec_idx = 0;
        unsigned time_scale = 1;
        unsigned units_name_time_idx = 0;
        vector<string> scope_name_vec;

        if (para_num == 0) {
            dut_name = ivl_scope_basename(ivl_stmt_call(net));
            prec = ivl_scope_time_precision(ivl_stmt_call(net));
            time_units = ivl_scope_time_units(ivl_stmt_call(net));
        } else if (para_num == 1) {
            ivl_expr_t para = ivl_stmt_parm(net, 0);
            if (IVL_EX_SCOPE == ivl_expr_type(para)) {
                scope_name_vec.push_back(ivl_scope_basename(ivl_expr_scope(para)));
                ivl_scope_t mod_scope = ivl_scope_parent(ivl_expr_scope(para));
                while (mod_scope != 0) {
                    scope_name_vec.push_back(ivl_scope_basename(mod_scope));
                    mod_scope = ivl_scope_parent(mod_scope);
                }
                dut_name = *scope_name_vec.rbegin();
                for (auto iter = scope_name_vec.rbegin() + 1; iter != scope_name_vec.rend();
                     iter++) {
                    dut_name += ".";
                    dut_name += *iter;
                }
                prec = ivl_scope_time_precision(ivl_expr_scope(para));
                time_units = ivl_scope_time_units(ivl_expr_scope(para));
            } else {
                PINT_UNSUPPORTED_NOTE(ivl_expr_file(para), ivl_expr_lineno(para),
                                      "$printtimescale para is not module.");
                dut_name = ivl_scope_basename(ivl_stmt_call(net));
                prec = ivl_scope_time_precision(ivl_stmt_call(net));
                time_units = ivl_scope_time_units(ivl_stmt_call(net));
            }
        } else {
            printf("Warning! More than one param in printtimescale.");
        }

        assert(prec >= -15);
        while (prec < 0) {
            units_name_prec_idx += 1;
            prec += 3;
        }
        while (prec > 0) {
            prec_scale *= 10;
            prec -= 1;
        }

        assert(time_units >= -15);
        while (time_units < 0) {
            units_name_time_idx += 1;
            time_units += 3;
        }
        while (time_units > 0) {
            time_scale *= 10;
            time_units -= 1;
        }

        stmt_code += "    pint_printf(\" ";
        stmt_code += string_format("Time scale of (%s) is %u%s / %u%s\\n", dut_name.c_str(),
                                   time_scale, units_names[units_name_time_idx], prec_scale,
                                   units_names[units_name_prec_idx]);
        stmt_code += "\");\n";

        // printf("Time scale of (%s) is %u%s / %u%s\n", dut_name.c_str(),
        //           time_scale, units_names[units_name_time_idx],
        //           prec_scale, units_names[units_name_prec_idx]);
        return;
    }

    // parse format string to print
    print_format_string = "\"";
    for (idx = 0; idx < para_num; idx += 1) {
        ivl_expr_t ex = ivl_stmt_parm(net, idx);
        if (ex == NULL) {
            print_format_string += " ";
            is_valid[idx] = false;
        } else if (ivl_expr_type(ex) == IVL_EX_STRING) {
            string content = ivl_expr_string(ex);
            size_t pos = content.find("%m");
            if (pos != string::npos) {
                if (NULL != thread->scope) {
                    content = content.substr(0, pos) + ivl_scope_name(thread->scope) +
                              content.substr(pos + 2, -1);
                } else {
                    content = content.substr(0, pos) + "null" + content.substr(pos + 2, -1);
                }
            }

            if (need_format_data_num == 0) {
                if (content != "\\000") {
                    print_format_string += content;
                }
                is_valid[idx] = false;
                parse_string_format(content, need_format_data_num, net);
            } else {
                need_format_data_num--;
                is_valid[idx] = true;
                paras_tmp_name[idx] = "\"" + content + "\"";
                is_para_immediate[idx] = false;
            }
        } else {
            is_valid[idx] = true;

            if (need_format_data_num) {
                need_format_data_num--;
            } else {
                if (!((is_fdisplay || is_fwrite) && (idx == 0))) {
                    if (!first_flag) {
                        print_format_string += ",";
                        first_flag = false;
                    }
                    switch (display_type) {
                    case 1:
                        print_format_string += "%d ";
                        break;
                    case 2:
                        print_format_string += "%h ";
                        break;
                    case 3:
                        print_format_string += "%o ";
                        break;
                    case 4:
                        print_format_string += "%b ";
                        break;
                    default:
                        print_format_string += "%d ";
                        break;
                    }
                }
            }

            if (is_monitor && pint_simu_info.monitor_exprs_name.count(ex)) {
                paras_tmp_name[idx] = pint_simu_info.monitor_exprs_name[ex] + "[0]";
                is_para_immediate[idx] = false;
            } else {
                // nxz
                ExpressionConverter converter(ex, net, 0);
                if (is_monitor || is_strobe) {
                    converter.GetPreparation(&monitor_expr_res_ope, &monitor_expr_res_dec);
                } else {
                    converter.GetPreparation(&stmt_code, &decl_code);
                }
                paras_tmp_name[idx] = converter.GetResultNameString();
                is_para_immediate[idx] = converter.IsImmediateNum();
            }
        }
    }

    if (need_format_data_num != 0) {
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net),
                               "missing argument for format specification!");
    }

    print_format_string += "\"";

    string sfunc_buff;
    idx = 0;
    if (is_write) {
        sfunc_buff += "    pint_simu_vpi(PINT_WRITE, ";
    } else if (is_fdisplay || is_fwrite) {
        string t, ch;
        if (is_fwrite) {
            t = "PINT_FWRITE";
        } else {
            t = "PINT_FDISPLAY";
        }
        if (is_para_immediate[0]) {
            ch = "";
        } else {
            ch = "[0]";
        }
        sfunc_buff += string_format("    pint_simu_vpi(%s, %s%s, ", t.c_str(),
                                    paras_tmp_name[0].c_str(), ch.c_str());
        idx = 1;
    } else {
        sfunc_buff += "    pint_simu_vpi(PINT_DISPLAY, ";
    }
    sfunc_buff += print_format_string;

    bool sys_time_flag = false;
    for (; idx < para_num; idx += 1) {
        ivl_expr_t expr = ivl_stmt_parm(net, idx);
        if (is_valid[idx]) {
            if (ivl_expr_type(expr) == IVL_EX_SIGNAL) {
                ivl_expr_t word = ivl_expr_oper1(expr);
                ivl_signal_t sig = ivl_expr_signal(expr);
                unsigned width = ivl_signal_width(sig);
                int signed_flag = ivl_signal_signed(sig);
                if (width <= 4) {
                    if (word != 0) {
                        if (ivl_expr_type(word) != IVL_EX_NUMBER) {
                        }
                        sfunc_buff += string_format(", %d, PINT_SIGNAL, %s, &", width,
                                                    signed_flag ? "PINT_SIGNED" : "PINT_UNSIGNED");
                        sfunc_buff += paras_tmp_name[idx];
                    } else {
                        sfunc_buff += string_format(", %d, PINT_SIGNAL, %s, &%s", width,
                                                    signed_flag ? "PINT_SIGNED" : "PINT_UNSIGNED",
                                                    paras_tmp_name[idx].c_str());
                    }
                } else {
                    if (word != 0) {
                        if (ivl_expr_type(word) == IVL_EX_NUMBER ||
                            ivl_expr_type(word) == IVL_EX_SIGNAL) {
                        } else {
                        }
                        sfunc_buff += string_format(", %d, PINT_SIGNAL, %s, ", width,
                                                    signed_flag ? "PINT_SIGNED" : "PINT_UNSIGNED");
                        sfunc_buff += paras_tmp_name[idx];
                    } else {
                        sfunc_buff += string_format(", %d, PINT_SIGNAL, %s, %s", width,
                                                    signed_flag ? "PINT_SIGNED" : "PINT_UNSIGNED",
                                                    paras_tmp_name[idx].c_str());
                    }
                }
            } else if (IVL_EX_STRING == ivl_expr_type(expr)) {
                // sometimes, signal is printed as %s. So even string need to match signal's
                // width-type-sign-data format
                if (idx != 0) {
                    sfunc_buff += ",0,PINT_STRING,0,";
                }
                string str = ivl_expr_string(expr);
                sfunc_buff += paras_tmp_name[idx];
            } else if (IVL_EX_SFUNC == ivl_expr_type(expr)) {
                if (!strcmp(ivl_expr_name(expr), "$time") ||
                    !strcmp(ivl_expr_name(expr), "$realtime")) {
                    if (!strcmp(ivl_expr_name(expr), "$realtime")) {
                        PINT_UNSUPPORTED_NOTE(ivl_expr_file(expr), ivl_expr_lineno(expr),
                                              "$realtime is not implemented yet.");
                    }
                    int tu_scope = ivl_scope_time_units(ivl_stmt_call(net));
                    unsigned tu_mul = 1;
                    unsigned tu_mul_2 = 1;
                    unsigned int time_unit_diff = tu_scope - pint_design_time_precision;
                    if (time_unit_diff <= 9 && time_unit_diff > 0) {
                        for (int i = time_unit_diff; i > 0; i--) {
                            tu_mul *= 10;
                        }
                    } else if (time_unit_diff > 9) {
                        tu_mul = 1e9;
                        for (int i = time_unit_diff - 9; i > 0; i--) {
                            tu_mul_2 *= 10;
                        }
                    }

                    if (time_unit_diff <= 9 && time_unit_diff > 0) {
                        sfunc_buff =
                            string_format(
                                "\tunsigned simuT_tmp[2] = {((unsigned*)&T)[0], "
                                "((unsigned*)&T)[1]};\n"
                                "\tsimu_time_t simuT = %lu * simuT_tmp[1] + simuT_tmp[0]/%u + "
                                "(simuT_tmp[1] * %u + simuT_tmp[0]%%%u)/%u;\n",
                                0x100000000L / tu_mul, tu_mul, 0x100000000L % tu_mul, tu_mul,
                                tu_mul) +
                            sfunc_buff;
                    } else if (time_unit_diff > 9) {
                        sfunc_buff =
                            string_format("\tunsigned simuT_tmp_2[2] = {((unsigned*)&simuT)[0], "
                                          "((unsigned*)&simuT)[1]};\n"
                                          "\tsimuT = %lu * simuT_tmp_2[1] + simuT_tmp_2[0]/%u + "
                                          "(simuT_tmp_2[1] * %u + simuT_tmp_2[0]%%%u)/%u;\n",
                                          0x100000000L / tu_mul_2, tu_mul_2,
                                          0x100000000L % tu_mul_2, tu_mul_2, tu_mul_2) +
                            sfunc_buff;
                        sfunc_buff =
                            string_format(
                                "\tunsigned simuT_tmp[2] = {((unsigned*)&T)[0], "
                                "((unsigned*)&T)[1]};\n"
                                "\tsimu_time_t simuT = %lu * simuT_tmp[1] + simuT_tmp[0]/%u + "
                                "(simuT_tmp[1] * %u + simuT_tmp[0]%%%u)/%u;\n",
                                0x100000000L / tu_mul, tu_mul, 0x100000000L % tu_mul, tu_mul,
                                tu_mul) +
                            sfunc_buff;
                    }

                    if (time_unit_diff > 0) {
                        sfunc_buff += ", 64, PINT_VALUE, PINT_UNSIGNED, &simuT";
                    } else {
                        sfunc_buff += ", 64, PINT_VALUE, PINT_UNSIGNED, &T";
                    }
                    sys_time_flag = true;
                }
            } else {
                unsigned width = ivl_expr_width(expr);
                int signed_flag = ivl_expr_signed(expr);
                if (is_para_immediate[idx]) {
                    if (width > 32)
                        width = 32;
                    sfunc_buff += string_format(", %d, PINT_VALUE, %s, &%s", width,
                                                signed_flag ? "PINT_SIGNED" : "PINT_UNSIGNED",
                                                paras_tmp_name[idx].c_str());
                } else {
                    sfunc_buff += string_format(", %d, PINT_SIGNAL, %s, ", width,
                                                signed_flag ? "PINT_SIGNED" : "PINT_UNSIGNED");
                    if (width <= 4) {
                        sfunc_buff += string_format("&");
                    }
                    sfunc_buff += paras_tmp_name[idx];
                }
            }
        }
    }
    sfunc_buff += ");\n";
    if (sys_time_flag) {
        sfunc_buff = "{\n" + sfunc_buff + "}\n";
    }

    if (is_monitor) {
        unsigned monitor_id = pint_simu_info.monitor_stat_id[net];
        stmt_code += string_format("    monitor_func_ptr = &monitor_function%u;\n", monitor_id);
        inc_buff += string_format("void monitor_function%u(void);\n", monitor_id);

        monitor_func_buff += string_format("void monitor_function%u(){\n", monitor_id);
        monitor_func_buff += string_format(
            "    static unsigned char monitor_function%u_force_print_flag = 1;\n", monitor_id);
        monitor_func_buff += string_format(
            "    unsigned char monitor_expr_change = monitor_function%u_force_print_flag;\n",
            monitor_id, monitor_id);
        monitor_func_buff += string_format(
            "    if (monitor%d_update_flag || monitor_function%u_force_print_flag){\n", monitor_id,
            monitor_id);
        monitor_func_buff +=
            string_format("        unsigned char case_equality_result = 0;\n", monitor_id);

        for (idx = 0; idx < para_num; idx += 1) {
            ivl_expr_t expr = ivl_stmt_parm(net, idx);
            if (pint_simu_info.monitor_exprs_name.count(expr) == 0) {
                continue;
            }
            string monitor_expr_result = pint_simu_info.monitor_exprs_name[expr];
            unsigned expr_width = ivl_expr_width(expr);
            if (expr_width <= 4) {
                monitor_func_buff +=
                    string_format("if (%s[0] != %s[1]){\n", monitor_expr_result.c_str(),
                                  monitor_expr_result.c_str());
                monitor_func_buff += "monitor_expr_change = 1;\n";
                monitor_func_buff +=
                    string_format("%s[1] = %s[0];\n}\n", monitor_expr_result.c_str(),
                                  monitor_expr_result.c_str());
            } else {
                monitor_func_buff += string_format(
                    "pint_case_equality_int(&case_equality_result, %s[0], %s[1], %u);\n",
                    monitor_expr_result.c_str(), monitor_expr_result.c_str(), expr_width);
                monitor_func_buff += string_format("if (!case_equality_result){\n");
                monitor_func_buff += "monitor_expr_change = 1;\n";
                monitor_func_buff += string_format("pint_copy_int_int_same(%s[0], %s[1], %u);\n}\n",
                                                   monitor_expr_result.c_str(),
                                                   monitor_expr_result.c_str(), expr_width);
            }
        }
        monitor_func_buff += "    }\n";
        monitor_func_buff += string_format("if (monitor_expr_change){\n");
        monitor_func_buff += monitor_expr_res_dec;
        monitor_func_buff += monitor_expr_res_ope;
        monitor_func_buff += sfunc_buff;
        monitor_func_buff += "    }\n";
        monitor_func_buff +=
            string_format("    monitor_function%u_force_print_flag = 0;\n", monitor_id);
        monitor_func_buff += string_format("    monitor%d_update_flag = 0;\n", monitor_id);
        monitor_func_buff += "}\n";
    } else if (is_strobe) {
        stmt_code +=
            string_format("    if (!pint_enqueue_flag_set_any(pint_monitor_enqueue_flag, %u)){\n",
                          pint_strobe_num);
        stmt_code += string_format("        pint_enqueue_thread(&strobe_function%u, MONITOR_Q);\n",
                                   pint_strobe_num);
        stmt_code += string_format("    }\n", pint_strobe_num);
        strobe_func_buff += string_format("void strobe_function%u(){\n", pint_strobe_num);
        thread_declare_buff += string_format("void strobe_function%u();\n", pint_strobe_num);
        strobe_func_buff += string_format(
            "    pint_enqueue_flag_clear_any(pint_monitor_enqueue_flag, %u);\n", pint_strobe_num);
        strobe_func_buff += monitor_expr_res_dec;
        strobe_func_buff += monitor_expr_res_ope;
        strobe_func_buff += sfunc_buff;
        strobe_func_buff += "}\n";
        pint_strobe_num++;
    } else {
        stmt_code += sfunc_buff;
    }
    return;
}

void StmtGen::show_stmt_nb_assign_info(ivl_lval_t lval, unsigned lval_idx) {
    string dst_name;
    string src_name;
    string nb_elist_name;
    string nb_llist_name;
    string p_list_name;
    string nb_sig;
    string nb_sig_declare;
    string ty;
    string x;
    unsigned size;
    unsigned array_size;
    int update_size;
    ivl_signal_t sig = ivl_lval_sig(lval);
    pint_sig_info_t sig_info = pint_find_signal(sig);
    assert(sig_info);

    size = sig_info->width;
    if (pint_signal_has_sen_lpm(sig_info))
        nb_llist_name = "l_list_" + sig_info->sig_name;
    else
        nb_llist_name = "NULL";
    if (pint_signal_has_sen_evt(sig_info))
        nb_elist_name = "e_list_" + sig_info->sig_name;
    else
        nb_elist_name = "NULL";

    if (sig_info->sens_process_list.size()) {
        p_list_name = "p_list_" + sig_info->sig_name;
    } else {
        p_list_name = "NULL";
    }

    if (sig_info->arr_words > 1) {
        // array
        dst_name = "(unsigned char*)" + sig_info->sig_name;
        src_name = "(unsigned char*)" + sig_info->sig_name + "_nb";
        array_size = ivl_signal_array_count(sig);
        // save info
        pint_nbassign_save.type = 1;
        pint_nbassign_save.size = size;
        pint_nbassign_save.src = src_name;
        pint_nbassign_save.dst = dst_name;
        pint_nbassign_save.offset = 0;
        pint_nbassign_save.updt_size = 0;
        pint_nbassign_save.array_size = array_size;
        pint_nbassign_save.l_list = nb_llist_name;
        pint_nbassign_save.e_list = nb_elist_name;
        pint_nbassign_save.p_list = p_list_name;

        key_val = string_format("%s_%d", sig_info->sig_name.c_str(), thread->id);
    } else {
        // signal
        if (size > 4) {
            dst_name = "(unsigned char*)" + sig_info->sig_name;
            src_name = string_format("(unsigned char*)%s_nb", sig_info->sig_name.c_str());
        } else {
            dst_name = "&" + sig_info->sig_name;
            src_name = string_format("&%s_nb", sig_info->sig_name.c_str());
        }
        update_size = ivl_lval_width(lval);
        // save info
        pint_nbassign_save.type = 0;
        pint_nbassign_save.size = size;
        pint_nbassign_save.src = src_name;
        pint_nbassign_save.dst = dst_name;
        pint_nbassign_save.updt_size = update_size;
        pint_nbassign_save.l_list = nb_llist_name;
        pint_nbassign_save.e_list = nb_elist_name;
        pint_nbassign_save.p_list = p_list_name;

        key_val = sig_info->sig_name;
    }
    if (sig_info->is_dump_var) {
        pint_nbassign_save.vcd_symbol_ptr = sig_info->sig_name + "_symbol";
        pint_nbassign_save.vcd_symbol = sig_info->dump_symbol;

    } else {
        pint_nbassign_save.vcd_symbol_ptr = "NULL";
        pint_nbassign_save.vcd_symbol = "NULL";
    }

    if (pint_pli_mode && sig_info->is_interface && (IVL_SIP_OUTPUT == sig_info->port_type)) {
        pint_nbassign_save.is_output_interface = 1;
        pint_nbassign_save.output_port_id = sig_info->output_port_id;
    } else {
        pint_nbassign_save.is_output_interface = 0;
    }

    // save hash map
    std::stringstream offset_s;
    offset_s << pint_nbassign_save.offset;
    map<string, pint_nbassign_info>::iterator it;
    it = nbassign_map.find(key_val);
    key_val_array[lval_idx] = key_val;
    if (it == nbassign_map.end()) {
        pint_show_signal_define(nb_sig, nb_sig_declare, sig_info, 1);
        // pint_signal_type_init(sig_info, &nb_sig, &nb_sig_declare, true);
        pint_nbassign_save.nb_sig = nb_sig;
        pint_nbassign_save.nb_sig_declare = nb_sig_declare;
        if (sig_info->arr_words > 1) {
            pint_nbassign_save.sig_nb_id = pint_nbassign_array_num++;
        } else {
            pint_nbassign_save.sig_nb_id = pint_nbassign_num++;
        }
        nbassign_map[key_val] = pint_nbassign_save;
    }
}

unsigned int pint_get_lval_idx_width(ivl_expr_t net) {
    assert(net);
    unsigned int width = 0;
    ivl_expr_t oper1 = ivl_expr_oper1(net);
    const ivl_expr_type_t code = ivl_expr_type(oper1);
    if (code == IVL_EX_SIGNAL) {
        ivl_signal_t sig = ivl_expr_signal(oper1);
        pint_sig_info_t sig_info = pint_find_signal(sig);
        width = sig_info->width;
    } else {
        std::cout << "ival idx expr type error!" << std::endl;
    }
    return width;
}

void StmtGen::pint_nb_assign_enqueue_process(unsigned enqueue_flag, string nb_id,
                                             string array_idx) {
    if (enqueue_flag == 1) {
        stmt_code += string_format("        pint_enqueue_nb_thread_array_by_idx(%s, %s);\n",
                                   nb_id.c_str(), array_idx.c_str());
    } else {
        stmt_code += string_format("        pint_enqueue_nb_thread_by_idx(&pint_nb_list[%s]);\n", nb_id.c_str());
    }
}

// subarray base is expr
void StmtGen::pint_op_base_show(ivl_expr_t oper_po, ExpressionConverter ex_partoff,
                                string partoff_result) {
    if (!ex_partoff.IsImmediateNum()) {
        stmt_code += code_of_expr_value(oper_po, partoff_result.c_str(), ex_partoff.IsResultNxz());
    } else {
        stmt_code += partoff_result;
    }
}

string StmtGen::gen_signal_value_var(bool isImmediateNum, bool sig_nxz, ivl_expr_t expr,
                                     const string *sig_name) {
    static unsigned count = 0;
    string value_tmp_name;
    if (isImmediateNum) {
        value_tmp_name = *sig_name;
    } else {
        value_tmp_name = string_format("%s_value%u", sig_name->c_str(), count++);
        decl_code += "    int " + value_tmp_name + ";\n";
        stmt_code += "    " + value_tmp_name + " = ";
        stmt_code += code_of_expr_value(expr, sig_name->c_str(), sig_nxz) + ";\n";
    }

    return value_tmp_name;
}

typedef struct {
    string signal_bits_addr;
    string idx;
    unsigned sig_width;
    // nxz
    bool sig_nxz;
    bool assign_full; // pint_asgn_is_full()
    string assign_base;
    unsigned assign_width;
    unsigned rval_offset;
    string signal_nb_bits_addr;
} assign_lval_info;

// nxz
static void get_expr_value(ivl_expr_t expr, ivl_statement_t stmt, string &prepare_buff,
                           string &result) {
    if (!expr) {
        result = "";
    } else {
        // nxz
        ExpressionConverter convt(expr, stmt, 0, NUM_EXPRE_DIGIT);
        if (convt.IsImmediateNum()) {
            result = convt.GetResultNameString();
        } else {
            string declare;
            string operation;
            convt.GetPreparation(&operation, &declare);

            result = "tmp_value";
            result += string_format("%d", lval_idx_num);
            declare += string_format("int %s;\n", result.c_str());

            operation += string_format(
                "%s = %s;\n", result.c_str(),
                code_of_expr_value(expr, convt.GetResultName(), convt.IsResultNxz()).c_str());
            lval_idx_num++;

            prepare_buff += declare + "{\n" + operation + "}\n";
        }
    }
}

static assign_lval_info gen_lval_info(ivl_lval_t lval, ivl_statement_t stmt, bool assign_flag,
                                      string delay, int &rval_off, string &prep_code,
                                      string &dec_code) {
    assign_lval_info assign_l_info;
    ivl_signal_t sig = ivl_lval_sig(lval);
    // nxz
    bool sig_nxz = pint_proc_sig_nxz_get(stmt, sig);
    pint_sig_info_t sig_info = pint_find_signal(sig);
    unsigned arr_words = sig_info->arr_words;
    ivl_expr_t idx = ivl_lval_idx(lval);
    unsigned sig_width = sig_info->width;
    int set_nb_addr = (assign_flag && sig_info->has_nb) || ((!assign_flag) && (delay != ""));

    // local sig as lval, should be declared as global
    if (sig_info->is_local) {
        pint_revise_sig_info_to_global(sig);
    }

    // gen signal_bits_addr
    assign_l_info.signal_bits_addr = sig_width > 4 ? "" : "&";
    if (set_nb_addr) {
        assign_l_info.signal_nb_bits_addr = sig_width > 4 ? "" : "&";
    }
    if (arr_words == 1) {
        assign_l_info.idx = "";
        assign_l_info.signal_bits_addr += pint_proc_get_sig_name(sig, "", assign_flag);
        if (set_nb_addr) {
            assign_l_info.signal_nb_bits_addr += pint_proc_get_sig_name(sig, "", 0);
        }
    } else {
        if (!idx) { //  sig_info is arr, but idx = NULL
            assign_l_info.idx = string_format("%u", pint_get_arr_idx_from_sig(sig));
        } else {
            // nxz
            get_expr_value(idx, stmt, prep_code, assign_l_info.idx);
        }
        const char *si = assign_l_info.idx.c_str();
        assign_l_info.signal_bits_addr += pint_proc_get_sig_name(sig, si, assign_flag);
        if (set_nb_addr) {
            assign_l_info.signal_nb_bits_addr += pint_proc_get_sig_name(sig, si, 0);
        }
    }

    if ((!assign_flag) && (delay != "")) {
        string nb_delay_key = assign_l_info.signal_nb_bits_addr + " and " + delay;
        const char *type = (sig_width <= 4) ? "unsigned char*" : "unsigned*";
        int mem_size = (sig_width + 31) / 32 * 4 * 2;
        string tmp_mem_name;

        if (nb_delay_same_info.count(nb_delay_key) == 0) {
            tmp_mem_name = string_format("tmp_nb_mem%d", lval_idx_num++);
            nb_delay_same_info[nb_delay_key] = tmp_mem_name;
            dec_code += string_format("%s %s = NULL;\n", type, tmp_mem_name.c_str());
        } else {
            tmp_mem_name = nb_delay_same_info[nb_delay_key];
        }
        prep_code += string_format(
            "if (%s == NULL) {%s = (%s)pint_mem_alloc(%d + sizeof(pint_future_nbassign), CROSS_T);"
            "((pint_future_nbassign_t)((char*)%s + %d))->enqueue_flag = 0;\n}",
            tmp_mem_name.c_str(), tmp_mem_name.c_str(), type, mem_size, tmp_mem_name.c_str(),
            mem_size);
        assign_l_info.signal_bits_addr = tmp_mem_name;
    }

    // gen sig_width
    assign_l_info.sig_width = sig_width;

    // nxz
    assign_l_info.sig_nxz = sig_nxz;

    // gen assign_full
    assign_l_info.assign_full = pint_asgn_is_full(lval);

    // gen assign_base
    ivl_expr_t part_off = ivl_lval_part_off(lval);
    if (part_off) {
        string par_off_name;
        // nxz
        get_expr_value(part_off, stmt, prep_code, par_off_name);
        assign_l_info.assign_base = par_off_name;
    } else {
        assign_l_info.assign_base = "0";
    }

    // gen assign_width
    assign_l_info.assign_width = ivl_lval_width(lval);

    // gen sig_width
    assign_l_info.rval_offset = rval_off;
    rval_off += ivl_lval_width(lval);

    return assign_l_info;
}

typedef struct {
    string signal_bits_addr;
    unsigned sig_width;
    // nxz
    bool sig_nxz;
} assign_rval_info;

// nxz
static assign_rval_info gen_rval_info(ivl_expr_t rval, ivl_statement_t stmt, string &prep_code) {
    assign_rval_info rval_info;
    unsigned width = ivl_expr_width(rval);
    // nxz
    ExpressionConverter convt(rval, stmt, 0, NUM_EXPRE_SIGNAL);
    bool sig_nxz = convt.IsResultNxz();
    string tmp_opercode;
    string tmp_deccode;
    convt.GetPreparation(&tmp_opercode, &tmp_deccode);
    prep_code += tmp_deccode + tmp_opercode;

    rval_info.signal_bits_addr = width > 4 ? "" : "&";
    rval_info.signal_bits_addr += convt.GetResultNameString();
    rval_info.sig_width = width;
    rval_info.sig_nxz = sig_nxz;
    return rval_info;
}
unsigned use_change_flag = 0;
static string gen_assign_copy_code(const assign_lval_info &lval_info,
                                   const assign_rval_info &rval_info, unsigned care_change_flag,
                                   unsigned copy_to_nb, unsigned assign_flag, unsigned has_evt) {
    string clear_code;
    string copy_code;
    unsigned l_sig_width = lval_info.sig_width;
    unsigned l_assign_width = lval_info.assign_width;
    string l_assign_offset = lval_info.assign_base;
    unsigned r_sig_width = rval_info.sig_width;
    unsigned rval_off = lval_info.rval_offset;
    string lval_addr = lval_info.signal_bits_addr;
    string rval_addr = rval_info.signal_bits_addr;
    string lval_nb_addr = lval_info.signal_nb_bits_addr;

    bool l_assign_full = lval_info.assign_full;
    bool copy_low_noedge = l_assign_full ? rval_info.sig_nxz : false;
    bool copy_low = care_change_flag ? false : copy_low_noedge;
    bool clear_high_noedge =
        l_assign_full ? (!lval_info.sig_nxz && rval_info.sig_nxz) : false; // lval: false -> true
    bool clear_high = care_change_flag ? false : clear_high_noedge;
    unsigned l_sig_cnt = (l_sig_width + 31) / 32;

    unsigned lval_skip = (lval_addr.c_str()[0] == '&') ? 1 : 0;
    unsigned rval_skip = (rval_addr.c_str()[0] == '&') ? 1 : 0;
    string lval_v = (lval_addr.c_str()[0] == '&') ? "" : "*";
    string rval_v = (rval_addr.c_str()[0] == '&') ? "" : "*";

    if (PINT_COPY_VALID_ONLY && clear_high) {
        if (l_sig_width <= 4) {
            clear_code =
                string_format("%s%s &= 0x0f;\n", lval_v.c_str(), lval_addr.c_str() + lval_skip);
            if (copy_to_nb) {
                clear_code += string_format("%s%s &= 0x0f;\n", lval_v.c_str(),
                                            lval_nb_addr.c_str() + lval_skip);
            }
        } else {
            if (l_sig_width <= 32) {
                clear_code = string_format("((unsigned*)%s)[1] = 0;\n", lval_addr.c_str());
                if (copy_to_nb) {
                    clear_code += string_format("((unsigned*)%s)[1] = 0;\n", lval_nb_addr.c_str());
                }
            } else {
                clear_code =
                    string_format("for (int tmp_i = %u; tmp_i < %u; tmp_i++)"
                                  "{((unsigned*)%s)[tmp_i] = 0;}\n",
                                  l_sig_cnt, l_sig_cnt * 2, lval_addr.c_str(), rval_addr.c_str());
                if (copy_to_nb) {
                    clear_code += string_format("for (int tmp_i = %u; tmp_i < %u; tmp_i++)"
                                                "{((unsigned*)%s)[tmp_i] = 0;}\n",
                                                l_sig_cnt, l_sig_cnt * 2, lval_nb_addr.c_str(),
                                                rval_addr.c_str());
                }
            }
        }
    }

    if ((l_assign_offset == "0") && (rval_off == 0) && (l_sig_width == r_sig_width) &&
        (l_assign_width == l_sig_width) && (!care_change_flag)) {
        if (l_sig_width <= 4) {
            copy_code = string_format("%s%s = %s%s;\n", lval_v.c_str(), lval_addr.c_str()+lval_skip, rval_v.c_str(), rval_addr.c_str()+rval_skip);
            if (copy_to_nb) {
                copy_code +=
                    string_format("%s%s = %s%s;\n",  lval_v.c_str(), lval_nb_addr.c_str()+lval_skip, rval_v.c_str(), rval_addr.c_str()+rval_skip);
            }
            copy_code = clear_code + copy_code;
            return copy_code;
        } else {
            unsigned copy_cnt = l_sig_cnt;

            if (!PINT_COPY_VALID_ONLY || !copy_low) {
                copy_cnt *= 2;
            }
            if (l_sig_width <= 32) {
                copy_code = string_format("((unsigned*)%s)[0] = ((unsigned*)%s)[0];\n", lval_addr.c_str(), rval_addr.c_str());
                if (!PINT_COPY_VALID_ONLY || !copy_low) {
                    copy_code += string_format("((unsigned*)%s)[1] = ((unsigned*)%s)[1];\n", lval_addr.c_str(), rval_addr.c_str());
                }
                if (copy_to_nb) {
                    copy_code += string_format("((unsigned*)%s)[0] = ((unsigned*)%s)[0];\n", lval_nb_addr.c_str(), rval_addr.c_str());
                    if (!PINT_COPY_VALID_ONLY || !copy_low) {
                        copy_code += string_format("((unsigned*)%s)[1] = ((unsigned*)%s)[1];\n", lval_nb_addr.c_str(), rval_addr.c_str());
                    }
                }
            } else {
                copy_code = string_format("for (int tmp_i = 0; tmp_i < %u; tmp_i++)"
                                        "{((unsigned*)%s)[tmp_i] = ((unsigned*)%s)[tmp_i];}\n",
                                        copy_cnt, lval_addr.c_str(), rval_addr.c_str());
                if (copy_to_nb) {
                    copy_code += string_format("for (int tmp_i = 0; tmp_i < %u; tmp_i++)"
                                            "{((unsigned*)%s)[tmp_i] = ((unsigned*)%s)[tmp_i];}\n",
                                            copy_cnt, lval_nb_addr.c_str(), rval_addr.c_str());
                }
            }
            copy_code = clear_code + copy_code;
            return copy_code;
        }
    }

    // default
    if (copy_code == "") {
        if ((l_assign_offset == "0") && (rval_off == 0) && (l_sig_width == r_sig_width) && (l_assign_width == l_sig_width)) {
            use_change_flag = 0;
            if (care_change_flag) { 
                if (l_sig_width <= 4) {
                    copy_code = string_format("\tif(%s%s != %s%s) {\n", lval_v.c_str(), (lval_info.signal_bits_addr.c_str())+lval_skip, rval_v.c_str(), rval_info.signal_bits_addr.c_str()+rval_skip);
                    if (has_evt && assign_flag) {
                        copy_code += string_format("\t\tchange_flag = edge_1bit((char)(%s%s&0x11), (char)(%s%s&0x11));\n", lval_v.c_str(), lval_info.signal_bits_addr.c_str()+lval_skip,  rval_v.c_str(), rval_info.signal_bits_addr.c_str()+rval_skip);
                    }
                    copy_code += string_format("\t\t%s%s = %s%s;\n", lval_v.c_str(), lval_info.signal_bits_addr.c_str()+lval_skip, rval_v.c_str(), rval_info.signal_bits_addr.c_str()+rval_skip);
                    
                    if (copy_to_nb) {
                        copy_code += string_format("\t\t%s%s = %s%s;\n", lval_v.c_str(), lval_nb_addr.c_str()+lval_skip, rval_v.c_str(), rval_info.signal_bits_addr.c_str()+rval_skip);
                    }
                } else if(l_sig_width <= 32){
                    if (copy_low) {
                        copy_code = string_format("\tif((%s[0] != %s[0])) {\n", lval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str());
                        if (has_evt && assign_flag) {
                            copy_code += string_format("\t\tchange_flag = edge_1bit((char)(%s[0]&0x1), (char)(%s[0]&0x1));\n", lval_info.signal_bits_addr.c_str(),  rval_info.signal_bits_addr.c_str());
                        }
                        copy_code += string_format("\t\t%s[0] = %s[0];\n", lval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str());
                        if (copy_to_nb) {
                            copy_code += string_format("\t\t%s[0] = %s[0];\n", lval_nb_addr.c_str(), rval_info.signal_bits_addr.c_str());
                        }
                    } else {
                        copy_code = string_format("\tif((%s[0] != %s[0]) || (%s[1] != %s[1])) {\n", lval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str(), lval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str());
                        if (has_evt && assign_flag) {
                            copy_code += string_format("\t\tchange_flag = edge_1bit((char)(%s[0]&0x1) | (char)((%s[1]&0x1)<<4), (char)(%s[0]&0x1)|(char)((%s[1]&0x1)<<4));\n", lval_info.signal_bits_addr.c_str(),  lval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str());
                        }
                        copy_code += string_format("\t\t%s[0] = %s[0]; %s[1] = %s[1];\n", lval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str(), lval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str());                 
                        if (copy_to_nb) {
                            copy_code += string_format("\t\t%s[0] = %s[0]; %s[1] = %s[1];\n", lval_nb_addr.c_str(), rval_info.signal_bits_addr.c_str(), lval_nb_addr.c_str(), rval_info.signal_bits_addr.c_str());                 
                        }
                    }
                } else {
                    copy_code = string_format("\tif(!pint_case_equality_int_ret(%s,%s,%u)) {\n", lval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str(), lval_info.assign_width);
                    if (has_evt && assign_flag) {
                        copy_code += string_format("\t\tchange_flag = edge_1bit((char)(%s[0]&0x1) | (char)((%s[%d]&0x1)<<4), (char)(%s[0]&0x1)|(char)((%s[%d]&0x1)<<4));\n", lval_info.signal_bits_addr.c_str(), lval_info.signal_bits_addr.c_str(),  l_sig_cnt, rval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str(), l_sig_cnt);
                    }                    
                    copy_code += string_format("\t\tpint_copy_int(%s,%s,%u);\n", lval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str(), lval_info.assign_width);
                    if (copy_to_nb) {
                        copy_code += string_format("\t\tpint_copy_int(%s,%s,%u);\n", lval_nb_addr.c_str(), rval_info.signal_bits_addr.c_str(), lval_info.assign_width);
                    }
                }
            } else {
                if (l_sig_width <= 4) {
                    copy_code += string_format("\t\t%s%s = %s%s;\n", lval_v.c_str(), lval_info.signal_bits_addr.c_str()+lval_skip, rval_v.c_str(), rval_info.signal_bits_addr.c_str()+rval_skip);
                    if (copy_to_nb) {
                        copy_code += string_format("\t\t%s%s = %s%s;\n", lval_v.c_str(), lval_nb_addr.c_str()+lval_skip, rval_v.c_str(), rval_info.signal_bits_addr.c_str()+rval_skip);
                    }
                } else if(l_sig_width <= 32){
                    if (copy_low) {
                        copy_code += string_format("\t\t%s[0] = %s[0];\n", lval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str());
                        if (copy_to_nb) {
                            copy_code += string_format("\t\t%s[0] = %s[0];\n", lval_nb_addr.c_str(), rval_info.signal_bits_addr.c_str());
                        }
                    } else {
                        copy_code += string_format("\t\t%s[0] = %s[0]; %s[1] = %s[1];\n", lval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str(), lval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str());
                        if (copy_to_nb) {
                            copy_code += string_format("\t\t%s[0] = %s[0]; %s[1] = %s[1];\n", lval_nb_addr.c_str(), rval_info.signal_bits_addr.c_str(), lval_nb_addr.c_str(), rval_info.signal_bits_addr.c_str());
                        }
                    }
                } else {
                    copy_code += string_format("\t\tpint_copy_int<%s>(%s,%s,%u);\n", lval_info.signal_bits_addr.c_str(), rval_info.signal_bits_addr.c_str(), lval_info.assign_width, B2S(copy_low));
                    if (copy_to_nb) {
                        copy_code += string_format("\t\tpint_copy_int<%s>(%s,%s,%u);\n", lval_nb_addr.c_str(), rval_info.signal_bits_addr.c_str(), lval_info.assign_width, B2S(copy_low));
                    }
                }
            }
        } else {
            const char *change_flag_string = care_change_flag ? "change_flag, " : "";
            const char *edge_string = care_change_flag ? "" : "no";
            use_change_flag = 1;
            copy_code = string_format(
                "pint_copy_signal_common_%sedge(%s %s,%u,%u,%s,%u,%s,%u,%s);\n", edge_string,
                change_flag_string, rval_info.signal_bits_addr.c_str(), rval_info.sig_width,
                lval_info.rval_offset, lval_info.signal_bits_addr.c_str(), lval_info.sig_width,
                lval_info.assign_base.c_str(), lval_info.assign_width, B2S(copy_low));

            if (copy_to_nb) {
                copy_code +=
                    string_format("pint_copy_signal_common_noedge(%s,%u,%u,%s,%u,%s,%u,%s);\n",
                                rval_info.signal_bits_addr.c_str(), rval_info.sig_width,
                                lval_info.rval_offset, lval_nb_addr.c_str(), lval_info.sig_width,
                                lval_info.assign_base.c_str(), lval_info.assign_width, B2S(copy_low));
            }
        }
    }

    copy_code = clear_code + copy_code;
    return copy_code;
}

void StmtGen::pint_assign_process(ivl_statement_t net, bool assign_flag, string delay) {
    char opcode = 0;
    if (assign_flag) {
        opcode = ivl_stmt_opcode(net);
    }
    ivl_expr_t rval = ivl_stmt_rval(net);
    // convert +=,-=,&= ....  to a=a+x, a=a-x,a=a&x and so on
    if (opcode != 0) {
        if (ivl_stmt_lvals(net) != 1) {
            PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net),
                                   "Type = %c, nlval = %u.", opcode, ivl_stmt_lvals(net));
        }
        ivl_lval_t lval = ivl_stmt_lval(net, 0);
        ivl_signal_t sig = ivl_lval_sig(lval);
        ivl_expr_t new_rval_expr = ivl_new_expr(IVL_EX_BINARY, IVL_VT_LOGIC, ivl_lval_width(lval),
                                                ivl_expr_signed(rval), ivl_expr_sized(rval));
        ivl_expr_t new_lval_expr = ivl_new_expr(IVL_EX_SIGNAL, IVL_VT_LOGIC, ivl_lval_width(lval),
                                                ivl_expr_signed(rval), ivl_expr_sized(rval));

        ivl_set_expr_signal(new_lval_expr, sig, NULL);
        ivl_set_expr_binary(new_rval_expr, opcode, rval, new_lval_expr);

        rval = new_rval_expr;
    }
    stmt_code += "{\n";
    // nxz
    assign_rval_info rval_info = gen_rval_info(rval, net, stmt_code);

    int rval_offset = 0;
    for (unsigned idx = 0; idx < ivl_stmt_lvals(net); idx += 1) {
        string lvl_prepare_code;
        string lvl_dec_code;
        ivl_lval_t lval = ivl_stmt_lval(net, idx);

        // nxz
        assign_lval_info lval_info = gen_lval_info(lval, net, assign_flag, delay, rval_offset,
                                                   lvl_prepare_code, lvl_dec_code);
        stmt_code += lvl_prepare_code;
        decl_code += lvl_dec_code;

        string valid_judge_str = "";
        if (lval_info.idx != "" &&
            (ivl_signal_array_count(ivl_lval_sig(lval)) > 1)) { //  sig = arra[idx];
            ivl_signal_t sig = ivl_lval_sig(lval);
            valid_judge_str += string_format("((unsigned)%s < %d)", lval_info.idx.c_str(),
                                             ivl_signal_array_count(sig));
        }

        if (valid_judge_str != "") {
            stmt_code += string_format("if (%s){\n", valid_judge_str.c_str());
        }

        ivl_signal_t sig = ivl_lval_sig(lval);
        pint_sig_info_t sig_info = pint_find_signal(sig);
        const char *sn = sig_info->sig_name.c_str();
        const char *wn = lval_info.idx.c_str(); //  arr_word info
        unsigned copy_to_nb = (assign_flag && sig_info->has_nb);
        // here to finely choose appropriate function to implement this copy task based on src and
        // dst info
        unsigned care_change_flag = 0;
        //  nb-asgn with delay can not be mult_asgn, and needs update right now
        if (assign_flag) {
            care_change_flag = pint_proc_has_addl_opt_after_sig_asgn(sig);
        } else if (delay == "") {
            care_change_flag = 1;
        }
        if (pint_sig_is_force_var(sig_info)) {
            if (sig_info->force_info->assign_idx.size()) {
                stmt_code += string_format("\tif(!force_en_%s && !assign_en_%s){\n", sn, sn);
            } else {
                stmt_code += string_format("\tif(!force_en_%s){\n", sn);
            }
        }
        if (assign_flag) {
            pint_proc_mult_opt_before_sig_asgn(stmt_code, lval, wn);
        }
        stmt_code += gen_assign_copy_code(lval_info, rval_info, care_change_flag, copy_to_nb, assign_flag, pint_signal_has_sen_evt(sig_info));
        if (assign_flag) {
            pint_proc_mult_opt_when_sig_asgn(stmt_code, sig, wn, lval_info.assign_base.c_str(),
                                             lval_info.assign_width);
        }
        if (assign_flag) {
            if (care_change_flag) {
                if (use_change_flag) {
                    stmt_code += "\tif(change_flag != 0){\n";
                }
                pint_proc_mult_opt_after_sig_asgn(stmt_code, sig, wn);
                stmt_code += "\t}\n";
            }
        } else {
            unsigned sig_nb_id = nbassign_map.find(key_val_array[idx])->second.sig_nb_id;
            string nb_id = string_format("%u", sig_nb_id);
            if (delay == "") {
                if (care_change_flag) {
                    if (use_change_flag) {
                        stmt_code += "\tif (change_flag != 0){\n";
                    }
                    pint_nb_assign_enqueue_process(lval_info.idx != "" ? 1 : 0, nb_id, lval_info.idx);
                    stmt_code += "\t}";
                }
            } else {
                stmt_code += string_format(
                    "pint_enqueue_nb_future(%s, %s, %u, %s, %u, %s, %u, %s, %u);\n",
                    lval_info.signal_bits_addr.c_str(), lval_info.signal_nb_bits_addr.c_str(),
                    lval_info.sig_width, lval_info.assign_base.c_str(), lval_info.assign_width,
                    (lval_info.idx == "" ? "-1" : lval_info.idx.c_str()), thread->delay_num,
                    delay.c_str(), sig_nb_id);
                thread->delay_num += 2;
            }
        }
        if (pint_sig_is_force_var(sig_info)) {
            stmt_code += "\t}  //  force_var\n";
        }
        if (valid_judge_str != "") {
            stmt_code += "}\n";
        }
    }
    stmt_code += "\n}\n";
}

void StmtGen::pint_delay_process(ivl_statement_t net) {
    const ivl_statement_type_t code = ivl_statement_type(net);
    thread->pc++;

    if (0 == thread->have_processed_delay) {
        global_init_buff += string_format("    pint_future_thread[%d].run_func = pint_thread_%d;\n",
                                          thread->delay_num, thread->id);
        thread->future_idx = thread->delay_num;
        thread->delay_num++;
        thread->have_processed_delay = 1;
    }

    string delay_stru_str;
    if (pint_in_task) {
        delay_stru_str = "p_delay[0]";
    } else {
        delay_stru_str = string_format("pint_future_thread[%d]", thread->future_idx);
    }

    if (code == IVL_ST_DELAY) {
        stmt_code += string_format("    //delay %lu\n", ivl_stmt_delay_val(net));
        if (ivl_stmt_delay_val(net) > 0) {
            stmt_code += string_format("        %s.T = T + %lu;\n", delay_stru_str.c_str(),
                                       ivl_stmt_delay_val(net));
            stmt_code +=
                string_format("        %s.type = FUTURE_THREAD_TYPE;\n", delay_stru_str.c_str());
            stmt_code +=
                string_format("        pint_enqueue_thread_future(&%s);\n", delay_stru_str.c_str());
        } else {
            stmt_code += string_format("        pint_enqueue_thread(%s.run_func, DELAY0_Q);\n",
                                       delay_stru_str.c_str());
        }
    } else {
        ivl_expr_t delay_expr = ivl_stmt_delay_expr(net);
        // nxz
        ExpressionConverter converter(delay_expr, net);
        converter.GetPreparation(&stmt_code, &decl_code);
        string delay_var_name = converter.GetResultNameString();
        string delay_value = gen_signal_value_var(
            converter.IsImmediateNum(), converter.IsResultNxz(), delay_expr, &delay_var_name);

        stmt_code += string_format("    //delayx %s\n", delay_value.c_str());
        stmt_code += string_format("    if (%s > 0){\n", delay_value.c_str());
        stmt_code +=
            string_format("        %s.T = T + %s;\n", delay_stru_str.c_str(), delay_value.c_str());
        stmt_code +=
            string_format("        %s.type = FUTURE_THREAD_TYPE;\n", delay_stru_str.c_str());
        stmt_code +=
            string_format("        pint_enqueue_thread_future(&%s);\n", delay_stru_str.c_str());
        stmt_code += "    }else{\n";
        stmt_code += string_format("        pint_enqueue_thread(%s.run_func, DELAY0_Q);\n",
                                   delay_stru_str.c_str());
        stmt_code += "    }\n";
    }

    if (pint_in_task) {
        stmt_code += string_format("    *pint_task_pc_x = %d;\n", thread->pc);
    } else {
        if (!pint_always_wait_one_event) {
            stmt_code += string_format("    pint_thread_pc_x[%d] = %d;\n", thread->id, thread->pc);
        }
    }

    if (!pint_always_wait_one_event) {
        stmt_code += pint_proc_gen_exit_code(pint_in_task, -1);
        stmt_code += string_format("PC_%d:\n    ;\n", thread->pc);
    }
    if (IVL_ST_NOOP == ivl_statement_type(ivl_stmt_sub_stmt(net))) {
        stmt_code += string_format("    ;\n");
    }

    StmtGen delay_gen(ivl_stmt_sub_stmt(net), thread, net, 0, 0);
    AppendStmtCode(delay_gen);
}

// nxz
void pint_value_plusargs(ivl_expr_t expr1, ivl_expr_t expr2, ivl_statement_t stmt, string &ope_code,
                         string &dec_code, const char *ret) {
    vector<string> para_names;

    // para1, string
    if (ivl_expr_type(expr1) != IVL_EX_STRING) {
        PINT_UNSUPPORTED_ERROR(ivl_expr_file(expr1), ivl_expr_lineno(expr1),
                               "param 1 is not string!");
        return;
    }
    const char *str_value = ivl_expr_string(expr1);
    para_names.push_back((string)str_value);
    char *p_walk = (char *)para_names[0].c_str();
    while ((p_walk[0] != 0) && (p_walk[0] != '%')) {
        p_walk++;
    }
    if (p_walk[0] == 0) {
        return;
    }
    p_walk[0] = 0;
    p_walk++;
    while ((p_walk[0] != 0) && (p_walk[0] > '0') && (p_walk[0] < '9')) {
        p_walk++;
    }
    if (p_walk[0] == 0) {
        return;
    }

    // para2, signal
    int width = ivl_expr_width(expr2);
    if (ivl_expr_type(expr2) != IVL_EX_SIGNAL) {
        PINT_UNSUPPORTED_ERROR(ivl_expr_file(expr2), ivl_expr_lineno(expr2),
                               "param 2 is not signal!");
        return;
    }
    // nxz
    ExpressionConverter convert2(expr2, stmt);
    convert2.GetPreparation(&ope_code, &dec_code);
    para_names.push_back(convert2.GetResultNameString());

    const char *ch = (width <= 4) ? "&" : "";
    if (ret == NULL) {
        ope_code +=
            string_format("(void)pint_get_value_plusargs(\"%s\", '%c', %s%s, %d);\n",
                          para_names[0].c_str(), p_walk[0], ch, para_names[1].c_str(), width);
    } else {
        ope_code +=
            string_format("%s = pint_get_value_plusargs(\"%s\", '%c', %s%s, %d);\n", ret,
                          para_names[0].c_str(), p_walk[0], ch, para_names[1].c_str(), width);
    }
    exists_sys_args.insert(para_names[0]);
}

void pint_test_plusargs(ivl_expr_t expr1, string &ope_code, const char *ret) {
    // para1, string
    if (ivl_expr_type(expr1) != IVL_EX_STRING) {
        PINT_UNSUPPORTED_ERROR(ivl_expr_file(expr1), ivl_expr_lineno(expr1),
                               "param 1 is not string!");
        return;
    }
    const char *str_value = ivl_expr_string(expr1);

    if (ret == NULL) {
        ope_code += string_format("(void)pint_get_test_plusargs(\"%s\");\n", str_value);
    } else {
        ope_code += string_format("%s = pint_get_test_plusargs(\"%s\");\n", ret, str_value);
    }
    exists_sys_args.insert((string)str_value);
}

/******************* <pint_case_process> *******************/
void pint_gen_case_condition(string &ret, unsigned id_type, unsigned width, const char *in0,
                             const char *in1) {
    if (width <= 4) {
        switch (id_type) {
        case 0: // IVL_ST_CASE
            ret = string_format("%s == %s", in0, in1);
            return;
        case 1: // IVL_ST_CASEX
            ret = string_format("pint_equality_ignore_xz_char_ret(&%s, &%s, %u)", in0, in1, width);
            return;
        case 2: // IVL_ST_CASEZ
            ret = string_format("pint_equality_ignore_z_char_ret(&%s, &%s, %u)", in0, in1, width);
            return;
        case 3: // IVL_ST_CASER
            ret = string_format("%s == %s", in0, in1);
            return;
        default:
            printf("Error!  --pint_gen_case_condition, statement type not supproted\n");
        }
    } else {
        switch (id_type) {
        case 0: // IVL_ST_CASE
            ret = string_format("pint_case_equality_int_ret(%s, %s, %u)", in0, in1, width);
            return;
        case 1: // IVL_ST_CASEX
            ret = string_format("pint_equality_ignore_xz_int_ret(%s, %s, %u)", in0, in1, width);
            return;
        case 2: // IVL_ST_CASEZ
            ret = string_format("pint_equality_ignore_z_int_ret(%s, %s, %u)", in0, in1, width);
            return;
        case 3: // IVL_ST_CASER
            ret = string_format("%s == %s", in0, in1);
            return;
        default:
            printf("Error!  --pint_gen_case_condition, statement type not supproted\n");
        }
    }
}

void pint_concat_binary_char(const char *bits, string &out_str, unsigned width) {
    out_str = "0b00000000";
    int h_idx = -1;
    int l_idx = -1;
    for (int j = width - 1; j >= 0; j--) {
        l_idx = 9 - j;
        h_idx = l_idx - 4;
        switch (bits[j]) {
        case '0':
            break;
        case '1':
            out_str[l_idx] = '1';
            // t_str[h_idx] = '0';
            break;
        case 'x':
            out_str[l_idx] = '1';
            out_str[h_idx] = '1';
            break;
        case 'z':
            // t_str[l_idx] = '1';
            out_str[h_idx] = '1';
            break;
        default:
            printf("Error!  --pint_concat_binary, bit %d is %c\n", j, bits[j]);
            break;
        }
    }
}
void pint_concat_binary_int(const char *bits, string &out_str, unsigned width) {
    out_str = "0b00000000000000000000000000000000";
    int h_idx = -1;
    int l_idx = -1;
    for (int j = width - 1; j >= 0; j--) {
        l_idx = 33 - j;
        h_idx = l_idx - 16;
        switch (bits[j]) {
        case '0':
            break;
        case '1':
            out_str[l_idx] = '1';
            // t_str[h_idx] = '0';
            break;
        case 'x':
            out_str[l_idx] = '1';
            out_str[h_idx] = '1';
            break;
        case 'z':
            // t_str[l_idx] = '1';
            out_str[h_idx] = '1';
            break;
        default:
            printf("Error!  --pint_concat_binary, bit %d is %c\n", j, bits[j]);
            break;
        }
    }
}

void StmtGen::pint_case_process(ivl_statement_t net, unsigned id_type, unsigned ind) {
    if (!PINT_SIMPLE_CODE_EN) {
        stmt_code += string_format("//  case: begin\n");
    }
    ind = 4;
    ivl_expr_t ex_cond = ivl_stmt_cond_expr(net);
    unsigned width = ivl_expr_width(ex_cond);
    unsigned cnt = ivl_stmt_case_count(net);
    ivl_expr_t ex[cnt + 1];
    string ex_name[cnt + 1];
    ivl_statement_t stmt[cnt];
    ex[cnt] = ex_cond;
    for (unsigned i = 0; i < cnt; i++) {
        ex[i] = ivl_stmt_case_expr(net, i);
        stmt[i] = ivl_stmt_case_stmt(net, i);
    }

    bool switch_flag = false;
    bool tmp_flag = true;
    if (width <= 16 && id_type == 0) {
        for (unsigned i = 0; i < cnt; i++) {
            if (ex[i]) {
                if (ivl_expr_type(ex[i]) != IVL_EX_NUMBER) {
                    tmp_flag = false;
                    break;
                }
            }
        }
        if (tmp_flag)
            switch_flag = true;
    }

    if (switch_flag) {
        for (unsigned i = 0; i < cnt; i++) {
            if (ex[i]) {
                const char *bits = ivl_expr_bits(ex[i]);
                string t_str;
                if (width <= 4)
                    pint_concat_binary_char(bits, t_str, width);
                else
                    pint_concat_binary_int(bits, t_str, width);
                ex_name[i] = t_str;
            }
        }

        if (ex[cnt]) {
            // nxz
            ExpressionConverter converter(ex[cnt], net, 0, NUM_EXPRE_SIGNAL);
            converter.GetPreparation(&stmt_code, &decl_code);
            ex_name[cnt] = converter.GetResultNameString();
        }
    } else {
        for (unsigned i = 0; i <= cnt; i++) {
            if (ex[i]) {
                // nxz
                ExpressionConverter converter(ex[i], net, 0, NUM_EXPRE_SIGNAL);
                converter.GetPreparation(&stmt_code, &decl_code);
                ex_name[i] = converter.GetResultNameString();
            }
        }
    }

    const char *name[cnt + 1];
    for (unsigned i = 0; i <= cnt; i++) {
        name[i] = ex_name[i].c_str();
    }

    int default_idx = -1;
    int first_state_flag = 1;

    if (switch_flag) {
        if (width > 4)
            stmt_code +=
                string_format("%*sswitch(pint_concat_ptr_to_int(%s)){\n", ind, "", name[cnt]);
        else
            stmt_code += string_format("%*sswitch(%s){\n", ind, "", name[cnt]);

        for (unsigned i = 0; i < cnt; i++) {
            if (ex_name[i] == "") {
                default_idx = i;
            } else {
                string fmt;
                bool repeat_flag = false;
                for (unsigned jj = 0; jj < i; jj++) {
                    if (ex_name[i] == ex_name[jj]) {
                        repeat_flag = true;
                        break;
                    }
                }
                if (!repeat_flag) {
                    fmt = "case " + ex_name[i] + ":";
                    stmt_code += string_format("%*s %s\n", ind, "", fmt.c_str());
                    if (stmt[i]) {
                        StmtGen case_gen(stmt[i], thread, net, i, ind + 4);
                        AppendStmtCode(case_gen);
                        stmt_code += string_format("%*s break;\n", ind, "");
                    }
                }
            }
        }

        if ((default_idx >= 0) && (stmt[default_idx])) {
            if (ivl_statement_type(stmt[default_idx]) != IVL_ST_NOOP) {
                stmt_code += string_format("%*s default:\n", ind, "");
                StmtGen case_gen(stmt[default_idx], thread, net, default_idx, ind + 4);
                AppendStmtCode(case_gen);
                stmt_code += "\nbreak;\n";
            }
        }
        stmt_code += string_format("%*s}\n", ind, "");
    } else {
        for (unsigned i = 0; i < cnt; i++) {
            if (ex_name[i] == "") {
                default_idx = i;
            } else {
                string fmt;
                pint_gen_case_condition(fmt, id_type, width, name[cnt], name[i]);
                if (first_state_flag) {
                    first_state_flag = 0;
                    stmt_code += string_format("%*sif(%s){\n", ind, "", fmt.c_str());
                } else {
                    stmt_code += string_format("%*s}else if(%s){\n", ind, "", fmt.c_str());
                }
                if (stmt[i]) {
                    StmtGen case_gen(stmt[i], thread, net, i, ind + 4);
                    AppendStmtCode(case_gen);
                }
            }
        }
        stmt_code += string_format("%*s}else{\n", ind, "");
        if ((default_idx >= 0) && (stmt[default_idx])) {
            StmtGen case_gen(stmt[default_idx], thread, net, default_idx, ind + 4);
            AppendStmtCode(case_gen);
        }

        stmt_code += string_format("%*s}\n", ind, "");
    }

    if (!PINT_SIMPLE_CODE_EN) {
        stmt_code += string_format("//  case: end\n");
    }
}

void StmtGen::pint_repeat_process(ivl_statement_t net) {
    unsigned int idx = pint_repeat_idx++;
    start_buff += string_format("unsigned int repeat_cnt_%u = 0;\n", idx);
    inc_buff += string_format("extern unsigned int repeat_cnt_%u;\n", idx);

    ivl_expr_t expr_cnt = ivl_stmt_cond_expr(net);
    ivl_statement_t sub_stmt = ivl_stmt_sub_stmt(net);
    // nxz
    ExpressionConverter obj_cnt(expr_cnt, net);
    obj_cnt.GetPreparation(&stmt_code, &decl_code);
    const char *sn_ret = obj_cnt.GetResultName();

    if (obj_cnt.IsImmediateNum()) {
        stmt_code += string_format("\trepeat_cnt_%u = %s;\n", idx, sn_ret);
    } else {
        stmt_code +=
            string_format("\trepeat_cnt_%u = %s;\n", idx,
                          code_of_expr_value(expr_cnt, sn_ret, obj_cnt.IsResultNxz()).c_str());
    }

    stmt_code += string_format("\tif(!repeat_cnt_%u)  goto  REPEAT_%u_END;\n", idx, idx);
    stmt_code += string_format("REPEAT_%u_ST:\n;\n", idx);
    stmt_code += string_format("\trepeat_cnt_%u--;\n", idx);

    StmtGen repeat_gen(sub_stmt, thread, net, 0, 0);
    AppendStmtCode(repeat_gen);
    stmt_code += string_format("\tif(repeat_cnt_%u)  goto  REPEAT_%u_ST;\n", idx, idx);
    stmt_code += string_format("REPEAT_%u_END:\n;\n", idx);
}

void pint_stmt_show_source_file(ivl_statement_t net) {
    const char *file = ivl_stmt_file(net);
    unsigned lineno = ivl_stmt_lineno(net);
    string buff;
    pint_source_file_ret(buff, file, lineno);
    printf("//  %s[%04u]:\t%s\n", file, lineno, buff.c_str());
}

// bool is_nex_sensitivity(ivl_nexus_t nex, std::set<ivl_nexus_t> &deletable_signals) {
//     bool is_add_lpm = true;
//     if (deletable_signals.find(nex) != deletable_signals.end()) { // found
//         is_add_lpm = false;
//         // debug
//         unsigned int nptrs = ivl_nexus_ptrs(nex);
//         ivl_nexus_ptr_t ptr;
//         ivl_signal_t sig;
//         for (unsigned int i = 0; i < nptrs; i++) {
//             ptr = ivl_nexus_ptr(nex, i);
//             sig = ivl_nexus_ptr_sig(ptr);
//             // check if the signal can be removed from sensitive
//             if (sig) {
//                 const char *sig_name = ivl_signal_basename(sig);
//                 std::cout << "the deletalbe signal: " << sig_name
//                           << "  file: " << ivl_signal_file(sig)
//                           << "  line: " << ivl_signal_lineno(sig) << std::endl;
//             }
//         }
//     }
//     return is_add_lpm;
// }

bool is_nex_sensitivity(ivl_nexus_t nex, std::set<ivl_signal_t> &sensitive_signals) {
    // sensitive signal remove
    unsigned int nptrs = ivl_nexus_ptrs(nex);
    ivl_nexus_ptr_t ptr;
    ivl_signal_t sig;
    bool is_add_lpm = true;
    for (unsigned int i = 0; i < nptrs; i++) {
        ptr = ivl_nexus_ptr(nex, i);
        sig = ivl_nexus_ptr_sig(ptr);
        // check if the signal can be removed from sensitive
        if (sig) {
            if (sensitive_signals.find(sig) != sensitive_signals.end()) { // found
                // debug: output the deletable signal
                is_add_lpm = false;
                // const char *sig_name = ivl_signal_basename(sig);
                // std::cout << "the deletalbe signal: " << sig_name
                //           << "  file: " << ivl_signal_file(sig)
                //           << "  line: " << ivl_signal_lineno(sig) << std::endl;
                break;
            }
        }
    }
    return is_add_lpm;
}

void statement_add_list(ivl_statement_t net, unsigned ind,
                        std::set<ivl_signal_t> &deletable_signals) {
    unsigned idx;
    const ivl_statement_type_t code = ivl_statement_type(net);
    string delay_value = "";

    switch (code) {
    case IVL_ST_WAIT:
        for (unsigned idx = 0; idx < ivl_stmt_nevent(net); idx += 1) {
            ivl_event_t evt = ivl_stmt_events(net, idx);
            if (evt != 0) {
                pint_event_info_t event_info = pint_find_event(evt);
                if (event_info) {
                    if (thread_to_lpm == (-1)) {
                        if ((event_info->any_num > 0) && (event_info->pos_num == 0) &&
                            (event_info->neg_num == 0)) {
                            for (unsigned idx = 0; idx < event_info->any_num; idx += 1) {
                                ivl_nexus_t nex = ivl_event_any(event_info->evt, idx);
                                pint_sig_info_add_event(nex, event_info->event_id, ANY);
                            }
                        }
                    } else {
                        mcc_event_ids.push_back(event_info->event_id);
                        for (unsigned idx = 0; idx < event_info->any_num; idx += 1) {
                            ivl_nexus_t nex = ivl_event_any(event_info->evt, idx);
                            if (is_nex_sensitivity(nex, deletable_signals)) {
                                pint_sig_info_add_lpm(nex, thread_to_lpm);
                            }
                        }
                    }
                }
            }
        }

        break;
    case IVL_ST_BLOCK: {
        unsigned cnt = ivl_stmt_block_count(net);

        for (idx = 0; idx < cnt; idx += 1) {
            ivl_statement_t cur = ivl_stmt_block_stmt(net, idx);
            statement_add_list(cur, ind + 4, deletable_signals);
        }
        break;
    }
    case IVL_ST_CASSIGN: {
        unsigned nlhs = ivl_stmt_lvals(net);
        unsigned i;
        string tmp_str;
        for (i = 0; i < nlhs; i++) {
            int flag = pint_show_assign_lval(tmp_str, pint_force_lval[ivl_stmt_lval(net, i)],
                                             pint_force_idx[net], net);
            if (flag == (-1)) {
                pint_force_idx[net] = -1;
            }
        }
        break;
    }

    default:
        break;
    }
}

StmtGen::StmtGen(ivl_statement_t net_, pint_thread_info_t thread_, ivl_statement_t parent,
                 unsigned sub_idx, unsigned ind_) {
    if (net_ == NULL || thread_ == NULL) {
        is_valid = false;
        return;
    }

    is_valid = true;
    net = net_;
    ind = ind_;
    thread = thread_;

    stmt_code.clear();
    decl_code.clear();
    if (parent) {
        // If parent = NULL, up at the begining
        pint_parse_mult_up_oper(stmt_code, parent, sub_idx);
    }
    GenStmtCode();
    if (parent) { //  If parent = NULL, up at last
        string mult_down_code;
        unsigned has_mult_down = pint_parse_mult_down_oper(mult_down_code, parent, sub_idx);
        if (has_mult_down) {
            ivl_statement_type_t type = ivl_statement_type(net);
            if (type == IVL_ST_WAIT || type == IVL_ST_DELAY || type == IVL_ST_DELAYX) {
                stmt_code = mult_down_code + stmt_code; // down before sub_stmt
            } else {
                stmt_code += mult_down_code; // down after sub_stmt
            }
        }
    }
}

string StmtGen::GetStmtGenCode() { return stmt_code; }

string StmtGen::GetStmtDeclCode() { return decl_code; }

void StmtGen::AppendStmtCode(StmtGen &child_stmt) {
    stmt_code += child_stmt.GetStmtGenCode();
    decl_code += child_stmt.GetStmtDeclCode();
}

void StmtGen::show_assign_stmt() {
    string delay_value = "";
    pint_assign_process(net, true, delay_value);
}

void StmtGen::show_nbassign_stmt() {
    for (unsigned idx = 0; idx < ivl_stmt_lvals(net); idx += 1) {
        show_stmt_nb_assign_info(ivl_stmt_lval(net, idx), idx);
    }

    string delay_value = "";
    ivl_expr_t delay_expr = ivl_stmt_delay_expr(net);
    if (delay_expr) {
        if (ivl_expr_type(delay_expr) == IVL_EX_DELAY) {
            // nxz
            ExpressionConverter converter(delay_expr, net, 0, NUM_EXPRE_DIGIT);
            delay_value = converter.GetResultNameString();
            delay_value = (delay_value == "0") ? "" : delay_value;
        } else {
            // nxz
            ExpressionConverter converter(delay_expr, net);
            converter.GetPreparation(&stmt_code, &decl_code);
            string delay_var_name = converter.GetResultNameString();
            delay_value = gen_signal_value_var(converter.IsImmediateNum(), converter.IsResultNxz(),
                                               delay_expr, &delay_var_name);
        }
    }
    if (ivl_stmt_nevent(net)) {
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net),
                               "nb assign triggerd by event.");
    }

    pint_assign_process(net, false, delay_value);
}

void StmtGen::show_case_stmt(unsigned case_type) { pint_case_process(net, case_type, ind); }

void StmtGen::show_condition_stmt() {
    ivl_expr_t ex = ivl_stmt_cond_expr(net);
    ivl_statement_t t = ivl_stmt_cond_true(net);
    ivl_statement_t f = ivl_stmt_cond_false(net);
    unsigned ex_width = ivl_expr_width(ex);
    if (!PINT_SIMPLE_CODE_EN) {
        stmt_code += "//  if: begin\n";
    }
    if (ex) {
        if ((ex_width == 1) && (ivl_expr_type(ex) == IVL_EX_UNARY) &&
            ((ivl_expr_opcode(ex) == '~') ||
             ((ivl_expr_opcode(ex) == '!') && (ivl_expr_width(ivl_expr_oper1(ex)) == 1)))) {
            ivl_expr_t oper1 = ivl_expr_oper1(ex);
            // nxz
            ExpressionConverter converter(oper1, net, 0);
            converter.GetPreparation(&stmt_code, &decl_code);
            stmt_code += string_format("if (%s == 0){", converter.GetResultName());
        } else {
            // nxz
            ExpressionConverter converter(ex, net, 0);
            converter.GetPreparation(&stmt_code, &decl_code);

            if (converter.IsImmediateNum()) {
                stmt_code += "    if (";
                stmt_code += string_format("%s, %d) {\n", converter.GetResultName(), ex_width);
            } else {
                if (ex_width == 1) {
                    stmt_code += string_format("if (%s == 1){\n", converter.GetResultName());
                } else if (ex_width > 4) {
                    stmt_code += "    if (pint_is_true_int(";
                    stmt_code += string_format("%s, %d)) {\n", converter.GetResultName(), ex_width);
                } else {
                    stmt_code += "    if (pint_is_true_char(&";
                    stmt_code += string_format("%s, %d)) {\n", converter.GetResultName(), ex_width);
                }
            }
        }
    } else {
        stub_errors += 1;
    }

    if (t) {
        StmtGen cond_true(t, thread, net, 0, ind + 4);
        AppendStmtCode(cond_true);
    }

    if (f) {
        stmt_code += "\n    } else {\n";
        StmtGen cond_false(f, thread, net, 1, ind + 4);
        AppendStmtCode(cond_false);
    }

    stmt_code += "    }\n";
    if (!PINT_SIMPLE_CODE_EN) {
        stmt_code += "//  if: end\n";
    }
}

void StmtGen::show_stask_stmt() {
    if (!PINT_SIMPLE_CODE_EN) {
        stmt_code +=
            string_format("    //Call %s(%u parameters); %s:%u\n", ivl_stmt_name(net),
                          ivl_stmt_parm_count(net), ivl_stmt_file(net), ivl_stmt_lineno(net));
    }

    bool is_display = false;
    bool is_write = false;
    bool is_monitor = false;
    bool is_strobe = false;
    bool is_fdisplay = false;
    bool is_fwrite = false;
    bool is_printtimescale = false;

    if ((!strcmp(ivl_stmt_name(net), "$readmemh")) || (!strcmp(ivl_stmt_name(net), "$readmemb")) ||
        (!strcmp(ivl_stmt_name(net), "$fopen"))) {
        ivl_signal_t sig = ivl_expr_signal(ivl_stmt_parm(net, 1));
        pint_sig_info_t sig_info = pint_find_signal(sig);
        unsigned arr_words = ivl_signal_array_count(sig);
        unsigned wid = sig_info->width;
        ivl_expr_t param_file = ivl_stmt_parm(net, 0);
        string file_name;
        int para_num = ivl_stmt_parm_count(net);
        ivl_expr_t param_start = NULL;
        ivl_expr_t param_end = NULL;
        stmt_code += "\t{\n";
        if (para_num == 3) {
            param_start = ivl_stmt_parm(net, 2);
        } else if (para_num == 4) {
            param_start = ivl_stmt_parm(net, 2);
            param_end = ivl_stmt_parm(net, 3);
        }

        if (param_start && param_end) {
            if (IVL_VT_REAL == ivl_expr_value(param_start)) {
                PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net),
                                       "third argument(start address) is a real value.");
            }
            ExpressionConverter st_expr(param_start, net, 0);
            st_expr.GetPreparation(&stmt_code, &decl_code);

            if (IVL_VT_REAL == ivl_expr_value(param_end)) {
                PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net),
                                       "fourth argument(finish address) is a real value.");
            }
            ExpressionConverter end_expr(param_end, net, 0);
            end_expr.GetPreparation(&stmt_code, &decl_code);

            if (!st_expr.IsImmediateNum() && !end_expr.IsImmediateNum()) {
                stmt_code += string_format("\tint start = %s[0];\n\tint len = %s[0] - %s[0] + 1;\n",
                                           st_expr.GetResultName(), end_expr.GetResultName(),
                                           st_expr.GetResultName());
            } else if (!st_expr.IsImmediateNum() && st_expr.IsImmediateNum()) {
                stmt_code += string_format("\tint start = %s[0];\n\tint len = %s - %s[0] + 1;\n",
                                           st_expr.GetResultName(), end_expr.GetResultName(),
                                           st_expr.GetResultName());
            } else {
                stmt_code += string_format("\tint start = %s;\n\tint len = %s - %s + 1;\n",
                                           st_expr.GetResultName(), end_expr.GetResultName(),
                                           st_expr.GetResultName());
            }
        } else if (param_start) {
            if (IVL_VT_REAL == ivl_expr_value(param_start)) {
                PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net),
                                       "third argument (start address) is a real value.");
            }
            ExpressionConverter st_expr(param_start, net, 0);
            st_expr.GetPreparation(&stmt_code, &decl_code);

            if (!st_expr.IsImmediateNum()) {
                stmt_code +=
                    string_format("\tint start = %s[0];\n\tint len = %u - %s[0];\n",
                                  st_expr.GetResultName(), arr_words, st_expr.GetResultName());
            } else {
                stmt_code +=
                    string_format("\tint start = %s;\n\tint len = %u - %s;\n",
                                  st_expr.GetResultName(), arr_words, st_expr.GetResultName());
            }
        } else {
            stmt_code += string_format("\tint start = 0;\n\tint len = %u;\n", arr_words);
        }

        // nxz
        pint_show_expression_string(decl_code, stmt_code, param_file, net, pint_process_str_idx);
        file_name = string_format("str_%u", pint_process_str_idx++);
        if (pint_pli_mode) {
            stmt_code += string_format("    {\n\tchar tmp_name[1024];\n"
                                       "    int i = 0;\n"
                                       "    int offset = 0;\n"
                                       "    while(%s[i]) {tmp_name[i+offset] = %s[i]; i++;}\n"
                                       "    tmp_name[i+offset] = 0;\n",
                                       file_name.c_str(), file_name.c_str());
        } else {
            stmt_code +=
                string_format("    {\n\tchar tmp_name[1024];\n"
                              "    int i = 0;\n"
                              "    int offset = 0;\n"
                              "    if (%s[0] != '/') {\n"
                              "    tmp_name[0] = '.';tmp_name[1] = '.';tmp_name[2] = "
                              "'/';tmp_name[3] = '.';tmp_name[4] = '.';tmp_name[5] = '/';\n"
                              "    offset = 6;}\n"
                              "    while(%s[i]) {tmp_name[i+offset] = %s[i]; i++;}\n"
                              "    tmp_name[i+offset] = 0;\n",
                              file_name.c_str(), file_name.c_str(), file_name.c_str());
        }
        stmt_code +=
            string_format("    pint_simu_vpi(PINT_SEND_MSG, %d, \"%s\", tmp_name, %s, start, len, "
                          "%d);\n\t}\n\t}\n",
                          pint_vpimsg_num++, ivl_stmt_name(net), sig_info->sig_name.c_str(), wid);
        return;
    } else if (!strcmp(ivl_stmt_name(net), "$fclose")) {
        ivl_signal_t sig = ivl_expr_signal(ivl_stmt_parm(net, 0));
        pint_sig_info_t sig_info = pint_find_signal(sig);
        stmt_code += string_format("    pint_simu_vpi(PINT_SEND_MSG, %d, \"$fclose\", %s[0]);\n",
                                   pint_vpimsg_num++, sig_info->sig_name.c_str());
    } else if (!strcmp(ivl_stmt_name(net), "$value$plusargs")) {
        int para_num = ivl_stmt_parm_count(net);
        if (para_num == 2) {
            // nxz
            pint_value_plusargs(ivl_stmt_parm(net, 0), ivl_stmt_parm(net, 1), net, stmt_code,
                                decl_code, NULL);
        } else {
            PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net),
                                   "%d param, only support 2!", para_num);
        }
    } else if (!strcmp(ivl_stmt_name(net), "$test$plusargs")) {
        int para_num = ivl_stmt_parm_count(net);
        if (para_num == 1) {
            pint_test_plusargs(ivl_stmt_parm(net, 0), stmt_code, NULL);
        } else {
            PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net),
                                   "%d param, only support 1!", para_num);
        }
    } else if (!strcmp(ivl_stmt_name(net), "$finish")) {
        stmt_code += "    pint_stop_simu();";
        if (ivl_stmt_parm_count(net)) {
            stmt_code += "    //";
        }
        stmt_code += "\n";
        stmt_code += pint_proc_gen_exit_code(pint_in_task, -1);
        return;
    } else if (!strcmp(ivl_stmt_name(net), "$dumpfile")) {
        ivl_expr_t param = ivl_stmt_parm(net, 0);
        string file_name;
        int idx;
        int count = simu_header_buff.size();
        dump_file_buff.clear();
        // nxz
        pint_show_expression_string(decl_code, stmt_code, param, net, pint_process_str_idx);
        file_name = string_format("str_%u", pint_process_str_idx++);
        if (pint_pli_mode) {
            stmt_code += string_format("    {\n\tchar tmp_name[1024];\n"
                                       "    int i = 0;\n"
                                       "    int offset = 0;\n"
                                       "    while(%s[i]) {tmp_name[i+offset] = %s[i]; i++;}\n"
                                       "    tmp_name[i+offset] = 0;\n",
                                       file_name.c_str(), file_name.c_str());
        } else {
            stmt_code +=
                string_format("    {\n\tchar tmp_name[1024];\n"
                              "    int i = 0;\n"
                              "    int offset = 0;\n"
                              "    if (%s[0] != '/') {\n"
                              "    tmp_name[0] = '.';tmp_name[1] = '.';tmp_name[2] = "
                              "'/';tmp_name[3] = '.';tmp_name[4] = '.';tmp_name[5] = '/';\n"
                              "    offset = 6;}\n"
                              "    while(%s[i]) {tmp_name[i+offset] = %s[i]; i++;}\n"
                              "    tmp_name[i+offset] = 0;\n",
                              file_name.c_str(), file_name.c_str(), file_name.c_str());
        }
        stmt_code += string_format("    pint_simu_vpi(PINT_SEND_MSG, %d, \"$fopen\", tmp_name, "
                                   "&simu_fd, \"0\", 0, 0);\n\t}\n");

        // write dump header
        stmt_code += string_format("    pint_simu_vpi(PINT_DUMP_FILE, simu_fd);\n");
        for (idx = 0; idx < count; idx++) {
            if ((idx % PINT_DUMP_FILE_LINE_NUM) == 0) {
                if (idx > 0) {
                    dump_file_buff += "}\n\n";
                }
                stmt_code +=
                    string_format("    pint_dump_file_%d();\n", idx / PINT_DUMP_FILE_LINE_NUM);
                dump_file_buff += string_format("void pint_dump_file_%d(void) {\n",
                                                idx / PINT_DUMP_FILE_LINE_NUM);
                thread_declare_buff +=
                    string_format("void pint_dump_file_%d(void);\n", idx / PINT_DUMP_FILE_LINE_NUM);
            }
            dump_file_buff += string_format("    pint_simu_vpi(PINT_DUMP_VAR, \"%s\");\n",
                                            simu_header_buff[idx].c_str());
        }
        dump_file_buff += "}\n\n";
        pint_vcddump_init(&stmt_code);
        stmt_code += string_format("    pint_dump_enable = 1;\n");

        return;
    } else if (!strcmp(ivl_stmt_name(net), "$dumpvars")) {
        pint_dump_vars_num++;
        return;
    } else if (!strcmp(ivl_stmt_name(net), "$dumpoff")) {
        stmt_code += "    pint_dump_enable = 0;\n";
        pint_vcddump_off(&stmt_code);

        return;
    } else if (!strcmp(ivl_stmt_name(net), "$dumpon")) {
        stmt_code += "    pint_dump_enable = 1;\n";
        pint_vcddump_on(&stmt_code);
        return;
    } else if (!strcmp(ivl_stmt_name(net), "$monitoron")) {
        stmt_code += "    monitor_on = 1;\n";
        return;
    } else if (!strcmp(ivl_stmt_name(net), "$monitoroff")) {
        stmt_code += "    monitor_on = 0;\n";
        return;
    } else if (!strcmp(ivl_stmt_name(net), "$display")) {
        display_type = 1;
        is_display = true;
    } else if (!strcmp(ivl_stmt_name(net), "$displayh")) {
        display_type = 2;
        is_display = true;
    } else if (!strcmp(ivl_stmt_name(net), "$displayo")) {
        display_type = 3;
        is_display = true;
    } else if (!strcmp(ivl_stmt_name(net), "$displayb")) {
        display_type = 4;
        is_display = true;
    } else if (!strcmp(ivl_stmt_name(net), "$write")) {
        is_write = true;
    } else if (!strcmp(ivl_stmt_name(net), "$monitor")) {
        is_monitor = true;
    } else if (!strcmp(ivl_stmt_name(net), "$strobe")) {
        is_strobe = true;
    } else if (!strcmp(ivl_stmt_name(net), "$fdisplay")) {
        is_fdisplay = true;
    } else if (!strcmp(ivl_stmt_name(net), "$fwrite")) {
        is_fwrite = true;
    } else if (!strcmp(ivl_stmt_name(net), "$printtimescale")) {
        is_printtimescale = true;
    } else if (!strcmp(ivl_stmt_name(net), "$stop")) {
        PINT_UNSUPPORTED_WARN(ivl_stmt_file(net), ivl_stmt_lineno(net), "$stop");
    } else {
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net), "sys func %s",
                               ivl_stmt_name(net));
    }

    if (is_display || is_write || is_monitor || is_strobe || is_fdisplay || is_fwrite ||
        is_printtimescale) {
        show_stmt_print(net);
    }
}

void StmtGen::show_utask_stmt() {
    string scope_name;
    string arg;
    unsigned arg_num = 0;
    ivl_scope_t scope = ivl_stmt_call(net);
    scope_name = ivl_scope_name_c(scope);
    bool need_pc_flag = ivl_task_need_pc(scope);
    invoked_fun_names.insert(scope_name);
    if (!pint_in_task) {
        set<ivl_event_t> *wait_events = (set<ivl_event_t> *)ivl_task_wait_events(scope);
        for (set<ivl_event_t>::iterator it = wait_events->begin(); it != wait_events->end(); it++) {
            ivl_event_t evt = *it;
            pint_event_info_t event_info = pint_find_event(evt);
            if (event_info) {
                // printf("Task %s wait event %s!\n", ivl_scope_name(scope), ivl_event_name(evt));
                if (!pint_find_process_event(evt)) {
                    event_info->wait_count++;
                    pint_add_process_event(evt);
                }
            }
        }
    }

    if (ivl_scope_sigs(scope)) {
        for (unsigned idx = 0; idx < ivl_scope_sigs(scope); idx += 1) {
            ivl_signal_t sig = ivl_scope_sig(scope, idx);
            ivl_signal_port_t type = ivl_signal_port(sig);
            pint_sig_info_t sig_info = pint_find_signal(sig);

            if ((type == IVL_SIP_INPUT) || (type == IVL_SIP_INOUT) || (type == IVL_SIP_OUTPUT)) {
                if (arg_num == 0) {
                    arg += sig_info->sig_name;
                } else {
                    arg += ", " + sig_info->sig_name;
                }
                arg_num++;
            }
        }
    }

    if (need_pc_flag) {
        thread->pc++;
        if (arg != "") {
            arg += ", ";
        }

        int cur_stack_size = ivl_task_cur_stack_size((ivl_scope_t)cur_ivl_task_scope);
        if (pint_in_task) {
            stmt_code += string_format("    *pint_task_pc_x = %d;\n", thread->pc);
            arg += string_format("(char*)sp + %d, p_thread, p_delay", cur_stack_size);

            if (ivl_task_max_stack_size(scope) >= MAX_TASK_STACK_SIZE) {
                arg += string_format(", stack_total, cur_stack + %d", cur_stack_size);
            }

        } else {
            if (!pint_always_wait_one_event) {
                stmt_code +=
                    string_format("    pint_thread_pc_x[%d] = %d;\n", thread->id, thread->pc);
            }
            int max_stack_size = ivl_task_max_stack_size(scope);
            if (max_stack_size > 0) {
                stmt_code += string_format("    static unsigned task_stack_%d[%d] = {0};\n",
                                           thread->pc, max_stack_size / 4);
                arg += string_format("task_stack_%d", thread->pc);
            } else {
                arg += "NULL";
            }
            arg += string_format(",  pint_thread_%d", thread->id);

            if (ivl_task_need_pc(scope)) {
                if (0 == thread->have_processed_delay) {
                    global_init_buff +=
                        string_format("    pint_future_thread[%d].run_func = pint_thread_%d;\n",
                                      thread->delay_num, thread->id);
                    thread->future_idx = thread->delay_num;
                    thread->delay_num++;
                    thread->have_processed_delay = 1;
                }
                arg += string_format(", pint_future_thread + %d", thread->future_idx);
            } else {
                arg += ", NULL";
            }

            if (ivl_task_max_stack_size(scope) >= MAX_TASK_STACK_SIZE) {
                arg += string_format(", %d, 0", ivl_task_max_stack_size(scope));
            }
        }
        if (!pint_always_wait_one_event) {
            stmt_code += string_format("PC_%d:\n    ;\n", thread->pc);
        }
    }

    if (need_pc_flag) {
        stmt_code += string_format("if (%s(%s) == -1){\n", scope_name.c_str(), arg.c_str());
        if (pint_in_task) {
            stmt_code += "    return -1;\n}\n";
        } else {
            stmt_code += "    return;\n}\n";
        }
    } else {
        stmt_code += string_format("%s(%s);\n", scope_name.c_str(), arg.c_str());
    }
}

void StmtGen::show_wait_stmt() {
    if (thread_to_lpm == -1) {
        show_stmt_wait(net, ind);
    } else {
        StmtGen wait_gen(ivl_stmt_sub_stmt(net), thread, net, 0, ind + 4);
        AppendStmtCode(wait_gen);
    }
}

void StmtGen::show_while_stmt() {
    string ex_buf;
    if (!PINT_SIMPLE_CODE_EN) {
        stmt_code += "//  while: begin\n";
    }
    ivl_expr_t ex = ivl_stmt_cond_expr(net);
    unsigned ex_width = ivl_expr_width(ex);

    // nxz
    ExpressionConverter converter(ex, net, 0);
    converter.GetPreparation(&ex_buf, &decl_code);
    stmt_code += ex_buf;

    if (converter.IsImmediateNum()) {
        stmt_code += string_format("   while (%s) {\n", converter.GetResultName());
    } else {
        if (ex_width == 1) {
            stmt_code += string_format("while(%s == 1) {\n", converter.GetResultName());
        } else if (ex_width > 4) {
            stmt_code += "    while (pint_is_true_int(";
            stmt_code += string_format("%s, %d)) {\n", converter.GetResultName(), ex_width);
        } else {
            stmt_code += "    while (pint_is_true_char(&";
            stmt_code += string_format("%s, %d)) {\n", converter.GetResultName(), ex_width);
        }
    }

    StmtGen while_gen(ivl_stmt_sub_stmt(net), thread, net, 0, ind + 2);
    AppendStmtCode(while_gen);
    stmt_code += ex_buf;
    stmt_code += "    }\n";
    if (!PINT_SIMPLE_CODE_EN) {
        stmt_code += "//  while: end\n";
    }
}

void StmtGen::show_forever_stmt() {
    if (!PINT_SIMPLE_CODE_EN) {
        stmt_code += "\n//forever\n";
    }
    stmt_code += string_format("    while(1) {\n");

    StmtGen forever_gen(ivl_stmt_sub_stmt(net), thread, net, 0, ind + 2);
    AppendStmtCode(forever_gen);
    stmt_code += "    }\n";
}

void StmtGen::show_fork_join_stmt() {
    unsigned cnt = ivl_stmt_block_count(net);

    if (pint_in_task) {
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net), "fork-join in task");
    }

    // parent thread
    if (!PINT_SIMPLE_CODE_EN) {
        stmt_code += "\n// fork_join begin\n";
    }
    thread->pc++;
    start_buff +=
        string_format("int fork_cnt_%d_%d __attribute__((section(\".builtin_simu_buffer\")));\n",
                      thread->id, thread->fork_idx);
    inc_buff += string_format("extern int fork_cnt_%d_%d;\n", thread->id, thread->fork_idx);
    global_init_buff += string_format("    fork_cnt_%d_%d = 0;\n", thread->id, thread->fork_idx);

    for (unsigned idx = 0; idx < cnt; idx += 1) {
        stmt_code += string_format("    pint_enqueue_thread(pint_thread_%d, INACTIVE_Q);\n",
                                   thread->id + idx + 1);
    }
    stmt_code += string_format("    pint_thread_pc_x[%d] = %d;\n", thread->id, thread->pc);
    stmt_code += "    return;\n";
    stmt_code += string_format("PC_%d:\n    ;\n", thread->pc);
    stmt_code +=
        string_format("    if (%d != fork_cnt_%d_%d) return;\n", cnt, thread->id, thread->fork_idx);
    stmt_code += string_format("    fork_cnt_%d_%d = 0;\n", thread->id, thread->fork_idx);
    if (!PINT_SIMPLE_CODE_EN) {
        stmt_code += "// fork_join end\n";
    }

    // child thread
    for (unsigned idx = 0; idx < cnt; idx += 1) {
        string child_expr_buff;
        thread_child_ind[thread->id + idx + 1] = "child_thread";
        child_thread_buff += string_format("\nvoid pint_thread_%d(void){\n", thread->id + idx + 1);
        child_thread_buff +=
            string_format("// fork_join child thread from pint_thread_%d\n", thread->id);
        if (pint_perf_on) {
            unsigned thread_perf_id = global_lpm_num + monitor_lpm_num + global_thread_lpm_num1 + force_lpm_num +
                                      thread->id + idx + 1 + PINT_PERF_BASE_ID;
            child_thread_buff += string_format("    NCORE_PERF_SUMMARY(%d);\n", thread_perf_id);
        }
        thread_declare_buff += string_format("void pint_thread_%d(void);\n", thread->id + idx + 1);
        unsigned curr_pos = child_thread_buff.length();

        // do the statement job
        ivl_statement_t cur = ivl_stmt_block_stmt(net, idx);
        struct pint_thread_info thread_info;
        pint_init_thread_info(&thread_info, thread->id + idx + 1, thread->to_lpm_flag,
                              thread->delay_num, thread->scope, thread->proc);
        StmtGen fork_join_gen(cur, &thread_info, net, idx, ind + 4);
        child_thread_buff += fork_join_gen.GetStmtGenCode();
        child_expr_buff += fork_join_gen.GetStmtDeclCode();
        thread->delay_num = thread_info.delay_num;

        // end process
        child_thread_buff +=
            string_format("    int old_value = pint_atomic_add(&fork_cnt_%d_%d, 1);\n", thread->id,
                          thread->fork_idx);
        child_thread_buff += string_format(
            "    if (%d == old_value) pint_enqueue_thread(pint_thread_%d, INACTIVE_Q);\n\n",
            cnt - 1, thread->id);
        if (thread_info.pc) {
            child_thread_buff +=
                string_format("    pint_thread_pc_x[%d] = 0;\n", thread->id + idx + 1);
        }
        child_thread_buff += "    return;\n";
        child_thread_buff += "}\n";

        if (thread_info.pc) {
            pint_thread_pc_max =
                (thread_info.pc > pint_thread_pc_max) ? thread_info.pc : pint_thread_pc_max;
            child_thread_buff.insert(curr_pos, string_format("    PINT_PROCESS_PC_%d(%d);\n",
                                                             thread_info.pc, thread->id + idx + 1));
        }

        child_expr_buff += "\tunsigned change_flag = 0;\n";
        child_thread_buff.insert(curr_pos, child_expr_buff);
        child_expr_buff.clear();
    }

    // update output
    thread->fork_idx++;
    thread->fork_cnt += cnt;
}

void StmtGen::GenStmtCode() {
    const ivl_statement_type_t type = ivl_statement_type(net);

    switch (type) {
    case IVL_ST_ALLOC:
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net), "alloc stmt");
        break;

    case IVL_ST_ASSIGN:
        show_assign_stmt();
        break;

    case IVL_ST_ASSIGN_NB:
        show_nbassign_stmt();
        break;

    case IVL_ST_BLOCK: {
        unsigned cnt = ivl_stmt_block_count(net);
        for (unsigned idx = 0; idx < cnt; idx += 1) {
            ivl_statement_t cur = ivl_stmt_block_stmt(net, idx);
            StmtGen block_gen(cur, thread, net, idx, ind + 4);
            AppendStmtCode(block_gen);
        }
        break;
    }

    case IVL_ST_CASEX:
        show_case_stmt(1);
        break;

    case IVL_ST_CASEZ:
        show_case_stmt(2);
        break;

    case IVL_ST_CASER:
        show_case_stmt(3);
        break;

    case IVL_ST_CASE:
        show_case_stmt(0);
        break;

    case IVL_ST_CASSIGN:
        show_stmt_cassign(net);
        break;

    case IVL_ST_CONDIT: {
        show_condition_stmt();
        break;
    }

    case IVL_ST_CONTRIB:
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net), "contrib stmt");
        break;

    case IVL_ST_DEASSIGN:
        show_stmt_deassign(net);
        break;

    case IVL_ST_DELAY:
        pint_delay_process(net);
        break;

    case IVL_ST_DELAYX:
        pint_delay_process(net);
        break;

    case IVL_ST_DISABLE:
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net), "disable stmt");
        stmt_code +=
            "    printf(\"Not support disable statement yet, simply stop whole test.\\n\");\n";
        stmt_code += "    pint_stop_simu();\n";

        if (pint_in_task) {
            stmt_code += "\n\treturn -1;\n";
        } else {
            stmt_code += "\n\treturn;\n";
        }

        break;

    case IVL_ST_DO_WHILE: {
        StmtGen do_while_gen(ivl_stmt_sub_stmt(net), thread, net, 0, ind + 2);
        AppendStmtCode(do_while_gen);
        break;
    }

    case IVL_ST_FORCE:
        show_stmt_force(net);
        break;

    case IVL_ST_FORK:
        show_fork_join_stmt();
        break;

    case IVL_ST_FORK_JOIN_ANY: {
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net), "fork-join-any stmt");
        unsigned cnt = ivl_stmt_block_count(net);
        for (unsigned idx = 0; idx < cnt; idx += 1) {
            ivl_statement_t cur = ivl_stmt_block_stmt(net, idx);
            StmtGen fork_join_any_gen(cur, thread, net, idx, ind + 4);
            AppendStmtCode(fork_join_any_gen);
        }
        break;
    }

    case IVL_ST_FORK_JOIN_NONE: {
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net), "fork-join-none stmt");
        unsigned cnt = ivl_stmt_block_count(net);
        for (unsigned idx = 0; idx < cnt; idx += 1) {
            ivl_statement_t cur = ivl_stmt_block_stmt(net, idx);
            StmtGen fork_join_none_gen(cur, thread, net, idx, ind + 4);
            AppendStmtCode(fork_join_none_gen);
        }
        break;
    }

    case IVL_ST_FREE:
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net), "free stmt");
        break;

    case IVL_ST_NOOP:
        break;

    case IVL_ST_RELEASE:
        show_stmt_release(net);
        break;

    case IVL_ST_STASK: {
        show_stask_stmt();
        break;
    }

    case IVL_ST_TRIGGER:
        show_stmt_trigger(net, ind);
        break;

    case IVL_ST_UTASK:
        show_utask_stmt();
        break;

    case IVL_ST_WAIT:
        show_wait_stmt();
        break;

    case IVL_ST_WHILE:
        show_while_stmt();
        break;

    case IVL_ST_REPEAT: {
        pint_repeat_process(net);
        break;
    }
    case IVL_ST_FOREVER:
        show_forever_stmt();
        break;
    default:
        PINT_UNSUPPORTED_ERROR(ivl_stmt_file(net), ivl_stmt_lineno(net),
                               "unknown statement type (%d)", type);
        break;
    }
}
