#pragma once
#ifndef IVL_ANALYSIS_SENSITIVE_H
#define IVL_ANALYSIS_SENSITIVE_H
/* clang-format off */

#include "priv.h"
#include <stdio.h>

int analysis_signal_sensitivity(const ivl_process_t net, 
                               std::set<ivl_signal_t> &deletable_signals);
int analysis_sensitive(const ivl_process_t net, std::set<ivl_signal_t> &deletable_signals);

/* clang-format on */
#endif