#pragma once
#ifndef IVL_MULTI_ASSIGN_H
#define IVL_MULTI_ASSIGN_H
/* clang-format off */

#include "priv.h"
#include <stdio.h>
#include <unordered_map>
#include <unordered_set>

#define MultiDownLimit (3)

// mutil-assign dataflow analysis result
using dfa_massign_t = std::unordered_map<
    ivl_statement_t,
    std::unordered_multimap<ivl_signal_t, unsigned int>>;

using up_down_t = std::unordered_map<
    ivl_statement_t,
    std::unordered_map<ivl_signal_t, unsigned int>>;
using assign_cnt_t = std::unordered_map<ivl_signal_t, unsigned int>;

/******************************************************************************** Std length -100 */
struct pint_mult_info_s{
    up_down_t p_down;
    up_down_t p_up;
    assign_cnt_t p_cnt;
};
typedef struct pint_mult_info_s *pint_mult_info_t;
/*
  dataflow analysis for single process.
  p_down_result/p_up_result: indicate signal down/up pos
  success: return 0
*/
int analysis_massign(
    const ivl_process_t net, 
    dfa_massign_t *p_down_result,
    dfa_massign_t *p_up_result,
    std::set<ivl_signal_t> *p_nb_lval_sigs,
    std::set<ivl_signal_t> *p_lval_sigs);

void display_massign_result(FILE *file, const dfa_massign_t *result_map);

int analysis_massign_v2(
    const ivl_process_t net, 
    up_down_t *p_down,
    up_down_t *p_up, 
    assign_cnt_t *p_assigncnt);

void display_massign_result_2(FILE *file, const up_down_t *p_result_map);

// nxz
using dfa_sig_nxz_t = std::unordered_map<
    ivl_statement_t,
    std::unordered_set<ivl_signal_t>>; 

// int analysis_sig_nxz(const ivl_process_t net, dfa_sig_nxz_t *p_sig_nxz);

/* clang-format on */
#endif
