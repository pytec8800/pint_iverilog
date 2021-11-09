/*
 * Copyright (c) 2007-2013 Stephen Williams (steve@icarus.com)
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
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>
#include <vector>

bool pint_ex_number_has_xz(const char *bits, unsigned width) {
    int i;
    for (i = width - 1; i >= 0; i--) {
        if (bits[i] == 'x' || bits[i] == 'z')
            return true;
    }
    return false;
}

void pint_show_ex_number(string &oper, string &decl, bool *p_is_immediate_num, bool *p_has_xz,
                         const char *name, const char *bits, unsigned width,
                         ex_number_type_t cmd_type, const char *file, unsigned lineno) {
    //  cmd_type:   EX_NUM_DATA       , 0,  generate data only, used by 'show_signal'
    //              EX_NUM_ASSIGN_SIG , 1,  generate assign statement, and only return signal type
    //              EX_NUM_DEFINE_SIG , 2,  generate define statement, and only return signal type
    //              EX_NUM_ASSIGN_AUTO, 17, generate assign statement, and ret type is auto, used by
    //              'statement.c'
    //              EX_NUM_DEFINE_AUTO, 18, generate define statement, and ret type is auto, used by
    //              'show_number_expression'

    bool has_xz = pint_ex_number_has_xz(bits, width);
    if (p_has_xz) {
        *p_has_xz = has_xz;
    }

    int i;
    if (cmd_type & EX_NUM_AUTO_MASK) {
        unsigned width_vld = width;
        for (i = width - 1; i >= 0; i--) {
            if (bits[i] == '0')
                width_vld--;
            else
                break;
        }
        if (width_vld <= 32 && !has_xz) {
            unsigned int value = 0;
            for (i = width_vld - 1; i >= 0; i--) {
                if (bits[i] == '1')
                    value = (value << 1) + 1;
                else
                    value <<= 1;
            }
            switch (cmd_type) {
            case EX_NUM_ASSIGN_AUTO:
                oper += string_format("0x%x", value);
                break;
            case EX_NUM_DEFINE_AUTO:
                decl += string_format("const unsigned int %s = 0x%x;\n", name, value);
                break;
            default:
                break;
            }
            if (p_is_immediate_num) {
                *p_is_immediate_num = true;
            }
            return;
        }
    }
    if (width <= 4) {
        unsigned char value = 0;
        for (i = width - 1; i >= 0; i--) {
            switch (bits[i]) {
            case '0':
                value <<= 1;
                break;
            case '1':
                value = (value << 1) + 1;
                break;
            case 'x':
                value = (value << 1) + 0x11;
                break;
            case 'z':
                value = (value << 1) + 0x10;
                break;
            default:
                PINT_UNSUPPORTED_ERROR(file, lineno, "bit %d is %c.", i, bits[i]);
                break;
            }
        }
        switch (cmd_type) {
        case EX_NUM_DATA:
            oper += string_format("0x%x", value);
            break;
        case EX_NUM_ASSIGN_AUTO:
        case EX_NUM_ASSIGN_SIG:
            oper += string_format("%s = 0x%x;\n", name, value);
            break;
        case EX_NUM_DEFINE_AUTO:
        case EX_NUM_DEFINE_SIG:
            decl += string_format("const unsigned char %s = 0x%x;\n", name, value);
            break;
        default:
            PINT_UNSUPPORTED_ERROR(file, lineno, "cmd_type %d.", cmd_type);
            break;
        }
    } else {
        unsigned is_same_value = 1;
        unsigned int idx = width - 1;
        unsigned int len = width & 0x1f; // len, [1, 32]
        unsigned cnt = (width + 0x1f) >> 5;
        if (len == 0)
            len = 32;
        unsigned int hd, ld;
        char tmp[cnt * 2][12];
        int j;
        for (i = cnt - 1; i >= 0; i--, len = 32) {
            hd = 0;
            ld = 0;
            for (j = len - 1; j >= 0; j--, idx--) {
                switch (bits[idx]) {
                case '0':
                    hd <<= 1;
                    ld <<= 1;
                    break;
                case '1':
                    hd <<= 1;
                    ld = (ld << 1) + 1;
                    break;
                case 'x':
                    hd = (hd << 1) + 1;
                    ld = (ld << 1) + 1;
                    break;
                case 'z':
                    hd = (hd << 1) + 1;
                    ld <<= 1;
                    break;
                default:
                    PINT_UNSUPPORTED_ERROR(file, lineno, " bit %d is %c.\n", i, bits[i]);
                    break;
                }
            }
            sprintf(tmp[cnt + i], "0x%x", hd);
            sprintf(tmp[i], "0x%x", ld);
        }
        switch (cmd_type) {
        case EX_NUM_DATA:
            oper += "{";
            for (unsigned i = 0; i < cnt * 2; i++) {
                oper += string_format("%s, ", tmp[i]);
            }
            oper.erase(oper.end() - 2, oper.end());
            oper += "}";
            break;
        case EX_NUM_ASSIGN_AUTO:
        case EX_NUM_ASSIGN_SIG: 
        {
            is_same_value = 1;
            for (unsigned i = 1; i < cnt * 2; i++) {
                if (strcmp(tmp[0],tmp[i])) {
                    is_same_value = 0;
                    break;
                }
            }

            if (is_same_value) {
                if (!(strcmp(tmp[0],"0x0")) && use_const0_buff) {
                    if (cnt * 2 > pint_const0_max_word) {
                        pint_const0_max_word = cnt * 2;
                    }
                    oper += string_format("\t%s = pint_const_0;\n", name);
                } else {
                    oper += string_format("\tfor(int i = 0; i < %d;i++)%s[i] = %s;\n", cnt * 2, name, tmp[0]);
                }
            } else {
                for (unsigned i = 0; i < cnt * 2; i++) {
                    oper += string_format("\t%s[%u] = %s;\n", name, i, tmp[i]);
                }
            }
            break;
        }

        case EX_NUM_DEFINE_AUTO:
        case EX_NUM_DEFINE_SIG:
        {
            unsigned is_const_0 = 1;
            for (unsigned i = 0; i < cnt * 2; i++) {
                if (strcmp(tmp[i],"0x0")) {
                    is_const_0 = 0;
                    break;
                }
            }
            if (is_const_0 && use_const0_buff) {
                decl += string_format("const unsigned int* %s = pint_const_0;\n", name);
                if (cnt * 2 > pint_const0_max_word) {
                    pint_const0_max_word = cnt * 2;
                }
            } else {
                if (cnt == 1) {
                    decl += string_format("const unsigned int %s[%u] = {%s, %s};\n", name, cnt * 2,
                                        tmp[0], tmp[1]);
                } else {
                    decl += string_format("unsigned int %s[%u] = {", name, cnt * 2);
                    for (unsigned i = 0; i < cnt * 2; i++) {
                        decl += string_format("%s,", tmp[i]);
                    }
                    decl += string_format("};\n");
                }
            }
            break;
        }
        default:
            PINT_UNSUPPORTED_ERROR(file, lineno, "cmd_type %d.\n", cmd_type);
            break;
        }
    }
    if (p_is_immediate_num) {
        *p_is_immediate_num = false;
    }
}

void show_expression(ivl_expr_t net, unsigned ind) {
    (void)net;
    (void)ind;
}

// checked
string code_of_expr_value(ivl_expr_t expr, const char *name, bool sig_nxz) {
    unsigned width = ivl_expr_width(expr);
    int is_signed = ivl_expr_signed(expr);
    const char *wid_type = (width <= 4) ? "char" : "int";
    const char *sign_type = is_signed ? "_s" : "_u";
    const char *ch = (width <= 4) ? "&" : "";
    const char *sig_nxz_s = sig_nxz ? "true" : "false";

    string res = string_format("pint_get_value_%s%s<%s>(%s%s, %u)", wid_type, sign_type, sig_nxz_s,
                               ch, name, width);
    return res;
}

string code_of_sig_value(const char *sig_name, unsigned width, bool is_signed, bool sig_nxz) {
    const char *wid_type = (width <= 4) ? "char" : "int";
    const char *sign_type = is_signed ? "_s" : "_u";
    const char *ch = (width <= 4) ? "&" : "";
    const char *sig_nxz_s = sig_nxz ? "true" : "false";

    string res = string_format("pint_get_value_%s%s<%s>(%s%s, %u)", wid_type, sign_type, sig_nxz_s,
                               ch, sig_name, width);
    return res;
}

string code_of_bit_select(const char *src_name, unsigned src_width, const char *dst_name,
                          unsigned dst_width, const char *base, const char *src_nxz_s) {
    string res;
    if (dst_width <= 4) {
        if (src_width <= 4) {
            res = string_format("pint_get_subarray_char_char<%s>(&%s, &%s, %s, %u, %u);\n",
                                src_nxz_s, dst_name, src_name, base, dst_width, src_width);
        } else {
            res = string_format("pint_get_subarray_int_char<%s>(&%s, %s, %s, %u, %u);\n", src_nxz_s,
                                dst_name, src_name, base, dst_width, src_width);
        }
    } else {
        if (src_width <= 4) {
            res = string_format("pint_get_subarray_char_int<%s>(%s, &%s, %s, %u, %u);\n", src_nxz_s,
                                dst_name, src_name, base, dst_width, src_width);
        } else {
            res = string_format("pint_get_subarray_int_int<%s>(%s, %s, %s, %u, %u);\n", src_nxz_s,
                                dst_name, src_name, base, dst_width, src_width);
        }
    }

    return res;
}

// nxz
extern void pint_value_plusargs(ivl_expr_t expr1, ivl_expr_t expr2, ivl_statement_t stmt,
                                string &ope_code, string &dec_code, const char *ret);
extern void pint_test_plusargs(ivl_expr_t expr1, string &ope_code, const char *ret);

/*********************Expression refactor***********************/
unsigned int ExpressionConverter::express_tmp_var_id = 0;
unsigned int ExpressionConverter::nest_depth = 0;
map<string, vector<string>> ExpressionConverter::design_file_content;
set<pint_sig_info_t> ExpressionConverter::sensitive_signals;

void ExpressionConverter::show_array_expression() {
    PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net), "array expression");
    // is_result_nxz = false;
}

void ExpressionConverter::show_array_pattern_expression() {
    PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net), "array pattern expression");
    // is_result_nxz = false;
}

void ExpressionConverter::show_branch_access_expression() {
    PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net), "branch access expression");
    // is_result_nxz = false;
}

void ExpressionConverter::show_binary_expression() {
    ivl_expr_t oper1 = ivl_expr_oper1(net);
    ivl_expr_t oper2 = ivl_expr_oper2(net);
    unsigned int width = ivl_expr_width(net);
    char op = ivl_expr_opcode(net);
    unsigned expect_width = 0;
    if ((op == '*') || (op == '+') || (op == '-') || (op == '/') || (op == '%')) {
        expect_width = width;
    }
    ExpressionConverter oper1_converter_tmp(oper1, stmt, expect_width, NUM_EXPRE_SIGNAL);
    ExpressionConverter oper2_converter_tmp(oper2, stmt, expect_width, NUM_EXPRE_SIGNAL);
    ExpressionConverter *oper1_converter = &oper1_converter_tmp;
    ExpressionConverter *oper2_converter = &oper2_converter_tmp;

    // exchange oper1 and oper2, to calculate the simpler one first
    if ((op == 'a') || (op == '&') || (op == '*') || (op == 'o') || ((op == '|') && (width == 1))) {
        if (oper1_converter_tmp.Sensitive_Signals()->size() >
            oper2_converter_tmp.Sensitive_Signals()->size()) {
            oper1_converter = &oper2_converter_tmp;
            oper2_converter = &oper1_converter_tmp;
            ivl_expr_t oper_tmp = oper1;
            oper1 = oper2;
            oper2 = oper_tmp;
        }
    }

    bool sig1_nxz = oper1_converter->IsResultNxz();
    bool sig2_nxz = oper2_converter->IsResultNxz();
    bool sig_nxz = sig1_nxz && sig2_nxz;
    bool result_nxz = false; // local var
    is_result_nxz = sig_nxz;

    AppendClearCode(!result_nxz && sig_nxz);

    AppendOperCode(oper1_converter, "oper1");

    if ((op == 'a') || (op == '&') || (op == '*')) {
        if (width <= 4) {
            operation_code += string_format("if (pint_get_value_char_u<%s>(&%s, %d) != 0){",
                                            B2S(sig1_nxz), oper1_converter->GetResultName(), width);
        } else {
            operation_code += string_format("if (pint_get_value_int_u<%s>(%s, %d) != 0){",
                                            B2S(sig1_nxz), oper1_converter->GetResultName(), width);
        }
    }

    if ((op == 'o') || ((op == '|') && (width == 1))) {
        if (width <= 4) {
            operation_code +=
                string_format("if (pint_is_true_char<%s>(&%s, %d)){%s = 1;}else{", B2S(sig1_nxz),
                              oper1_converter->GetResultName(), width, GetResultName());
        } else {
            operation_code +=
                string_format("if (pint_is_true_int<%s>(%s, %d)){%s = 1;}else{", B2S(sig1_nxz),
                              oper1_converter->GetResultName(), width, GetResultName());
        }
    }

    AppendOperCode(oper2_converter, "oper2");

    int is_signed = ivl_expr_signed(net);
    unsigned int cnt = (width + 0x1f) >> 5; // Valid when 'width > 4'
    unsigned int width_oper1 = ivl_expr_width(oper1);
    unsigned int width_oper2 = ivl_expr_width(oper2);
    const char *name0 = GetResultName(); // Result name
    const char *name1 = oper1_converter->GetResultName();
    const char *name2 = oper2_converter->GetResultName();
    int is_ret_bool = pint_binary_ret_bool(op);

    if (width <= 4 || is_ret_bool)
        declare_code += string_format("unsigned char %s;\n", name0);
    else
        declare_code += string_format("unsigned int %s[%u];\n", name0, cnt * 2);

    if (is_ret_bool) {
        width = width_oper1;
        if (ivl_expr_signed(oper1) && ivl_expr_signed(oper2))
            is_signed = 1;
        else
            is_signed = 0;
    }
    const char *sign = is_signed ? "_s" : "_u";
    int is_support = 1;
    string fmt_value_oper2;
    const char *value_oper2 = NULL;
    if (op == 'l' || op == 'r' || op == 'R') {
        if (oper2_converter->IsImmediateNum()) {
            fmt_value_oper2 = name2;
        } else {
            fmt_value_oper2 = code_of_expr_value(oper2, name2, sig2_nxz);
        }
        value_oper2 = fmt_value_oper2.c_str(); // format name of value_oper2
    }

    switch (op) {
    //  Ret char
    case 'e': // ==
        if (width <= 4)
            operation_code += string_format("pint_logical_equality_char<%s>(&%s, &%s, &%s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_logical_equality_int<%s>(&%s, %s, %s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        break;
    case 'n': // !=
        if (width <= 4)
            operation_code += string_format("pint_logical_inquality_char<%s>(&%s, &%s, &%s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_logical_inquality_int<%s>(&%s, %s, %s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        break;
    case 'E': // ===
        if (width <= 4)
            operation_code += string_format("pint_case_equality_char<%s>(&%s, &%s, &%s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_case_equality_int<%s>(&%s, %s, %s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);

        is_result_nxz = true;
        break;
    case 'N': // !==
        if (width <= 4)
            operation_code += string_format("pint_case_inquality_char<%s>(&%s, &%s, &%s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_case_inquality_int<%s>(&%s, %s, %s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);

        is_result_nxz = true;
        break;
    case '>': // >
        if (width <= 4)
            operation_code += string_format("pint_greater_than_char%s<%s>(&%s, &%s, &%s, %u);\n",
                                            sign, B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_greater_than_int%s<%s>(&%s, %s, %s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, name2, width);
        break;
    case '<': // <
        if (width <= 4)
            operation_code += string_format("pint_less_than_char%s<%s>(&%s, &%s, &%s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_less_than_int%s<%s>(&%s, %s, %s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, name2, width);
        break;
    case 'G': // >=
        if (width <= 4)
            operation_code +=
                string_format("pint_greater_than_oreq_char%s<%s>(&%s, &%s, &%s, %u);\n", sign,
                              B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_greater_than_oreq_int%s<%s>(&%s, %s, %s, %u);\n",
                                            sign, B2S(sig_nxz), name0, name1, name2, width);
        break;
    case 'L': // <=
        if (width <= 4)
            operation_code += string_format("pint_less_than_oreq_char%s<%s>(&%s, &%s, &%s, %u);\n",
                                            sign, B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_less_than_oreq_int%s<%s>(&%s, %s, %s, %u);\n",
                                            sign, B2S(sig_nxz), name0, name1, name2, width);
        break;
    case 'a': // &&
        if (width <= 4)
            operation_code += string_format("pint_logical_and_char<%s>(&%s, &%s, &%s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_logical_and_int<%s>(&%s, %s, %s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        operation_code += string_format("}else{ %s = 0;}", GetResultName());
        break;
    case 'o': // ||
        if (width <= 4)
            operation_code += string_format("pint_logical_or_char<%s>(&%s, &%s, &%s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_logical_or_int<%s>(&%s, %s, %s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        operation_code += "}";
        break;
    // Ret char/int
    case '+':
        if (width <= 4)
            operation_code += string_format("pint_add_char%s<%s>(&%s, &%s, &%s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_add_int%s<%s>(%s, %s, %s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, name2, width);
        break;
    case '-':
        if (width <= 4)
            operation_code += string_format("pint_sub_char%s<%s>(&%s, &%s, &%s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_sub_int%s<%s>(%s, %s, %s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, name2, width);
        break;
    case '*':
        if (width <= 4)
            operation_code += string_format("pint_mul_char%s<%s>(&%s, &%s, &%s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_mul_int%s<%s>(%s, %s, %s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, name2, width);
        operation_code += string_format("}else{pint_set_sig_zero<%s>(%s%s, %d);}", B2S(sig_nxz),
                                        (width <= 4) ? "&" : "", GetResultName(), width);
        break;
    case '/':
        if (width <= 4)
            operation_code += string_format("pint_divided_char%s<%s>(&%s, &%s, &%s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_divided_int%s<%s>(%s, %s, %s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, name2, width);

        is_result_nxz = false; // a/b, b may be 0
        break;
    case '%':
        if (width <= 4)
            operation_code += string_format("pint_modulob_char%s<%s>(&%s, &%s, &%s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_modulob_int%s<%s>(%s, %s, %s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, name2, width);

        is_result_nxz = false; // a%b, b may be 0
        break;
    case 'p': // 'a ** b'
    {
        string out_name = (width <= 4) ? string("&") + name0 : name0;
        string in1_name = (width_oper1 <= 4) ? string("&") + name1 : name1;
        string in2_name = (width_oper2 <= 4) ? string("&") + name2 : name2;

        if (ivl_expr_signed(oper1) || ivl_expr_signed(oper2))
            operation_code += string_format(
                "pint_power_s_common<%s>(%s, %u, %s, %u, %u, %s, %u, %u);\n", B2S(sig_nxz),
                out_name.c_str(), width, in1_name.c_str(), width_oper1, ivl_expr_signed(oper1),
                in2_name.c_str(), width_oper2, ivl_expr_signed(oper2));
        else
            operation_code += string_format("pint_power_u_common<%s>(%s, %u, %s, %u, %s, %u);\n",
                                            B2S(sig_nxz), out_name.c_str(), width, in1_name.c_str(),
                                            width_oper1, in2_name.c_str(), width_oper2);

        is_result_nxz = false; // 0**0 etc
        break;
    }
    case 'l': // 'a << b' or 'a <<< b'
        if (width <= 4)
            operation_code += string_format("pint_lshift_char<%s>(&%s, &%s, %s, %u);\n",
                                            B2S(sig_nxz), name0, name1, value_oper2, width);
        else
            operation_code += string_format("pint_lshift_int<%s>(%s, %s, %s, %u);\n", B2S(sig_nxz),
                                            name0, name1, value_oper2, width);
        break;
    case 'r': // 'a >> b'

        if (width <= 4)
            operation_code += string_format("pint_rshift_char_u<%s>(&%s, &%s, %s, %u);\n",
                                            B2S(sig_nxz), name0, name1, value_oper2, width);
        else
            operation_code += string_format("pint_rshift_int_u<%s>(%s, %s, %s, %u);\n",
                                            B2S(sig_nxz), name0, name1, value_oper2, width);
        break;
    case 'R': // 'a >>> b'
        if (width <= 4)
            operation_code += string_format("pint_rshift_char%s<%s>(&%s, &%s, %s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, value_oper2, width);
        else
            operation_code += string_format("pint_rshift_int%s<%s>(%s, %s, %s, %u);\n", sign,
                                            B2S(sig_nxz), name0, name1, value_oper2, width);
        break;
    case '&':
        if (width == 1)
            operation_code +=
                string_format("%s = (%s == 0 || %s == 0) ? 0 : (%s == 1 && %s == 1 ? 1 : 0x11);\n",
                              name0, name1, name2, name1, name2);
        else if (width <= 4)
            operation_code += string_format("pint_bitw_and_char<%s>(&%s, &%s, &%s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_bitw_and_int<%s>(%s, %s, %s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);

        operation_code += string_format("}else{pint_set_sig_zero<%s>(%s%s, %d);}", B2S(sig_nxz),
                                        (width <= 4) ? "&" : "", GetResultName(), width);
        break;
    case '|':
        if (width == 1)
            operation_code +=
                string_format("%s = (%s == 1 || %s == 1) ? 1 : (%s == 0 && %s == 0 ? 0 : 0x11);\n",
                              name0, name1, name2, name1, name2);
        else if (width <= 4)
            operation_code += string_format("pint_bitw_or_char<%s>(&%s, &%s, &%s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_bitw_or_int<%s>(%s, %s, %s, %u);\n", B2S(sig_nxz),
                                            name0, name1, name2, width);
        if (width == 1) {
            operation_code += "}";
        }
        break;
    case '^':
        if (width <= 4)
            operation_code += string_format("pint_bitw_exclusive_or_char<%s>(&%s, &%s, &%s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_bitw_exclusive_or_int<%s>(%s, %s, %s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        break;
    case 'A': // 'a ~& b'
        if (width <= 4)
            operation_code += string_format("pint_bitw_nand_char<%s>(&%s, &%s, &%s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_bitw_nand_int<%s>(%s, %s, %s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        break;
    case 'O': // 'a ~| b'
        if (width <= 4)
            operation_code += string_format("pint_bitw_nor_char<%s>(&%s, &%s, &%s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_bitw_nor_int<%s>(%s, %s, %s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        break;
    case 'X': // 'a ~^ b'
        if (width <= 4)
            operation_code += string_format("pint_bitw_equivalence_char<%s>(&%s, &%s, &%s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        else
            operation_code += string_format("pint_bitw_equivalence_int<%s>(%s, %s, %s, %u);\n",
                                            B2S(sig_nxz), name0, name1, name2, width);
        break;
    default:
        is_support = 0;
        break;
    }
    if (is_support == 0) {
        PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net),
                               "binary expression opcode: %c", op);
        is_result_nxz = false;
    }
}

void ExpressionConverter::show_binary_expression_real() {
    ivl_expr_t oper1 = ivl_expr_oper1(net);
    ivl_expr_t oper2 = ivl_expr_oper2(net);
    ExpressionConverter oper1_converter(oper1, stmt, 0, NUM_EXPRE_SIGNAL);
    ExpressionConverter oper2_converter(oper2, stmt, 0, NUM_EXPRE_SIGNAL);
    bool sig1_nxz = oper1_converter.IsResultNxz();
    bool sig2_nxz = oper2_converter.IsResultNxz();
    is_result_nxz = true;
    AppendOperCode(&oper1_converter, "oper1");
    AppendOperCode(&oper2_converter, "oper2");
    char op = ivl_expr_opcode(net);

    unsigned int width = ivl_expr_width(net);
    unsigned int width_oper1 = ivl_expr_width(oper1);
    unsigned int width_oper2 = ivl_expr_width(oper2);

    string name1;
    if (width_oper1 > width) {
        declare_code += string_format("float %s_oper1;\n", GetResultName());
        name1 = string_format("%s_oper1", GetResultName());
        if (width_oper1 <= 4) {
            operation_code +=
                string_format("pint_sig_to_real_char<%s>(&%s_oper1, &%s, %u);\n", B2S(sig1_nxz),
                              GetResultName(), oper1_converter.GetResultName(), width_oper1);
        } else {
            operation_code +=
                string_format("pint_sig_to_real_int<%s>(&%s_oper1, %s, %u);\n", B2S(sig1_nxz),
                              GetResultName(), oper1_converter.GetResultName(), width_oper1);
        }
    } else {
        name1 = oper1_converter.GetResultName();
    }

    string name2;
    if (width_oper2 > width) {
        declare_code += string_format("float %s_oper2;\n", GetResultName());
        name2 = string_format("%s_oper2", GetResultName());
        if (width_oper2 <= 4) {
            operation_code +=
                string_format("pint_sig_to_real_char<%s>(&%s_oper2, &%s, %u);\n", B2S(sig2_nxz),
                              GetResultName(), oper2_converter.GetResultName(), width_oper2);
        } else {
            operation_code +=
                string_format("pint_sig_to_real_int<%s>(&%s_oper2, %s, %u);\n", B2S(sig2_nxz),
                              GetResultName(), oper2_converter.GetResultName(), width_oper2);
        }
    } else {
        name2 = oper2_converter.GetResultName();
    }

    switch (op) {
    case '+':
    case '-':
    case '*':
    case '/':
        declare_code += string_format("float %s;\n", GetResultName());
        operation_code +=
            string_format("%s = %s %c %s;\n", GetResultName(), name1.c_str(), op, name2.c_str());

        // if (op == '/') {
        //     is_result_nxz = false;  // a/b, b may be 0, will cause coredump
        // }
        break;
    default:
        PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net),
                               "binary real expression opcode: %c", op);
        is_result_nxz = false;
        break;
    }
}

void ExpressionConverter::show_enumtype_expression() {
    PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net), "enumtype expression");
    // is_result_nxz = false;
}

void ExpressionConverter::show_function_call() {
    ivl_scope_t def = ivl_expr_def(net);
    unsigned width = ivl_expr_width(net);
    unsigned idx;
    string func_call_code;

    if (ivl_expr_value(net) == IVL_VT_REAL) {
        declare_code += string_format("float %s;\n", GetResultName());
    } else if (width <= 4) {
        declare_code += string_format("unsigned char %s;\n", GetResultName());
    } else {
        int word_num = (width + 31) / 32 * 2;
        declare_code += string_format("unsigned int %s[%d];\n", GetResultName(), word_num);
    }
    string comment;
    func_call_code = string_format("%s(%s,", ivl_scope_name_c(def), GetResultName());
    invoked_fun_names.insert(ivl_scope_name_c(def));
    for (idx = 0; idx < ivl_expr_parms(net); idx += 1) {
        ivl_expr_t param = ivl_expr_parm(net, idx);
        unsigned para_width = ivl_signal_width(ivl_scope_port(def, idx + 1)); // idx 0 is for output
        ExpressionConverter oper_converter(param, stmt, para_width, NUM_EXPRE_SIGNAL, true);
        comment = string_format("para%d", idx);
        AppendOperCode(&oper_converter, comment.c_str());
        func_call_code += (idx > 0) ? ", " : "";
        func_call_code += string_format("%s", oper_converter.GetResultName());
    }
    func_call_code += ");\n";
    operation_code += func_call_code;

    if (ivl_expr_value(net) == IVL_VT_REAL) {
        is_result_nxz = true;
    } else {
        // is_result_nxz = false;
    }
}

void ExpressionConverter::show_sysfunc_call() {
    unsigned int width = ivl_expr_width(net);
    const char *result_name_char = GetResultName();
    if (!strcmp(ivl_expr_name(net), "$time") || !strcmp(ivl_expr_name(net), "$realtime")) {
        if (!strcmp(ivl_expr_name(net), "$realtime")) {
            PINT_UNSUPPORTED_WARN(ivl_expr_file(net), ivl_expr_lineno(net),
                                  "$realtime is not implemented yet.");
        }
        unsigned tu_mul = 1;
        unsigned tu_mul_2 = 1;
        unsigned int time_unit_diff = pint_current_scope_time_unit - pint_design_time_precision;
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

        // for (int i = time_unit_diff; i > 0; i--) {
        //     tu_mul *= 10;
        // }
        if (time_unit_diff <= 9 && time_unit_diff > 0) {
            operation_code += string_format(
                "\tunsigned simuT_tmp[2] = {((unsigned*)&T)[0], ((unsigned*)&T)[1]};\n"
                "\tsimu_time_t simuT = %lu * simuT_tmp[1] + simuT_tmp[0]/%u + (simuT_tmp[1] * %u + "
                "simuT_tmp[0]%%%u)/%u;\n",
                0x100000000L / tu_mul, tu_mul, 0x100000000L % tu_mul, tu_mul, tu_mul);

            // unsigned simuT_tmp[2] = {((unsigned*)&T)[0], ((unsigned*)&T)[1]};
            // simu_time_t simuT = 1 * simuT_tmp[1] + simuT_tmp[0]/3567587328 + (simuT_tmp[1] *
            // 727379968 + simuT_tmp[0]%3567587328)/3567587328;
        } else if (time_unit_diff > 9) {
            operation_code += string_format(
                "\tunsigned simuT_tmp[2] = {((unsigned*)&T)[0], ((unsigned*)&T)[1]};\n"
                "\tsimu_time_t simuT = %lu * simuT_tmp[1] + simuT_tmp[0]/%u + (simuT_tmp[1] * %u + "
                "simuT_tmp[0]%%%u)/%u;\n",
                0x100000000L / tu_mul, tu_mul, 0x100000000L % tu_mul, tu_mul, tu_mul);
            operation_code += string_format(
                "\tunsigned simuT_tmp_2[2] = {((unsigned*)&simuT)[0], ((unsigned*)&simuT)[1]};\n"
                "\tsimuT = %lu * simuT_tmp_2[1] + simuT_tmp_2[0]/%u + (simuT_tmp_2[1] * %u + "
                "simuT_tmp_2[0]%%%u)/%u;\n",
                0x100000000L / tu_mul_2, tu_mul_2, 0x100000000L % tu_mul_2, tu_mul_2, tu_mul_2);
        }

        if (numExprType == NUM_EXPRE_SIGNAL) {
            declare_code += string_format(
                "unsigned int %s[4] = {((unsigned*)&T)[0], ((unsigned*)&T)[1], 0, 0};\n",
                result_name_char); // width_T = 64bit
            if (time_unit_diff > 0) {
                operation_code +=
                    string_format("pint_copy_value(%s, simuT, 64);\n", result_name_char);
            } else {
                operation_code += string_format("pint_copy_value(%s, T, 64);\n", result_name_char);
            }
        } else {
            if (time_unit_diff > 0) {
                result_name = "simuT";
            } else {
                result_name = "T";
            }
        }

        is_result_nxz = true;
    } else if (!strcmp(ivl_expr_name(net), "$fopen")) {
        ivl_expr_t param = ivl_expr_parm(net, 0);
        ivl_expr_t arg;
        string file_name;
        string open_arg;
        pint_show_expression_string(declare_code, operation_code, param, stmt,
                                    pint_process_str_idx);
        file_name = string_format("str_%u", pint_process_str_idx++);
        if (ivl_expr_parms(net) > 1) {
            arg = ivl_expr_parm(net, 1);
            pint_show_expression_string(declare_code, operation_code, arg, stmt,
                                        pint_process_str_idx);
            open_arg = string_format("str_%u", pint_process_str_idx++);
        } else {
            open_arg = "\"0\"";
        }

        declare_code += string_format("unsigned int %s[2];\n", result_name_char);
        if (pint_pli_mode) {
            operation_code += string_format("    {char tmp_name[1024];\n"
                                            "    int offset = 0;\n"
                                            "    int i = 0;\n"
                                            "    while(%s[i]) {tmp_name[i+offset] = %s[i]; i++;}\n"
                                            "    tmp_name[i+offset] = 0;\n",
                                            file_name.c_str(), file_name.c_str());
        } else {
            operation_code += string_format(
                "    {char tmp_name[1024];\n"
                "    int offset = 0;\n"
                "    int i = 0;\n"
                "    if (%s[0] != '/') {\n"
                "    tmp_name[0] = '.';tmp_name[1] = '.';tmp_name[2] = '/';tmp_name[3] = "
                "'.';tmp_name[4] = '.';tmp_name[5] = '/';\n"
                "    offset = 6;}\n"
                "    while(%s[i]) {tmp_name[i+offset] = %s[i]; i++;}\n"
                "    tmp_name[i+offset] = 0;\n",
                file_name.c_str(), file_name.c_str(), file_name.c_str());
        }
        operation_code += string_format("    pint_simu_vpi(PINT_SEND_MSG, %d, \"$fopen\", "
                                        "tmp_name, &pint_fd[%d], %s, 0, 0);}\n",
                                        pint_vpimsg_num++, pint_open_file_num, open_arg.c_str());
        operation_code += string_format("    %s[0] = pint_fd[%d];\n    %s[1] = 0;\n",
                                        result_name_char, pint_open_file_num, result_name_char);
        pint_open_file_num++;

        is_result_nxz = true;
    } else if (!strcmp(ivl_expr_name(net), "$fgetc")) {
        ivl_signal_t sig = ivl_expr_signal(ivl_expr_parm(net, 0));
        pint_sig_info_t sig_info = pint_find_signal(sig);
        declare_code += string_format("static unsigned int %s[2];\n", result_name_char);
        operation_code +=
            string_format("    pint_simu_vpi(PINT_SEND_MSG, %d, \"$fgetc\", %s[0], %s, 0, 0, 0);\n",
                          pint_vpimsg_num++, sig_info->sig_name.c_str(), result_name_char);

        // code = $ungetc(chr, fd);

        is_result_nxz = true;
    } else if (!strcmp(ivl_expr_name(net), "$ungetc")) {
        // pint_sig_info_t sig_info_chr = pint_find_signal(ivl_expr_signal(ivl_expr_parm(net, 0)));
        ExpressionConverter oper_converter_chr(ivl_expr_parm(net, 0), stmt, 0);
        string comment = string_format("oper%d", 0);
        AppendOperCode(&oper_converter_chr, comment.c_str());

        string sig_info_chr;
        if (!oper_converter_chr.IsImmediateNum()) {
            sig_info_chr = string_format("%s[0]", oper_converter_chr.GetResultName());
        } else {
            sig_info_chr = oper_converter_chr.GetResultName();
        }

        pint_sig_info_t sig_info_fd = pint_find_signal(ivl_expr_signal(ivl_expr_parm(net, 1)));

        declare_code += string_format("static unsigned int %s[2];\n", result_name_char);
        operation_code += string_format(
            "    pint_simu_vpi(PINT_SEND_MSG, %d, \"$ungetc\", %s[0], %s[0], %s, 0, 0);\n",
            pint_vpimsg_num++, sig_info_fd->sig_name.c_str(), sig_info_chr.c_str(),
            result_name_char);

        // code = $fgets(str, fd);

        is_result_nxz = true;
    } else if (!strcmp(ivl_expr_name(net), "$fgets")) {
        pint_sig_info_t sig_info_str = pint_find_signal(ivl_expr_signal(ivl_expr_parm(net, 0)));
        pint_sig_info_t sig_info_fd = pint_find_signal(ivl_expr_signal(ivl_expr_parm(net, 1)));

        declare_code += string_format("static unsigned int %s[2];\n", result_name_char);
        operation_code += string_format(
            "    pint_simu_vpi(PINT_SEND_MSG, %d, \"$fgets\", %s[0], %s, %s, %d, 0);\n",
            pint_vpimsg_num++, sig_info_fd->sig_name.c_str(), sig_info_str->sig_name.c_str(),
            result_name_char, sig_info_str->width);

        is_result_nxz = true;
    } else if (!strcmp(ivl_expr_name(net), "$feof") || !strcmp(ivl_expr_name(net), "$ftell") ||
               !strcmp(ivl_expr_name(net), "$rewind")) {
        ExpressionConverter oper_converter_fd(ivl_expr_parm(net, 0), stmt, 0);
        string comment = string_format("oper%d", 0);
        AppendOperCode(&oper_converter_fd, comment.c_str());

        string sig_info_fd;
        if (!oper_converter_fd.IsImmediateNum()) {
            sig_info_fd = string_format("%s[0]", oper_converter_fd.GetResultName());
        } else {
            sig_info_fd = oper_converter_fd.GetResultName();
        }
        declare_code += string_format("static unsigned int %s[2];\n", result_name_char);
        operation_code += string_format(
            "    pint_simu_vpi(PINT_SEND_MSG, %d, \"%s\", %s, %s, 0, 0, 0);\n", pint_vpimsg_num++,
            ivl_expr_name(net), sig_info_fd.c_str(), result_name_char);

        // result = $fseek(fd, 0, 0);

        is_result_nxz = true;
    } else if (!strcmp(ivl_expr_name(net), "$fseek")) {
        string sig_info_fd;
        declare_code += string_format("static unsigned int %s[2];\n", result_name_char);
        declare_code += string_format("static unsigned int %s_offset[2];\n", result_name_char);
        declare_code += string_format("static unsigned int %s_whence[2];\n", result_name_char);

        ExpressionConverter oper_converter_fd(ivl_expr_parm(net, 0), stmt, 0);
        string comment = string_format("oper%d", 0);
        AppendOperCode(&oper_converter_fd, comment.c_str());

        if (!oper_converter_fd.IsImmediateNum()) {
            sig_info_fd = string_format("%s[0]", oper_converter_fd.GetResultName());
        } else {
            sig_info_fd = oper_converter_fd.GetResultName();
        }

        ExpressionConverter oper_converter(ivl_expr_parm(net, 1), stmt, 0);
        comment = string_format("oper%d", 1);
        AppendOperCode(&oper_converter, comment.c_str());

        if (!oper_converter.IsImmediateNum()) {
            operation_code += string_format(
                "    %s_offset[0] = %s[0];\n     %s_offset[1] = %s[1];\n", result_name_char,
                oper_converter.GetResultName(), result_name_char, oper_converter.GetResultName());
        } else {
            operation_code +=
                string_format("    %s_offset[0] = %s;\n    %s_offset[1] = 0;\n", result_name_char,
                              oper_converter.GetResultName(), result_name_char);
        }
        ExpressionConverter oper_converter_1(ivl_expr_parm(net, 2), stmt, 0);
        comment = string_format("oper%d", 2);
        AppendOperCode(&oper_converter_1, comment.c_str());
        if (!oper_converter_1.IsImmediateNum()) {
            operation_code +=
                string_format("    %s_whence[0] = %s[0];\n     %s_whence[1] = %s[1];\n",
                              result_name_char, oper_converter_1.GetResultName(), result_name_char,
                              oper_converter_1.GetResultName());
        } else {
            operation_code +=
                string_format("    %s_whence[0] = %s;\n    %s_whence[1] = 0;\n", result_name_char,
                              oper_converter_1.GetResultName(), result_name_char);
        }
        operation_code += string_format("    pint_simu_vpi(PINT_SEND_MSG, %d, \"$fseek\", %s, "
                                        "%s_offset[0], %s_whence[0], %s, 0);\n",
                                        pint_vpimsg_num++, sig_info_fd.c_str(), result_name_char,
                                        result_name_char, result_name_char);

        is_result_nxz = true;
    } else if (!strcmp(ivl_expr_name(net), "$random")) {
        if (ivl_expr_parms(net) > 0) {
            PINT_UNSUPPORTED_WARN(ivl_expr_file(net), ivl_expr_lineno(net), "$random with seed");
        }

        width = (width > 31) ? 31 : width;
        unsigned int range = (1L << width);
        if (width <= 4) {
            declare_code += string_format("unsigned char %s;\n", result_name_char);
            operation_code += string_format("%s = pint_random()%%%u;\n", result_name_char, range);
        } else {
            declare_code += string_format("unsigned int %s[%u];\n", result_name_char, 2);
            operation_code +=
                string_format("%s[0] = pint_random()%%%u;\n", result_name_char, range);
            operation_code += string_format("%s[1] = 0;\n", result_name_char);
        }

        is_result_nxz = true;
    } else if (!strcmp(ivl_expr_name(net), "$urandom")) {
        width = (width > 31) ? 31 : width;

        unsigned int range = (1L << width);
        if (width <= 4) {
            declare_code += string_format("unsigned char %s;\n", result_name_char);
            operation_code += string_format("%s = pint_urandom()%%%u;\n", result_name_char, range);
        } else {
            declare_code += string_format("unsigned int %s[%u];\n", result_name_char, 2);
            operation_code +=
                string_format("%s[0] = pint_urandom()%%%u;\n", result_name_char, range);
            operation_code += string_format("%s[1] = 0;\n", result_name_char);
        }

        is_result_nxz = true;
    } else if (!strcmp(ivl_expr_name(net), "$value$plusargs")) {
        int para_num = ivl_expr_parms(net);
        if (para_num == 2) {
            declare_code += string_format("unsigned %s[2];\n", result_name_char);
            string tmp = string_format("%s[0]", result_name_char);
            // nxz
            pint_value_plusargs(ivl_expr_parm(net, 0), ivl_expr_parm(net, 1), stmt, operation_code,
                                declare_code, tmp.c_str());
            operation_code += string_format("%s[1] = 0;\n", result_name_char);

            is_result_nxz = true;
        } else {
            PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net),
                                   "$varlue$plugars has %d param, only support 2!", para_num);
            is_result_nxz = false;
        }
    } else if (!strcmp(ivl_expr_name(net), "$test$plusargs")) {
        int para_num = ivl_expr_parms(net);
        if (para_num == 1) {
            declare_code += string_format("unsigned %s[2];\n", result_name_char);
            string tmp = string_format("%s[0]", result_name_char);
            pint_test_plusargs(ivl_expr_parm(net, 0), operation_code, tmp.c_str());
            operation_code += string_format("%s[1] = 0;\n", result_name_char);

            is_result_nxz = true;
        } else {
            PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net),
                                   "$test$plugars has %d param, only support 1!", para_num);
            is_result_nxz = false;
        }
    } else {
        static unsigned err = 0;
        if (width <= 4) {
            declare_code += string_format("unsigned char err_%d = 0;\n", err);
        } else {
            declare_code += string_format("unsigned int err_%d[%u];\n", err, width);
        }
        PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net), "system func %s ",
                               ivl_expr_name(net));
        err++;

        is_result_nxz = false;
    }
}

void ExpressionConverter::show_memory_expression() {
    PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net), "memory expression");
    // is_result_nxz = false;
}

void ExpressionConverter::show_new_expression() {
    PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net), "new expression");
    // is_result_nxz = false;
}

void ExpressionConverter::show_null_expression() {
    PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net), "null expression");
    // is_result_nxz = false;
}

void ExpressionConverter::show_property_expression() {
    PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net), "property expression");
    // is_result_nxz = false;
}

void ExpressionConverter::show_number_expression() {
    unsigned int width = ivl_expr_width(net);
    const char *bits = ivl_expr_bits(net);
    int is_signed = ivl_expr_signed(net);
    ex_number_type_t type =
        (numExprType == NUM_EXPRE_SIGNAL) ? EX_NUM_DEFINE_SIG : EX_NUM_DEFINE_AUTO;
    bool has_xz;
    pint_show_ex_number(operation_code, declare_code, &is_immediate_num, &has_xz, GetResultName(),
                        bits, width, type, ivl_expr_file(net), ivl_expr_lineno(net));
    is_result_nxz = !has_xz;

    if ((is_immediate_num) && (numExprType == NUM_EXPRE_DIGIT)) {
        operation_code = "";
        declare_code = "";
        int imme_value = GetImmediateNumValue(net);
        if (is_signed) {
            result_name = string_format("%d", imme_value);
        } else {
            result_name = string_format("%u", (unsigned)imme_value);
        }
    }
}

void ExpressionConverter::show_delay_expression() {
    if (numExprType == NUM_EXPRE_DIGIT) {
        result_name = string_format("%u", ivl_expr_delay_val(net));
    } else {
        declare_code += string_format("unsigned %s;\n", GetResultName());
        operation_code += string_format("%s = %u;\n", GetResultName(), ivl_expr_delay_val(net));
    }
    is_immediate_num = true;
    is_result_nxz = true;
}

void ExpressionConverter::show_real_number_expression() {
    double real_value = ivl_expr_dvalue(net);
    declare_code += string_format("float %s = %f;\n", GetResultName(), real_value);
    is_result_nxz = true;
}

void ExpressionConverter::show_string_expression() {
    const char *value = ivl_expr_string(net);

    if (numExprType == NUM_EXPRE_SIGNAL) {
        unsigned str_len = ivl_expr_width(net) / 8;
        int half_word_num = (str_len + 3) / 4;
        unsigned *tmp_buff = (unsigned *)malloc(half_word_num * sizeof(unsigned) * 2);
        memset(tmp_buff, 0, half_word_num * sizeof(unsigned) * 2);

        int const_len = strlen(value);
        vector<char> zy_str;
        for (int i = 0; i < const_len; i++) {
            if (value[i] == '\\') {
                char tmp_c = 0;
                for (int j = i + 1; j < i + 4; j++) {
                    tmp_c = 8 * tmp_c;
                    if (j < const_len) {
                        tmp_c += (value[j] - '0');
                    }
                }
                zy_str.push_back(tmp_c);
                i = i + 3;
            } else {
                zy_str.push_back(value[i]);
            }
        }
        int real_len = zy_str.size();
        for (int i = 0; i < real_len; i++) {
            ((char *)tmp_buff)[i] = zy_str[real_len - 1 - i];
        }

        declare_code +=
            string_format("static unsigned int %s[%u] = {", GetResultName(), half_word_num * 2);

        for (int i = 0; i < half_word_num; i++) {
            declare_code += string_format("0x%08x, ", tmp_buff[i]);
        }

        declare_code += "};\n";
    } else {
        declare_code += "const char* " + string(GetResultName()) + " = \"" + value + "\";\n";
    }
    is_result_nxz = true;
}

void ExpressionConverter::show_select_expression() {
    int is_signed = ivl_expr_signed(net);
    unsigned width = ivl_expr_width(net);
    const char *sign = is_signed ? "_s" : "_u";
    ivl_expr_t oper1 = ivl_expr_oper1(net);
    ivl_expr_t oper2 = ivl_expr_oper2(net);
    ExpressionConverter oper1_converter(oper1, stmt, 0, NUM_EXPRE_SIGNAL);
    bool sig1_nxz = oper1_converter.IsResultNxz();
    bool result_nxz = false; // local var
    unsigned int width_oper1 = ivl_expr_width(oper1);
    if (ivl_expr_value(oper1) != IVL_VT_STRING && !oper2)
        if (!is_signed && (width_oper1 == width || (width_oper1 - 1) / 32 == (width - 1) / 32))
            oper1_converter.is_pad_declare = true;
    AppendOperCode(&oper1_converter, "oper1");

    if (ivl_expr_value(oper1) == IVL_VT_STRING) {
        PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net), "Select bits from string");
        // is_result_nxz = false;
    } else if (oper2) {
        ExpressionConverter oper2_converter(oper2, stmt, 0, NUM_EXPRE_DIGIT);
        bool sig2_nxz = oper2_converter.IsResultNxz();
        AppendOperCode(&oper2_converter, "oper2");

        if (ivl_expr_width(net) <= 4) {
            declare_code += string_format("unsigned char %s;\n", GetResultName());
        } else {
            unsigned words = (ivl_expr_width(net) / 32 + (ivl_expr_width(net) % 32 ? 1 : 0)) * 2;
            declare_code += string_format("unsigned int %s[%d];\n", GetResultName(), words);
        }

        if (!oper2_converter.IsImmediateNum()) {
            string value_name = string_format("value_of_%s", oper2_converter.GetResultName());
            operation_code += string_format(
                "int %s = %s;\n", value_name.c_str(),
                code_of_expr_value(oper2, oper2_converter.GetResultName(), sig2_nxz).c_str());

            operation_code += string_format("if (%s != VALUE_XZ){\n", value_name.c_str());

            AppendClearCode(!result_nxz && sig1_nxz);

            operation_code += code_of_bit_select(
                oper1_converter.GetResultName(), ivl_expr_width(oper1), GetResultName(),
                ivl_expr_width(net), value_name.c_str(), B2S(sig1_nxz));

            operation_code +=
                string_format("}else{\npint_set_sig_x(%s%s, %d);", (width <= 4) ? "&" : "",
                              GetResultName(), ivl_expr_width(net));
            operation_code += "}\n";

            is_result_nxz = false;
        } else {
            AppendClearCode(!result_nxz && sig1_nxz);

            operation_code += code_of_bit_select(
                oper1_converter.GetResultName(), ivl_expr_width(oper1), GetResultName(),
                ivl_expr_width(net), oper2_converter.GetResultName(), B2S(sig1_nxz));

            int base = GetImmediateNumValue(oper2);
            unsigned src_width = ivl_expr_width(oper1);
            unsigned dst_width = ivl_expr_width(net);
            if ((base >= 0) && (base + dst_width <= src_width)) {
                is_result_nxz = sig1_nxz;
            } else {
                is_result_nxz = false;
            }
        }

        return;
    } else { // Type: pad
        if (width <= 4) { // width_oper1 <= 4
            if (oper1_converter.is_pad_declare) {
                result_name = oper1_converter.GetResultName();
            } else if (is_signed) {
                declare_code += string_format("unsigned char %s;\n", GetResultName());
                operation_code += string_format(
                    "pint_pad_char_to_char_s<%s>(&%s, &%s, %u, %u);\n", B2S(sig1_nxz),
                    GetResultName(), oper1_converter.GetResultName(), width_oper1, width);
            } else {
                declare_code += string_format("unsigned char %s;\n", GetResultName());
                operation_code +=
                    string_format("%s = %s;\n", GetResultName(), oper1_converter.GetResultName());
            }
        } else {
            unsigned int cnt = (width + 0x1f) >> 5;
            if (width_oper1 <= 4) {
                declare_code += string_format("unsigned int  %s[%d];\n", GetResultName(), cnt * 2);

                AppendClearCode(!result_nxz && sig1_nxz);

                operation_code += string_format(
                    "pint_pad_char_to_int%s<%s>(%s, &%s, %d, %d);\n", sign, B2S(sig1_nxz),
                    GetResultName(), oper1_converter.GetResultName(), width_oper1, width);
            } else {
                if (oper1_converter.is_pad_declare) {
                    result_name = oper1_converter.GetResultName();
                } else {
                    declare_code +=
                        string_format("unsigned int  %s[%d];\n", GetResultName(), cnt * 2);

                    AppendClearCode(!result_nxz && sig1_nxz);

                    operation_code += string_format(
                        "pint_pad_int_to_int%s<%s>(%s, %s, %d, %d);\n", sign, B2S(sig1_nxz),
                        GetResultName(), oper1_converter.GetResultName(), width_oper1, width);
                }
            }
        }

        is_result_nxz = sig1_nxz;
    }
}

void ExpressionConverter::show_shallowcopy() {
    PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net), "shallowcopy expression");
    // is_result_nxz = false;
}

void ExpressionConverter::show_signal_expression() {
    unsigned width = ivl_expr_width(net);
    ivl_signal_t sig = ivl_expr_signal(net);
    // nxz
    bool sig_nxz = pint_proc_sig_nxz_get(stmt, sig);
    bool result_nxz = false; // local var
    is_result_nxz = sig_nxz;
    pint_sig_info_t sig_info = pint_find_signal(sig);
    ivl_expr_t word = ivl_expr_oper1(net);
    int word_num = (width + 31) / 32 * 2;
    int arr_words = (int)sig_info->arr_words;
    string proc_sig_name;
    sensitive_signals.insert(sig_info);
    if (arr_words == 1) {
        // if (!word) {
        proc_sig_name = pint_proc_get_sig_name(sig, "", 1);
        if ((is_must_gen_tmp == false) || (width <= 4)) {
            result_name = proc_sig_name;
        } else {
            declare_code += string_format("unsigned int %s[%u];\n", GetResultName(), word_num);

            AppendClearCode(!result_nxz && sig_nxz);

            operation_code += string_format("pint_copy_int<%s>(%s, %s, %u);\n", B2S(sig_nxz),
                                            GetResultName(), proc_sig_name.c_str(), width);
        }
        // } else {
        //     // a[0:0] has bug
        // }
    } else {         // Can not be mult_assign
        if (!word) { //  can not get arr_idx from expression
            int arr_idx = pint_get_arr_idx_from_sig(sig);
            assert(arr_idx != -1);
            proc_sig_name =
                pint_proc_get_sig_name(sig, string_format("%u", (unsigned)arr_idx).c_str(), 1);
            if ((is_must_gen_tmp == false) || (width <= 4)) {
                result_name = proc_sig_name;
            } else {
                declare_code += string_format("unsigned int %s[%u];\n", GetResultName(), word_num);

                AppendClearCode(!result_nxz && sig_nxz);

                operation_code += string_format("pint_copy_int<%s>(%s, %s, %u);\n", B2S(sig_nxz),
                                                GetResultName(), proc_sig_name.c_str(), width);
            }
        } else {
            ExpressionConverter oper1_converter(word, stmt, 0);
            string type;
            string ch;
            string w_type;
            string w_ch;
            unsigned w_width = ivl_expr_width(word);

            if (width <= 4) {
                type = "char";
                ch = "&";
            } else {
                type = "int";
                ch = "";
            }

            if (oper1_converter.IsImmediateNum()) {
                int arr_idx = GetImmediateNumValue(word);
                // asume no array has more than 2G elements, truncate arr_idx to 2G
                if ((ivl_expr_signed(word) == 0) && (arr_idx < 0)) {
                    arr_idx = 0x7fffffff;
                }

                proc_sig_name =
                    pint_proc_get_sig_name(sig, string_format("%u", (unsigned)arr_idx).c_str(), 1);
                if ((is_must_gen_tmp == false) || (width <= 4)) {
                    if (width <= 4) {
                        declare_code += string_format("unsigned char %s;\n", GetResultName());
                    } else {
                        declare_code += string_format("unsigned int* %s;\n", GetResultName());
                    }
                    if (arr_idx < arr_words) {
                        pint_proc_mult_opt_before_sig_use(operation_code, sig, arr_idx);
                        operation_code +=
                            string_format("%s = %s;\n", GetResultName(), proc_sig_name.c_str());

                    } else {
                        operation_code +=
                            string_format("pint_set_sig_x(%s%s, %d);\n", (width <= 4) ? "&" : "",
                                          GetResultName(), width);
                        is_result_nxz = false;
                    }
                } else {
                    declare_code +=
                        string_format("unsigned int %s[%d];\n", GetResultName(), word_num);

                    if (arr_idx < arr_words) {
                        AppendClearCode(!result_nxz && sig_nxz);
                        pint_proc_mult_opt_before_sig_use(operation_code, sig, arr_idx);
                        operation_code += string_format(
                            "pint_copy_%s<%s>(%s%s, %s%s, %u);\n", type.c_str(), B2S(sig_nxz),
                            ch.c_str(), GetResultName(), ch.c_str(), proc_sig_name.c_str(), width);
                    } else {
                        operation_code += string_format("pint_set_sig_x(%s%s, %u);\n", ch.c_str(),
                                                        GetResultName(), width);
                        is_result_nxz = false;
                    }
                }
            } else {
                AppendOperCode(&oper1_converter, "word");
                if (w_width <= 4) {
                    w_type = "char";
                    w_ch = "&";
                } else {
                    w_type = "int";
                    w_ch = "";
                }
                proc_sig_name = pint_proc_get_sig_name(sig, "value_tmp", 1);
                if (width <= 4) {
                    declare_code += string_format("unsigned char %s;\n", GetResultName());
                } else {
                    declare_code +=
                        string_format("unsigned int %s_tmp[%d];\n", GetResultName(), word_num);
                    declare_code += string_format("unsigned int* %s = %s_tmp;\n", GetResultName(),
                                                  GetResultName());
                }

                operation_code +=
                    string_format("unsigned value_tmp = (unsigned)%s;\n",
                                  code_of_expr_value(word, oper1_converter.GetResultName(),
                                                     oper1_converter.IsResultNxz())
                                      .c_str());

                operation_code += string_format("if (value_tmp < %d){\n", arr_words);
                pint_proc_mult_opt_before_sig_use(operation_code, sig, "value_tmp");
                if ((is_must_gen_tmp == false) || (width <= 4)) {
                    operation_code +=
                        string_format("%s = %s;\n", GetResultName(), proc_sig_name.c_str());
                } else {
                    AppendClearCode(!result_nxz && sig_nxz);

                    operation_code +=
                        string_format("    pint_copy_int<%s>(%s, %s, %d);\n", B2S(sig_nxz),
                                      GetResultName(), proc_sig_name.c_str(), width);
                }

                operation_code += string_format("}else{");
                operation_code += string_format("pint_set_sig_x(%s%s, %u);\n", ch.c_str(),
                                                GetResultName(), width);
                operation_code += string_format("}");

                is_result_nxz = false;
            }
        }
    }
}

void ExpressionConverter::show_ternary_expression() {
    ivl_expr_t oper1 = ivl_expr_oper1(net);
    ivl_expr_t oper2 = ivl_expr_oper2(net);
    ivl_expr_t oper3 = ivl_expr_oper3(net);
    ExpressionConverter oper1_converter(oper1, stmt, 0, NUM_EXPRE_SIGNAL);
    ExpressionConverter oper2_converter(oper2, stmt, 0, (ivl_expr_type(oper2) == IVL_EX_STRING)
                                                            ? NUM_EXPRE_SIGNAL
                                                            : NUM_EXPRE_VARIABLE);
    ExpressionConverter oper3_converter(oper3, stmt, 0, (ivl_expr_type(oper3) == IVL_EX_STRING)
                                                            ? NUM_EXPRE_SIGNAL
                                                            : NUM_EXPRE_VARIABLE);
    bool sig1_nxz = oper1_converter.IsResultNxz();
    bool sig2_nxz = oper2_converter.IsResultNxz();
    bool sig3_nxz = oper3_converter.IsResultNxz();
    bool sig_nxz = sig1_nxz && sig2_nxz && sig3_nxz;
    bool result_nxz = false; // local var
    is_result_nxz = sig_nxz;
    AppendOperCode(&oper1_converter, "oper1");

    unsigned width = ivl_expr_width(net);
    unsigned width_sel = ivl_expr_width(oper1);
    if (width_sel > 1) {
        PINT_UNSUPPORTED_ERROR(ivl_expr_file(oper1), ivl_expr_lineno(oper1), "width_sel(%u) > 1.",
                               width_sel);
    }
    unsigned word = ((width + 0x1f) >> 5) * 2;
    int is_imm_oper2 = oper2_converter.IsImmediateNum();
    int is_imm_oper3 = oper3_converter.IsImmediateNum();
    const char *sn0 = GetResultName();
    const char *sn1 = oper1_converter.GetResultName();
    const char *sn2 = oper2_converter.GetResultName();
    const char *sn3 = oper3_converter.GetResultName();
    string oper2_dec;
    string oper3_dec;
    string oper2_operation;
    string oper3_operation;
    oper2_converter.GetPreparation(&oper2_operation, &oper2_dec);
    oper3_converter.GetPreparation(&oper3_operation, &oper3_dec);

    operation_code += oper2_dec;
    operation_code += oper3_dec;
    operation_code += string_format("if(%s){\n", sn1);
    operation_code += oper2_operation + "}\n";
    // operation_code += string_format("if((%s & 0xf0) || (%s == 0)){\n", sn1, sn1);
    if (!sig1_nxz) {
        operation_code += string_format("if((%s & 0xf0) || (%s == 0)){\n", sn1, sn1);
    } else {
        operation_code += string_format("if(%s == 0){\n", sn1);
    }
    operation_code += oper3_operation + "}\n";

    if (width <= 4) {
        declare_code += string_format("unsigned char %s;\n", sn0);

        // operation_code += string_format("if(%s & 0xf0){\n", sn1);
        // if (is_imm_oper2)
        //     operation_code +=
        //         string_format("\tunsigned char ternary_ture  = (unsigned char)%s & 0x0f;\n",
        //         sn2);
        // if (is_imm_oper3)
        //     operation_code +=
        //         string_format("\tunsigned char ternary_false = (unsigned char)%s & 0x0f;\n",
        //         sn3);
        // operation_code += string_format("\tpint_mux_char_char(&%s, &%s, &%s, &%s, %u, %u);\n",
        // sn0,
        //                                 is_imm_oper3 ? "ternary_false" : sn3,
        //                                 is_imm_oper2 ? "ternary_ture" : sn2, sn1, width,
        //                                 width_sel);
        // operation_code += string_format("}else if(%s){\n", sn1);
        if (!sig1_nxz) {
            operation_code += string_format("if(%s & 0xf0){\n", sn1);
            if (is_imm_oper2)
                operation_code +=
                    string_format("\tunsigned char ternary_ture  = (unsigned char)%s;\n", sn2);
            if (is_imm_oper3)
                operation_code +=
                    string_format("\tunsigned char ternary_false = (unsigned char)%s;\n", sn3);
            operation_code +=
                string_format("\tpint_mux_char_char<%s>(&%s, &%s, &%s, &%s, %u, %u);\n",
                              B2S(sig_nxz), sn0, is_imm_oper3 ? "ternary_false" : sn3,
                              is_imm_oper2 ? "ternary_ture" : sn2, sn1, width, width_sel);
            operation_code += string_format("}else if(%s){\n", sn1);
        } else {
            operation_code += string_format("if(%s){\n", sn1);
        }

        operation_code += string_format("\t%s = %s;\n", sn0, sn2);
        operation_code += "}else{\n";
        operation_code += string_format("\t%s = %s;\n", sn0, sn3);
        operation_code += "}\n";
    } else {
        declare_code += string_format("unsigned int  %s[%u];\n", sn0, word);

        // operation_code += string_format("if(%s & 0xf0){\n", sn1);
        // if (is_imm_oper2) {
        //     operation_code += string_format("\tunsigned int  ternary_ture[%u];\n", word);
        //     operation_code +=
        //         string_format("\tpint_copy_value(ternary_ture, %s, %u);\n", sn2, width);
        // }
        // if (is_imm_oper3) {
        //     operation_code += string_format("\tunsigned int  ternary_false[%u];\n", word);
        //     operation_code +=
        //         string_format("\tpint_copy_value(ternary_false, %s, %u);\n", sn3, width);
        // }
        // operation_code += string_format("\tpint_mux_int_char(%s, %s, %s, &%s, %u, %u);\n", sn0,
        //                                 is_imm_oper3 ? "ternary_false" : sn3,
        //                                 is_imm_oper2 ? "ternary_ture" : sn2, sn1, width,
        //                                 width_sel);
        // operation_code += string_format("}else if(%s){\n", sn1);
        if (!sig1_nxz) {
            operation_code += string_format("if(%s & 0xf0){\n", sn1);
            if (is_imm_oper2) {
                operation_code += string_format("\tunsigned int  ternary_ture[%u];\n", word);
                operation_code += string_format("\tpint_copy_value<%s>(ternary_ture, %s, %u);\n",
                                                B2S(sig_nxz), sn2, width);
            }
            if (is_imm_oper3) {
                operation_code += string_format("\tunsigned int  ternary_false[%u];\n", word);
                operation_code += string_format("\tpint_copy_value<%s>(ternary_false, %s, %u);\n",
                                                B2S(sig_nxz), sn3, width);
            }
            AppendClearCode(!result_nxz && sig_nxz);
            operation_code +=
                string_format("\tpint_mux_int_char<%s>(%s, %s, %s, &%s, %u, %u);\n", B2S(sig_nxz),
                              sn0, is_imm_oper3 ? "ternary_false" : sn3,
                              is_imm_oper2 ? "ternary_ture" : sn2, sn1, width, width_sel);
            operation_code += string_format("}else if(%s){\n", sn1);
        } else {
            operation_code += string_format("if(%s){\n", sn1);
        }

        if (is_imm_oper2) {
            operation_code +=
                string_format("\tpint_copy_value<false>(%s, %s, %u);\n", sn0, sn2, width);
        } else {
            AppendClearCode(!result_nxz && sig2_nxz);
            operation_code +=
                string_format("\tpint_copy_int<%s>(%s, %s, %u);\n", B2S(sig2_nxz), sn0, sn2, width);
        }
        operation_code += "}else{\n";
        if (is_imm_oper3) {
            operation_code +=
                string_format("\tpint_copy_value<false>(%s, %s, %u);\n", sn0, sn3, width);
        } else {
            AppendClearCode(!result_nxz && sig3_nxz);
            operation_code +=
                string_format("\tpint_copy_int<%s>(%s, %s, %u);\n", B2S(sig3_nxz), sn0, sn3, width);
        }
        operation_code += "}\n";
    }
}

void ExpressionConverter::show_unary_expression() {
    ivl_expr_t oper1 = ivl_expr_oper1(net);
    ExpressionConverter oper1_converter(oper1, stmt, 0);
    bool sig_nxz = oper1_converter.IsResultNxz();
    bool result_nxz = false; // local var
    is_result_nxz = sig_nxz;
    unsigned width = ivl_expr_width(oper1);
    unsigned width_expr = ivl_expr_width(net);
    char op = ivl_expr_opcode(net);
    string opcode;
    string type;
    string value;
    string ch_out, ch_in;

    if (op == '!' && ivl_expr_value(net) == IVL_VT_REAL) {
        // fprintf(out, "%*sERROR: Real argument to unary ! !?\n", ind,"");
        stub_errors += 1;
    }
    AppendOperCode(&oper1_converter, "oper1");

    switch (op) {
    default:
        opcode = string_format(" %c ", op);
        PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net),
                               "unary expression opcode: %c", op);
        break;
    case '!':
        opcode = "pint_logical_invert";
        break;
    case '~':
        opcode = "pint_bitw_invert";
        break;
    case '|': //|
        opcode = "pint_reduction_or";
        break;

    case 'N': //~|
        opcode = "pint_reduction_nor";
        break;

    case '&': //&
        opcode += "pint_reduction_and";
        break;

    case 'A': //~&
        opcode = "pint_reduction_nand";
        break;

    case '^': //^
        opcode = "pint_reduction_xor";
        break;

    case 'X': //~^
        opcode = "pint_reduction_xnor";
        break;

    case 'm':
        opcode = "abs";
        PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net),
                               "unary expression opcode: %c", op);
        break;
    case '-':
        opcode = "pint_minus";
        break;
    case 'r':
        declare_code += string_format("float %s;\n", GetResultName());
        if (oper1_converter.IsImmediateNum()) {
            operation_code +=
                string_format("pint_sig_to_real_value(&%s, %s, %u);\n", GetResultName(),
                              oper1_converter.GetResultName(), width);
        } else if (width <= 4) {
            operation_code +=
                string_format("pint_sig_to_real_char<%s>(&%s, &%s, %u);\n", B2S(sig_nxz),
                              GetResultName(), oper1_converter.GetResultName(), width);
        } else {
            operation_code +=
                string_format("pint_sig_to_real_int<%s>(&%s, %s, %u);\n", B2S(sig_nxz),
                              GetResultName(), oper1_converter.GetResultName(), width);
        }

        is_result_nxz = true;

        return;
    case 'v':
        if (width_expr <= 4) {
            declare_code += string_format("unsigned char %s;\n", GetResultName());
            operation_code +=
                string_format("pint_real_to_sig_char<%s>(&%s, &%s, %u);\n", B2S(sig_nxz),
                              GetResultName(), oper1_converter.GetResultName(), width_expr);
        } else {
            int word_num = (width_expr + 31) / 32 * 2;
            declare_code += string_format("unsigned int %s[%d];\n", GetResultName(), word_num);

            AppendClearCode(!result_nxz && sig_nxz);

            operation_code +=
                string_format("pint_real_to_sig_int<%s>(%s, &%s, %u);\n", B2S(sig_nxz),
                              GetResultName(), oper1_converter.GetResultName(), width_expr);
        }

        is_result_nxz = true;

        return;
    }

    if ((op != '~' && op != '-') || (width_expr <= 4)) {
        declare_code += string_format("unsigned char %s;\n", GetResultName());
        ch_out = "&";
    } else {
        unsigned cnt = (width_expr + 31) >> 5;
        declare_code += string_format("unsigned int %s[%u];\n", GetResultName(), cnt * 2);

        AppendClearCode(!result_nxz && sig_nxz);

        ch_out = "";
    }

    if (width <= 4) {
        type = "char";
        ch_in = "&";
        // if (op == '~' || op == '-') {
        //     declare_code += string_format("unsigned char %s;\n", GetResultName());
        // }
    } else {
        type = "int";
        ch_in = "";
        // if (op == '~' || op == '-') {
        //     unsigned cnt = width_expr >> 5;
        //     if (width_expr & 0x1f)
        //         cnt++;
        //     declare_code += string_format("unsigned int %s[%u];\n", GetResultName(), cnt * 2);
        //     ch_out = "";
        // } else {
        //     ch_out = "&";
        // }
    }

    if (oper1_converter.IsImmediateNum()) { // has been optimized by frontend
        if (op == '~' || op == '-') {       // small function is not implemented
            PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net),
                                   "in is immediate num, unary expression opcode: %c", op);
        }
        value = "_value";
        ch_in = "";
    } else {
        value = "";
    }
    operation_code += string_format("%s_%s%s<%s>(%s%s, %s%s, %u);\n", opcode.c_str(), type.c_str(),
                                    value.c_str(), B2S(sig_nxz), ch_out.c_str(), GetResultName(),
                                    ch_in.c_str(), oper1_converter.GetResultName(), width);
}

void ExpressionConverter::show_concat_expression() {
    unsigned idx;
    ivl_expr_t expr;
    unsigned width = ivl_expr_width(net);
    unsigned lshift;
    string ch;
    string ch_tmp;

    if (width <= 4) {
        declare_code += string_format("unsigned char %s;\n", GetResultName());
        operation_code += string_format("%s = 0;\n", GetResultName());
        ch = "&";
    } else {
        unsigned words = (width + 31) / 32;
        declare_code += string_format("unsigned int %s[%d];\n", GetResultName(), words * 2);
        // out must be cleared all because of the pint_concat() is not cleared internally
        operation_code += string_format("pint_set_int(%s, %d, 0);\n", GetResultName(), width);
        // operation_code += string_format("%s[%d] = 0;\n", GetResultName(), words - 1);
        // operation_code += string_format("%s[%d] = 0;\n", GetResultName(), words * 2 - 1);
        ch = "";
    }

    if (width == 0) {
        return;
    }

    unsigned nrepeat = ivl_expr_repeat(net);
    unsigned nparms = ivl_expr_parms(net);
    unsigned nexpr_width = 0;
    for (idx = 0; idx < nparms; idx++) {
        expr = ivl_expr_parm(net, idx);
        nexpr_width += ivl_expr_width(expr);
    }
    if (width <= 4) {
        if (nrepeat > 1) {
            operation_code += string_format("unsigned char %s_tmp;\n", GetResultName());
            operation_code += string_format("%s_tmp = 0;\n", GetResultName());
        } else { // nrepeat == 1
            operation_code +=
                string_format("unsigned char &%s_tmp = %s;\n", GetResultName(), GetResultName());
        }
        ch_tmp = "&";
    } else {
        if (nrepeat > 1) {
            unsigned nexpr_words = (nexpr_width + 31) / 32;
            operation_code +=
                string_format("unsigned int %s_tmp[%d];\n", GetResultName(), nexpr_words * 2);
            // out must be cleared all because of the pint_concat() is not cleared internally
            operation_code +=
                string_format("pint_set_int(%s_tmp, %d, 0);\n", GetResultName(), nexpr_width);
            // operation_code += string_format("%s_tmp[%d] = 0;\n", GetResultName(), nexpr_words -
            // 1);
            // operation_code += string_format("%s_tmp[%d] = 0;\n", GetResultName(), nexpr_words * 2
            // - 1);
        } else { // nrepeat == 1
            operation_code +=
                string_format("unsigned int* %s_tmp = %s;\n", GetResultName(), GetResultName());
        }
        ch = "";
    }

    bool sig_nxz = true;
    lshift = nexpr_width;
    for (idx = 0; idx < nparms; idx++) {
        // lshift = 0;
        // for (int s = ivl_expr_parms(net) - 1; s > (int)idx; s--) {
        //     lshift += ivl_expr_width(ivl_expr_parm(net, s));
        // }

        expr = ivl_expr_parm(net, idx);
        unsigned expr_width = ivl_expr_width(expr);
        lshift -= expr_width;

        // if ((expr_width == 0) || (ivl_expr_repeat(net) == 0)) {
        //     continue;
        // }

        NUM_EXPRE_TYPE numExprType = (expr_width > 32) ? NUM_EXPRE_SIGNAL : NUM_EXPRE_VARIABLE;
        ExpressionConverter oper_converter(expr, stmt, 0, numExprType);

        bool sig0_nxz = oper_converter.IsResultNxz();
        sig_nxz &= sig0_nxz;

        string comment = string_format("oper%d", idx);
        AppendOperCode(&oper_converter, comment.c_str());

        if (!oper_converter.IsImmediateNum()) {
            operation_code += string_format("pint_concat<%s>(%s%s_tmp, ", B2S(sig_nxz),
                                            ch_tmp.c_str(), GetResultName());
            if (expr_width <= 4) {
                operation_code += string_format("&%s", oper_converter.GetResultName());
            } else {
                operation_code += string_format("%s", oper_converter.GetResultName());
            }
        } else {
            operation_code +=
                string_format("pint_concat_value<%s>(%s%s_tmp, %s", B2S(sig_nxz), ch_tmp.c_str(),
                              GetResultName(), oper_converter.GetResultName());
        }

        operation_code += string_format(", %d, %d, %d);\n", nexpr_width, expr_width, lshift);
    }

    if (nrepeat > 1) {
        lshift = 0;
        operation_code += string_format("pint_concat_repeat<%s>(%s%s, %s%s_tmp, %d, %d, %d, %d);",
                                        B2S(sig_nxz), ch.c_str(), GetResultName(), ch_tmp.c_str(),
                                        GetResultName(), width, nexpr_width, lshift, nrepeat);
    }

    is_result_nxz = sig_nxz;
}

// nxz
ExpressionConverter::ExpressionConverter(ivl_expr_t net_, ivl_statement_t stmt_,
                                         unsigned expect_width_, NUM_EXPRE_TYPE numExprType_,
                                         bool mustGenTmp) {

    if (net_ == NULL) {
        is_Expression_Valid = false;
        return;
    }

    is_Expression_Valid = true;
    net = net_;
    // nxz
    stmt = stmt_;
    is_result_nxz = false; // default
    expect_width = expect_width_;
    numExprType = numExprType_;
    is_must_gen_tmp = mustGenTmp;
    is_pad_declare = false;
    is_immediate_num = false;

    const ivl_expr_type_t code = ivl_expr_type(net);
    const ivl_variable_type_t res_type = ivl_expr_value(net);
    const char *op_name = NULL;

    switch (code) {
    case IVL_EX_ARRAY:
        op_name = "ARRAY";
        break;

    case IVL_EX_ARRAY_PATTERN:
        op_name = "ARRAY_PATTERN";
        break;
    case IVL_EX_BACCESS:
        op_name = "BACCESS";
        break;
    case IVL_EX_BINARY:
        op_name = "BINARY";
        break;
    case IVL_EX_CONCAT:
        op_name = "CONCAT";
        break;
    case IVL_EX_ENUMTYPE:
        op_name = "ENUMTYPE";
        break;
    case IVL_EX_MEMORY:
        op_name = "MEMORY";
        break;
    case IVL_EX_NEW:
        op_name = "NEW";
        break;
    case IVL_EX_NULL:
        op_name = "NULL";
        break;
    case IVL_EX_PROPERTY:
        op_name = "PROPERTY";
        break;
    case IVL_EX_NUMBER:
        op_name = "NUMBER";
        break;
    case IVL_EX_SELECT:
        op_name = "SELECT";
        break;
    case IVL_EX_STRING:
        op_name = "STRING";
        break;
    case IVL_EX_SFUNC:
        op_name = "SFUNC";
        break;
    case IVL_EX_SIGNAL:
        op_name = "SIGNAL";
        break;
    case IVL_EX_TERNARY:
        op_name = "TERNARY";
        break;
    case IVL_EX_UNARY:
        op_name = "UNARY";
        break;
    case IVL_EX_UFUNC:
        op_name = "UFUNC";
        break;
    case IVL_EX_REALNUM:
        op_name = "REALNUM";
        break;
    case IVL_EX_SHALLOWCOPY:
        op_name = "SHALLOWCOPY";
        break;
    case IVL_EX_DELAY:
        op_name = "DELAY";
        break;
    default:
        PINT_UNSUPPORTED_ERROR(ivl_expr_file(net), ivl_expr_lineno(net), "expr type %u", code);
        op_name = "default";
        break;
    }

    result_name = string_format("tmp_%s%s_%u", op_name, res_type == IVL_VT_REAL ? "_real" : "",
                                express_tmp_var_id++);

    const char *file_name = ivl_expr_file(net);
    unsigned int line = ivl_expr_lineno(net);
    char line_content[10240];

    nest_depth++;
    if (file_name != NULL) {
        string file_name_string = (string)file_name;

        if (design_file_content.count(file_name_string) == 0) {
            vector<string> lines;
            FILE *design_file = fopen(file_name, "r");
            char *res = fgets(line_content, sizeof(line_content), design_file);

            while (res != NULL) {
                while ((res[0] == ' ') && (res[1] != 0))
                    res++;
                lines.push_back((string)res);
                res = fgets(line_content, sizeof(line_content), design_file);
            }
            design_file_content[file_name_string] = lines;
        }

        if (nest_depth <= 1) {
            content_file_line = string_format(
                "//%u: %s", line, design_file_content[file_name_string][line - 1].c_str());
        }
    }
    if (nest_depth <= 1) {
        sensitive_signals.clear();
    }
    GenPreparationCode();
    nest_depth--;
}

int ExpressionConverter::GetImmediateNumValue(ivl_expr_t word) {
    const char *p_bits = ivl_expr_bits(word);
    unsigned bit_size = ivl_expr_width(word);
    unsigned value = 0;

    for (unsigned bit_i = 0; bit_i < bit_size; bit_i++) {
        value |= (unsigned)(p_bits[bit_i] - '0') << bit_i;
    }

    if (ivl_expr_signed(word) && (p_bits[bit_size - 1] == '1')) {
        for (unsigned bit_i = bit_size; bit_i < 32; bit_i++) {
            value |= 1U << bit_i;
        }
    }

    return (int)value;
}

void ExpressionConverter::MatchResultWidth() {
    int sign_flag = ivl_expr_signed(net);
    int src_width = ivl_expr_width(net);
    int dst_width = expect_width;

    if ((dst_width == 0) || (dst_width == src_width)) {
        return;
    }

    bool sig_nxz = is_result_nxz;

    string old_name = result_name;
    result_name = string_format("tmp_%s_%u", (dst_width < src_width) ? "reduce" : "pad",
                                express_tmp_var_id++);
    if (dst_width <= 4) {
        declare_code += string_format("unsigned char %s;\n", result_name.c_str());
    } else {
        int word_size = (dst_width + 31) / 32;
        declare_code += string_format("unsigned int %s[%d];\n", result_name.c_str(), 2 * word_size);
    }

    if (dst_width < src_width) {
        if (dst_width <= 4) {
            if (src_width <= 4) {
                operation_code +=
                    string_format("pint_reduce_char_to_char<%s>(&%s, &%s, %d, %d);\n", B2S(sig_nxz),
                                  result_name.c_str(), old_name.c_str(), src_width, dst_width);
            } else {
                operation_code +=
                    string_format("pint_reduce_int_to_char<%s>(&%s, %s, %d, %d);\n", B2S(sig_nxz),
                                  result_name.c_str(), old_name.c_str(), src_width, dst_width);
            }
        } else {
            operation_code +=
                string_format("pint_reduce_int_to_int<%s>(%s, %s, %d, %d);\n", B2S(sig_nxz),
                              result_name.c_str(), old_name.c_str(), src_width, dst_width);
        }
    } else {
        if (dst_width <= 4) {
            if (src_width <= 4) {
                operation_code +=
                    string_format("pint_pad_char_to_char_s<%s>(&%s, &%s, %d, %d);\n", B2S(sig_nxz),
                                  result_name.c_str(), old_name.c_str(), src_width, dst_width);
            }
        } else {
            const char *sign_str = sign_flag ? "s" : "u";
            if (src_width <= 4) {
                operation_code += string_format("pint_pad_char_to_int_%s<%s>(%s, &%s, %d, %d);\n",
                                                sign_str, B2S(sig_nxz), result_name.c_str(),
                                                old_name.c_str(), src_width, dst_width);
            } else {
                operation_code += string_format("pint_pad_int_to_int_%s<%s>(%s, %s, %d, %d);\n",
                                                sign_str, B2S(sig_nxz), result_name.c_str(),
                                                old_name.c_str(), src_width, dst_width);
            }
        }
    }
}

bool ExpressionConverter::pint_binary_ret_bool(char op) {
    if ((op == 'n') || (op == 'N') || (op == 'e') || (op == 'E') || (op == '>') || (op == '<') ||
        (op == 'G') || (op == 'L') || (op == 'a') || (op == 'o')) {
        return 1;
    } else {
        return 0;
    }
}

void ExpressionConverter::AppendClearCode(bool gen_code) {
    if (!gen_code) {
        return;
    }

    unsigned int width = ivl_expr_width(net);
    if (width > 4) {
        const char *name0 = GetResultName();    // Result name
        unsigned int cnt = (width + 0x1f) >> 5; // Valid when 'width > 4'

        operation_code += string_format("for (unsigned int i = %u; i < %u; i++) {\n", cnt, cnt * 2);
        operation_code += string_format("   %s[i] = 0;\n", name0);
        operation_code += string_format("}\n");
    }
}

void ExpressionConverter::AppendOperCode(ExpressionConverter *oper_converter, const char *comment) {
    string oper_prepare_code;
    if (oper_converter->is_pad_declare) {
        declare_code = oper_converter->declare_code + declare_code;
        oper_prepare_code = oper_converter->preparation_code;
    } else
        oper_converter->GetPreparation(&oper_prepare_code, NULL);

    if (oper_prepare_code != "") {
        // operation_code += string_format("//%s of %s\n", comment, result_name.c_str());
        (void)comment;
        operation_code += oper_prepare_code;
    }
}

void ExpressionConverter::FormatPreparationCode() {
    if (nest_depth <= 1) {
        if (!PINT_SIMPLE_CODE_EN && operation_code != "") {
            preparation_code += content_file_line;
        }
    }

    if (operation_code != "") {
        preparation_code += ("{\n" + operation_code + "}\n");
    }
}

void ExpressionConverter::GenPreparationCode() {
    const ivl_expr_type_t code = ivl_expr_type(net);
    const ivl_variable_type_t res_type = ivl_expr_value(net);
    switch (code) {
    case IVL_EX_ARRAY:
        show_array_expression();
        break;

    case IVL_EX_ARRAY_PATTERN:
        show_array_pattern_expression();
        break;

    case IVL_EX_BACCESS:
        show_branch_access_expression();
        break;

    case IVL_EX_BINARY:
        if (res_type != IVL_VT_REAL) {
            show_binary_expression();
        } else {
            PINT_UNSUPPORTED_WARN(ivl_expr_file(net), ivl_expr_lineno(net), "BINARY oper real.");
            show_binary_expression_real();
        }
        break;

    case IVL_EX_CONCAT:
        show_concat_expression();
        break;

    case IVL_EX_ENUMTYPE:
        show_enumtype_expression();
        break;

    case IVL_EX_MEMORY:
        show_memory_expression();
        break;

    case IVL_EX_NEW:
        show_new_expression();
        break;

    case IVL_EX_NULL:
        show_null_expression();
        break;

    case IVL_EX_PROPERTY:
        show_property_expression();
        break;

    case IVL_EX_NUMBER:
        show_number_expression();
        break;

    case IVL_EX_SELECT:
        show_select_expression();
        break;

    case IVL_EX_STRING:
        show_string_expression();
        break;

    case IVL_EX_SFUNC:
        show_sysfunc_call();
        break;

    case IVL_EX_SIGNAL:
        if (res_type == IVL_VT_REAL) {
            PINT_UNSUPPORTED_WARN(ivl_expr_file(net), ivl_expr_lineno(net), "signal is real.");
        }
        show_signal_expression();
        break;

    case IVL_EX_TERNARY:
        show_ternary_expression();
        break;

    case IVL_EX_UNARY:
        show_unary_expression();
        break;

    case IVL_EX_UFUNC:
        show_function_call();
        break;

    case IVL_EX_REALNUM:
        PINT_UNSUPPORTED_WARN(ivl_expr_file(net), ivl_expr_lineno(net), "realnum.");
        show_real_number_expression();
        break;

    case IVL_EX_SHALLOWCOPY:
        show_shallowcopy();
        break;

    case IVL_EX_DELAY:
        show_delay_expression();
        break;

    default:
        // fprintf(out, "%*s<expr_type=%d>\n", ind, "", code);
        PINT_UNSUPPORTED_WARN(ivl_expr_file(net), ivl_expr_lineno(net), "expr_type=%d", code);
        break;
    }

    MatchResultWidth();
    FormatPreparationCode();
}

/*******************< Expression handling string>*******************/
void pint_show_expression_string(string &buff_pre, string &buff_oper, ivl_expr_t net,
                                 ivl_statement_t stmt, unsigned idx) {
    ivl_expr_type_t type = ivl_expr_type(net);
    int is_support = 1;
    const char *remark = "";
    unsigned ii;
    int real_len;

    switch (type) {
    case IVL_EX_NONE: //  0,
        is_support = 0;
        break;
    case IVL_EX_ARRAY:   //  18,
    case IVL_EX_BACCESS: //  19,
    case IVL_EX_BINARY:  //  2,
        is_support = 0;
        break;
    case IVL_EX_CONCAT:  //  3,
    {
        ivl_expr_t expr;
        // max length = 256
        buff_pre += string_format("\tchar str_%u[2048];\n", idx);
        // buff_pre += string_format("\tmemset(str_%u,0,2048);\n", idx);
        real_len = 0;
        for (ii = 0; ii < ivl_expr_parms(net); ii++) {
            expr = ivl_expr_parm(net, ii);
            ivl_expr_type_t t_type = ivl_expr_type(expr);

            switch (t_type) {
            case IVL_EX_SIGNAL: {
                ivl_signal_t sig = ivl_expr_signal(expr);
                bool sig_nxz = pint_proc_sig_nxz_get(stmt, sig);
                ivl_expr_t word = ivl_expr_oper1(expr);
                // pint_signal_info_t  sig_info = pint_find_signal_from_sig(sig);
                pint_sig_info_t sig_info = pint_find_signal(sig);
                const char *sn = sig_info->sig_name.c_str();
                unsigned width = sig_info->width;
                unsigned cnt = ((width + 7) >> 3);

                if (!word) { //  signal
                    if (width <= 4) {
                        buff_oper +=
                            string_format("\tpint_sig_to_string_char<%s>(str_%u+%u, %s, %u);\n",
                                          B2S(sig_nxz), idx, real_len, sn, width);
                    } else {
                        buff_oper +=
                            string_format("\tpint_sig_to_string_int<%s>(str_%u+%u, %s, %u);\n",
                                          B2S(sig_nxz), idx, real_len, sn, width);
                    }
                } else { //  arr, no test
                    if (ivl_expr_type(word) != IVL_EX_NUMBER) {
                        is_support = 0;
                        remark = "idx of arr is not a number";
                        break;
                    }
                    unsigned arr_idx = pint_get_bits_value(ivl_expr_bits(word));
                    if (width <= 4) {
                        buff_oper +=
                            string_format("\tpint_sig_to_string_char<%s>(str_%u+%u, %s[%u], %u);\n",
                                          B2S(sig_nxz), idx, real_len, sn, arr_idx, width);
                    } else {
                        buff_oper +=
                            string_format("\tpint_sig_to_string_int<%s>(str_%u+%u, %s[%u], %u);\n",
                                          B2S(sig_nxz), idx, real_len, sn, arr_idx, width);
                    }
                }
                real_len += cnt;
                break;
            }
            case IVL_EX_STRING: {
                const char *value = ivl_expr_string(expr);
                unsigned str_len = (ivl_expr_width(expr) + 7) / 8;
                buff_pre += string_format("\tchar str_const_%u[] =\"%s\";\n", idx, value);
                buff_oper += string_format("\t{\n");
                buff_oper += string_format("\tint ii=0;\n");
                buff_oper += string_format("\tint strlen = pint_get_string_len(str_%u);\n", idx);
                buff_oper += string_format("\tfor(;ii<%u;ii++){str_%u[strlen+ii] = "
                                           "str_const_%u[ii];}\n",
                                           str_len, idx, idx);
                buff_oper += string_format("\tstr_%u[strlen+ii] = 0;\n", idx);
                buff_oper += string_format("\t}\n");
                real_len += str_len;

                break;
            }
            default:
                is_support = 0;
                break;
            }
        }
        // buff_oper += string_format("\tprintf(\"%%s\\n\",str_%u);\n", idx);
        break;
    }
    case IVL_EX_DELAY:         //  20,
    case IVL_EX_ENUMTYPE:      //  21,
    case IVL_EX_EVENT:         //  17,
    case IVL_EX_MEMORY:        //  4,
    case IVL_EX_NEW:           //  23,
    case IVL_EX_NULL:          //  22,
    case IVL_EX_NUMBER:        //  5,
    case IVL_EX_ARRAY_PATTERN: //  26,
    case IVL_EX_PROPERTY:      //  24,
    case IVL_EX_REALNUM:       //  16,
    case IVL_EX_SCOPE:         //  6,
    case IVL_EX_SELECT:        //  7,
    case IVL_EX_SFUNC:         //  8,
    case IVL_EX_SHALLOWCOPY:   //  25,
        is_support = 0;
        break;
    case IVL_EX_SIGNAL:        //  9,
    {
        ivl_signal_t sig = ivl_expr_signal(net);
        bool sig_nxz = pint_proc_sig_nxz_get(stmt, sig);
        ivl_expr_t word = ivl_expr_oper1(net);
        pint_sig_info_t sig_info = pint_find_signal(sig);
        const char *sn = sig_info->sig_name.c_str();
        unsigned width = sig_info->width;
        // unsigned            cnt = ((width + 3) >> 2) +1;
        unsigned cnt = ((width + 7) >> 3);

        buff_pre += string_format("\tchar str_%u[%u];\n", idx, cnt + 1);
        buff_pre += string_format("\tstr_%u[%u] = 0;\n", idx, cnt);
        if (!word) { //  signal
            if (width <= 4) {
                buff_oper += string_format("\tpint_sig_to_string_char<%s>(str_%u, %s, %u);\n",
                                           B2S(sig_nxz), idx, sn, width);
            } else {
                buff_oper += string_format("\tpint_sig_to_string_int<%s>(str_%u, %s, %u);\n",
                                           B2S(sig_nxz), idx, sn, width);
            }
        } else { //  arr, no test
            if (ivl_expr_type(word) != IVL_EX_NUMBER) {
                is_support = 0;
                remark = "idx of arr is not a number";
                break;
            }
            unsigned arr_idx = pint_get_bits_value(ivl_expr_bits(word));
            if (width <= 4) {
                buff_oper += string_format("\tpint_sig_to_string_char<%s>(str_%u, %s[%u], %u);\n",
                                           B2S(sig_nxz), idx, sn, arr_idx, width);
            } else {
                buff_oper += string_format("\tpint_sig_to_string_int<%s>(str_%u, %s[%u], %u);\n",
                                           B2S(sig_nxz), idx, sn, arr_idx, width);
            }
        }
        break;
    }
    case IVL_EX_STRING: //  10,
    {
        const char *value = ivl_expr_string(net);
        buff_pre += string_format("\tchar str_%u[] =\"%s\";\n", idx, value);
        break;
    }
    case IVL_EX_TERNARY: //  11,
    case IVL_EX_UFUNC:   //  12,
    case IVL_EX_ULONG:   //  13,
    case IVL_EX_UNARY:   //  14,
    default:
        is_support = 0;
        break;
    }
    if (!is_support) {
        PINT_UNSUPPORTED_ERROR(
            ivl_expr_file(net), ivl_expr_lineno(net),
            "expression_string, expr type not supported. Type_id = %u, Remark(%s)", type, remark);
    }
}

bool pint_oper_is_expr(ivl_expr_t net) {
    const ivl_expr_type_t code = ivl_expr_type(net);
    switch (code) {
    case IVL_EX_ARRAY:
    case IVL_EX_BACCESS:
    case IVL_EX_ENUMTYPE:
    case IVL_EX_MEMORY:
    case IVL_EX_NULL:
    case IVL_EX_NUMBER:
    case IVL_EX_STRING:
    case IVL_EX_REALNUM:
        return 0;

    case IVL_EX_SIGNAL: {
        // pint_sig_info_t sig_info = pint_find_signal(ivl_expr_signal(net));
        // pint_add_process_sens_signal(sig_info, pint_process_num);

        ivl_expr_t word = ivl_expr_oper1(net);
        if (word != 0) {
            if ((ivl_expr_type(word) != IVL_EX_NUMBER) && (ivl_expr_type(word) != IVL_EX_SIGNAL)) {
                return 1;
            }
        }
        return 0;
    }
    default:
        return 1;
    }
}

unsigned pint_show_expression(ivl_expr_t net) {
    const ivl_expr_type_t code = ivl_expr_type(net);
    size_t idx;
    ivl_expr_t oper1;
    ivl_expr_t oper2;

    if (!pint_oper_is_expr(net)) {
        return 0;
    }

    switch (code) {
    case IVL_EX_ARRAY_PATTERN:
        for (idx = 0; idx < ivl_expr_parms(net); idx += 1) {
            pint_show_expression(ivl_expr_parm(net, idx));
        }
        break;

    case IVL_EX_BINARY:
        oper1 = ivl_expr_oper1(net);
        oper2 = ivl_expr_oper2(net);
        pint_show_expression(oper1);
        pint_show_expression(oper2);
        break;

    case IVL_EX_CONCAT: {
        for (idx = 0; idx < ivl_expr_parms(net); idx += 1) {
            pint_show_expression(ivl_expr_parm(net, idx));
        }
        break;
    }

    case IVL_EX_NEW:
        if (ivl_expr_oper1(net)) {
            pint_show_expression(ivl_expr_oper1(net));
        }
        if (ivl_expr_oper2(net)) {
            pint_show_expression(ivl_expr_oper2(net));
        }
        break;

    case IVL_EX_PROPERTY:
        pint_show_expression(ivl_expr_oper1(net));
        break;

    case IVL_EX_SELECT:
        oper1 = ivl_expr_oper1(net);
        oper2 = ivl_expr_oper2(net);
        if (oper1) {
            pint_show_expression(oper1);
        }
        if (oper2) {
            pint_show_expression(oper2);
        }
        break;

    case IVL_EX_SFUNC: {
        unsigned cnt = ivl_expr_parms(net);
        unsigned jdx;
        pint_process_info[pint_process_num]->always_run = 1;

        for (jdx = 0; jdx < cnt; jdx += 1) {
            pint_show_expression(ivl_expr_parm(net, jdx));
        }
    } break;

    case IVL_EX_TERNARY:
        pint_show_expression(ivl_expr_oper1(net));
        pint_show_expression(ivl_expr_oper2(net));
        pint_show_expression(ivl_expr_oper3(net));
        break;

    case IVL_EX_UNARY:
        pint_show_expression(ivl_expr_oper1(net));
        break;

    case IVL_EX_UFUNC:
        for (idx = 0; idx < ivl_expr_parms(net); idx += 1) {
            pint_show_expression(ivl_expr_parm(net, idx));
        }
        break;

    case IVL_EX_SHALLOWCOPY:
        pint_show_expression(ivl_expr_oper1(net));
        break;

    case IVL_EX_SIGNAL: {
        ivl_expr_t word = ivl_expr_oper1(net);
        if (word) {
            pint_show_expression(word);
        }
        break;
    }
    case IVL_EX_NONE:
        break;

    default:
        printf("pint_expr_type_error:%u\n", code);
        break;
    }
    return 1;
}
