#pragma once
#ifndef IVL_USE_DEF_H
#define IVL_USE_DEF_H
// clang-format off

#include "dataflow.h"
#include "syntax_visit.h"

#include <type_traits>
#include <memory>
#include <vector>
#include <tuple>

struct sig_ud_s;
using signal_ud_list_t = std::set<sig_ud_s>;
using ud_solver_t = DFA_solver_t<ivl_statement_t, sig_ud_s>;

struct sig_ud_s {
    ivl_signal_t m_sig;                // key
    std::set<ivl_statement_t> m_stmts; // position that signal defined

    sig_ud_s(const ivl_signal_t &sig) : m_sig(sig) {}
    sig_ud_s(const ivl_signal_t &sig, const std::set<ivl_statement_t> &stmts)
        : m_sig(sig), m_stmts(stmts) {}
    sig_ud_s() {}
    bool operator<(const sig_ud_s &r) const { return this->m_sig < r.m_sig; }
    bool operator==(const sig_ud_s &r) const {
        if (this->m_sig == r.m_sig) {
            return (this->m_stmts == r.m_stmts);
        } else {
            return false;
        }
    }
};

inline void replace_ud(signal_ud_list_t &list, const sig_ud_s &e) {
    auto sig = e.m_sig;
    auto search = list.find(sig);
    if (search == list.end()) { // not found.
        list.emplace(e);        // insert immediately
    } else {                    // found
        assert(e.m_sig == search->m_sig);
        // replace stmts
        list.erase(search);
        list.emplace(e);
    }
}

// It's usually used for union. (C = A U B)
inline void append_ud(signal_ud_list_t &list, const sig_ud_s &e) {
    auto sig = e.m_sig;
    auto search = list.find(sig);
    if (search == list.end()) { // not found.
        list.emplace(e);        // insert immediately
    } else {                    // found
        assert(e.m_sig == search->m_sig);
        // merge stmts
        sig_ud_s tt = *search;
        tt.m_stmts.insert(e.m_stmts.begin(), e.m_stmts.end());
        list.erase(search);
        list.emplace(std::move(tt));
    }
}

// clang-format off
inline int null_gen(signal_ud_list_t &, const ivl_statement_t &) { return 0; }
inline int null_kill(signal_ud_list_t &, const ivl_statement_t &) { return 0; }
inline int null_meet(signal_ud_list_t &, const signal_ud_list_t &, const signal_ud_list_t &) {return 0;}
inline int null_transfer(signal_ud_list_t &, const signal_ud_list_t &, const signal_ud_list_t &, const signal_ud_list_t &) {return 0;}
// clang-format on

//// definition implementation
//============================================================================================
// phase 1
inline int definition_gen(signal_ud_list_t &gen_out, const ivl_statement_t &net) {
    assert(net);
    assert(gen_out.empty());

    auto code = ivl_statement_type(net);
    if (code == IVL_ST_ASSIGN || code == IVL_ST_ASSIGN_NB) {
        unsigned int n_lval = ivl_stmt_lvals(net);
        for (unsigned int i = 0; i < n_lval; i++) {
            ivl_lval_t lval = ivl_stmt_lval(net, i);
            ivl_signal_t sig = ivl_lval_sig(lval);

            if (ivl_signal_array_count(sig) == 1 &&
                ivl_lval_part_off(lval) == NULL) // not array && not part-select
            {
                sig_ud_s e;
                e.m_sig = sig;
                e.m_stmts.emplace(net);
                gen_out.emplace(std::move(e));
            }
        }
    }
    return 0;
}
inline int def_gen_all(signal_ud_list_t &gen_out, const ivl_statement_t &net) {
    assert(net);
    assert(gen_out.empty());

    auto code = ivl_statement_type(net);
    if (code == IVL_ST_ASSIGN || code == IVL_ST_ASSIGN_NB) {
        unsigned int n_lval = ivl_stmt_lvals(net);
        for (unsigned int i = 0; i < n_lval; i++) {
            ivl_lval_t lval = ivl_stmt_lval(net, i);
            ivl_signal_t sig = ivl_lval_sig(lval);

            // if (ivl_signal_array_count(sig) == 1 &&
            //    ivl_lval_part_off(lval) == NULL) // not array && not part-select
            {
                sig_ud_s e;
                e.m_sig = sig;
                e.m_stmts.emplace(net);
                gen_out.emplace(std::move(e));
            }
        }
    }
    return 0;
}

// dfa abstract is ivl_statement_t, basic block is ivl_statement_t
inline int definition_transfer(signal_ud_list_t &out, const signal_ud_list_t &in,
                               const signal_ud_list_t &gen, const signal_ud_list_t &kill) {
    // out = gen U (in-kill)
    assert(kill.empty());
    out = in;
    for (const auto &defs : gen) {
        if (!defs.m_stmts.empty()) {
            auto search = out.find(defs);
            if (search == out.end()) { // not found
                // add `defs` to out-set
                out.emplace(defs);
            } else { // found
                // replace(or killed) out-set by defs.m_stmts.
                sig_ud_s e = *search;
                assert(e.m_sig == defs.m_sig);

                e.m_stmts.insert(defs.m_stmts.begin(), defs.m_stmts.end());
                out.erase(search);
                out.insert(std::move(e));
            }
        }
    }

    return 0;
}

// in = in1 U in2
// in = in U in1 would be better
inline int definition_meet(signal_ud_list_t &in, const signal_ud_list_t &in1,
                           const signal_ud_list_t &in2) {
    signal_ud_list_t tmp_out = in1; // using tmp_out because in/in1/in2 could be the same.
    for (const auto &defs : in2) {
        auto search = tmp_out.find(defs);
        if (search == tmp_out.end()) { // not found
            tmp_out.emplace(defs);
        } else { // found
            sig_ud_s e = *search;
            assert(e.m_sig == defs.m_sig);

            e.m_stmts.insert(defs.m_stmts.begin(), defs.m_stmts.end());
            tmp_out.erase(search);
            tmp_out.insert(std::move(e));
        }
    }
    in = tmp_out;
    return 0;
}

// in = in1 n in2
// in = in n in1 would be better
inline int definition_meet_anticipate(signal_ud_list_t &in, const signal_ud_list_t &in1,
                                      const signal_ud_list_t &in2) {
    signal_ud_list_t tmp_out;

    for (const auto &defs : in2) {
        auto search = in1.find(defs);
        if (search != in1.end()) {                                  // found
            if (!(search->m_stmts.empty() || defs.m_stmts.empty())) // no definition
            {
                // merge stmts
                sig_ud_s e = *search;
                e.m_stmts.insert(defs.m_stmts.begin(), defs.m_stmts.end());
                tmp_out.emplace(std::move(e));
            }
        }
    }
    in = tmp_out;
    return 0;
}

//// use implementation
//============================================================================================
//// find all signals in current expression
//// NOTE: the signal that appears in the expr must be used. it cannot be
/// assigned.
// class find_expr_sigs_t : public expr_base_visitor_s{
// public:
//    VISITOR_DECL_BEGIN_V2(find_expr_sigs_t);
//
// public:
//    // append result in user_data
//    VisitFuncDef(IVL_EX_SIGNAL, net, user_data)
//    {
//        auto& uses = *static_cast<std::set<ivl_signal_t>*>(user_data);
//        //assert(uses.empty());
//        ivl_signal_t sig = ivl_expr_signal(net);
//        uses.emplace(sig);
//
//        //signal_ud_list_t& ud_list = *user_data;
//        //ivl_signal_t sig = ivl_expr_signal(net);
//        //sig_ud_s e;
//        //e.m_sig = sig;
//        //e.m_stmts.emplace(net);
//        //ud_list.emplace(std::move(e));
//
//        return 0;
//    }
//};

// find all signals in current expression
// NOTE: the signal that appears in the expr must be used. it cannot be
// assigned.
class find_expr_sigs_t : public expr_visitor_s {
    VISITOR_DECL_BEGIN_V3

  public:
    // append result in user_data
    visit_result_t Visit_SIGNAL(ivl_expr_t net, ivl_expr_t, void *user_data) override {
        auto &uses = *static_cast<std::set<ivl_signal_t> *>(user_data);
        ivl_signal_t sig = ivl_expr_signal(net);
        uses.emplace(sig);
        return VisitResult_Recurse;
    }
};

// Collect all signals that be used in current 'net'. (Will not visite the sub-stmt)
inline int collect_all_sigs_exclude_lval(const ivl_statement_t &net, std::set<ivl_signal_t> &sigs) {
    find_expr_sigs_t sigs_finder;

    auto code = ivl_statement_type(net);

    switch (code) {
    case IVL_ST_ASSIGN:
    case IVL_ST_ASSIGN_NB:
    case IVL_ST_CASSIGN:
    case IVL_ST_DEASSIGN: {
        ivl_expr_t r_expr = ivl_stmt_rval(net);
        assert(r_expr);

        // collect use of signals at the right side of '='
        sigs_finder(r_expr, &sigs);

        // collect use of signals at the left side of '='.
        for (unsigned int idx = 0; idx < ivl_stmt_lvals(net); idx++) {
            ivl_lval_t lval = ivl_stmt_lval(net, idx);

            ivl_lval_t lval_nest = ivl_lval_nest(lval);
            assert(lval_nest == nullptr && "not support lvalue-nest.");

            ivl_signal_t sig = ivl_lval_sig(lval);
            assert(sig);

            ivl_expr_t sub_idx = ivl_lval_idx(lval);
            if (sub_idx) {
                sigs_finder(sub_idx, &sigs);
            } else if (ivl_signal_array_count(sig) > 1) {
                fprintf(stderr, "%*sERROR: Address missing on signal with "
                                "word count=%u\n",
                        0, "", ivl_signal_array_count(sig));
                std::exit(-1);
                // stub_errors += 1;
            }

            ivl_expr_t loff = ivl_lval_part_off(lval);
            if (loff) {
                // fprintf(out, "%*sPart select base:\n", ind+4, "");
                sigs_finder(loff, &sigs);
            }
        }
        break;
    }
    case IVL_ST_CASE:
    case IVL_ST_CASEX:
    case IVL_ST_CASEZ: {
        ivl_expr_t ex = ivl_stmt_cond_expr(net);
        assert(ex);
        sigs_finder(ex, &sigs);

        unsigned int count = ivl_stmt_case_count(net);
        assert(count > 0);
        // unsigned int default_case = count;
        for (unsigned int idx = 0; idx < count; idx++) {
            ivl_expr_t cex = ivl_stmt_case_expr(net, idx);
            if (cex) {
                sigs_finder(cex, &sigs);
            }
        }
        break;
    }
    case IVL_ST_CONDIT: {
        auto e = ivl_stmt_cond_expr(net);
        assert(e);
        sigs_finder(e, &sigs);
        break;
    }
    case IVL_ST_CONTRIB: {
        // not support
        // ivl_expr_t lval = ivl_stmt_lexp(net);
        // ivl_expr_t rval = ivl_stmt_rval(net);
        NetAssertWrongPath(net);
        break;
    }
    case IVL_ST_DELAYX: {
        ivl_expr_t ex = ivl_stmt_delay_expr(net);
        sigs_finder(ex, &sigs);
        break;
    }
    case IVL_ST_STASK: {
        for (unsigned int i = 0; i < ivl_stmt_parm_count(net); i++) {
            ivl_expr_t e = ivl_stmt_parm(net, i);
            assert(e);
            sigs_finder(e, &sigs);
        }
        break;
    }
    case IVL_ST_WHILE:
    case IVL_ST_REPEAT:
    case IVL_ST_DO_WHILE: {
        ivl_expr_t e = ivl_stmt_cond_expr(net);
        assert(e);
        sigs_finder(e, &sigs);
        break;
    }
    default: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
    }
    }

    return 0;
}

// collect all use of signals in current statement
inline int use_gen(signal_ud_list_t &gen_out, const ivl_statement_t &net) {
    assert(net);
    assert(gen_out.empty());

    std::set<ivl_signal_t> sigs;
    int re = collect_all_sigs_exclude_lval(net, sigs);

    for (const auto &sig : sigs) {
        replace_ud(gen_out, {sig, {net}});
    }

    return re;
}

// collect all definition of signals in current statement.
inline int use_kill(signal_ud_list_t &kill_out, const ivl_statement_t &net) {
    assert(net);
    assert(kill_out.empty());

    auto code = ivl_statement_type(net);
    if (code == IVL_ST_ASSIGN || code == IVL_ST_ASSIGN_NB) {
        unsigned int n_lval = ivl_stmt_lvals(net);
        for (unsigned int i = 0; i < n_lval; i++) {
            ivl_lval_t lval = ivl_stmt_lval(net, i);
            ivl_signal_t sig = ivl_lval_sig(lval);

            if (ivl_signal_array_count(sig) == 1 &&
                ivl_lval_part_off(lval) == NULL) // not array && not part-select
            {
                // include array signal and part-assign signal
                sig_ud_s e;
                e.m_sig = sig;
                e.m_stmts.emplace(net);
                kill_out.emplace(std::move(e));
            }
        }
    }

    return 0;
}

// include part and array signal
inline int use_kill_allsignal(signal_ud_list_t &kill_out, const ivl_statement_t &net) {
    assert(net);
    assert(kill_out.empty());

    auto code = ivl_statement_type(net);
    if (code == IVL_ST_ASSIGN || code == IVL_ST_ASSIGN_NB) {
        unsigned int n_lval = ivl_stmt_lvals(net);
        for (unsigned int i = 0; i < n_lval; i++) {
            ivl_lval_t lval = ivl_stmt_lval(net, i);
            ivl_signal_t sig = ivl_lval_sig(lval);

            // if (ivl_signal_array_count(sig) == 1 &&
            //    ivl_lval_part_off(lval) == NULL) // not array && not part-select
            {
                // include array signal and part-assign signal
                sig_ud_s e;
                e.m_sig = sig;
                e.m_stmts.emplace(net);
                kill_out.emplace(std::move(e));
            }
        }
    }

    return 0;
}
// NOTE:
// use-analysis use backward iterate method.
// so 'DFA_node_s.in' is out-flowvalue of curent vertex,
//    'DFA_node_s.out' is in-flowvalue of current vertex.
inline int use_transfer(signal_ud_list_t &in,         // fv-in
                        const signal_ud_list_t &out,  // fv-out
                        const signal_ud_list_t &uses, // gen
                        const signal_ud_list_t &defs  // kill
                        ) {
    /* in = use U (out - def) */
    // 1. in = out - def
    in = out;
    for (const auto &def : defs) {
        assert(!def.m_stmts.empty());
        auto search = in.find(def);
        if (search != in.end()) { // found
            // kill all stmts(use) of current signal
            in.erase(search);
        }
    }

    // 2. in = use U in
    for (const auto &use : uses) {
        assert(!use.m_stmts.empty());
        append_ud(in, use);
    }

    return 0;
}

inline int use_meet(signal_ud_list_t &out, const signal_ud_list_t &out1,
                    const signal_ud_list_t &out2) {
    auto tmp = out1; // WARNING: using tmp because in/in1/in2 could be the same.
    for (const auto &use : out2) {
        assert(!use.m_stmts.empty());
        append_ud(tmp, use);
    }
    out = tmp;
    return 0;
}

// in = in1 n in2
// in = in n in1 would be better
inline int use_meet_anticipate(signal_ud_list_t &out, const signal_ud_list_t &out1,
                               const signal_ud_list_t &out2) {
    signal_ud_list_t tmp_out;
    for (const auto &use : out2) {
        auto search = out1.find(use);
        if (search != out1.end()) { // found
            assert(!search->m_stmts.empty());
            // merge stmts
            sig_ud_s e = *search;
            e.m_stmts.insert(use.m_stmts.begin(), use.m_stmts.end());
            tmp_out.emplace(std::move(e));
        }
    }
    out = tmp_out;
    return 0;
}

// collect all use of signals in current statement
inline int sensitive_gen(signal_ud_list_t &gen_out, const ivl_statement_t &net) {
    assert(net);
    assert(gen_out.empty());

    std::set<ivl_signal_t> sigs;
    int re = collect_all_sigs_exclude_lval(net, sigs);

    for (const auto &sig : sigs) {
        replace_ud(gen_out, {sig, {net}});
    }

    return re;
}

// collect all use of signals in current statement
inline int sensitive_kill(signal_ud_list_t &gen_out, const ivl_statement_t &net) {
    assert(net);
    assert(gen_out.empty());

    auto code = ivl_statement_type(net);
    if (code == IVL_ST_ASSIGN || code == IVL_ST_ASSIGN_NB) {
        unsigned int n_lval = ivl_stmt_lvals(net);
        for (unsigned int i = 0; i < n_lval; i++) {
            ivl_lval_t lval = ivl_stmt_lval(net, i);
            ivl_signal_t sig = ivl_lval_sig(lval);

            if (ivl_signal_array_count(sig) == 1 &&
                ivl_lval_part_off(lval) == NULL) // not array && not part-select
            {
                sig_ud_s e;
                e.m_sig = sig;
                e.m_stmts.emplace(net);
                gen_out.emplace(std::move(e)); // kill
                                               // auto search = gen_out.find(sig);
                                               // if(search == gen_out.end()){ // not found.
                                               //  sig_ud_s e;//element
                                               //  e.m_sig = sig;
                                               //  e.m_stmts.emplace(net);
                                               //  gen_out.emplace(std::move(e));
                                               //}
                                               // else{ // found
                                               //  sig_ud_s e = *search; // copy current element
                                               //  e.m_stmts.emplace(net);
                                               //  gen_out.erase(search);
                                               //  gen_out.insert(std::move(e));
                                               //}
            }
        }
    }
    return 0;
}

inline int sensitive_meet(signal_ud_list_t &out, const signal_ud_list_t &out1,
                          const signal_ud_list_t &out2) {
    auto tmp = out1; // WARNING: using tmp because in/in1/in2 could be the same.
    for (const auto &use : out2) {
        assert(!use.m_stmts.empty());
        append_ud(tmp, use);
    }
    out = tmp;
    return 0;
}

// NOTE:
// use-analysis use backward iterate method.
// so 'DFA_node_s.in' is out-flowvalue of curent vertex,
//    'DFA_node_s.out' is in-flowvalue of current vertex.
inline int sensitive_transfer(signal_ud_list_t &in,         // fv-in
                              const signal_ud_list_t &out,  // fv-out
                              const signal_ud_list_t &uses, // gen
                              const signal_ud_list_t &defs  // kill
                              ) {
    /* in = use U (out - def) */
    // 1. in = out - def
    in = out;
    for (const auto &def : defs) {
        assert(!def.m_stmts.empty());
        auto search = in.find(def);
        if (search != in.end()) { // found
            // kill all stmts(use) of current signal
            in.erase(search);
        }
    }

    // 2. in = use U in
    for (const auto &use : uses) {
        assert(!use.m_stmts.empty());
        append_ud(in, use);
    }

    return 0;
}
template <typename Graph, typename Vertex = typename boost::graph_traits<Graph>::vertex_descriptor>
void ud_v_printer(FILE *p_file, const Vertex &s, const Graph &g) {
    const auto &vp = g[s];

    ivl_statement_t bb = vp.bb;
    const signal_ud_list_t &in = vp.in;
    const signal_ud_list_t &out = vp.out;
    const signal_ud_list_t &gen = vp.gen;
    const signal_ud_list_t &kill = vp.kill;

    std::string label_content;
    label_content.reserve(256);

    auto print_flow_value = [&label_content](const signal_ud_list_t &fv_set) {
        for (const auto &e : fv_set) {
            auto sig = e.m_sig;
            const char *sig_name = ivl_signal_basename(sig);

            label_content.append(sig_name);
            label_content.append("[");

            for (const auto &stmt : e.m_stmts) {
                assert(stmt);
                unsigned int lineno = ivl_stmt_lineno(stmt);

                label_content.append(std::to_string(lineno));
                label_content.append(",");
            }

            label_content.append("],\n");
        }
    };

    /*
      format:
      in:\n
      a[1,2,3,],\n
      b[1,],\n
    */
    label_content.append("in:\n");
    print_flow_value(in);

    label_content.append("gen:\n");
    print_flow_value(gen);

    label_content.append("kill:\n");
    print_flow_value(kill);

    label_content.append("out:\n");
    print_flow_value(out);

    label_content.append("stmttype:");
    label_content.append(std::to_string(bb == nullptr ? -1 : (int)ivl_statement_type(bb)) + "\n");

    // if (s==m_entry || s==m_exit)
    if (bb == nullptr) {
        // 0 is entry
        fprintf(p_file, "V%lld [label=\"%s\n%s\"];\n", (long long)s, s == 0 ? "Entry" : "Exit",
                label_content.c_str());
    } else {
        assert(bb);
        fprintf(p_file, "V%lld [label=\"lineno=%d\n%s\"];\n", (long long)s,
                (int)ivl_stmt_lineno(bb), label_content.c_str());
    }
};


// clang-format on
#endif
