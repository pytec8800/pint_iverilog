#include "config.h"
#include "priv.h"
#include <assert.h>
#include <string.h>

// #define DEBUG_VEFA_ANALYSIS

stmt_list_t blks;
fg_edge_list_t fg_edges;
expr_list_t exprs;
link_list_t l_sig_list;
int cur_blk_id;
int last_blk_id;

vefa_expr_info_t analysis_expression(ivl_expr_t net, unsigned ind, struct expr_companion2 signals);
static int analysis_comp_expr(ivl_expr_t e1, ivl_expr_t e2);
expr_list_t analysis_find_expr(ivl_expr_t net, expr_list_t exprs);
expr_list_t analysis_find_same_expr(ivl_expr_t net, expr_list_t exprs);
void analysis_statement(ivl_statement_t net, unsigned ind, ivl_signal_t sig_list[],
                        sig_anls_result_t stmt_in, sig_anls_result_t stmt_out, int n_sig,
                        int n_all_sig, int opcode);
void analysis_statement_expr(ivl_statement_t net, unsigned ind, expr_anls_result_t stmt_in,
                             expr_anls_result_t stmt_out, int n_expr, int opcode);

// #pragma GCC push_options
// #pragma GCC optimize("O0")

void vefa_free_expr_info(vefa_expr_info_t p) {
    link_list_t p1, p2;
    p1 = p->sig_dpnd;
    while (p1) {
        p2 = p1->next;
        free(p1);
        p1 = p2;
    }
}

vefa_expr_info_t vefa_malloc_expr_info() {
    vefa_expr_info_t p;
    p = (vefa_expr_info_t)malloc(sizeof(struct vefa_expr_info));
    if (p == NULL) {
        printf("ERROR: malloc failed in vefa_malloc_expr_info()\n");
        exit(1);
    }
    p->is_const = 0;
    p->is_binary = 0;
    p->is_allocated = 0;
    p->called_func = 0;
    p->n_reg = 2;
    p->sig_dpnd = NULL;
    p->loop_linear = 0;
    p->biv = 0;
    return p;
}

vefa_expr_info_t anls_show_number_expression(ivl_expr_t net, int gen_value) {
    assert(ivl_expr_type(net) == IVL_EX_NUMBER);
    vefa_expr_info_t expr_p = vefa_malloc_expr_info();
    unsigned width = ivl_expr_width(net);
    const char *bits = ivl_expr_bits(net);

    unsigned idx;
    int is_binary = 1;
    unsigned int *vf_ex_bin =
        (unsigned int *)malloc(sizeof(int) * ((width + 31) >> 5) * 2); // fixme get_word_size
    for (idx = 0; idx < width; idx += 32) {
        *(vf_ex_bin + idx / 16) = 0;
        *(vf_ex_bin + idx / 16 + 1) = 0;
    }
    for (idx = 0; idx < width; idx++) {
        switch (bits[idx]) {
        case '0':
            break;
        case '1':
            *(vf_ex_bin + idx / 32) += (1 << (idx & 0x1f));
            break;
        case 'x':
        case 'X':
            is_binary = 0;
            break;
        case 'z':
        case 'Z':
            is_binary = 0;
            break;
        default:
            PINT_UNSUPPORTED_WARN(ivl_expr_file(net), ivl_expr_lineno(net),
                                  "only 0/1/x/X/z/Z are supported");
            break;
        }
    }
    expr_p->is_const = 1;
    expr_p->is_binary = is_binary;
    if (width <= 32 && gen_value == 1) {
        if (is_binary) {
            expr_p->num = *vf_ex_bin;
        }
    }
    free(vf_ex_bin);
    return expr_p;
}

// comp 0 the same, 1 different
static int analysis_comp_binary_expr(ivl_expr_t e1, ivl_expr_t e2) {
    int rv;
    ivl_expr_t oper1_1 = ivl_expr_oper1(e1);
    ivl_expr_t oper2_1 = ivl_expr_oper2(e1);
    ivl_expr_t oper1_2 = ivl_expr_oper1(e2);
    ivl_expr_t oper2_2 = ivl_expr_oper2(e2);
    switch (ivl_expr_opcode(e1)) {

    case '<':
        if (ivl_expr_opcode(e2) == '<') {
            rv = analysis_comp_expr(oper1_1, oper1_2) || analysis_comp_expr(oper2_1, oper2_2);
        } else if (ivl_expr_opcode(e2) == '>') {
            rv = analysis_comp_expr(oper1_1, oper2_2) || analysis_comp_expr(oper2_1, oper1_2);
        } else {
            rv = 1;
        }
        break;
    case '>':
        if (ivl_expr_opcode(e2) == '>') {
            rv = analysis_comp_expr(oper1_1, oper1_2) || analysis_comp_expr(oper2_1, oper2_2);
        } else if (ivl_expr_opcode(e2) == '<') {
            rv = analysis_comp_expr(oper1_1, oper2_2) || analysis_comp_expr(oper2_1, oper1_2);
        } else {
            rv = 1;
        }
        break;
    case '+':
    case '&':
    case 'a':
    case '*':
    case '^':
    case 'X':
    case '|':
    case 'o':
    case 'e':
    case 'n':
    case 'N':
    case 'E':
        if (ivl_expr_opcode(e2) == ivl_expr_opcode(e1)) {
            rv = (analysis_comp_expr(oper1_1, oper1_2) || analysis_comp_expr(oper2_1, oper2_2)) &&
                 (analysis_comp_expr(oper1_1, oper2_2) || analysis_comp_expr(oper2_1, oper1_2));
        } else {
            rv = 1;
        }
        break;
    case '-':
    case '/':
    case '%':
    case 'G':
        if (ivl_expr_opcode(e2) == ivl_expr_opcode(e1)) {
            rv = analysis_comp_expr(oper1_1, oper1_2) || analysis_comp_expr(oper2_1, oper2_2);
        } else {
            rv = 1;
        }
        break;
    default:
        rv = 1;
    }
    return rv;
}

// return 0 if the same
// return 1 if not the same
static int analysis_comp_expr(ivl_expr_t e1, ivl_expr_t e2) {
    const ivl_expr_type_t code = ivl_expr_type(e1);
    int width;
    int idx, cnt;
    int rv = 1;
    if (e1 == e2)
        return 0;
    if ((ivl_expr_type(e1) != ivl_expr_type(e2)) || (ivl_expr_width(e1) != ivl_expr_width(e2))) {
        rv = 1;
    } else {
        switch (code) {
        case IVL_EX_ARRAY:
            if (ivl_expr_signal(e1) == ivl_expr_signal(e2))
                rv = 0;
            break;

        case IVL_EX_BINARY:
            rv = analysis_comp_binary_expr(e1, e2);
            break;

        case IVL_EX_CONCAT:
            if (ivl_expr_parms(e1) != ivl_expr_parms(e2)) {
                rv = 1;
            } else if (ivl_expr_repeat(e1) != ivl_expr_repeat(e2)) {
                rv = 1;
            } else {
                rv = 0;
                for (unsigned i = 0; i < ivl_expr_parms(e1); i++) {
                    if (analysis_comp_expr(ivl_expr_parm(e1, i), ivl_expr_parm(e2, i))) {
                        rv = 1;
                    }
                }
            }
            break;

        case IVL_EX_NUMBER: {
            const char *bits1 = ivl_expr_bits(e1);
            const char *bits2 = ivl_expr_bits(e2);
            rv = 0;
            width = ivl_expr_width(e1);
            for (idx = 0; idx < width; idx++) {
                switch (bits1[idx]) {
                case '0':
                case '1':
                    if (bits1[idx] != bits2[idx])
                        rv = 1;
                    break;
                case 'x':
                case 'X':
                    if (bits2[idx] != 'x' && bits2[idx] != 'X')
                        rv = 1;
                    break;
                case 'z':
                case 'Z':
                    if (bits2[idx] != 'z' && bits2[idx] != 'Z')
                        rv = 1;
                    break;
                default:
                    PINT_UNSUPPORTED_WARN(ivl_expr_file(e1), ivl_expr_lineno(e1),
                                          "comp_expr only 0/1/x/X/z/Z are supported");
                    break;
                }
            }
        } break;

        case IVL_EX_SELECT:
            if (analysis_comp_expr(ivl_expr_oper1(e1), ivl_expr_oper1(e2))) {
                rv = 1;
            } else {
                if ((ivl_expr_oper2(e1) != NULL) && (ivl_expr_oper2(e2) != NULL)) {
                    rv = analysis_comp_expr(ivl_expr_oper2(e1), ivl_expr_oper2(e2));
                } else if ((ivl_expr_oper2(e1) == NULL) && (ivl_expr_oper2(e2) == NULL)) {
                    rv = 0;
                } else {
                    rv = 1;
                }
            }
            break;

        case IVL_EX_STRING:
            if (strcmp(ivl_expr_string(e1), ivl_expr_string(e2))) {
                rv = 1;
            } else {
                rv = 0;
            }
            break;

        case IVL_EX_SFUNC:
            if (strcmp(ivl_expr_name(e1), ivl_expr_name(e2))) {
                rv = 1;
            } else if (ivl_expr_parms(e1) != ivl_expr_parms(e2)) {
                rv = 1;
            } else {
                rv = 0;
                cnt = ivl_expr_parms(e1);
                for (idx = 0; idx < cnt; idx += 1) {
                    if (analysis_comp_expr(ivl_expr_parm(e1, idx), ivl_expr_parm(e2, idx)))
                        rv = 1;
                }
            }
            break;

        case IVL_EX_SIGNAL:
            if (ivl_expr_signal(e1) == ivl_expr_signal(e2)) {
                if (ivl_expr_oper1(e1) == NULL && ivl_expr_oper1(e2) == NULL)
                    rv = 0;
                else if (ivl_expr_oper1(e1) != NULL && ivl_expr_oper1(e2) != NULL) {
                    rv = analysis_comp_expr(ivl_expr_oper1(e1), ivl_expr_oper1(e2));
                } else {
                    rv = 1;
                }
            } else {
                rv = 1;
            }
            break;

        case IVL_EX_TERNARY:
            rv = 0;
            if (analysis_comp_expr(ivl_expr_oper1(e1), ivl_expr_oper1(e2)))
                rv = 1;
            if (analysis_comp_expr(ivl_expr_oper2(e1), ivl_expr_oper2(e2)))
                rv = 1;
            if (analysis_comp_expr(ivl_expr_oper3(e1), ivl_expr_oper3(e2)))
                rv = 1;
            break;

        case IVL_EX_UNARY:
            if (ivl_expr_opcode(e1) == ivl_expr_opcode(e2)) {
                rv = analysis_comp_expr(ivl_expr_oper1(e1), ivl_expr_oper1(e2));
            } else {
                rv = 1;
            }
            break;

        case IVL_EX_UFUNC:
            if (ivl_expr_def(e1) != ivl_expr_def(e2)) {
                rv = 1;
            } else if (ivl_expr_parms(e1) != ivl_expr_parms(e2)) {
                rv = 1;
            } else {
                rv = 0;
                cnt = ivl_expr_parms(e1);
                for (idx = 0; idx < cnt; idx += 1) {
                    if (analysis_comp_expr(ivl_expr_parm(e1, idx), ivl_expr_parm(e2, idx)))
                        rv = 1;
                }
            }
            break;

        default:
            // not meet yet
            PINT_UNSUPPORTED_NOTE(ivl_expr_file(e1), ivl_expr_lineno(e1),
                                  "do not know how to handle %d in comp_expr", code);
            break;
        }
    }
    return rv;
}

static expr_list_t analysis_add_expr(ivl_expr_t net, expr_list_t exprs,
                                     struct expr_bundle expr_bdl) {
    const ivl_expr_type_t code = ivl_expr_type(net);
    expr_list_t p, p1, p2;
    expr_list_t last_p = NULL;
    int idx, cnt;
    int expr_index = expr_bdl.index;

    // printf("adding expr %d\n",ivl_expr_type(net));
    p = exprs;
    while (p) {
        if (analysis_comp_expr(p->net, net) == 0) {
            p2 = (struct expr_list *)malloc(sizeof(struct expr_list));
            if (p2 == NULL) {
                printf("malloc failed in analysis_add_expr\n");
                exit(2);
            }
            p2->next_diff = p->next_diff;
            p2->next_same = NULL;
            p2->net = net;
            p2->index = expr_bdl.index;
            // If an expr is already in the list,
            // the sig_dpnd list will be pointed to the first expression's sig_dpnd
            // Therefore, when free, only need to free once for the first expression.
            p2->sig_dpnd = p->sig_dpnd;

            p1 = p;
            while (p1->next_same) {
                p1 = p1->next_same;
            }
            p1->next_same = p2;
            break;
        }
        last_p = p;
        p = p->next_diff;
    }
    if (p == NULL) {
        p = (struct expr_list *)malloc(sizeof(struct expr_list));
        if (p == NULL) {
            printf("malloc failed in analysis_add_expr\n");
            exit(2);
        }
        p->next_same = NULL;
        p->next_diff = NULL;
        p->net = net;
        p->index = expr_bdl.index;
        struct expr_companion2 expr_cpn;
        expr_cpn.sig_list = NULL;
        expr_cpn.n_sig = 0;
        expr_cpn.stmt_in_sig = NULL;
        vefa_expr_info_t expr_p = analysis_expression(net, 0, expr_cpn);
        p->sig_dpnd = expr_p->sig_dpnd;
        expr_p->sig_dpnd = NULL;
        vefa_free_expr_info(expr_p);
        if (exprs == NULL)
            exprs = p;
        else {
            last_p->next_diff = p;
        }
    }
    // if expr_bdl is valid, check if sub-expr should be added
    // This is needed to stop adding sub-exprs after an expr is found in available list in the used
    // analysis
    if (expr_bdl.exprs && expr_bdl.stmt_in) {
        p = analysis_find_expr(net, expr_bdl.exprs);
        if (NULL != p) {
            if (p->id >= 0) {
                if ((expr_bdl.stmt_in + p->id)->avail)
                    return exprs;
            }
        }
    }
    // analysis_show_expr(exprs);
    switch (code) {
    case IVL_EX_ARRAY:
        break;

    case IVL_EX_BINARY:
        exprs = analysis_add_expr(ivl_expr_oper1(net), exprs, expr_bdl);
        exprs = analysis_add_expr(ivl_expr_oper2(net), exprs, expr_bdl);
        break;

    case IVL_EX_CONCAT:
        cnt = ivl_expr_parms(net);
        for (idx = 0; idx < cnt; idx += 1) {
            exprs = analysis_add_expr(ivl_expr_parm(net, idx), exprs, expr_bdl);
        }
        break;

    case IVL_EX_NUMBER:
        break;

    case IVL_EX_SELECT:
        exprs = analysis_add_expr(ivl_expr_oper1(net), exprs, expr_bdl);
        expr_bdl.index = 1;
        if (ivl_expr_oper2(net))
            exprs = analysis_add_expr(ivl_expr_oper2(net), exprs, expr_bdl);
        expr_bdl.index = expr_index;
        break;

    case IVL_EX_STRING:
        break;

    case IVL_EX_SFUNC:
        cnt = ivl_expr_parms(net);
        for (idx = 0; idx < cnt; idx += 1) {
            exprs = analysis_add_expr(ivl_expr_parm(net, idx), exprs, expr_bdl);
        }
        break;

    case IVL_EX_SIGNAL:

        expr_bdl.index = 1;
        if (ivl_expr_oper1(net))
            exprs = analysis_add_expr(ivl_expr_oper1(net), exprs, expr_bdl);
        expr_bdl.index = expr_index;
        break;

    case IVL_EX_TERNARY:
        exprs = analysis_add_expr(ivl_expr_oper1(net), exprs, expr_bdl);
        if (expr_bdl.avail_anls) {
            p1 = NULL;
            p2 = NULL;
            p1 = analysis_add_expr(ivl_expr_oper2(net), p1, expr_bdl);
            p2 = analysis_add_expr(ivl_expr_oper3(net), p2, expr_bdl);
            p = p1; // pick an expr from p1 first and then try to find it in p2
            while (p) {
                if (analysis_find_same_expr(p->net, p2)) {
                    exprs = analysis_add_expr(p->net, exprs, expr_bdl);
                }
                last_p = p;
                p = p->next_diff;
            }
        } else {
            exprs = analysis_add_expr(ivl_expr_oper2(net), exprs, expr_bdl);
            exprs = analysis_add_expr(ivl_expr_oper3(net), exprs, expr_bdl);
        }
        break;

    case IVL_EX_UNARY:
        exprs = analysis_add_expr(ivl_expr_oper1(net), exprs, expr_bdl);
        break;

    case IVL_EX_UFUNC:
        cnt = ivl_expr_parms(net);
        for (idx = 0; idx < cnt; idx += 1) {
            exprs = analysis_add_expr(ivl_expr_parm(net, idx), exprs, expr_bdl);
        }
        break;

    default:
        // not meet yet
        PINT_UNSUPPORTED_NOTE(ivl_expr_file(net), ivl_expr_lineno(net),
                              "do not know how to handle %d in add_expr", code);
        break;
    }
    return exprs;
}
static int analysis_mark_expr(expr_list_t exprs, ivl_signal_t *sig_list, int n_sig) {
    expr_list_t p, p1;
    int i;
    int id = 0;
    // set all id = -1
    p = exprs;
    while (p) {
        p1 = p;
        while (p1) {
            p1->id = -1;
            p1->sig_id = -1;
            p1->sig = NULL;
            p1 = p1->next_same;
        }
        p = p->next_diff;
    }
    // set signal expr to 0 ~ n_sig-1;
    for (i = 0; i < n_sig; i++) {
        p = exprs;
        while (p) {
            if (ivl_expr_type(p->net) == IVL_EX_SIGNAL && ivl_expr_oper1(p->net) == NULL &&
                ivl_expr_signal(p->net) == *(sig_list + i)) {
                p1 = p;
                while (p1) {
                    p1->id = id;
                    p1->sig_id = i;
                    p1->sig = *(sig_list + i);
                    p1 = p1->next_same;
                }
                id++;
                break;
            }
            p = p->next_diff;
        }
#ifdef DEBUG_VEFA_ANALYSIS
        if (p == NULL) {
            printf("%s is not found in the expr list\n", ivl_signal_basename(*(sig_list + i)));
        }
#endif
    }
    // if(n_sig == id)printf("All %d binary signals are found in expr\n",n_sig);
    // assign id to all exprs that show up more than one time.
    p = exprs;
    while (p) {
        if (p->id < 0) {
            if (ivl_expr_type(p->net) == IVL_EX_NUMBER) {
                // Not store number expression
            } else {
                p1 = p->next_same;
                while (p1) {
                    p1->id = id;
                    p1 = p1->next_same;
                }
                if (p->next_same)
                    p->id = id++;
            }
        }
        p = p->next_diff;
    }
    return id;
}

static int analysis_count_sig_expr(expr_list_t exprs) {
    expr_list_t p;
    link_list_t mem_list = NULL, p1;
    int cnt = 0;
    p = exprs;
    while (p) {
        if (ivl_expr_type(p->net) == IVL_EX_SIGNAL) {
            if (ivl_expr_oper1(p->net) == NULL) {
                cnt++;
            } else {
                p1 = mem_list;
                while (p1) {
                    if (p1->item == ivl_expr_signal(p->net))
                        break;
                    p1 = p1->next;
                }
                if (p1 == NULL) {
                    p1 = (struct link_list *)malloc(sizeof(struct link_list));
                    if (p1 == NULL) {
                        printf("malloc failed in analysis_count_sig_expr\n");
                        exit(2);
                    }
                    p1->item = ivl_expr_signal(p->net);
                    p1->next = mem_list;
                    mem_list = p1;
                    cnt++;
                }
            }
        }
        p = p->next_diff;
    }
    p1 = mem_list;
    while (mem_list) {
        p1 = mem_list->next;
        free(mem_list);
        mem_list = p1;
    }
    return cnt;
}

void analysis_free_expr(expr_list_t exprs) {
    expr_list_t p, p1, p2;
    link_list_t lp, lp1;
    p = exprs;
    while (p) {
        lp = p->sig_dpnd;
        while (lp) {
            lp1 = lp->next;
            free(lp); // Only need to free the first expression
            lp = lp1;
        }
        p2 = p->next_diff;
        while (p) {
            p1 = p->next_same;
            free(p);
            p = p1;
        }
        p = p2;
    }
}

void analysis_show_expr(expr_list_t exprs) {
    expr_list_t p, p1;
    printf("/****START analysis_show_exprs****/\n");
    p = exprs;
    while (p) {
        if (ivl_expr_type(p->net) == IVL_EX_BINARY)
            printf("%c : ", ivl_expr_opcode(p->net));
        printf("id:%d (sig_id:%d) line:%d,type:%d indx:%d \n", p->id, p->sig_id,
               ivl_expr_lineno(p->net), ivl_expr_type(p->net), p->index);
        p1 = p->next_same;
        while (p1) {
            printf("\tid:%d (sig_id:%d) line:%d,type:%d indx:%d \n", p->id, p->sig_id,
                   ivl_expr_lineno(p1->net), ivl_expr_type(p1->net), p1->index);
            p1 = p1->next_same;
        }
        p = p->next_diff;
    }
    printf("/****END analysis_show_exprs****/\n");
}
static expr_list_t analysis_find_expr_by_index(int i, expr_list_t exprs) {
    expr_list_t p;
    p = exprs;
    while (p) {
        if (p->id == i)
            break;
        p = p->next_diff;
    }
    return p;
}

// This will return if the same expr is found. The same means if analysis_comp_expr returns 0
expr_list_t analysis_find_same_expr(ivl_expr_t net, expr_list_t exprs) {
    expr_list_t p;
    p = exprs;
    while (p) {
        if (analysis_comp_expr(p->net, net) == 0)
            return p;
        p = p->next_diff;
    }
    return p;
}

expr_list_t analysis_find_expr(ivl_expr_t net, expr_list_t exprs) {
    expr_list_t p, p1;
    p = exprs;
    while (p) {
        if (p->net == net)
            return p;
        p1 = p->next_same;
        while (p1) {
            if (p1->net == net)
                return p1;
            p1 = p1->next_same;
        }
        p = p->next_diff;
    }
    return p;
}

static fg_edge_list_t analysis_add_edge(int start, int end, vefa_fg_edge_type_t type) {
    fg_edge_list_t new_item, p;
    new_item = (struct fg_edge_list *)malloc(sizeof(struct fg_edge_list));
    if (new_item == NULL) {
        printf("malloc failed in analysis_add_edge\n");
        exit(2);
    }
    new_item->start = start;
    new_item->end = end;
    new_item->type = type;
    new_item->next = NULL;

    if (fg_edges) {
        p = fg_edges;
        while (p->next)
            p = p->next;
        p->next = new_item;
    } else {
        fg_edges = new_item;
    }
    return new_item;
}

// a blk can be empty, no statement, but if there is a statement, there is not empty node at end
static stmt_list_t analysis_add_new_blk(int blk_id, int loop_level) {
    stmt_list_t p, new_item;
    new_item = (struct stmt_list *)malloc(sizeof(struct stmt_list));
    if (new_item == NULL) {
        printf("malloc failed in analysis_add_new_blk\n");
        exit(2);
    }
    new_item->net = NULL;
    new_item->next_blk = NULL;
    new_item->next_stmt = NULL;
    new_item->pre_stmt = NULL;
    new_item->blk_id = blk_id;
    new_item->loop_level = loop_level;
    new_item->loop_depth = loop_level;
    new_item->dly_in_loop = 0;
    new_item->loop_st_type = 0;

    if (blks == NULL)
        blks = new_item;
    else {
        p = blks;
        while (p->next_blk)
            p = p->next_blk;
        p->next_blk = new_item;
    }
    return new_item;
}

static stmt_list_t analysis_add_new_statement(int blk_id, ivl_statement_t net) {
    stmt_list_t p, new_item;

    p = blks;
    while (p->next_blk && p->blk_id != blk_id)
        p = p->next_blk;
    if (p->blk_id != blk_id) {
        printf("cannot find blk id %d\n", blk_id);
        exit(2);
    }
    while (p->next_stmt) {
        if (ivl_statement_type(net) == IVL_ST_WAIT || ivl_statement_type(net) == IVL_ST_DELAY) {
            p->dly_in_loop = 1;
        }
        p = p->next_stmt;
    }
    if (p->net == NULL) {
        if (ivl_statement_type(net) == IVL_ST_WAIT || ivl_statement_type(net) == IVL_ST_DELAY) {
            p->dly_in_loop = 1;
        }
        p->net = net;
    } else {

        new_item = (struct stmt_list *)malloc(sizeof(struct stmt_list));
        if (new_item == NULL) {
            printf("malloc failed in analysis_add_new_statement\n");
            exit(2);
        }
        new_item->net = net;
        new_item->next_blk = NULL;
        new_item->next_stmt = NULL;
        new_item->pre_stmt = p;
        new_item->loop_st_type = 0;
        new_item->blk_id = blk_id;
        if (ivl_statement_type(net) == IVL_ST_WAIT || ivl_statement_type(net) == IVL_ST_DELAY) {
            new_item->dly_in_loop = 1;
        } else {
            new_item->dly_in_loop = 0;
        }
        p->next_stmt = new_item;
        p = new_item;
    }
    return p;
}

static stmt_list_t analysis_find_block(int blk_id) {
    stmt_list_t p;

    p = blks;
    while (p->next_blk && p->blk_id != blk_id)
        p = p->next_blk;
    if (p->blk_id != blk_id) {
        return NULL;
    } else {
        return p;
    }
}

void analysis_init() {
    cur_blk_id = 0;
    last_blk_id = 0;
    fg_edges = NULL;
    blks = NULL;
    exprs = NULL;
    l_sig_list = NULL;
    analysis_add_new_blk(0, 0);
}

expr_list_t analysis_collect_expr(ivl_statement_t net, unsigned ind, expr_list_t exprs,
                                  int sub_stmt, struct expr_bundle expr_bdl) {
    const ivl_statement_type_t code = ivl_statement_type(net);
    unsigned idx;
    ivl_lval_t lval;
    int expr_index = expr_bdl.index;

    switch (code) {

    case IVL_ST_ASSIGN:
    case IVL_ST_ASSIGN_NB:
        for (unsigned i = 0; i < ivl_stmt_lvals(net); i++) {
            lval = ivl_stmt_lval(net, i);
            expr_bdl.index = 1;
            if (ivl_lval_idx(lval))
                exprs = analysis_add_expr(ivl_lval_idx(lval), exprs, expr_bdl);
            if (ivl_lval_part_off(lval))
                exprs = analysis_add_expr(ivl_lval_part_off(lval), exprs, expr_bdl);
            expr_bdl.index = expr_index;
        }
        exprs = analysis_add_expr(ivl_stmt_rval(net), exprs, expr_bdl);
        break;

    case IVL_ST_BLOCK: {
        unsigned cnt = ivl_stmt_block_count(net);
        for (idx = 0; idx < cnt; idx += 1) {
            ivl_statement_t cur = ivl_stmt_block_stmt(net, idx);
            exprs = analysis_collect_expr(cur, ind ? ind : ind + 8, exprs, sub_stmt, expr_bdl);
        }
        break;
    }

    case IVL_ST_CASEX:
    case IVL_ST_CASEZ:
    case IVL_ST_CASER:
    case IVL_ST_CASE: {
        // for the fork block
        exprs = analysis_add_expr(ivl_stmt_cond_expr(net), exprs, expr_bdl);

        unsigned cnt = ivl_stmt_case_count(net);

        for (idx = 0; idx < cnt && sub_stmt; idx += 1) {
            // The cond_expr is tricky to check availability, because it is not part of cast_stmt.
            // when evaluated basic block, the expr need to be explicitly added to the case_stmt.
            // Not expect to have big disadvantage to ignore this expression.
            // ivl_expr_t ex = ivl_stmt_case_expr(net, idx);
            // if(ex)exprs = analysis_add_expr(ex,exprs);
            ivl_statement_t st = ivl_stmt_case_stmt(net, idx);
            exprs =
                analysis_collect_expr(st, ind ? ind + 48 : ind + 8 + 48, exprs, sub_stmt, expr_bdl);
        }

        break;
    }

    case IVL_ST_CONDIT: {
        exprs = analysis_add_expr(ivl_stmt_cond_expr(net), exprs, expr_bdl);
        ivl_statement_t t = ivl_stmt_cond_true(net);
        ivl_statement_t f = ivl_stmt_cond_false(net);
        if (t && sub_stmt)
            exprs = analysis_collect_expr(t, ind, exprs, sub_stmt, expr_bdl);

        if (f && sub_stmt)
            exprs = analysis_collect_expr(f, ind, exprs, sub_stmt, expr_bdl);

        break;
    }

    case IVL_ST_NOOP:
        break;
    case IVL_ST_DELAY:
    case IVL_ST_WAIT:
        if (ivl_stmt_sub_stmt(net) && sub_stmt) {
            exprs = analysis_collect_expr(ivl_stmt_sub_stmt(net), ind, exprs, sub_stmt, expr_bdl);
        }
        break;

    case IVL_ST_STASK:
        if (strcmp(ivl_stmt_name(net), "$monitor")) { // exclude $monitor
            for (idx = 0; idx < ivl_stmt_parm_count(net); idx += 1) {
                if (ivl_expr_type(ivl_stmt_parm(net, idx)) != IVL_EX_NONE)
                    exprs = analysis_add_expr(ivl_stmt_parm(net, idx), exprs, expr_bdl);
            }
        }
        break;

    case IVL_ST_WHILE:
        exprs = analysis_add_expr(ivl_stmt_cond_expr(net), exprs, expr_bdl);
        if (sub_stmt)
            exprs = analysis_collect_expr(ivl_stmt_sub_stmt(net), ind, exprs, sub_stmt, expr_bdl);
        break;

    default:
        PINT_UNSUPPORTED_NOTE(ivl_stmt_file(net), ivl_stmt_lineno(net),
                              "unknown statement type (%u)", code);
        break;
    }
    return exprs;
}

// ind is depth in loop
void analysis_flow_graph(ivl_statement_t net, unsigned ind) {
    const ivl_statement_type_t code = ivl_statement_type(net);
    int source;
    int sink;
    unsigned idx;

    switch (code) {

    case IVL_ST_ASSIGN:
    case IVL_ST_ASSIGN_NB:
        analysis_add_new_statement(cur_blk_id, net);
        break;

    case IVL_ST_BLOCK: {
        unsigned cnt = ivl_stmt_block_count(net);
        for (idx = 0; idx < cnt; idx += 1) {
            ivl_statement_t cur = ivl_stmt_block_stmt(net, idx);
            analysis_flow_graph(cur, ind);
        }
        break;
    }

    case IVL_ST_CASEX:
    case IVL_ST_CASEZ:
    case IVL_ST_CASER:
    case IVL_ST_CASE: {
        // for the fork block
        analysis_add_new_statement(cur_blk_id, net);
        int case_fork_id = cur_blk_id;
        // for the join block
        int case_join_id = ++last_blk_id;
        analysis_add_new_blk(case_join_id, ind);

        unsigned cnt = ivl_stmt_case_count(net);

        for (idx = 0; idx < cnt; idx += 1) {
            ivl_statement_t st = ivl_stmt_case_stmt(net, idx);
            cur_blk_id = ++last_blk_id;
            analysis_add_edge(case_fork_id, cur_blk_id, VEFA_EDGE_NORMAL);
            analysis_add_new_blk(cur_blk_id, ind);
            analysis_flow_graph(st, ind);
            analysis_add_edge(cur_blk_id, case_join_id, VEFA_EDGE_NORMAL);
        }
        cur_blk_id = case_join_id;

        break;
    }

    case IVL_ST_CONDIT: {
        ivl_statement_t t = ivl_stmt_cond_true(net);
        ivl_statement_t f = ivl_stmt_cond_false(net);
        int cond_branch_id = cur_blk_id;
        analysis_add_new_statement(cur_blk_id, net);

        int cond_join_id = ++last_blk_id;
        analysis_add_new_blk(cond_join_id, ind);

        if (t) {
            cur_blk_id = ++last_blk_id;
            analysis_add_new_blk(cur_blk_id, ind);
            analysis_add_edge(cond_branch_id, cur_blk_id, VEFA_EDGE_IF_TRUE);
            analysis_flow_graph(t, ind);
            analysis_add_edge(cur_blk_id, cond_join_id, VEFA_EDGE_NORMAL);
        } else {
            analysis_add_edge(cond_branch_id, cond_join_id, VEFA_EDGE_NORMAL);
        }

        if (f) {
            cur_blk_id = ++last_blk_id;
            analysis_add_new_blk(cur_blk_id, ind);
            analysis_add_edge(cond_branch_id, cur_blk_id, VEFA_EDGE_IF_FALSE);
            analysis_flow_graph(f, ind);
            analysis_add_edge(cur_blk_id, cond_join_id, VEFA_EDGE_NORMAL);
        } else {
            if (t)
                analysis_add_edge(cond_branch_id, cond_join_id, VEFA_EDGE_NORMAL);
        }
        cur_blk_id = cond_join_id;
        break;
    }

    case IVL_ST_NOOP:
        break;
    case IVL_ST_DELAY:
    case IVL_ST_WAIT:
        if (analysis_find_block(cur_blk_id)->net != NULL) {
            analysis_add_edge(cur_blk_id, last_blk_id + 1, VEFA_EDGE_NORMAL);
            cur_blk_id = ++last_blk_id;
            analysis_add_new_blk(cur_blk_id, ind);
        }
        analysis_add_new_statement(cur_blk_id, net);

        if (ivl_stmt_sub_stmt(net)) {
            analysis_add_edge(cur_blk_id, last_blk_id + 1, VEFA_EDGE_NORMAL);
            cur_blk_id = ++last_blk_id;
            analysis_add_new_blk(cur_blk_id, ind);
            analysis_flow_graph(ivl_stmt_sub_stmt(net), ind);
        }
        break;

    case IVL_ST_STASK:
        analysis_add_new_statement(cur_blk_id, net);
        break;

    case IVL_ST_WHILE:
        // condition block
        if (analysis_find_block(cur_blk_id)->net != NULL) {
            analysis_add_edge(cur_blk_id, last_blk_id + 1, VEFA_EDGE_NORMAL);
            cur_blk_id = ++last_blk_id;
            analysis_add_new_blk(cur_blk_id, ind + 1);
        }
        analysis_add_new_statement(cur_blk_id, net);
        source = cur_blk_id;

        // next block
        cur_blk_id = ++last_blk_id;
        analysis_add_new_blk(cur_blk_id, ind);
        analysis_add_edge(source, cur_blk_id, VEFA_EDGE_NORMAL);
        sink = cur_blk_id;

        // loop body
        cur_blk_id = ++last_blk_id;
        analysis_add_edge(source, cur_blk_id, VEFA_EDGE_WHILE_FWD);
        analysis_add_new_blk(cur_blk_id, ind + 1);
        analysis_flow_graph(ivl_stmt_sub_stmt(net), ind + 1);
        analysis_add_edge(cur_blk_id, source, VEFA_EDGE_WHILE_BWD);

        cur_blk_id = sink;
        break;

    default:
        PINT_UNSUPPORTED_NOTE(ivl_stmt_file(net), ivl_stmt_lineno(net),
                              "unknown statement type (%u)", code);
        break;
    }
}

// pack nodes(blocks) into array
static stmt_list_t *analysis_blk_array(int n_blk) {
    stmt_list_t blk;
    stmt_list_t *block_list;
    block_list = (stmt_list_t *)malloc(n_blk * sizeof(stmt_list_t));
    if (block_list == NULL) {
        printf("failed malloc in analysis_blk_array\n");
        exit(2);
    }
    blk = blks;
    while (blk) {
        block_list[blk->blk_id] = blk;
        blk = blk->next_blk;
    }
    return block_list;
}

// pack all fg_eges link list into array
static fg_edge_list_t *analysis_edge_array(int n_edge) {
    fg_edge_list_t e;
    int i;
    fg_edge_list_t *edges;
    edges = (fg_edge_list_t *)malloc(n_edge * sizeof(fg_edge_list_t));
    if (edges == NULL) {
        printf("failed malloc in analysis_edge_array\n");
        exit(2);
    }
    e = fg_edges;
    i = 0;
    while (e) {
        *(edges + i) = e;
        i++;
        e = e->next;
    }
    return edges;
}
// allocate two dimentional array
static expr_anls_result_t *analysis_allocate_2dim_array(int dim1, int dim2) {
    expr_anls_result_t *p1;
    expr_anls_result_t p2;
    p1 = (expr_anls_result_t *)malloc(dim1 * sizeof(expr_anls_result_t));
    if (p1 == NULL) {
        printf("failed malloc in analysis_allocate_2dim_array\n");
        exit(2);
    }
    for (int i = 0; i < dim1; i++) {
        p2 = (expr_anls_result_t)malloc(dim2 * sizeof(struct expr_anls_result));
        if (p2 == NULL) {
            printf("failed malloc in analysis_allocate_2dim_array\n");
            exit(2);
        }
        for (int j = 0; j < dim2; j++) {
            (p2 + j)->avail = 0;
            (p2 + j)->used = 0;
            (p2 + j)->anti = 0;
            (p2 + j)->binary = 0;
        }
        *(p1 + i) = p2;
    }
    return p1;
}

static void analysis_free_2dim_array(expr_anls_result_t *p, int dim1) {
    if (p == NULL)
        return;
    for (int i = 0; i < dim1; i++) {
        free(*(p + i));
    }
    free(p);
}

// allocate two dimentional array for list of signals that can be represented as binary
// for each of blocks and statement
static sig_anls_result_t *analysis_allocate_signal_array(int dim1, int dim2) {
    sig_anls_result_t *p1;
    sig_anls_result_t p2;
    p1 = (sig_anls_result_t *)malloc(dim1 * sizeof(sig_anls_result_t));
    if (p1 == NULL) {
        printf("failed malloc in analysis_allocate_signal_array\n");
        exit(2);
    }
    for (int i = 0; i < dim1; i++) {
        p2 = (sig_anls_result_t)malloc(dim2 * sizeof(struct sig_anls_result));
        if (p2 == NULL) {
            printf("failed malloc in analysis_allocate_signal_array\n");
            exit(2);
        }
        for (int j = 0; j < dim2; j++) {
            (p2 + j)->binary = 0;
            (p2 + j)->anti_binary = 0;
            (p2 + j)->sensitive = 0;
            (p2 + j)->loop_linear = 0;
        }
        *(p1 + i) = p2;
    }
    return p1;
}

static void analysis_free_signal_array(sig_anls_result_t *p, int dim1) {
    for (int i = 0; i < dim1; i++) {
        free(*(p + i));
    }
    free(p);
}

#ifdef DEBUG_VEFA_ANALYSIS
static void analysis_show_signal_array(sig_anls_result_t *p, int dim1, int dim2, int opcode) {
    int value;
    sig_anls_result_t ep;
    printf("/***");
    switch (opcode) {
    case 0:
        printf(" binary dim1=%d dim2=%d***/\n", dim1, dim2);
        break;

    case 1:
        printf(" anti_binary dim1=%d dim2=%d***/\n", dim1, dim2);
        break;

    case 2:
        printf(" sesitivity dim1=%d dim2=%d***/\n", dim1, dim2);
        break;
    }
    for (int i = 0; i < dim1; i++) {
        printf("%3d: ", i);
        for (int j = 0; j < dim2; j++) {
            ep = *(p + i) + j;
            switch (opcode) {
            case 0:
                value = ep->binary;
                break;
            case 1:
                value = ep->anti_binary;
                break;
            case 2:
                value = ep->sensitive;
                break;
            }
            printf("%d ", value);
        }
        printf("\n");
    }
}
#endif

void analysis_free_fg() {
    stmt_list_t p, blk, p2;
    fg_edge_list_t e, e2;
    blk = blks;
    while (blk) {
        p = blk;
        blk = blk->next_blk;
        while (p) {
            p2 = p->next_stmt;
            free(p);
            p = p2;
        }
    }
    e = fg_edges;
    while (e) {
        e2 = e->next;
        free(e);
        e = e2;
    }
}

void analysis_free_process_info(struct process_info *p) {

    if (p->stmt_in) {
        analysis_free_signal_array(p->stmt_in, p->n_stmt);
        p->stmt_in = NULL;
    }
    if (p->edges) {
        free(p->edges);
        p->edges = NULL;
    }
    if (p->block_list) {
        free(p->block_list);
        p->block_list = NULL;
    }
    if (p->stmt_list) {
        free(p->stmt_list);
        p->stmt_list = NULL;
    }
    if (p->sig_list) {
        free(p->sig_list);
        p->sig_list = NULL;
    }

    if (p->exprs) {
        analysis_free_expr(p->exprs);
        p->exprs = NULL;
    }
    if (p->stmt_in_expr) {
        analysis_free_2dim_array(p->stmt_in_expr, p->n_stmt);
        p->stmt_in_expr = NULL;
    }
    if (p->stmt_out_expr) {
        analysis_free_2dim_array(p->stmt_out_expr, p->n_stmt);
        p->stmt_out_expr = NULL;
    }
    if (p->stmt_mid_expr) {
        analysis_free_2dim_array(p->stmt_mid_expr, p->n_stmt);
        p->stmt_mid_expr = NULL;
    }
    analysis_free_fg();
}

static ivl_event_t analysis_find_event() {
    ivl_event_t evnt = NULL;
    stmt_list_t p, blk;
    blk = blks;
    while (blk) {
        p = blk;
        while (p) {
            if (p->net) {
                if (ivl_statement_type(p->net) == IVL_ST_WAIT) {
                    evnt = ivl_stmt_events(p->net, 0);
                    break;
                }
            }
            p = p->next_stmt;
        }
        if (evnt != NULL)
            break;
        blk = blk->next_blk;
    }
    return evnt;
}

static void analysis_count_l_sigs(int *n_l_sig) {
    stmt_list_t p, blk;
    int n_sig = 0;
    ivl_signal_t sig;
    ivl_lval_t lval;
    link_list_t p1, p2;
    blk = blks;
    while (blk) {
        p = blk;
        while (p) {
            if (p->net) {
                if (ivl_statement_type(p->net) == IVL_ST_ASSIGN) {
                    for (unsigned i = 0; i < ivl_stmt_lvals(p->net); i++) {
                        lval = ivl_stmt_lval(p->net, i);
                        sig = ivl_lval_sig(lval);
                        p1 = l_sig_list;
                        while (p1) {
                            if (p1->item == sig)
                                break;
                            p1 = p1->next;
                        }
                        if (p1 == NULL) {
                            p1 = (struct link_list *)malloc(sizeof(struct link_list));
                            if (p1 == NULL) {
                                printf("malloc failed in analysis_count_l_sig\n");
                                exit(2);
                            }
                            p1->item = sig;
                            p1->next = l_sig_list;
                            l_sig_list = p1;
                            n_sig++;
                        }
                    }
                }
            }
            p = p->next_stmt;
        }
        blk = blk->next_blk;
    }
    *n_l_sig = n_sig;
    // collect l_sig for ASSIGN_NB
    blk = blks;
    while (blk) {
        p = blk;
        while (p) {
            if (p->net) {
                if (ivl_statement_type(p->net) == IVL_ST_ASSIGN_NB) {
                    for (unsigned i = 0; i < ivl_stmt_lvals(p->net); i++) {
                        lval = ivl_stmt_lval(p->net, i);
                        sig = ivl_lval_sig(lval);
                        p1 = l_sig_list;
                        p2 = p1; // p2 is last one
                        while (p1) {
                            p2 = p1;
                            if (p1->item == sig)
                                break;
                            p1 = p1->next;
                        }
                        if (p1 == NULL) {
                            p1 = (struct link_list *)malloc(sizeof(struct link_list));
                            if (p1 == NULL) {
                                printf("malloc failed in analysis_count_l_sig\n");
                                exit(2);
                            }
                            p1->item = sig;
                            p1->next = NULL;
                            if (p2 == NULL) {
                                l_sig_list = p1;
                            } else {
                                p2->next = p1;
                            }
                            n_sig++;
                        }
                    }
                }
            }
            p = p->next_stmt;
        }
        blk = blk->next_blk;
    }
    *(n_l_sig + 1) = n_sig;
}

static int analysis_place_l_sigs(ivl_signal_t sig_list[]) {
    link_list_t p1;
    int cnt = 0;
    p1 = l_sig_list;
    while (p1) {
        sig_list[cnt++] = (ivl_signal_t)p1->item;
        p1 = p1->next;
    }
    return cnt;
}

// analysis_find_l_sigs() will collect some of signals from ST_ASSIGN
// pack rest of signals into array
static int analysis_signal_array(int n_sig, expr_list_t exprs, ivl_signal_t sig_list[]) {
    int idx = n_sig;
    expr_list_t p;
    ivl_signal_t sig;
    p = exprs;
    while (p) {
        if (ivl_expr_type(p->net) == IVL_EX_SIGNAL
            //           && ivl_expr_oper1(p->net)==NULL
        ) {
            sig = ivl_expr_signal(p->net);
            int new_sig = 1;
            for (int i = 0; i < idx; i++) {
                if (sig_list[i] == sig)
                    new_sig = 0;
            }
            if (new_sig) {
                sig_list[idx] = sig;
                idx++;
            }
        }
        p = p->next_diff;
    }
    return idx;
}

// if no_print is 0, will print
// if sizes is a valid pointer, it will get the number of blocks, statments and edges
// Meaning of 2(0 L76) statement_type(stmt_id lineno)
/********
file:/home/xingzhi/sim_ver/DW_ram_r_w_s_dff.v
blk id:0 2(0 L76) 9(1 L102)
blk id:1
blk id:2 19(2 L103) 19(3 L105)
Edges:
  0 :   2 ===>start: end
  2 :   1
  0 :   1
blk: 3, stmt: 4, edge: 3
********/
static void analysis_show_blks(int no_print, int *sizes) {
    int n_blk, n_stmt, n_edge;
    stmt_list_t p, blk;
    fg_edge_list_t e;
    n_blk = 0;
    n_stmt = 0;
    n_edge = 0;
    if (!no_print && blks->net)
        printf("file:%s line:%d\n", ivl_stmt_file(blks->net), ivl_stmt_lineno(blks->net));
    blk = blks;
    while (blk) {
        n_blk++;
        if (!no_print)
            printf("blk id:%d lvl:%d dpt:%d dly:%d\n", blk->blk_id, blk->loop_level,
                   blk->loop_depth, blk->dly_in_loop);
        if (!no_print && blk->net)
            printf("\t");
        p = blk;
        while (p) {
            if (p->net) {
                n_stmt++;
                if (!no_print)
                    printf("%d(%d L%d) ", ivl_statement_type(p->net), p->stmt_id,
                           ivl_stmt_lineno(p->net));
            }
            p = p->next_stmt;
        }
        if (!no_print && blk->net)
            printf("\n");
        blk = blk->next_blk;
    }
    if (!no_print)
        printf("Edges:\n");
    e = fg_edges;
    while (e) {
        n_edge++;
        if (!no_print)
            printf("%3d : %3d\n", e->start, e->end);
        e = e->next;
    }
    if (!no_print)
        printf("blk: %d, stmt: %d, edge: %d\n\n", n_blk, n_stmt, n_edge);
    if (sizes) {
        sizes[0] = n_blk;
        sizes[1] = n_stmt;
        sizes[2] = n_edge;
    }
}

// assign IDs to the statements
static ivl_statement_t *analysis_stmt_array(int n_stmt) {
    int stmt_id;
    stmt_list_t p, blk;
    ivl_statement_t *stmt_list;
    stmt_list = (ivl_statement_t *)malloc(n_stmt * sizeof(ivl_statement_t *));
    if (stmt_list == NULL) {
        printf("failed malloc in analysis_stmt_array\n");
        exit(2);
    }
    stmt_id = 0;
    blk = blks;
    while (blk) {
        p = blk;
        while (p) {
            if (p->net) {
                p->stmt_id = stmt_id;
                stmt_list[stmt_id] = p->net;
                stmt_id++;
            }
            p = p->next_stmt;
        }
        blk = blk->next_blk;
    }
    return stmt_list;
}

static int analysis_comp_sig_event(ivl_signal_t sig, ivl_event_t net) {
    unsigned idx;
    ivl_nexus_t nex;
    if (net == NULL)
        return 100;
    nex = ivl_signal_nex(sig, 0);
    for (idx = 0; idx < ivl_event_nany(net); idx += 1) {
        if (nex == ivl_event_any(net, idx))
            return 0;
    }
    for (idx = 0; idx < ivl_event_nneg(net); idx += 1) {
        if (nex == ivl_event_neg(net, idx))
            return -1;
    }

    for (idx = 0; idx < ivl_event_npos(net); idx += 1) {
        if (nex == ivl_event_pos(net, idx))
            return 1;
    }
    return 100;
}

static void analysis_sig_binary(sig_anls_result_t *blk_in, sig_anls_result_t *blk_out,
                                sig_anls_result_t *stmt_in, sig_anls_result_t *stmt_out,
                                fg_edge_list_t *edges, stmt_list_t *block_list, int opcode,
                                ivl_signal_t *sig_list, ivl_event_t evnt, int n_l_sig,
                                int n_all_sig, int n_blk, int n_stmt, int n_edge) {
    int *anls_temp;
    int i, j, k;
    int stmt_id = 0;
    int iter;
    int change;
    int exit_blk_id;
    anls_temp = (int *)malloc(n_all_sig * sizeof(int));
    if (anls_temp == NULL) {
        printf("malloc faile in analysis_sig_binary\n");
        exit(2);
    }

    // find EXIT block
    for (i = 1; i < n_blk; i++) {
        for (j = 0; j < n_edge; j++) {
            if ((*(edges + j))->start == i)
                break;
        }
        if (j == n_edge)
            break;
    }
    exit_blk_id = i;

    switch (opcode) {
    case 0:
        for (i = 0; i < n_blk; i++) {
            for (j = 0; j < n_all_sig; j++) {
                (*(blk_out + i) + j)->binary = 1;
            }
        }
        do {
            change = 0;
            for (i = 0; i < n_blk; i++) {
                // blk_in & of source blk_out
                if (i > 0) {
                    for (j = 0; j < n_all_sig; j++) {
                        *(anls_temp + j) = 1;
                    }
                    for (j = 0; j < n_edge; j++) {
                        if ((*(edges + j))->end == i) { // incoming edge to node i
                            int edge_start = (*(edges + j))->start;
                            for (k = 0; k < n_all_sig; k++) {
                                int end_binary = 0;
                                if ((*(edges + j))->type ==
                                    VEFA_EDGE_IF_FALSE) { // a special case, edge guarantees binary
                                    stmt_list_t p = *(block_list + edge_start);
                                    while (p->next_stmt)
                                        p = p->next_stmt;
                                    ivl_expr_t ex = ivl_stmt_cond_expr(p->net);
                                    if (ivl_expr_type(ex) == IVL_EX_BINARY &&
                                        ivl_expr_opcode(ex) == 'N') {
                                        ivl_expr_t oper1 = ivl_expr_oper1(ex);
                                        ivl_expr_t oper2 = ivl_expr_oper2(ex);
                                        if (ivl_expr_type(oper2) == IVL_EX_NUMBER &&
                                            ivl_expr_type(oper1) == IVL_EX_BINARY &&
                                            ivl_expr_opcode(oper1) == '^') {

                                            ivl_expr_t oper1_1 = ivl_expr_oper1(oper1);
                                            ivl_expr_t oper1_2 = ivl_expr_oper2(oper1);
                                            unsigned oper2_width = ivl_expr_width(oper2);
                                            const char *oper2_bits = ivl_expr_bits(oper2);
                                            int all_zero = 1;
                                            for (unsigned l = 0; l < oper2_width; l++) {
                                                if (oper2_bits[l] != '0') {
                                                    all_zero = 0;
                                                    break;
                                                }
                                            }
                                            if (all_zero &&
                                                ivl_expr_type(oper1_1) == IVL_EX_SIGNAL &&
                                                ivl_expr_oper1(oper1_1) == NULL &&
                                                analysis_comp_expr(oper1_1, oper1_2) == 0 &&
                                                *(sig_list + k) == ivl_expr_signal(oper1_1)) {
                                                end_binary = 1;
                                            }
                                        }
                                    }
                                } // special case

                                *(anls_temp + k) =
                                    (*(anls_temp + k)) &
                                    ((*(blk_out + edge_start) + k)->binary | end_binary);
                            } // k
                        }
                    } // j
                    for (k = 0; k < n_all_sig; k++) {
                        if (*(anls_temp + k) != (*(blk_in + i) + k)->binary) {
                            (*(blk_in + i) + k)->binary = *(anls_temp + k);
                            change = 1;
                        }
                    }
                    if ((*(block_list + i))->net) {
                        stmt_id = (*(block_list + i))->stmt_id;
                        for (k = 0; k < n_all_sig; k++) {
                            (*(stmt_in + stmt_id) + k)->binary = *(anls_temp + k);
                        }
                    }
                }
                // calculate blk_out as well as stmt_in/stmt_out from blk_in
                if ((*(block_list + i))->net) {
                    stmt_list_t p = *(block_list + i);
                    while (p) {
                        if (p->net) {
                            stmt_id = p->stmt_id;
                            analysis_statement(p->net, 0, sig_list, *(stmt_in + stmt_id),
                                               *(stmt_out + stmt_id), n_l_sig, n_all_sig, opcode);
                            if ((p->next_stmt) && (p->next_stmt)->net) {
                                for (k = 0; k < n_all_sig; k++) {
                                    (*(stmt_in + stmt_id + 1) + k)->binary =
                                        (*(stmt_out + stmt_id) + k)->binary;
                                }
                            }
                        }
                        p = p->next_stmt;
                    }
                    for (k = 0; k < n_all_sig; k++) {
                        if ((*(blk_out + i) + k)->binary != (*(stmt_out + stmt_id) + k)->binary) {
                            change = 1;
                            (*(blk_out + i) + k)->binary = (*(stmt_out + stmt_id) + k)->binary;
                        }
                    }
                } else {
                    for (k = 0; k < n_all_sig; k++) {
                        if ((*(blk_out + i) + k)->binary != (*(blk_in + i) + k)->binary) {
                            change = 1;
                            (*(blk_out + i) + k)->binary = (*(blk_in + i) + k)->binary;
                        }
                    }
                }
            }
            // analysis_show_signal_array(stmt_in,n_stmt,n_all_sig,0);
        } while (change);
        break;

    case 1:
        for (i = 0; i < n_blk; i++) {
            for (j = 0; j < n_all_sig; j++) {
                (*(blk_out + i) + j)->anti_binary = 0;
                (*(blk_in + i) + j)->anti_binary = 0;
            }
        }
        iter = 0;
        // analysis_show_signal_array(stmt_in,n_stmt,n_all_sig,1);
        do {
            change = 0;
            for (i = 0; i < n_blk; i++) {
                if ((*(block_list + i))->loop_level == 0 ||
                    (*(block_list + i))->loop_level != (*(block_list + i))->loop_depth)
                    continue;
                if (i != exit_blk_id) {
                    for (j = 0; j < n_all_sig; j++) {
                        *(anls_temp + j) = 0;
                    }
                    for (j = 0; j < n_edge; j++) {
                        if ((*(edges + j))->start == i) {
                            int edge_end = (*(edges + j))->end;
                            for (k = 0; k < n_all_sig; k++) {
                                *(anls_temp + k) =
                                    (*(anls_temp + k)) | (*(blk_in + edge_end) + k)->anti_binary;
                            }
                        }
                    }
                    for (k = 0; k < n_all_sig; k++) {
                        if (*(anls_temp + k) != (*(blk_out + i) + k)->anti_binary) {
                            (*(blk_out + i) + k)->anti_binary = *(anls_temp + k);
                            change = 1;
                        }
                    }
                    stmt_list_t p = *(block_list + i);
                    if (p->net) {
                        while (p->next_stmt)
                            p = p->next_stmt;
                        stmt_id = p->stmt_id;
                        for (k = 0; k < n_all_sig; k++) {
                            (*(stmt_out + stmt_id) + k)->anti_binary = *(anls_temp + k);
                        }
                    }
                }
                // calculate blk_out as well as stmt_in/stmt_out from blk_in
                if ((*(block_list + i))->net) {
                    stmt_list_t p = *(block_list + i);
                    while (p->next_stmt)
                        p = p->next_stmt;
                    while (p) {
                        if (p->net) {
                            stmt_id = p->stmt_id;
                            analysis_statement(p->net, 0, sig_list, *(stmt_in + stmt_id),
                                               *(stmt_out + stmt_id), n_l_sig, n_all_sig, opcode);
                            if ((p->pre_stmt) && (p->pre_stmt)->net) {
                                for (k = 0; k < n_all_sig; k++) {
                                    (*(stmt_out + stmt_id - 1) + k)->anti_binary =
                                        (*(stmt_in + stmt_id) + k)->anti_binary;
                                }
                            }
                        }
                        p = p->pre_stmt;
                    }
                    for (k = 0; k < n_all_sig; k++) {
                        if ((*(blk_in + i) + k)->anti_binary !=
                            (*(stmt_in + stmt_id) + k)->anti_binary) {
                            change = 1;
                            (*(blk_in + i) + k)->anti_binary =
                                (*(stmt_in + stmt_id) + k)->anti_binary;
                        }
                    }
                } else {
                    for (k = 0; k < n_all_sig; k++) {
                        if ((*(blk_in + i) + k)->anti_binary != (*(blk_out + i) + k)->anti_binary) {
                            change = 1;
                            (*(blk_in + i) + k)->anti_binary = (*(blk_out + i) + k)->anti_binary;
                        }
                    }
                }
            }
            // printf("iter:%d\n",iter++);
            // analysis_show_signal_array(stmt_in,n_stmt,n_all_sig,1);
        } while (change);
        break;

    case 2:
        for (i = 0; i < n_blk; i++) {
            for (j = 0; j < n_all_sig; j++) {
                (*(blk_out + i) + j)->sensitive = 0;
                (*(blk_in + i) + j)->sensitive = 0;
            }
        }
        iter = 0;
        // analysis_show_signal_array(stmt_in,n_stmt,n_all_sig,1);
        do {
            change = 0;
            for (i = 0; i < n_blk; i++) {
                if (i != exit_blk_id) {
                    for (j = 0; j < n_all_sig; j++) {
                        *(anls_temp + j) = 0;
                    }
                    for (j = 0; j < n_edge; j++) {
                        if ((*(edges + j))->start == i) {
                            int edge_end = (*(edges + j))->end;
                            for (k = 0; k < n_all_sig; k++) {
                                *(anls_temp + k) =
                                    (*(anls_temp + k)) | (*(blk_in + edge_end) + k)->sensitive;
                            }
                        }
                    }
                    for (k = 0; k < n_all_sig; k++) {
                        if (*(anls_temp + k) != (*(blk_out + i) + k)->sensitive) {
                            (*(blk_out + i) + k)->sensitive = *(anls_temp + k);
                            change = 1;
                        }
                    }
                    stmt_list_t p = *(block_list + i);
                    if (p->net) {
                        while (p->next_stmt)
                            p = p->next_stmt;
                        stmt_id = p->stmt_id;
                        for (k = 0; k < n_all_sig; k++) {
                            (*(stmt_out + stmt_id) + k)->sensitive = *(anls_temp + k);
                        }
                    }
                }
                // calculate blk_in as well as stmt_in/stmt_out from blk_out
                if ((*(block_list + i))->net) {
                    stmt_list_t p = *(block_list + i);
                    while (p->next_stmt)
                        p = p->next_stmt;
                    while (p) {
                        if (p->net) {
                            stmt_id = p->stmt_id;
                            analysis_statement(p->net, 0, sig_list, *(stmt_in + stmt_id),
                                               *(stmt_out + stmt_id), n_l_sig, n_all_sig, opcode);
                            if ((p->pre_stmt) && (p->pre_stmt)->net) {
                                for (k = 0; k < n_all_sig; k++) {
                                    (*(stmt_out + stmt_id - 1) + k)->sensitive =
                                        (*(stmt_in + stmt_id) + k)->sensitive;
                                }
                            }
                        }
                        p = p->pre_stmt;
                    }
                    for (k = 0; k < n_all_sig; k++) {
                        if ((*(blk_in + i) + k)->sensitive !=
                            (*(stmt_in + stmt_id) + k)->sensitive) {
                            change = 1;
                            (*(blk_in + i) + k)->sensitive = (*(stmt_in + stmt_id) + k)->sensitive;
                        }
                    }
                } else {
                    for (k = 0; k < n_all_sig; k++) {
                        if ((*(blk_in + i) + k)->sensitive != (*(blk_out + i) + k)->sensitive) {
                            change = 1;
                            (*(blk_in + i) + k)->sensitive = (*(blk_out + i) + k)->sensitive;
                        }
                    }
                }
            }
            // printf("iter:%d\n",iter++);
            // analysis_show_signal_array(stmt_in,n_stmt,n_all_sig,1);
        } while (change);

        for (i = 0; i < n_l_sig; i++) {
            if ((*(stmt_in + 0) + i)->sensitive != 0 && ivl_signal_array_count(sig_list[i]) > 1 &&
                analysis_comp_sig_event(sig_list[i], evnt) != 100) {
                PINT_UNSUPPORTED_NOTE(ivl_signal_file(sig_list[i]), ivl_signal_lineno(sig_list[i]),
                                      "Array signal %s is writen and read in the same process.",
                                      ivl_signal_basename(sig_list[i]));
            }
        }
        break;
    }

    (void)iter;
    (void)n_stmt;
    free(anls_temp);
}
static void analysis_expr_redundancy(expr_anls_result_t *blk_in, expr_anls_result_t *blk_out,
                                     expr_anls_result_t *stmt_in, expr_anls_result_t *stmt_out,
                                     fg_edge_list_t *edges, stmt_list_t *block_list, int opcode,
                                     int n_expr, int n_blk, int n_stmt, int n_edge) {
    int change, stmt_id;
    int exit_blk_id;
    int i, j, k;
    int *anls_temp = (int *)malloc(n_expr * sizeof(int));
    if (anls_temp == NULL) {
        printf("malloc failed in analysis_expr_redundancy \n");
        exit(2);
    }
    // find EXIT block
    for (i = 1; i < n_blk; i++) {
        for (j = 0; j < n_edge; j++) {
            if ((*(edges + j))->start == i)
                break;
        }
        if (j == n_edge)
            break;
    }
    exit_blk_id = i;

    switch (opcode) {
    case 0: // anticipate
        for (i = 0; i < n_blk; i++) {
            for (j = 0; j < n_expr; j++) {
                (*(blk_in + i) + j)->anti = 1;
            }
        }
        do {
            change = 0;
            for (i = 0; i < n_blk; i++) {
                // blk_out & of sink blk_in
                if (i != exit_blk_id) {
                    for (j = 0; j < n_expr; j++) {
                        *(anls_temp + j) = 1;
                    }
                    for (j = 0; j < n_edge; j++) {
                        if ((*(edges + j))->start == i) { // incoming edge to node i
                            int edge_end = (*(edges + j))->end;
                            for (k = 0; k < n_expr; k++) {
                                *(anls_temp + k) =
                                    (*(anls_temp + k)) & (*(blk_in + edge_end) + k)->anti;
                            }
                        }
                    }
                    for (k = 0; k < n_expr; k++) {
                        if (*(anls_temp + k) != (*(blk_out + i) + k)->anti) {
                            (*(blk_out + i) + k)->anti = *(anls_temp + k);
                            change = 1;
                        }
                    }
                    // copy to stmt_out
                    stmt_list_t p = *(block_list + i);
                    if (p->net) {
                        while (p->next_stmt)
                            p = p->next_stmt;
                        stmt_id = p->stmt_id;
                        for (k = 0; k < n_expr; k++) {
                            (*(stmt_out + stmt_id) + k)->anti = *(anls_temp + k);
                        }
                    }
                }
                // calculate blk_in as well as stmt_in/stmt_out from blk_out
                if ((*(block_list + i))->net) {
                    stmt_list_t p = *(block_list + i);
                    while (p->next_stmt)
                        p = p->next_stmt;
                    while (p) {
                        if (p->net) {
                            stmt_id = p->stmt_id;
                            analysis_statement_expr(p->net, 0, *(stmt_in + stmt_id),
                                                    *(stmt_out + stmt_id), n_expr, opcode);
                            if ((p->pre_stmt) && (p->pre_stmt)->net) {
                                for (k = 0; k < n_expr; k++) {
                                    (*(stmt_out + stmt_id - 1) + k)->anti =
                                        (*(stmt_in + stmt_id) + k)->anti;
                                }
                            }
                        }
                        p = p->pre_stmt;
                    }
                    for (k = 0; k < n_expr; k++) {
                        if ((*(blk_in + i) + k)->anti != (*(stmt_in + stmt_id) + k)->anti) {
                            change = 1;
                            (*(blk_in + i) + k)->anti = (*(stmt_in + stmt_id) + k)->anti;
                        }
                    }
                } else {
                    for (k = 0; k < n_expr; k++) {
                        if ((*(blk_in + i) + k)->anti != (*(blk_out + i) + k)->anti) {
                            change = 1;
                            (*(blk_in + i) + k)->anti = (*(blk_out + i) + k)->anti;
                        }
                    }
                }
            }
        } while (change);
        break;
    case 1:
        for (i = 0; i < n_blk; i++) {
            for (j = 0; j < n_expr; j++) {
                (*(blk_out + i) + j)->avail = 1;
            }
        }
        do {
            change = 0;
            for (i = 0; i < n_blk; i++) {
                // blk_in & of source blk_out
                if (i > 0) {
                    for (j = 0; j < n_expr; j++) {
                        *(anls_temp + j) = 1;
                    }
                    for (j = 0; j < n_edge; j++) {
                        if ((*(edges + j))->end == i) { // incoming edge to node i
                            int edge_start = (*(edges + j))->start;
                            for (k = 0; k < n_expr; k++) {
                                *(anls_temp + k) =
                                    (*(anls_temp + k)) & (*(blk_out + edge_start) + k)->avail;
                            }
                        }
                    }
                    for (k = 0; k < n_expr; k++) {
                        if (*(anls_temp + k) != (*(blk_in + i) + k)->avail) {
                            (*(blk_in + i) + k)->avail = *(anls_temp + k);
                            change = 1;
                        }
                    }
                    // copy to stmt_in
                    if ((*(block_list + i))->net) {
                        stmt_id = (*(block_list + i))->stmt_id;
                        for (k = 0; k < n_expr; k++) {
                            (*(stmt_in + stmt_id) + k)->avail = *(anls_temp + k);
                        }
                    }
                }
                // calculate blk_out as well as stmt_in/stmt_out from blk_in
                if ((*(block_list + i))->net) {
                    stmt_list_t p = *(block_list + i);
                    while (p) {
                        if (p->net) {
                            stmt_id = p->stmt_id;
                            analysis_statement_expr(p->net, 0, *(stmt_in + stmt_id),
                                                    *(stmt_out + stmt_id), n_expr, opcode);
                            if ((p->next_stmt) && (p->next_stmt)->net) {
                                for (k = 0; k < n_expr; k++) {
                                    (*(stmt_in + stmt_id + 1) + k)->avail =
                                        (*(stmt_out + stmt_id) + k)->avail;
                                }
                            }
                        }
                        p = p->next_stmt;
                    }
                    for (k = 0; k < n_expr; k++) {
                        if ((*(blk_out + i) + k)->avail != (*(stmt_out + stmt_id) + k)->avail) {
                            change = 1;
                            (*(blk_out + i) + k)->avail = (*(stmt_out + stmt_id) + k)->avail;
                        }
                    }
                } else {
                    for (k = 0; k < n_expr; k++) {
                        if ((*(blk_out + i) + k)->avail != (*(blk_in + i) + k)->avail) {
                            change = 1;
                            (*(blk_out + i) + k)->avail = (*(blk_in + i) + k)->avail;
                        }
                    }
                }
            }
        } while (change);
        break;

    case 2: // used(live)
        // Not necessary, since it is initialized as 0
        for (i = 0; i < n_blk; i++) {
            for (j = 0; j < n_expr; j++) {
                (*(blk_in + i) + j)->used = 0;
            }
        }
        do {
            change = 0;
            for (i = 0; i < n_blk; i++) {
                // blk_out | of sink blk_in
                if (i != exit_blk_id) {
                    for (j = 0; j < n_expr; j++) {
                        *(anls_temp + j) = 0;
                    }
                    for (j = 0; j < n_edge; j++) {
                        if ((*(edges + j))->start == i) {
                            int edge_end = (*(edges + j))->end;
                            for (k = 0; k < n_expr; k++) {
                                *(anls_temp + k) =
                                    (*(anls_temp + k)) | (*(blk_in + edge_end) + k)->used;
                            }
                        }
                    }
                    for (k = 0; k < n_expr; k++) {
                        if (*(anls_temp + k) != (*(blk_out + i) + k)->used) {
                            (*(blk_out + i) + k)->used = *(anls_temp + k);
                            change = 1;
                        }
                    }
                    // copy to stmt_out
                    stmt_list_t p = *(block_list + i);
                    if (p->net) {
                        while (p->next_stmt)
                            p = p->next_stmt;
                        stmt_id = p->stmt_id;
                        for (k = 0; k < n_expr; k++) {
                            (*(stmt_out + stmt_id) + k)->used = *(anls_temp + k);
                        }
                    }
                }
                // calculate blk_in as well as stmt_in/stmt_out from blk_out
                if ((*(block_list + i))->net) {
                    stmt_list_t p = *(block_list + i);
                    while (p->next_stmt)
                        p = p->next_stmt;
                    while (p) {
                        if (p->net) {
                            stmt_id = p->stmt_id;
                            analysis_statement_expr(p->net, 0, *(stmt_in + stmt_id),
                                                    *(stmt_out + stmt_id), n_expr, opcode);
                            if ((p->pre_stmt) && (p->pre_stmt)->net) {
                                for (k = 0; k < n_expr; k++) {
                                    (*(stmt_out + stmt_id - 1) + k)->used =
                                        (*(stmt_in + stmt_id) + k)->used;
                                }
                            }
                        }
                        p = p->pre_stmt;
                    }
                    for (k = 0; k < n_expr; k++) {
                        if ((*(blk_in + i) + k)->used != (*(stmt_in + stmt_id) + k)->used) {
                            change = 1;
                            (*(blk_in + i) + k)->used = (*(stmt_in + stmt_id) + k)->used;
                        }
                    }
                } else {
                    for (k = 0; k < n_expr; k++) {
                        if ((*(blk_in + i) + k)->used != (*(blk_out + i) + k)->used) {
                            change = 1;
                            (*(blk_in + i) + k)->used = (*(blk_out + i) + k)->used;
                        }
                    }
                }
            }
        } while (change);
        break;
    }

    (void)n_stmt;
    free(anls_temp);
}

void analysis_statement(ivl_statement_t net, unsigned ind, ivl_signal_t sig_list[],
                        sig_anls_result_t stmt_in, sig_anls_result_t stmt_out, int n_l_sig,
                        int n_all_sig, int opcode) {
    ivl_signal_t sig;
    ivl_lval_t lval;
    vefa_expr_info_t expr_p;
    expr_list_t expr_stmt = NULL;
    expr_list_t p, p1;
    struct expr_companion2 expr_cpn;
    struct expr_bundle expr_bdl;
    (void)n_l_sig;

    expr_bdl.exprs = NULL;
    expr_bdl.stmt_in = NULL;
    expr_bdl.avail_anls = 0;
    expr_bdl.index = 0;

    switch (opcode) {
    case 0:
        for (int i = 0; i < n_all_sig; i++) {
            (stmt_out + i)->binary = (stmt_in + i)->binary;
        }
        if (ivl_statement_type(net) == IVL_ST_ASSIGN) {
            expr_cpn.n_sig = n_all_sig;
            expr_cpn.sig_list = sig_list;
            expr_cpn.stmt_in_sig = stmt_in;
            expr_p = analysis_expression(ivl_stmt_rval(net), ind, expr_cpn);
            for (unsigned j = 0; j < ivl_stmt_lvals(net); j++) {
                lval = ivl_stmt_lval(net, j);
                sig = ivl_lval_sig(lval);
                if (ivl_signal_array_count(sig) == 1) {
                    for (int i = 0; i < n_all_sig; i++) {
                        if (sig_list[i] == sig) {
                            if (expr_p->is_binary &&
                                (ivl_lval_part_off(lval) == NULL || (stmt_in + i)->binary)) {
                                (stmt_out + i)->binary = 1;
                            } else {
                                (stmt_out + i)->binary = 0;
                            }
                            break;
                        }
                    }
                }
            }
            vefa_free_expr_info(expr_p);
        }
        if (ivl_statement_type(net) == IVL_ST_WAIT || ivl_statement_type(net) == IVL_ST_DELAY) {
            for (int i = 0; i < n_all_sig; i++) {
                (stmt_out + i)->binary = 0;
            }
        }
        break;

    case 1:
        for (int i = 0; i < n_all_sig; i++) {
            (stmt_in + i)->anti_binary = (stmt_out + i)->anti_binary;
        }

        expr_stmt = analysis_collect_expr(net, ind, expr_stmt, 0, expr_bdl);
        // analysis_show_expr(expr_stmt);
        p = expr_stmt;
        while (p) {
            p1 = p;
            while (p1) {
                if (p1->index > 0 && p1->sig_dpnd) {
                    link_list_t lp1 = p1->sig_dpnd;
                    while (lp1) {
                        for (int j = 0; j < n_all_sig; j++) {
                            if (lp1->item == *(sig_list + j)) {
                                (stmt_in + j)->anti_binary = 1;
                                break;
                            }
                        }
                        lp1 = lp1->next;
                    }
                }
                p1 = p1->next_same;
            }
            p = p->next_diff;
        }
        analysis_free_expr(expr_stmt);
        expr_stmt = NULL;

        if (ivl_statement_type(net) == IVL_ST_ASSIGN) {
            lval = ivl_stmt_lval(net, 0);
            sig = ivl_lval_sig(lval);
            if (ivl_signal_array_count(sig) == 1) {
                for (int i = 0; i < n_all_sig; i++) {
                    if (sig_list[i] == sig) {
                        int add_rval = (stmt_in + i)->anti_binary;
                        if (ivl_lval_part_off(lval) == NULL)
                            (stmt_in + i)->anti_binary = 0;
                        // if the lval sig is anti_binary, rval need to be binary, but the lval
                        // itself is no longer need to be anti_binary
                        if (add_rval) {
                            expr_cpn.n_sig = n_all_sig;
                            expr_cpn.sig_list = sig_list;
                            expr_cpn.stmt_in_sig = stmt_in;
                            expr_p = analysis_expression(ivl_stmt_rval(net), ind, expr_cpn);

                            link_list_t lp1 = expr_p->sig_dpnd;
                            while (lp1) {
                                for (int j = 0; j < n_all_sig; j++) {
                                    if (lp1->item == *(sig_list + j)) {
                                        (stmt_in + j)->anti_binary = 1;
                                        break;
                                    }
                                }
                                lp1 = lp1->next;
                            }
                            vefa_free_expr_info(expr_p);
                        }
                    }
                }
            }
        }
        if (ivl_statement_type(net) == IVL_ST_WAIT || ivl_statement_type(net) == IVL_ST_DELAY) {
            for (int i = 0; i < n_all_sig; i++) {
                (stmt_in + i)->anti_binary = 0;
            }
        }
        break;
    case 2:
        for (int i = 0; i < n_all_sig; i++) {
            (stmt_in + i)->sensitive = (stmt_out + i)->sensitive;
        }

        if (ivl_statement_type(net) == IVL_ST_ASSIGN) {
            for (unsigned j = 0; j < ivl_stmt_lvals(net); j++) {
                lval = ivl_stmt_lval(net, j);
                sig = ivl_lval_sig(lval);
                if (ivl_signal_array_count(sig) == 1) {
                    for (int i = 0; i < n_all_sig; i++) {
                        if (sig_list[i] == sig) {
                            if (ivl_lval_part_off(lval) == NULL) {
                                (stmt_in + i)->sensitive = 0;
                            }
                            break;
                        }
                    }
                }
            }
        }

        expr_stmt = analysis_collect_expr(net, ind, expr_stmt, 0, expr_bdl);
        // analysis_show_expr(expr_stmt);
        p = expr_stmt;
        while (p) {
            link_list_t lp1 = p->sig_dpnd;
            while (lp1) {
                for (int j = 0; j < n_all_sig; j++) {
                    if (lp1->item == *(sig_list + j)) {
                        (stmt_in + j)->sensitive = 1;
                        break;
                    }
                }
                lp1 = lp1->next;
            }
            p = p->next_diff;
        }
        analysis_free_expr(expr_stmt);
        expr_stmt = NULL;
        break;
    }
}

// compute forward/backward effect of statement for expr_anls_result
void analysis_statement_expr(ivl_statement_t net, unsigned ind, expr_anls_result_t stmt_in,
                             expr_anls_result_t stmt_out, int n_expr, int opcode) {
    // opcode 0-anti 1-avail 2-used(live)
    ivl_signal_t sig;
    ivl_lval_t lval;
    expr_list_t expr_stmt = NULL;
    expr_list_t p, p1;
    link_list_t lp;
    struct expr_bundle expr_bdl;
    expr_bdl.exprs = NULL;
    expr_bdl.stmt_in = NULL;
    expr_bdl.avail_anls = 0;
    switch (opcode) {
    case 0:
        for (int i = 0; i < n_expr; i++) {
            (stmt_in + i)->anti = 0;
        }
        expr_stmt = analysis_collect_expr(net, ind, expr_stmt, 0, expr_bdl);
        p = expr_stmt;
        while (p) {
            if (NULL != (p1 = analysis_find_expr(p->net, exprs))) {
                if (p1->id >= 0) {
                    (stmt_in + p1->id)->anti = 1;
                }
            }
            p = p->next_diff;
        }
        analysis_free_expr(expr_stmt);
        expr_stmt = NULL;

        if (ivl_statement_type(net) == IVL_ST_ASSIGN) {
            lval = ivl_stmt_lval(net, 0);
            sig = ivl_lval_sig(lval);
            for (int i = 0; i < n_expr; i++) {
                if ((stmt_out + i)->anti) {
                    p = analysis_find_expr_by_index(i, exprs);
                    if (p) {
                        lp = p->sig_dpnd;
                        int kill = 0;
                        while (lp) {
                            if ((ivl_signal_t)lp->item == sig) {
                                kill = 1;
                                break;
                            }
                            lp = lp->next;
                        }
                        if (kill == 0)
                            (stmt_in + i)->anti = 1;
                    } else {
                        printf("expr not found in exprs (analysis_statement_expr)\n");
                    }
                }
            }
        } else if (ivl_statement_type(net) == IVL_ST_WAIT ||
                   ivl_statement_type(net) == IVL_ST_DELAY) {
            for (int i = 0; i < n_expr; i++) {
                (stmt_in + i)->anti = 0;
            }
        } else {
            for (int i = 0; i < n_expr; i++) {
                if ((stmt_out + i)->anti)
                    (stmt_in + i)->anti = 1;
            }
        }
        break;
    case 1: // avail
        for (int i = 0; i < n_expr; i++) {
            (stmt_out + i)->avail = (stmt_in + i)->avail;
        }
        expr_bdl.avail_anls = 1;
        expr_stmt = analysis_collect_expr(net, ind, expr_stmt, 0, expr_bdl);
        p = expr_stmt;
        while (p) {
            if (NULL != (p1 = analysis_find_expr(p->net, exprs))) {
                if (p1->id >= 0) {
                    (stmt_out + p1->id)->avail = 1;
                }
            }
            p = p->next_diff;
        }
        analysis_free_expr(expr_stmt);
        expr_stmt = NULL;

        if (ivl_statement_type(net) == IVL_ST_ASSIGN) {
            lval = ivl_stmt_lval(net, 0);
            sig = ivl_lval_sig(lval);
            for (int i = 0; i < n_expr; i++) {
                p = analysis_find_expr_by_index(i, exprs);
                if (ivl_expr_type(p->net) == IVL_EX_SIGNAL && ivl_expr_oper1(p->net) == NULL &&
                    ivl_expr_signal(p->net) == sig && ivl_lval_idx(lval) == NULL &&
                    (ivl_lval_part_off(lval) == NULL || (stmt_in + i)->avail)) {
                    (stmt_out + i)->avail = 1;
                } else if ((stmt_out + i)->avail) {
                    if (p) {
                        lp = p->sig_dpnd;
                        while (lp) {
                            if ((ivl_signal_t)lp->item == sig) {
                                (stmt_out + i)->avail = 0;
                                break;
                            }
                            lp = lp->next;
                        }
                    } else {
                        printf("expr not found in exprs (analysis_statement_expr)\n");
                    }
                }
            }
        }
        if (ivl_statement_type(net) == IVL_ST_WAIT || ivl_statement_type(net) == IVL_ST_DELAY) {
            for (int i = 0; i < n_expr; i++) {
                (stmt_out + i)->avail = 0;
            }
        }
        break;
    case 2:
        for (int i = 0; i < n_expr; i++) {
            (stmt_in + i)->used = 0;
        }
        expr_bdl.exprs = exprs;
        expr_bdl.stmt_in = stmt_in;
        expr_stmt = analysis_collect_expr(net, ind, expr_stmt, 0, expr_bdl);
        p = expr_stmt;
        while (p) {
            if (NULL != (p1 = analysis_find_expr(p->net, exprs))) {
                if (p1->id >= 0) {
                    (stmt_in + p1->id)->used = 1;
                }
            }
            p = p->next_diff;
        }
        analysis_free_expr(expr_stmt);
        expr_stmt = NULL;

        if (ivl_statement_type(net) == IVL_ST_ASSIGN) {
            lval = ivl_stmt_lval(net, 0);
            sig = ivl_lval_sig(lval);
            for (int i = 0; i < n_expr; i++) {
                if ((stmt_out + i)->used) {
                    p = analysis_find_expr_by_index(i, exprs);
                    if (p) {
                        lp = p->sig_dpnd;
                        int kill = 0;
                        while (lp) {
                            if ((ivl_signal_t)lp->item == sig) {
                                kill = 1;
                                break;
                            }
                            lp = lp->next;
                        }
                        if (kill == 0)
                            (stmt_in + i)->used = 1;
                    } else {
                        printf("expr not found in exprs (analysis_statement_expr)\n");
                    }
                }
            }
        } else if (ivl_statement_type(net) == IVL_ST_WAIT ||
                   ivl_statement_type(net) == IVL_ST_DELAY) {
            for (int i = 0; i < n_expr; i++) {
                (stmt_in + i)->used = 0;
            }
        } else {
            for (int i = 0; i < n_expr; i++) {
                if ((stmt_out + i)->used)
                    (stmt_in + i)->used = 1;
            }
        }
        break;
    }
}
static int analysis_calculate_expr_cost(sig_anls_result_t *stmt_in, sig_anls_result_t *stmt_out,
                                        expr_anls_result_t *stmt_in_expr,
                                        expr_anls_result_t *stmt_out_expr, ivl_signal_t *sig_list,
                                        expr_anls_cost_t expr_cost, int n_stmt, int n_sig,
                                        int n_expr) {
    int i, j, k;
    expr_list_t p;
    vefa_expr_info_t expr_p;
    int offset = 0;
    int reg_alloc;
    struct expr_companion2 expr_cpn;

    // check if expr is binary
    for (i = 0; i < n_stmt; i++) {
        expr_cpn.sig_list = sig_list;
        expr_cpn.n_sig = n_sig;
        expr_cpn.stmt_in_sig = *(stmt_in + i);
        for (j = 0; j < n_expr; j++) {
            p = analysis_find_expr_by_index(j, exprs);
            expr_p = analysis_expression(p->net, 0, expr_cpn);
            (*(stmt_in_expr + i) + j)->binary = expr_p->is_binary;
            vefa_free_expr_info(expr_p);
        }
        expr_cpn.stmt_in_sig = *(stmt_out + i);
        for (j = 0; j < n_expr; j++) {
            p = analysis_find_expr_by_index(j, exprs);
            expr_p = analysis_expression(p->net, 0, expr_cpn);
            (*(stmt_out_expr + i) + j)->binary = expr_p->is_binary;
            vefa_free_expr_info(expr_p);
        }
    }
    // update expr_cost about binary
    for (i = 0; i < n_expr; i++) {
        (expr_cost + i)->binary = 1;
        for (j = 0; j < n_stmt; j++) {
            if (((*(stmt_in_expr + j) + i)->avail || (*(stmt_in_expr + j) + i)->used) &&
                !(*(stmt_in_expr + j) + i)->binary) {
                (expr_cost + i)->binary = 0;
            }
        }
    }
    // calculate number of registers needed
    for (i = 0; i < n_expr; i++) {
        p = analysis_find_expr_by_index(i, exprs);
        int width = ivl_expr_width(p->net);
        (expr_cost + i)->width = width;
        if ((expr_cost + i)->binary) {
            (expr_cost + i)->n_reg = 1; // if more than 32b, will hold address
            if (width > 32)
                (expr_cost + i)->value = 0;
            else
                (expr_cost + i)->value = 1;
        } else {
            if (ivl_expr_type(p->net) == IVL_EX_STRING || ivl_expr_type(p->net) == IVL_EX_ARRAY) {
                (expr_cost + i)->n_reg = 1;
                (expr_cost + i)->value = 0;
            } else if (width > 32) {
                (expr_cost + i)->n_reg = 1;
                (expr_cost + i)->value = 0;
            } else if (width > 16) {
                (expr_cost + i)->n_reg = 2;
                (expr_cost + i)->value = 1;
            } else {
                (expr_cost + i)->n_reg = 1;
                (expr_cost + i)->value = 1;
            }
        }
    }
    // find maximum number of registers needed for expr, if all exprs are register allocated
    for (i = 0; i < n_expr; i++) {
        (expr_cost + i)->need_alloc = 0;
        for (j = 0; j < n_stmt; j++) {
            if ((*(stmt_in_expr + j) + i)->avail && (*(stmt_in_expr + j) + i)->used) {
                (expr_cost + i)->need_alloc = 1;
            }
        }
    }
    // register allocation
    for (i = 0; i < n_expr; i++) {
        (expr_cost + i)->reg_l = 0;
        (expr_cost + i)->reg_h = 0;
        (expr_cost + i)->offset = 0;
        (expr_cost + i)->m_alloc = 0;
        for (j = 0; j < 6; j++)
            (expr_cost + i)->avail_regs[j] = 1;
    }
    for (i = 0; i < n_expr; i++) {
        if ((expr_cost + i)->need_alloc == 0)
            continue;
        reg_alloc = 0;
        for (j = 0; j < 6; j++) {
            if ((expr_cost + i)->avail_regs[j])
                break;
        }
        if (j < 6) {
            (expr_cost + i)->reg_l = j + 16;
            (expr_cost + i)->avail_regs[j] = 0;
            if ((expr_cost + i)->n_reg == 1)
                reg_alloc = 1;
        }
        if ((expr_cost + i)->n_reg > 1) {
            for (j = 0; j < 6; j++) {
                if ((expr_cost + i)->avail_regs[j])
                    break;
            }
            if (j < 6) {
                (expr_cost + i)->reg_h = j + 16;
                (expr_cost + i)->avail_regs[j] = 0;
                reg_alloc = 1;
            } else {
                (expr_cost + i)->reg_l = 0;
            }
        }
        if (reg_alloc) {
            for (j = 0; j < n_stmt; j++) {
                if ((*(stmt_in_expr + j) + i)->avail && (*(stmt_in_expr + j) + i)->used) {
                    for (k = i + 1; k < n_expr; k++) {
                        if ((*(stmt_in_expr + j) + k)->avail && (*(stmt_in_expr + j) + k)->used) {
                            (expr_cost + k)->avail_regs[(expr_cost + i)->reg_l - 16] = 0;
                            if ((expr_cost + i)->reg_h)
                                (expr_cost + k)->avail_regs[(expr_cost + i)->reg_h - 16] = 0;
                        }
                        // if an expr need a register for out, need to be reserved
                        if ((*(stmt_out_expr + j) + k)->avail && (*(stmt_out_expr + j) + k)->used) {
                            (expr_cost + k)->avail_regs[(expr_cost + i)->reg_l - 16] = 0;
                            if ((expr_cost + i)->reg_h)
                                (expr_cost + k)->avail_regs[(expr_cost + i)->reg_h - 16] = 0;
                        }
                    }
                }
            }
        } else {
            (expr_cost + i)->offset = offset;
            (expr_cost + i)->m_alloc = 1;
            offset += 4 * (expr_cost + i)->n_reg;
        }
    }
// print
#ifdef DEBUG_VEFA_ANALYSIS
    printf("expr_cost:\n");
    printf("n_reg  ");
    for (i = 0; i < n_expr; i++)
        printf("%4d ", (expr_cost + i)->n_reg);
    printf("\n");
    printf("binary ");
    for (i = 0; i < n_expr; i++)
        printf("%4d ", (expr_cost + i)->binary);
    printf("\n");
    printf("value ");
    for (i = 0; i < n_expr; i++)
        printf("%4d ", (expr_cost + i)->value);
    printf("\n");
    printf("nd_all ");
    for (i = 0; i < n_expr; i++)
        printf("%4d ", (expr_cost + i)->need_alloc);
    printf("\n");
    printf("width  ");
    for (i = 0; i < n_expr; i++)
        printf("%4d ", (expr_cost + i)->width);
    printf("\n");
    printf("reg_l  ");
    for (i = 0; i < n_expr; i++)
        printf("%4d ", (expr_cost + i)->reg_l);
    printf("\n");
    printf("reg_h  ");
    for (i = 0; i < n_expr; i++)
        printf("%4d ", (expr_cost + i)->reg_h);
    printf("\n");
    printf("offset ");
    for (i = 0; i < n_expr; i++)
        printf("%4d ", (expr_cost + i)->offset);
    printf("\n");
    printf("m_alloc");
    for (i = 0; i < n_expr; i++)
        printf("%4d ", (expr_cost + i)->m_alloc);
    printf("\n");
#endif
    return offset;
}

void analysis_process(ivl_statement_t net, unsigned ind, struct process_info *proc_info,
                      int opcode) {
    int graph_size[3]; // blk,stmt,edge
    ivl_signal_t *sig_list = NULL;
    stmt_list_t *block_list = NULL;
    ivl_statement_t *stmt_list = NULL;
    sig_anls_result_t *blk_in_sig, *blk_out_sig, *stmt_in_sig, *stmt_out_sig;
    expr_anls_result_t *stmt_in_expr = NULL, *stmt_out_expr = NULL, *stmt_mid_expr = NULL;
    fg_edge_list_t *edges;
    int n_l_sig[2]; //[0]-ASSiGN, [1] ASSIGN+ASSIGN_NB
    int n_sig2, n_edge, n_blk, n_stmt, n_expr = 0, n_all_sig;
    ivl_event_t evnt;

    struct expr_bundle expr_bdl;
    expr_bdl.exprs = NULL;
    expr_bdl.stmt_in = NULL;
    expr_bdl.avail_anls = 0;
    expr_bdl.index = 0;
    analysis_init();
    analysis_flow_graph(net, 0);
    analysis_show_blks(1, graph_size);

    n_blk = graph_size[0];
    n_stmt = graph_size[1];
    n_edge = graph_size[2];

    exprs = analysis_collect_expr(net, ind, exprs, 1, expr_bdl);
    n_sig2 = analysis_count_sig_expr(exprs);
    analysis_count_l_sigs(n_l_sig);
    // allocate space for signals
    sig_list = (ivl_signal_t *)malloc((n_l_sig[1] + n_sig2) * sizeof(ivl_signal_t));
    if (sig_list == NULL) {
        printf("failed malloc in analysis_process\n");
        exit(2);
    }
    // find all signals
    analysis_place_l_sigs(sig_list);
    n_all_sig = analysis_signal_array(n_l_sig[1], exprs, sig_list);

    block_list = analysis_blk_array(graph_size[0]);
    edges = analysis_edge_array(graph_size[2]);
    stmt_list = analysis_stmt_array(graph_size[1]);

    blk_in_sig = analysis_allocate_signal_array(graph_size[0], n_all_sig);
    blk_out_sig = analysis_allocate_signal_array(graph_size[0], n_all_sig);
    stmt_in_sig = analysis_allocate_signal_array(n_stmt, n_all_sig);
    stmt_out_sig = analysis_allocate_signal_array(n_stmt, n_all_sig);

    evnt = analysis_find_event();

    if (opcode >= 0) {
        analysis_sig_binary(blk_in_sig, blk_out_sig, stmt_in_sig, stmt_out_sig, edges, block_list,
                            opcode, sig_list, evnt, n_l_sig[0], n_all_sig, n_blk, n_stmt, n_edge);
    } else {
        analysis_sig_binary(blk_in_sig, blk_out_sig, stmt_in_sig, stmt_out_sig, edges, block_list,
                            2, sig_list, evnt, n_l_sig[0], n_all_sig, n_blk, n_stmt, n_edge);
        analysis_sig_binary(blk_in_sig, blk_out_sig, stmt_in_sig, stmt_out_sig, edges, block_list,
                            0, sig_list, evnt, n_l_sig[0], n_all_sig, n_blk, n_stmt, n_edge);
    }

#ifdef DEBUG_VEFA_ANALYSIS
    // print signals
    analysis_show_blks(0, graph_size);
    printf("n_l_sig[0]=%d n_l_sig[1]=%d n_sig2=%d n_all_sig=%d\n", n_l_sig[0], n_l_sig[1], n_sig2,
           n_all_sig);
    printf("*************  signals: ");
    printf("\n");
    for (int i = 0; i < n_all_sig; i++) {
        printf("%s file:%s line:%d\n", ivl_signal_basename(sig_list[i]),
               ivl_signal_file(sig_list[i]), ivl_signal_lineno(sig_list[i]));
    }
    printf("\n");
    analysis_show_signal_array(stmt_in_sig, n_stmt, n_all_sig, 0);
#endif

    /*
    analysis_compute_loop_depth(block_list,edges,n_blk,n_edge);
#ifdef DEBUG_VEFA_ANALYSIS
    analysis_show_blks(0,graph_size);
    analysis_show_signal_array(stmt_in_sig,n_stmt,n_sig,0);
    analysis_show_signal_array(stmt_in_sig,n_stmt,n_all_sig,1);
#endif
    n_expr = analysis_mark_expr(exprs,sig_list, n_sig);

    expr_cost = (expr_anls_cost_t) malloc(n_expr * sizeof(struct expr_anls_cost));
    if(expr_cost == NULL){
        printf("malloc failed in analysis_process\n");
        exit(2);
    }
#ifdef DEBUG_VEFA_ANALYSIS
    analysis_show_expr(exprs);
#endif

    blk_in_expr = analysis_allocate_2dim_array(n_blk,n_expr);
    blk_out_expr = analysis_allocate_2dim_array(n_blk,n_expr);
    stmt_in_expr = analysis_allocate_2dim_array(n_stmt,n_expr);
    stmt_out_expr = analysis_allocate_2dim_array(n_stmt,n_expr);
    stmt_mid_expr = analysis_allocate_2dim_array(n_stmt,n_expr);

    for(i=0;i<3;i++){
        analysis_expr_redundancy(blk_in_expr,blk_out_expr,stmt_in_expr,stmt_out_expr,edges,block_list,i,n_expr,n_blk,n_stmt,n_edge);
#ifdef DEBUG_VEFA_ANALYSIS
        //analysis_show_2dim_array(blk_in_expr,n_blk,n_expr,i);
        //analysis_show_2dim_array(blk_out_expr,n_blk,n_expr,i);
        analysis_show_2dim_array(stmt_in_expr,n_stmt,n_expr,i);
        analysis_show_2dim_array(stmt_out_expr,n_stmt,n_expr,i);
#endif
    }

    proc_info->expr_rsv_size =
analysis_calculate_expr_cost(stmt_in_sig,stmt_out_sig,stmt_in_expr,stmt_out_expr,sig_list,expr_cost,n_stmt,n_sig,n_expr);
#ifdef DEBUG_VEFA_ANALYSIS
    printf("used and avail\n");
    for(i=0;i<n_stmt;i++){
        printf("%3d: ",i);
        for(j=0;j<n_expr;j++){
            if((*(stmt_in_expr+i)+j)->avail & (*(stmt_in_expr+i)+j)->used){
                printf("1 ");
            }else{
                printf("0 ");
            }
        }
        printf("\n");
    }
    printf("used and !avail\n");
    for(i=0;i<n_stmt;i++){
        printf("%3d: ",i);
        for(j=0;j<n_expr;j++){
            if(!(*(stmt_in_expr+i)+j)->avail & (*(stmt_in_expr+i)+j)->used){
                printf("1 ");
            }else{
                printf("0 ");
            }
        }
        printf("\n");
    }
    printf("!used and avail\n");
    for(i=0;i<n_stmt;i++){
        printf("%3d: ",i);
        for(j=0;j<n_expr;j++){
            if((*(stmt_in_expr+i)+j)->avail & !(*(stmt_in_expr+i)+j)->used){
                printf("1 ");
            }else{
                printf("0 ");
            }
        }
        printf("\n");
    }
#endif
    */
    proc_info->n_stmt = n_stmt;
    proc_info->n_l_sig = n_l_sig[1];
    proc_info->n_sig = n_all_sig;
    proc_info->n_expr = n_expr;
    proc_info->n_edge = n_edge;
    proc_info->sig_list = sig_list;
    proc_info->block_list = block_list;
    proc_info->edges = edges;
    proc_info->stmt_in = stmt_in_sig;
    proc_info->stmt_list = stmt_list;
    proc_info->exprs = exprs;
    proc_info->stmt_in_expr = stmt_in_expr;
    proc_info->stmt_out_expr = stmt_out_expr;
    proc_info->stmt_mid_expr = stmt_mid_expr;
    proc_info->loop_mode = 0;

    // free memory for signals, blocks and edges

    analysis_free_signal_array(blk_in_sig, graph_size[0]);
    analysis_free_signal_array(blk_out_sig, graph_size[0]);
    // analysis_free_signal_array(stmt_in_sig,n_stmt);
    analysis_free_signal_array(stmt_out_sig, n_stmt);

    link_list_t p1;
    while (l_sig_list) {
        p1 = l_sig_list->next;
        free(l_sig_list);
        l_sig_list = p1;
    }
    // analysis_free_process_info(proc_info);
}

/****************expression****************/
static int analysis_expr_depend_linear_sig(struct link_list *sig_dpnd,
                                           struct expr_companion2 signals) {
    link_list_t p;
    p = sig_dpnd;
    while (p) {
        for (int i = 0; i < signals.n_sig; i++) {
            if (*(signals.sig_list + i) == p->item && (signals.stmt_in_sig + i)->loop_linear) {
                return 1;
            }
        }
        p = p->next;
    }
    return 0;
}

static vefa_expr_info_t analysis_binary_expression(ivl_expr_t net, unsigned ind,
                                                   struct expr_companion2 signals) {
    vefa_expr_info_t expr_p;
    vefa_expr_info_t expr_p1 = NULL;
    vefa_expr_info_t expr_p2 = NULL;
    link_list_t p;
    ivl_expr_t oper1 = ivl_expr_oper1(net);
    ivl_expr_t oper2 = ivl_expr_oper2(net);
    if (oper1) {
        expr_p1 = analysis_expression(oper1, ind + 3, signals);
    } else {
        PINT_UNSUPPORTED_WARN(ivl_expr_file(net), ivl_expr_lineno(net), "ERROR: Missing operand 1");
        return NULL;
    }
    if (oper2) {
        expr_p2 = analysis_expression(oper2, ind + 3, signals);
    } else {
        PINT_UNSUPPORTED_WARN(ivl_expr_file(net), ivl_expr_lineno(net), "ERROR: Missing operand 2");
        return NULL;
    }
    expr_p = vefa_malloc_expr_info();
    expr_p->is_binary = expr_p1->is_binary & expr_p2->is_binary;

    p = expr_p1->sig_dpnd;
    if (p == NULL) {
        expr_p->sig_dpnd = expr_p2->sig_dpnd;
    } else {
        while (p->next)
            p = p->next;
        p->next = expr_p2->sig_dpnd;
        expr_p->sig_dpnd = expr_p1->sig_dpnd;
    }
    // linear analysis
    if (ivl_expr_opcode(net) == '+') {
        expr_p->loop_linear = expr_p1->loop_linear + expr_p2->loop_linear;
    } else {
        if (analysis_expr_depend_linear_sig(expr_p->sig_dpnd, signals)) {
            expr_p->loop_linear = 2;
        }
    }
    // bit in vector
    switch (ivl_expr_opcode(net)) {

    case '<':
    case '>':
    case 'e':
    case 'n':
    case 'N':
    case 'E':
    case 'G':
    case '+':
    case '-':
        break;
    case '&':
    case 'a':
    case '^':
    case 'X':
    case '|':
    case 'o':
        if (expr_p1->biv == 1 && expr_p2->biv == 1) {
            expr_p->biv = 1;
        }
        break;
    case '*':
    case '/':
    case '%':
    case 'l':
    case 'r':
        break;
    default:
        if (ivl_expr_width(oper1) != ivl_expr_width(oper2)) {
            PINT_UNSUPPORTED_NOTE(ivl_expr_file(net), ivl_expr_lineno(net),
                                  "%c has different width", ivl_expr_opcode(net));
        }
        break;
    }
    expr_p1->sig_dpnd = NULL;
    expr_p2->sig_dpnd = NULL;
    vefa_free_expr_info(expr_p1);
    vefa_free_expr_info(expr_p2);
    return expr_p;
}

static vefa_expr_info_t analysis_ternary_expression(ivl_expr_t net, unsigned ind,
                                                    struct expr_companion2 signals) {
    vefa_expr_info_t expr_p = vefa_malloc_expr_info();
    vefa_expr_info_t expr_p1, expr_p2 = NULL, expr_p3 = NULL;

    expr_p1 = analysis_expression(ivl_expr_oper1(net), ind + 4, signals);
    expr_p2 = analysis_expression(ivl_expr_oper2(net), ind + 4, signals);
    expr_p3 = analysis_expression(ivl_expr_oper3(net), ind + 4, signals);
    expr_p->is_binary = expr_p1->is_binary & expr_p2->is_binary & expr_p3->is_binary;

    expr_p->sig_dpnd = expr_p3->sig_dpnd;

    if (expr_p1->sig_dpnd != NULL) {
        struct link_list *link_p = expr_p1->sig_dpnd;
        while (link_p->next)
            link_p = link_p->next;
        link_p->next = expr_p->sig_dpnd;
        expr_p->sig_dpnd = expr_p1->sig_dpnd;
    }
    if (expr_p2->sig_dpnd != NULL) {
        struct link_list *link_p = expr_p2->sig_dpnd;
        while (link_p->next)
            link_p = link_p->next;
        link_p->next = expr_p->sig_dpnd;
        expr_p->sig_dpnd = expr_p2->sig_dpnd;
    }

    if (expr_p1->loop_linear == 0 && expr_p2->loop_linear == 1 && expr_p3->loop_linear == 1) {
        expr_p->loop_linear = 1;
    } else if (expr_p1->loop_linear == 0 && expr_p2->loop_linear == 0 &&
               expr_p3->loop_linear == 0) {
        expr_p->loop_linear = 0;
    } else {
        expr_p->loop_linear = 2;
    }

    if (expr_p1->loop_linear == 0 && expr_p2->biv == 1 && expr_p3->biv == 1) {
        expr_p->biv = 1;
    }

    expr_p1->sig_dpnd = NULL;
    expr_p2->sig_dpnd = NULL;
    expr_p3->sig_dpnd = NULL;
    vefa_free_expr_info(expr_p1);
    vefa_free_expr_info(expr_p2);
    vefa_free_expr_info(expr_p3);
    return expr_p;
}

static vefa_expr_info_t analysis_unary_expression(ivl_expr_t net, unsigned ind,
                                                  struct expr_companion2 signals) {
    vefa_expr_info_t expr_p = analysis_expression(ivl_expr_oper1(net), ind + 4, signals);
    if (expr_p->loop_linear > 0)
        expr_p->loop_linear = 2;

    switch (ivl_expr_opcode(net)) {
    case '~':
        // will be the same as oper1
        break;
    case 'N':
    case '!':
    case '&':
    case 'A':
    case '^':
    case '|':
        expr_p->biv = 0;
        break;
    default:
        PINT_UNSUPPORTED_NOTE(ivl_expr_file(net), ivl_expr_lineno(net),
                              "ERROR unknown operator: %c", ivl_expr_opcode(net));
        break;
    }
    return expr_p;
}

static vefa_expr_info_t analysis_signal_expression(ivl_expr_t net, unsigned ind,
                                                   struct expr_companion2 signals) {
    vefa_expr_info_t expr_p1;
    vefa_expr_info_t expr_p = vefa_malloc_expr_info();
    ivl_expr_t word = ivl_expr_oper1(net);

    ivl_signal_t sig = ivl_expr_signal(net);

    expr_p->sig_dpnd = (struct link_list *)malloc(sizeof(struct link_list));
    if (expr_p->sig_dpnd == NULL) {
        printf("malloc failed in analysis_signal_expression\n");
        exit(2);
    }
    expr_p->sig_dpnd->item = (void *)sig;
    expr_p->sig_dpnd->next = NULL;

    if (word == 0) {
        for (int i = 0; i < signals.n_sig; i++) {
            if (*(signals.sig_list + i) == sig && (signals.stmt_in_sig + i)->binary) {
                expr_p->is_binary = 1;
            }
            if (*(signals.sig_list + i) == sig && (signals.stmt_in_sig + i)->loop_linear) {
                expr_p->loop_linear = 1;
            }
        }
    } else {
        // sig will not match loop_linear var, because this sig is array, but loop_linear var should
        // not be an array Need to check if the index is depends on loop_linear var.
        expr_p1 = analysis_expression(word, ind, signals);
        if (expr_p1->sig_dpnd != NULL) {
            struct link_list *link_p = expr_p1->sig_dpnd;
            while (link_p->next)
                link_p = link_p->next;
            link_p->next = expr_p->sig_dpnd;
            expr_p->sig_dpnd = expr_p1->sig_dpnd;
        }
        if (analysis_expr_depend_linear_sig(expr_p->sig_dpnd, signals)) {
            expr_p->loop_linear = 2;
        }
        expr_p1->sig_dpnd = NULL;
        vefa_free_expr_info(expr_p1);
    }

    expr_p->biv = 0; // either signal or array element
    return expr_p;
}

// update sig_dpnd and is_binary
vefa_expr_info_t analysis_expression(ivl_expr_t net, unsigned ind, struct expr_companion2 signals) {
    vefa_expr_info_t expr_p = NULL;
    vefa_expr_info_t expr_p1, expr_p2;
    link_list_t link_p;
    unsigned idx;
    const ivl_expr_type_t code = ivl_expr_type(net);

    switch (code) {

    case IVL_EX_ARRAY:
        expr_p = vefa_malloc_expr_info();
        break;

    case IVL_EX_BINARY:
        expr_p = analysis_binary_expression(net, ind, signals);
        break;

    case IVL_EX_CONCAT:
        expr_p = vefa_malloc_expr_info();
        expr_p->is_binary = 1;
        for (idx = 0; idx < ivl_expr_parms(net); idx += 1) {
            expr_p1 = analysis_expression(ivl_expr_parm(net, idx), ind + 3, signals);
            if (expr_p1->is_binary == 0)
                expr_p->is_binary = 0;
            if (expr_p1->sig_dpnd != NULL) {
                link_p = expr_p1->sig_dpnd;
                while (link_p->next)
                    link_p = link_p->next;
                link_p->next = expr_p->sig_dpnd;
                expr_p->sig_dpnd = expr_p1->sig_dpnd;
            }
            expr_p1->sig_dpnd = NULL;
            vefa_free_expr_info(expr_p1);
        }
        if (analysis_expr_depend_linear_sig(expr_p->sig_dpnd, signals)) {
            expr_p->loop_linear = 2;
        }

        expr_p->biv = 0;
        break;

    case IVL_EX_NUMBER:
        expr_p = anls_show_number_expression(net, 1);
        if (ivl_expr_width(net) == 1)
            expr_p->biv = 1;
        break;

    case IVL_EX_SELECT:
        expr_p = vefa_malloc_expr_info();
        expr_p1 = analysis_expression(ivl_expr_oper1(net), ind + 3, signals);
        expr_p->sig_dpnd = expr_p1->sig_dpnd;
        if (ivl_expr_oper2(net)) {
            expr_p2 = analysis_expression(ivl_expr_oper2(net), ind + 3, signals);
            expr_p->is_binary = expr_p1->is_binary & expr_p2->is_binary;

            if (expr_p2->sig_dpnd != NULL) {
                link_p = expr_p2->sig_dpnd;
                while (link_p->next)
                    link_p = link_p->next;
                link_p->next = expr_p->sig_dpnd;
                expr_p->sig_dpnd = expr_p2->sig_dpnd;
            }

            if (analysis_expr_depend_linear_sig(expr_p->sig_dpnd, signals)) {
                expr_p->loop_linear = 2;
            }
            if (ivl_expr_width(net) == 1 && expr_p1->loop_linear == 0 &&
                expr_p2->loop_linear == 1) {
                expr_p->biv = 1;
            }
            expr_p2->sig_dpnd = NULL;
            vefa_free_expr_info(expr_p2);

        } else {
            expr_p->is_binary = expr_p1->is_binary & (ivl_expr_width(net) <= 32);
            expr_p->loop_linear = expr_p1->loop_linear;
            expr_p->biv = expr_p1->biv;
        }
        expr_p1->sig_dpnd = NULL;
        vefa_free_expr_info(expr_p1);
        break;

    case IVL_EX_STRING:
        expr_p = vefa_malloc_expr_info();
        break;

    case IVL_EX_SFUNC:
        expr_p = vefa_malloc_expr_info();
        for (idx = 0; idx < ivl_expr_parms(net); idx += 1) {
            expr_p1 = analysis_expression(ivl_expr_parm(net, idx), ind + 3, signals);
            if (expr_p1->sig_dpnd != NULL) {
                link_p = expr_p1->sig_dpnd;
                while (link_p->next)
                    link_p = link_p->next;
                link_p->next = expr_p->sig_dpnd;
                expr_p->sig_dpnd = expr_p1->sig_dpnd;
            }
            expr_p1->sig_dpnd = NULL;
            vefa_free_expr_info(expr_p1);
        }
        if (analysis_expr_depend_linear_sig(expr_p->sig_dpnd, signals)) {
            expr_p->loop_linear = 2;
        }
        break;

    case IVL_EX_SIGNAL:
        expr_p = analysis_signal_expression(net, ind, signals);
        break;

    case IVL_EX_TERNARY:
        expr_p = analysis_ternary_expression(net, ind, signals);
        break;

    case IVL_EX_UNARY:
        expr_p = analysis_unary_expression(net, ind, signals);
        break;

    case IVL_EX_UFUNC:
        expr_p = vefa_malloc_expr_info();
        for (idx = 0; idx < ivl_expr_parms(net); idx += 1) {
            expr_p1 = analysis_expression(ivl_expr_parm(net, idx), ind + 3, signals);
            if (expr_p1->sig_dpnd != NULL) {
                link_p = expr_p1->sig_dpnd;
                while (link_p->next)
                    link_p = link_p->next;
                link_p->next = expr_p->sig_dpnd;
                expr_p->sig_dpnd = expr_p1->sig_dpnd;
            }
            expr_p1->sig_dpnd = NULL;
            vefa_free_expr_info(expr_p1);
        }
        if (analysis_expr_depend_linear_sig(expr_p->sig_dpnd, signals)) {
            expr_p->loop_linear = 2;
        }
        break;

    default:
        expr_p = vefa_malloc_expr_info();
        PINT_UNSUPPORTED_NOTE(ivl_expr_file(net), ivl_expr_lineno(net),
                              "analysis_expression <expr_type=%u>", code);
        break;
    }

    return expr_p;
}

// #pragma GCC pop_options
